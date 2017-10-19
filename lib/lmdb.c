/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmdb.c
 *
 * lmcount database API
 *
 */

#include "lmdb.h"
#include "lmlog.h"
#include "util_fns.h"

#include <sqlite3.h>
#include <sys/stat.h>
#include <limits.h>
#include <regex.h>

//

#ifndef LMDB_DISABLE_RRDTOOL

#include <rrd.h>

static const char		*__db_get_all_feature_counts_query =
		"SELECT issued, in_use, checked_timestamp FROM counts"
		"  WHERE feature_id = ?"
		"  ORDER BY checked_timestamp ASC";

typedef char			rrd_point_str_type[64];  /* %lld:%d   => max length should be 43 (with NUL), so 64 is very safe */

bool
__lmdb_rrd_create(
	const char		*rrd_path,
	int						feature_id,
	const char		*feature_string,
	sqlite3_stmt	*counts_query
)
{
	char					count_ts_str[32], ds_str[PATH_MAX];
	const char*   argv[12];
	int						query_rc, rc, argc, points;
	int						issued, in_use;
	sqlite3_int64	count_ts;
	
	// Get the first row:
	if ( counts_query ) {
		query_rc = sqlite3_step(counts_query);
		switch ( query_rc ) {
	
			case SQLITE_DONE:
				count_ts = (sqlite3_int64)time(NULL);
				rc = snprintf(ds_str, sizeof(ds_str), "DS:in_use:GAUGE:600:0:U");
				if ( rc >= sizeof(ds_str) ) {
					lmlogf(lmlog_level_error, "DS definition length %d exceeds max length %d", rc, (int)sizeof(ds_str));
					return false;
				}
				break;
			
			case SQLITE_ROW:
				issued = sqlite3_column_int(counts_query, 0);
				count_ts = sqlite3_column_int64(counts_query, 2);
				rc = snprintf(ds_str, sizeof(ds_str), "DS:in_use:GAUGE:600:0:%d", issued);
				if ( rc >= sizeof(ds_str) ) {
					lmlogf(lmlog_level_error, "DS definition length %d exceeds max length %d", rc, (int)sizeof(ds_str));
					return false;
				}
				break;
		
			default:
				lmlogf(lmlog_level_error, "failed data lookup query (SQLite3 error %d)", query_rc);
				return false;
	
		}
	} else {
		count_ts = (sqlite3_int64)time(NULL);
		rc = snprintf(ds_str, sizeof(ds_str), "DS:in_use:GAUGE:600:0:U");
		if ( rc >= sizeof(ds_str) ) {
			lmlogf(lmlog_level_error, "DS definition length %d exceeds max length %d", rc, (int)sizeof(ds_str));
			return false;
		}
	}
	
	// Construct the initial timestamp:
	rc = snprintf(count_ts_str, sizeof(count_ts_str), "%lld", (long long int)count_ts);
	LMASSERT ( rc < sizeof(count_ts_str) );
	
	argc = 0;
	//argv[argc++] = rrd_path;
	//argv[argc++] = "--start"; argv[argc++] = count_ts_str;
	//argv[argc++] = "--step"; argv[argc++] = "300";
	argv[argc++] = ds_str;
	argv[argc++] = "RRA:AVERAGE:0.5:1:5184";    /* 18 days @ 5 minutes*/
	argv[argc++] = "RRA:AVERAGE:0.5:12:1440";   /* 60 days @ 1 hour*/
	argv[argc++] = "RRA:AVERAGE:0.5:144:1800";  /* 180 days @ 12 hours */
	argv[argc++] = "RRA:AVERAGE:0.5:288:1080";   /* 1080 days @ 24 hours */
	argv[argc++] = "RRA:AVERAGE:0.5:2016:730";   /* 14 years @ 7 days */
	
	rrd_clear_error();
	rc = rrd_create_r(rrd_path, 300, (time_t)count_ts, argc, argv);
	if ( rc ) {
		lmlogf(lmlog_level_error, "failed to create rrd file for feature '%s' (id=%d): %s", feature_string, feature_id, rrd_get_error());
		return false;
	}
	if ( counts_query && (query_rc == SQLITE_ROW) ) {
		rrd_point_str_type				point_strs[12];
		
		//
		// Let's do 12 updates at a time:
		//
		points = 0;
		while ( query_rc == SQLITE_ROW ) {		
			issued = sqlite3_column_int(counts_query, 0);
			in_use = sqlite3_column_int(counts_query, 1);
			count_ts = sqlite3_column_int64(counts_query, 2);
			
			rc = snprintf(point_strs[points], sizeof(rrd_point_str_type), "%lld:%d", (long long int)count_ts, in_use);
			if ( rc < sizeof(rrd_point_str_type) ) {
				argv[points] = &point_strs[points][0];
				points++;
			}
			if ( points == 12 ) {
				rrd_clear_error();
				rrd_update_r(rrd_path, NULL, points, argv);
				
				// Start over next time:
				points = 0;
			}
			query_rc = sqlite3_step(counts_query);
		}
		if ( points > 0 && points < 12 ) {
			// Commit the last set of points:
			rrd_clear_error();
			rrd_update_r(rrd_path, NULL, points, argv);
		}
	}	
	return true;
}

bool
__lmdb_rrd_update(
	const char		*rrd_path,
	time_t				count_ts,
	int						in_use
)
{
	rrd_point_str_type	point;
	const char					*argv = &point[0];
	int									rc;
	
	rc = snprintf(point, sizeof(point), "%lld:%d", (long long int)count_ts, in_use);
	if ( rc < sizeof(point) ) {
		rrd_clear_error();
		if ( rrd_update_r(rrd_path, NULL, 1, (const char**)&argv) == 0 ) return true;
	}
	return false;
}

#endif /* LMDB_DISABLE_RRDTOOL */
	
//

void
__lmdb_sqlite_regexp_fn(
  sqlite3_context     *context,
  int                 argc,
  sqlite3_value       **argv
)
{
  bool                is_match = false;
  
  if ( argc == 2 ) {
    char              *regex = (char*)sqlite3_value_text(argv[0]);
    char              *string = (char*)sqlite3_value_text(argv[1]);
    
    if ( regex && string ) {
      regex_t         comp_regex;
      
      if ( regcomp(&comp_regex, regex, REG_EXTENDED) == 0 ) {
        if ( regexec(&comp_regex, string, 0, NULL, 0) == 0 ) {
          is_match = true;
        }
        regfree(&comp_regex);
      }
    }
  }
  sqlite3_result_int(context, is_match);
}

//

static const char   *__db_schema =
    "CREATE TABLE features (\n"
    "  feature_id            INTEGER PRIMARY KEY NOT NULL,\n"
    "  feature_string        TEXT,\n"
    "  vendor                TEXT,\n"
    "  version               TEXT\n"
    ");\n"
    "CREATE UNIQUE INDEX unique_features_idx\n"
    "  ON features(vendor, version, feature_string);\n"
    "CREATE TABLE counts (\n"
    "  feature_id            INTEGER NOT NULL REFERENCES features(feature_id)\n"
    "                        ON DELETE CASCADE,\n"
    "  issued                INTEGER NOT NULL DEFAULT 0,\n"
    "  in_use                INTEGER NOT NULL DEFAULT 0,\n"
    "  expiration_timestamp  BIGINT,\n"
    "  checked_timestamp     BIGINT NOT NULL\n"
    ");\n"
    "\n"
    ;

static const char   *__db_get_features_query =
    "SELECT feature_id, feature_string, vendor, version FROM features ORDER BY feature_string, vendor, version";

static const char   *__db_get_feature_by_name_query =
    "SELECT feature_id, feature_string, vendor, version FROM features WHERE feature_string = ?1 AND vendor = ?2 AND version = ?3";

static const char   *__db_get_feature_by_id_query =
    "SELECT feature_id, feature_string, vendor, version FROM features WHERE feature_id = ?1";

static const char   *__db_add_feature_query =
    "INSERT INTO features (feature_string, vendor, version) VALUES (?1, ?2, ?3)";

static const char   *__db_add_feature_count_query =
    "INSERT INTO counts (feature_id, in_use, issued, expiration_timestamp, checked_timestamp) VALUES"
    "  (?1, ?2, ?3, ?4, ?5)";

#define DB_QUERY_BASE_NOAGGR \
    "SELECT f.feature_id, f.vendor, f.version, f.feature_string, c.in_use, c.issued, c.checked_timestamp AS start_timestamp, c.expiration_timestamp AS expiration_timestamp" \
    "  FROM counts AS c" \
    "  INNER JOIN features AS f ON (f.feature_id = c.feature_id)"

#define DB_QUERY_BASE_AGGR \
    "SELECT f.feature_id, f.vendor, f.version, f.feature_string, MIN(c.in_use) AS in_use_min, MAX(c.in_use) AS in_use_max, AVG(c.in_use) AS in_use_avg, MIN(c.issued) AS issued_min, MAX(c.issued) AS issued_max, AVG(c.issued) AS issued_avg, MIN(c.checked_timestamp) AS start_timestamp, MAX(c.checked_timestamp) AS end_timestamp, MAX(c.expiration_timestamp) AS expiration_timestamp" \
    "  FROM counts AS c" \
    "  INNER JOIN features AS f ON (f.feature_id = c.feature_id)"

#define DB_QUERY_ORDER_BY \
    "  ORDER BY start_timestamp ASC, f.vendor, f.version, f.feature_string"
    
//

typedef struct _lmdb {
  unsigned int      ref_count;
  sqlite3           *db_handle;
  char              *db_path;
#ifndef LMDB_DISABLE_RRDTOOL
  char              *rrd_repodir;
#endif
  bool              is_read_only;
  lmfeatureset_ref  features;
} lmdb;

//

lmdb*
__lmdb_alloc(
  const char        *db_path
)
{
  size_t            db_path_len = 1 + (db_path ? strlen(db_path) : 0);
  lmdb              *new_db = malloc(sizeof(lmdb) + db_path_len);

  if ( new_db ) {
    new_db->ref_count = 1;
    new_db->db_handle = NULL;
    new_db->db_path = (void*)new_db + sizeof(lmdb);
    if ( db_path ) {
      strcpy(new_db->db_path, db_path);
    } else {
      new_db->db_path[0] = '\0';
    }
#ifndef LMDB_DISABLE_RRDTOOL
    new_db->rrd_repodir = NULL;
#endif
    new_db->features = lmfeatureset_create();
  }
  return new_db;
}

//

void
__lmdb_dealloc(
  lmdb      *the_db
)
{
#ifndef LMDB_DISABLE_RRDTOOL
  if ( the_db->rrd_repodir ) free((void*)the_db->rrd_repodir);
#endif
  if ( the_db->db_handle ) sqlite3_close(the_db->db_handle);
  if ( the_db->features ) lmfeatureset_release(the_db->features);
  free((void*)the_db);
}

//

bool
__lmdb_commit_feature_count(
  lmdb_ref      the_db,
  lmfeature_ref the_feature,
  time_t        check_timestamp
)
{
	//
	// All SQLite result codes are positive, so we use
	// -1 to indicate failue prior to sqlite3_step()
	// and -2 to mean that there was no 
	int           rc = -1;

  if ( lmfeature_is_modified(the_feature) ) {
    sqlite3_stmt  *stmt = NULL;
    sqlite3_int64 db_ts;
    time_t        raw_ts;
    
    if ( sqlite3_prepare_v2(the_db->db_handle, __db_add_feature_count_query, -1, &stmt, NULL) != SQLITE_OK ) goto exit_on_error;
    if ( sqlite3_bind_int(stmt, 1, lmfeature_get_feature_id(the_feature)) != SQLITE_OK ) goto exit_on_error;
    if ( sqlite3_bind_int(stmt, 2, lmfeature_get_in_use(the_feature)) != SQLITE_OK ) goto exit_on_error;
    if ( sqlite3_bind_int(stmt, 3, lmfeature_get_issued(the_feature)) != SQLITE_OK ) goto exit_on_error;
    
    raw_ts = lmfeature_get_expiration_date(the_feature);
    if ( raw_ts != lmfeature_no_expiration ) {
      db_ts = (sqlite3_int64)raw_ts;
      if ( sqlite3_bind_int64(stmt, 4, db_ts) != SQLITE_OK ) goto exit_on_error;
    } else {
      if ( sqlite3_bind_null(stmt, 4) != SQLITE_OK ) goto exit_on_error;
    }
    
    db_ts = (sqlite3_int64)check_timestamp;
    if ( sqlite3_bind_int64(stmt, 5, db_ts) != SQLITE_OK ) goto exit_on_error;
    
    rc = sqlite3_step(stmt);
    if ( rc == SQLITE_DONE ) {
      rc = 0;
    }

exit_on_error:
    if ( stmt ) sqlite3_finalize(stmt);
    
#ifndef LMDB_DISABLE_RRDTOOL
		if ( the_db->rrd_repodir ) {
			const char			*rrd_path	= strcatf("%s/%d.rrd", the_db->rrd_repodir, lmfeature_get_feature_id(the_feature));
			
			if ( rrd_path ) {
				bool						was_added = false;
			
				if ( ! file_exists(rrd_path) ) {
					//
					// We need to create the rrd file and fill-it with any old data:
					//
					if ( sqlite3_prepare_v2(the_db->db_handle, __db_get_all_feature_counts_query, -1, &stmt, NULL) == SQLITE_OK ) {
						if ( sqlite3_bind_int(stmt, 1, lmfeature_get_feature_id(the_feature)) == SQLITE_OK ) {
							__lmdb_rrd_create(rrd_path, lmfeature_get_feature_id(the_feature), lmfeature_get_feature_string(the_feature), stmt);
							was_added = true;
						} else {
							__lmdb_rrd_create(rrd_path, lmfeature_get_feature_id(the_feature), lmfeature_get_feature_string(the_feature), NULL);
						}
						sqlite3_finalize(stmt);
					} else {
						__lmdb_rrd_create(rrd_path, lmfeature_get_feature_id(the_feature), lmfeature_get_feature_string(the_feature), NULL);
					}
				}
				if ( ! was_added && file_exists(rrd_path) ) {
					__lmdb_rrd_update(rrd_path, check_timestamp, lmfeature_get_in_use(the_feature));
				}
				free((void*)rrd_path);
			}
		}
#endif
    
  	return rc ? false : true;
  }
  return true;
}

//

#ifndef LMDB_DISABLE_RRDTOOL

bool
__lmdb_rrd_file_for_feature_id(
  const char        *rrd_repodir,
  int               feature_id,
  char              *s,
  size_t            s_size
)
{
  int               c = snprintf(s, s_size, "%s/%d.rrd", rrd_repodir, feature_id);
  
  return ( c < s_size ) ? true : false;
}

#endif

//

const char*
lmdb_get_schema(void)
{
  return __db_schema;
}

//

lmdb_ref
__lmdb_create(
  const char        *db_path,
  bool              is_read_only
)
{

  struct stat       finfo;
  sqlite3           *db_handle;
  bool              is_new = true;
  int               rc, sqlite_flags = 0;
  
#ifndef LMDB_IGNORE_SQLITE_VERSION
  static bool       is_inited = false;
  if ( ! is_inited ) {
    /* Ensure we're using an SQLite library at least as new as the one
     * with which we were compiled:
     */
    LMASSERT( sqlite3_libversion_number() >= SQLITE_VERSION_NUMBER );
    is_inited = true;
  }
#endif

  if ( strcmp(db_path, ":memory:") && (stat(db_path, &finfo) == 0) ) {
    if ( ! S_ISREG(finfo.st_mode) ) {
      lmlogf(lmlog_level_error, "lmdb_create: %s is not a regular file", db_path);
      return NULL;
    }
    is_new = false;
  }
  
  if ( is_new && is_read_only ) {
    lmlog(lmlog_level_alert, "lmdb_create:  impossible to open a non-existent database read-only");
    return NULL;
  }
  
  sqlite_flags = is_read_only ? (SQLITE_OPEN_READONLY) : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  
  if ( (rc = sqlite3_open_v2(db_path, &db_handle, sqlite_flags, NULL)) == SQLITE_OK ) {
    LMDEBUG("successfully opened %sdatabase '%s'", is_read_only ? "read-only " : "", db_path);
    
#ifdef SQLITE_DBCONFIG_ENABLE_FKEY
    if ( (rc = sqlite3_db_config(db_handle, SQLITE_DBCONFIG_ENABLE_FKEY, 1, NULL)) == SQLITE_OK )
#else
    if ( (rc = sqlite3_exec(db_handle, "PRAGMA foreign_keys = ON", NULL, NULL, NULL)) == SQLITE_OK )
#endif
    {
      size_t          db_path_len;
      lmdb            *new_db;
      
      LMDEBUG("successfully enabled foreign key support on database");
      if ( is_new ) {
        if ( (rc = sqlite3_exec(db_handle, __db_schema, NULL, NULL, NULL)) != SQLITE_OK ) {
          lmlogf(lmlog_level_error, "failed to initialize database schema: %s", sqlite3_errmsg(db_handle));
          sqlite3_close(db_handle);
          return NULL;
        }
        LMDEBUG("successfully initialized database schema");
      }
      new_db = __lmdb_alloc(db_path);
      if ( new_db ) {
        new_db->db_handle = db_handle;
        new_db->is_read_only = is_read_only;
        
        // Register or regexp function:
        sqlite3_create_function(
            db_handle,
            "REGEXP",
            2,
            SQLITE_UTF8,
            NULL,
            __lmdb_sqlite_regexp_fn,
            NULL,
            NULL
          );
        
        return (lmdb_ref)new_db;
      } else {
        lmlog(lmlog_level_warn, "lmdb_create: failed to allocate new object");
      }
    } else {
      lmlogf(lmlog_level_error, "lmdb_create: failed to enable foreign key support on database (rc = %d)", rc);
      sqlite3_close(db_handle);
    }
  } else {
    lmlogf(lmlog_level_error, "lmdb_create: failed to open database '%s' (rc = %d)", rc);
  }
  return NULL;
}

//

lmdb_ref
lmdb_create(
  const char        *db_path
)
{
  return __lmdb_create(db_path, false);
}

//

lmdb_ref
lmdb_create_read_only(
  const char        *db_path
)
{
  return __lmdb_create(db_path, true);
}

//

lmdb_ref
lmdb_retain(
  lmdb_ref          the_db
)
{
  the_db->ref_count++;
  return the_db;
}

//

void
lmdb_release(
  lmdb_ref          the_db
)
{
  if ( --the_db->ref_count == 0 ) __lmdb_dealloc(the_db);
}

//

const char*
lmdb_get_path(
  lmdb_ref          the_db
)
{
  return the_db->db_path;
}

//

#ifndef LMDB_DISABLE_RRDTOOL

const char*
lmdb_get_rrd_repodir(
  lmdb_ref          the_db
)
{
  return the_db->rrd_repodir;
}

//

bool
lmdb_set_rrd_repodir(
  lmdb_ref          the_db,
  const char        *rrd_repodir
)
{
  if ( the_db->rrd_repodir ) {
    free((void*)the_db->rrd_repodir);
    the_db->rrd_repodir = NULL;
  }
  if ( rrd_repodir ) {
    if ( directory_exists(rrd_repodir) ) {
      if ( access(rrd_repodir, R_OK | W_OK) == 0 ) {
        the_db->rrd_repodir = strdup(rrd_repodir);
        return true;
      } else {
        lmlogf(lmlog_level_warn, "no read+write permission on RRD repository directory: %s", rrd_repodir);
      }
    } else {
      lmlogf(lmlog_level_warn, "RRD repository directory does not exist: %s", rrd_repodir);
    }
    return false;
  }
  return true;
}

#endif

//

void
lmdb_load_all_features(
  lmdb_ref          the_db
)
{
  sqlite3_stmt    *stmt = NULL;
  int             rc;
  int             tbl_feature_id;
  const char      *tbl_feature_string, *tbl_vendor, *tbl_version;
  lmfeature_ref   a_feature;
  bool            done = false;
  
  if ( sqlite3_prepare_v2(the_db->db_handle, __db_get_features_query, -1, &stmt, NULL) != SQLITE_OK ) {
    lmlogf(lmlog_level_warn, "failed while preparing query '%s': %s", __db_get_features_query, sqlite3_errmsg(the_db->db_handle));
    return;
  }
  while ( ! done ) {
    rc = sqlite3_step(stmt);
    switch ( rc ) {
    
      case SQLITE_ROW: {
        /* Extant feature, wrap with an lmfeature: */
        tbl_feature_id = sqlite3_column_int(stmt, 0);
        
         // Check the featureset first
        a_feature = lmfeatureset_get_feature_by_id(the_db->features, tbl_feature_id);
        if ( ! a_feature ) {
          tbl_feature_string = (const char*)sqlite3_column_text(stmt, 1);
          tbl_vendor = (const char*)sqlite3_column_text(stmt, 2);
          tbl_version = (const char*)sqlite3_column_text(stmt, 3);
          
          a_feature = lmfeature_create(tbl_feature_id, tbl_feature_string, tbl_vendor, tbl_version);
          if ( a_feature ) {
            bool      ok = lmfeatureset_add_feature(the_db->features, a_feature);
            lmfeature_release(a_feature);
          }
        }
        break;
      }
      
      case SQLITE_DONE:
        done = true;
        break;
      
      default:
        lmlogf(lmlog_level_warn, "query failed (rc = %d): %s", rc, sqlite3_errmsg(the_db->db_handle));
        break;
      
    }
  }
  sqlite3_finalize(stmt);
}

//

static const char   *__db_lookup_features_query =
    "SELECT feature_id, f.feature_string, f.vendor, f.version FROM features AS f";
static const char   *__db_lookup_features_orderby =
    " ORDER BY f.vendor, f.version, f.feature_string";

lmfeatureset_ref
lmdb_lookup_features(
  lmdb_ref            the_db,
  lmdb_predicate_ref  predicate
)
{
  lmfeatureset_ref    out_featureset = NULL;
  const char          *predicate_str = predicate ? lmdb_predicate_get_string(predicate) : NULL;
  const char          *query_str;
  
  if ( predicate_str ) {
    query_str = strcatm(__db_lookup_features_query, " WHERE ", predicate_str, __db_lookup_features_orderby, NULL);
    free((void*)predicate_str);
  } else {
    query_str = strcatm(__db_lookup_features_query, __db_lookup_features_orderby, NULL);
  }
  if ( query_str ) {
    sqlite3_stmt    *stmt;
    
    LMDEBUG("QUERY:  %s\n", query_str);
    if ( sqlite3_prepare_v2(the_db->db_handle, query_str, -1, &stmt, NULL) == SQLITE_OK ) {
      int           rc;
      
      out_featureset = lmfeatureset_create();
      if ( out_featureset ) {
        while ( (rc = sqlite3_step(stmt)) == SQLITE_ROW ) {
          lmfeature_ref   feature = lmfeature_create(
                                            sqlite3_column_int(stmt, 0),
                                            (const char*)sqlite3_column_text(stmt, 1),
                                            (const char*)sqlite3_column_text(stmt, 2),
                                            (const char*)sqlite3_column_text(stmt, 3)
                                          );
          if ( feature ) {
            lmfeatureset_add_feature(out_featureset, feature);
            lmfeature_release(feature);
          }
        }
      }
      sqlite3_finalize(stmt);
    }
    free((void*)query_str);
  }
  return out_featureset;
}

//

lmfeatureset_ref
lmdb_get_features(
  lmdb_ref          the_db
)
{
  return the_db->features;
}

//

lmfeature_ref
lmdb_get_feature_by_feature_id(
  lmdb_ref      the_db,
  int           feature_id
)
{
  // Check the featureset first
  lmfeature_ref feature = lmfeatureset_get_feature_by_id(the_db->features, feature_id);
  
  if ( ! feature ) {
    sqlite3_stmt  *stmt = NULL;
    int           rc;
    int           tbl_feature_id;
    const char    *tbl_feature_string, *tbl_vendor, *tbl_version;

    if ( sqlite3_prepare_v2(the_db->db_handle, __db_get_feature_by_id_query, -1, &stmt, NULL) != SQLITE_OK ) {
      lmlogf(lmlog_level_warn, "failed while preparing query '%s': %s", __db_get_feature_by_id_query, sqlite3_errmsg(the_db->db_handle));
      goto exit_on_error;
    }
    if ( sqlite3_bind_int(stmt, 1, feature_id) != SQLITE_OK ) {
      lmlogf(lmlog_level_warn, "failed while binding parameter 1 to query: %s", sqlite3_errmsg(the_db->db_handle));
      goto exit_on_error;
    }
    rc = sqlite3_step(stmt);
    switch ( rc ) {
    
      case SQLITE_ROW: {
        /* Extant feature, wrap with an lmfeature: */
        tbl_feature_id = sqlite3_column_int(stmt, 0);
        tbl_feature_string = (const char*)sqlite3_column_text(stmt, 1);
        tbl_vendor = (const char*)sqlite3_column_text(stmt, 2);
        tbl_version = (const char*)sqlite3_column_text(stmt, 3);
        
        feature = lmfeature_create(tbl_feature_id, tbl_feature_string, tbl_vendor, tbl_version);
        if ( feature ) {
          bool      ok = lmfeatureset_add_feature(the_db->features, feature);
          lmfeature_release(feature);
          if ( ! ok ) feature = NULL;
        }
        break;
      }
      
      default:
        lmlogf(lmlog_level_warn, "query failed (rc = %d): %s", rc, sqlite3_errmsg(the_db->db_handle));
        break;
      
    }
exit_on_error:
    if ( stmt ) sqlite3_finalize(stmt);
  }
  return feature;
    
}

//

lmfeature_ref
lmdb_get_feature_by_name(
  lmdb_ref      the_db,
  const char    *feature_string,
  const char    *vendor,
  const char    *version
)
{
  // Check the featureset first
  lmfeature_ref feature = lmfeatureset_get_feature_by_name(the_db->features, feature_string, vendor, version);
  
  if ( ! feature ) {
    sqlite3_stmt  *stmt = NULL;
    int           rc;
    int           tbl_feature_id;
    const char    *tbl_feature_string, *tbl_vendor, *tbl_version;

    if ( sqlite3_prepare_v2(the_db->db_handle, __db_get_feature_by_name_query, -1, &stmt, NULL) != SQLITE_OK ) {
      lmlogf(lmlog_level_warn, "failed while preparing query '%s': %s", __db_get_feature_by_name_query, sqlite3_errmsg(the_db->db_handle));
      goto exit_on_error;
    }
    if ( sqlite3_bind_text(stmt, 1, feature_string, -1, SQLITE_STATIC) != SQLITE_OK ) {
      lmlogf(lmlog_level_warn, "failed while binding parameter 1 to query: %s", sqlite3_errmsg(the_db->db_handle));
      goto exit_on_error;
    }
    if ( sqlite3_bind_text(stmt, 2, vendor, -1, SQLITE_STATIC) != SQLITE_OK ) {
      lmlogf(lmlog_level_warn, "failed while binding parameter 2 to query: %s", sqlite3_errmsg(the_db->db_handle));
      goto exit_on_error;
    }
    if ( sqlite3_bind_text(stmt, 3, version, -1, SQLITE_STATIC) != SQLITE_OK ) {
      lmlogf(lmlog_level_warn, "failed while binding parameter 3 to query: %s", sqlite3_errmsg(the_db->db_handle));
      goto exit_on_error;
    }
    rc = sqlite3_step(stmt);
    switch ( rc ) {
    
      case SQLITE_ROW: {
        /* Extant feature, wrap with an lmfeature: */
        tbl_feature_id = sqlite3_column_int(stmt, 0);
        tbl_feature_string = (const char*)sqlite3_column_text(stmt, 1);
        tbl_vendor = (const char*)sqlite3_column_text(stmt, 2);
        tbl_version = (const char*)sqlite3_column_text(stmt, 3);
        
        LMDEBUG("feature %s for vendor %s (version %s) found in database with id %d", feature_string, vendor, version, tbl_feature_id);
        
        feature = lmfeature_create(tbl_feature_id, tbl_feature_string, tbl_vendor, tbl_version);
        if ( feature ) {
          bool      ok = lmfeatureset_add_feature(the_db->features, feature);
          lmfeature_release(feature);
          if ( ! ok ) feature = NULL;
        }
        break;
      }
      
      case SQLITE_DONE: {
        /* If the database is read-only, we can't add the feature: */
        if ( the_db->is_read_only ) goto exit_on_error;
        
        /* Unknown feature, add it: */
        sqlite3_finalize(stmt);
        stmt = NULL;
        
        LMDEBUG("feature %s for vendor %s (version %s) not present in database", feature_string, vendor, version);
        
        if ( sqlite3_prepare_v2(the_db->db_handle, __db_add_feature_query, -1, &stmt, NULL) != SQLITE_OK ) goto exit_on_error;
        if ( sqlite3_bind_text(stmt, 1, feature_string, -1, SQLITE_STATIC) != SQLITE_OK ) goto exit_on_error;
        if ( sqlite3_bind_text(stmt, 2, vendor, -1, SQLITE_STATIC) != SQLITE_OK ) goto exit_on_error;
        if ( sqlite3_bind_text(stmt, 3, version, -1, SQLITE_STATIC) != SQLITE_OK ) goto exit_on_error;
        rc = sqlite3_step(stmt);
        if ( rc == SQLITE_DONE ) {
          sqlite3_int64     tbl_feature_id = sqlite3_last_insert_rowid(the_db->db_handle);
          
          feature = lmfeature_create(tbl_feature_id, feature_string, vendor, version);
          if ( feature ) {
            bool      ok = lmfeatureset_add_feature(the_db->features, feature);
            lmfeature_release(feature);
            if ( ! ok ) feature = NULL;
          }
        }
        break;
      }
      
      default:
        lmlogf(lmlog_level_warn, "query failed (rc = %d): %s", rc, sqlite3_errmsg(the_db->db_handle));
        break;

    }
exit_on_error:
    if ( stmt ) sqlite3_finalize(stmt);
  }
  return feature;
}

//

lmfeature_ref
lmdb_add_feature(
  lmdb_ref      the_db,
  lmfeature_ref new_feature
)
{
  lmfeature_ref from_db_feature;
  
  // It better have the proper feature_id:
  if ( lmfeature_get_feature_id(new_feature) != lmfeature_no_id ) return NULL;
  
  // Check to be sure it's not already present in the feature set:
  if ( lmfeatureset_get_feature_by_name(the_db->features, lmfeature_get_feature_string(new_feature), lmfeature_get_vendor(new_feature), lmfeature_get_version(new_feature)) ) return NULL;
  
  // Use its feature string, vendor, and version to add to the database (or load the record from the database):
  if ( (from_db_feature = lmdb_get_feature_by_name(the_db, lmfeature_get_feature_string(new_feature), lmfeature_get_vendor(new_feature), lmfeature_get_version(new_feature))) ) {
    // Set the new in-db feature's counts if it was modified:
    if ( lmfeature_is_modified(new_feature) ) {
      lmfeature_set_expiration_date(from_db_feature, lmfeature_get_expiration_date(new_feature));
      lmfeature_set_in_use(from_db_feature, lmfeature_get_in_use(new_feature));
      lmfeature_set_issued(from_db_feature, lmfeature_get_issued(new_feature));
    }
    return from_db_feature;
  }
  return NULL;
}

//

const time_t lmdb_check_timestamp_now = 0;

struct __lmdb_commit_counts_data {
  bool        ok;
  time_t      when;
  lmdb_ref    the_db;
};

bool
__lmdb_commit_counts_iterator(
  const void    *context,
  lmfeature_ref feature
)
{
  struct __lmdb_commit_counts_data   *CONTEXT = (struct __lmdb_commit_counts_data*)context;
  
  // Failure so long as at least one commit fails:
  if ( ! __lmdb_commit_feature_count(CONTEXT->the_db, feature, CONTEXT->when) ) CONTEXT->ok = false;

	return true;
}

bool
lmdb_commit_counts(
  lmdb_ref      the_db,
  time_t        check_timestamp
)
{
  if ( ! the_db->is_read_only ) {
    struct __lmdb_commit_counts_data context = {
                .ok = true,
                .when = check_timestamp,
                .the_db = the_db
              };
    
    if ( check_timestamp == lmdb_check_timestamp_now ) context.when = time(NULL);
    
    lmfeatureset_iterate(the_db->features, __lmdb_commit_counts_iterator, &context);
    return context.ok;
  }
  return false;
}

//
#if 0
#pragma mark -
#endif
//

typedef struct _lmdb_usage_report {
  lmdb_ref              				parent_db;
  lmdb_usage_report_aggregate  	aggregate;
  lmdb_usage_report_range				range;
  sqlite3_stmt          				*query;
} lmdb_usage_report;

lmdb_usage_report_ref
lmdb_usage_report_create(
  lmdb_ref              				the_db,
  lmdb_usage_report_aggregate		aggregate,
  lmdb_usage_report_range				range,
  lmdb_predicate_ref    				predicate
)
{
  if ( (aggregate > lmdb_usage_report_aggregate_undef) && 
       (aggregate < lmdb_usage_report_aggregate_max) &&
       (range > lmdb_usage_report_range_undef) && 
       (range < lmdb_usage_report_range_max)
  	)
	{
    lmdb_usage_report    *new_query = malloc(sizeof(lmdb_usage_report));
    
    if ( new_query ) {
      const char  *query_str = NULL;
      const char  *base_str, *order_str, *group_str = "";
      const char  *predicate_str = predicate ? lmdb_predicate_get_string(predicate) : NULL;
      
      new_query->parent_db = lmdb_retain(the_db);
      new_query->query = NULL;
      
      /* What aggregation should be performed? */
      switch ( aggregate ) {
      
        case lmdb_usage_report_aggregate_none:
          base_str = DB_QUERY_BASE_NOAGGR;
          order_str = DB_QUERY_ORDER_BY;
          break;
        case lmdb_usage_report_aggregate_hourly:
          base_str = DB_QUERY_BASE_AGGR;
          group_str = "  GROUP BY strftime('%Y%m%d%H', c.checked_timestamp, 'unixepoch', 'localtime'), c.feature_id, f.vendor, f.version, f.feature_string";
          order_str = DB_QUERY_ORDER_BY;
          break;
        case lmdb_usage_report_aggregate_daily:
          base_str = DB_QUERY_BASE_AGGR;
          group_str = "  GROUP BY strftime('%Y%m%d', c.checked_timestamp, 'unixepoch', 'localtime'), c.feature_id, f.vendor, f.version, f.feature_string";
          order_str = DB_QUERY_ORDER_BY;
          break;
        case lmdb_usage_report_aggregate_weekly:
          base_str = DB_QUERY_BASE_AGGR;
          group_str = "  GROUP BY strftime('%Y%W', c.checked_timestamp, 'unixepoch', 'localtime'), c.feature_id, f.vendor, f.version, f.feature_string";
          order_str = DB_QUERY_ORDER_BY;
          break;
        case lmdb_usage_report_aggregate_monthly:
          base_str = DB_QUERY_BASE_AGGR;
          group_str = "  GROUP BY strftime('%Y%m', c.checked_timestamp, 'unixepoch', 'localtime'), c.feature_id, f.vendor, f.version, f.feature_string";
          order_str = DB_QUERY_ORDER_BY;
          break;
        case lmdb_usage_report_aggregate_yearly:
          base_str = DB_QUERY_BASE_AGGR;
          group_str = "  GROUP BY strftime('%Y', c.checked_timestamp, 'unixepoch', 'localtime'), c.feature_id, f.vendor, f.version, f.feature_string";
          order_str = DB_QUERY_ORDER_BY;
          break;
        case lmdb_usage_report_aggregate_total:
          base_str = DB_QUERY_BASE_AGGR;
          group_str = "  GROUP BY c.feature_id, f.vendor, f.version, f.feature_string";
          order_str = DB_QUERY_ORDER_BY;
          break;
        
        case lmdb_usage_report_aggregate_undef:
        case lmdb_usage_report_aggregate_max:
          // Never gets here, but compilers love to complain about unhandled enums
          break;
          
      }
      
      switch ( range ) {
      
      	case lmdb_usage_report_range_none:
       		break;
          
        case lmdb_usage_report_range_last_check: {
        	predicate_str = strappendm(predicate_str, predicate_str ? " AND" : "c.checked_timestamp = (SELECT MAX(checked_timestamp) FROM counts)", predicate_str ? " c.checked_timestamp = (SELECT MAX(checked_timestamp) FROM counts)" : NULL, NULL);
         	break;
        }
          
        case lmdb_usage_report_range_last_hour: {
          predicate_str = strappendm(predicate_str, predicate_str ? " AND" : " strftime('%s', 'now') - c.checked_timestamp <= 3600", predicate_str ? " strftime('%s', 'now') - c.checked_timestamp <= 3600" : NULL, NULL);
           break;
        }
          
        case lmdb_usage_report_range_last_day: {
          predicate_str = strappendm(predicate_str, predicate_str ? " AND" : " strftime('%s', 'now') - c.checked_timestamp <= 86400", predicate_str ? " strftime('%s', 'now') - c.checked_timestamp <= 86400" : NULL, NULL);
           break;
        }
          
        case lmdb_usage_report_range_last_week: {
          predicate_str = strappendm(predicate_str, predicate_str ? " AND" : " strftime('%s', 'now') - c.checked_timestamp <= 604800", predicate_str ? " strftime('%s', 'now') - c.checked_timestamp <= 604800" : NULL, NULL);
           break;
        }
          
        case lmdb_usage_report_range_last_month: {
          predicate_str = strappendm(predicate_str, predicate_str ? " AND" : " strftime('%s', 'now') - c.checked_timestamp <= 2592000", predicate_str ? " strftime('%s', 'now') - c.checked_timestamp <= 2592000" : NULL, NULL);
           break;
        }
          
        case lmdb_usage_report_range_last_year: {
          predicate_str = strappendm(predicate_str, predicate_str ? " AND" : " strftime('%s', 'now') - c.checked_timestamp <= 31536000", predicate_str ? " strftime('%s', 'now') - c.checked_timestamp <= 31536000" : NULL, NULL);
           break;
        }
      
        case lmdb_usage_report_range_undef:
      	case lmdb_usage_report_range_max:
          // Never gets here, but compilers love to complain about unhandled enums
          break;
      }
      
      if ( predicate_str ) {
        query_str = strcatm(base_str, " WHERE " , predicate_str, group_str, order_str, NULL);
        free((void*)predicate_str);
      } else {
        query_str = strcatm(base_str, group_str, order_str, NULL);
      }
      if ( query_str ) {
        LMDEBUG("QUERY:  %s\n", query_str);
        if ( sqlite3_prepare_v2(the_db->db_handle, query_str, -1, &new_query->query, NULL) != SQLITE_OK ) {
          free((void*)new_query);
          new_query = NULL;
          lmdb_release(the_db);
        } else {
          new_query->aggregate = aggregate;
          new_query->range = range;
        }
        free((void*)query_str);
      }
    }
    return (lmdb_usage_report_ref)new_query;
  }
  return NULL;
}

//

void
lmdb_usage_report_release(
  lmdb_usage_report_ref  the_query
)
{
  if ( the_query->query ) sqlite3_finalize(the_query->query);
  lmdb_release(the_query->parent_db);
  free((void*)the_query);
}

//

bool
lmdb_usage_report_iterate(
  lmdb_usage_report_ref    the_query,
  lmdb_iterator_fn  iterator_fn,
  const void        *context
)
{
  bool              is_okay = false;
  
  if ( the_query->query ) {
    is_okay = true;
    while ( is_okay && (sqlite3_step(the_query->query) == SQLITE_ROW) ) {
      if ( iterator_fn ) {
        int               feature_id;
        lmdb_int_range_t  in_use, issued;
        sqlite3_int64     raw_ts;
        time_t            expire = 0;
        lmdb_time_range_t ts;
        
        const char        *vendor, *version, *feature_string;
        
        feature_id = sqlite3_column_int(the_query->query, 0);
        vendor = (const char*)sqlite3_column_text(the_query->query, 1);
        version = (const char*)sqlite3_column_text(the_query->query, 2);
        feature_string = (const char*)sqlite3_column_text(the_query->query, 3);
        
        if ( the_query->aggregate != lmdb_usage_report_aggregate_none ) {
          in_use.min = sqlite3_column_int(the_query->query, 4);
          in_use.max = sqlite3_column_int(the_query->query, 5);
          in_use.avg = sqlite3_column_int(the_query->query, 6);
          
          issued.min = sqlite3_column_int(the_query->query, 7);
          issued.max = sqlite3_column_int(the_query->query, 8);
          issued.avg = sqlite3_column_int(the_query->query, 9);
          
          ts.start = (time_t)sqlite3_column_int64(the_query->query, 10);
          ts.end = (time_t)sqlite3_column_int64(the_query->query, 11);
          
          expire = (time_t)sqlite3_column_int64(the_query->query, 12);
        } else {
          in_use.min = in_use.max = in_use.avg = sqlite3_column_int(the_query->query, 4);
          issued.min = issued.max = issued.avg = sqlite3_column_int(the_query->query, 5);
          ts.start = ts.end = (time_t)sqlite3_column_int64(the_query->query, 6);
          expire = (time_t)sqlite3_column_int64(the_query->query, 7);
        }
        
        /* Any NULL strings should have an empty string substituted: */
        if ( ! vendor ) vendor = "";
        if ( ! version ) version = "";
        if ( ! feature_string ) feature_string = "";
        
        if ( ! iterator_fn(context, feature_id, vendor, version, feature_string, in_use, issued, expire, ts) ) is_okay = false;
      }
    }
    sqlite3_reset(the_query->query);
  }  
  return is_okay;
}

//
#if 0
#pragma mark -
#endif
//

struct lmdb_field_descriptor {
  const char      *name;
  bool            should_be_quoted;
};

static struct lmdb_field_descriptor lmdb_field_descriptors[] = {
      { "", false },
      { "f.feature_id", false },
      { "f.feature_string", true },
      { "f.vendor", true },
      { "f.version", true },
      { "c.in_use", false },
      { "c.issued", false },
      { "c.expiration_timestamp", false },
      { "c.checked_timestamp", false },
      { "", false }
    };

struct lmdb_operator_descriptor {
  const char      *name;
  bool            is_unary;
};

static struct lmdb_operator_descriptor lmdb_operator_descriptors[] = {
      { "", false },
      { " = ", false },
      { " <> ", false },
      { " < ", false },
      { " <= ", false },
      { " > ", false },
      { " >= ", false },
      { " LIKE ", false },
      { " NOT LIKE ", false },
      { " GLOB ", false },
      { " REGEXP ", false },
      { " IS NULL ", true },
      { " IS NOT NULL ", true },
      { "", false }
    };

static const char* lmdb_combiner_names[] = {
      "",
      " AND ",
      " OR ",
      ""
    };

enum {
  lmdb_predicate_node_type_undef = 0,
  lmdb_predicate_node_type_test = 1,
  lmdb_predicate_node_type_combiner = 2,
  lmdb_predicate_node_type_expression = 3
};

#define LMDB_PREDICATE_NODE_HEADER_FIELDS \
  int                           node_type; \
  struct _lmdb_predicate_node   *next;

typedef struct _lmdb_predicate_node {
  LMDB_PREDICATE_NODE_HEADER_FIELDS
} lmdb_predicate_node;

//

typedef struct _lmdb_predicate_node_test {
  LMDB_PREDICATE_NODE_HEADER_FIELDS
  
  int         field;
  int         operator;
  char        value[1];
} lmdb_predicate_node_test;

lmdb_predicate_node*
lmdb_predicate_node_test_alloc(
  int         field,
  int         operator,
  const char  *value
)
{
  lmdb_predicate_node_test  *new_node = malloc(sizeof(lmdb_predicate_node_test) + (lmdb_operator_descriptors[operator].is_unary ? 0 : strlen(value)) );
  
  if ( new_node ) {
    new_node->node_type = lmdb_predicate_node_type_test;
    new_node->next = NULL;
    
    new_node->field = field;
    new_node->operator = operator;
    
    if ( ! lmdb_operator_descriptors[operator].is_unary ) {
      strcpy(new_node->value, value);
    } else {
      new_node->value[0] = '\0';
    }
  }
  return (lmdb_predicate_node*)new_node;
}

//

typedef struct _lmdb_predicate_node_combiner {
  LMDB_PREDICATE_NODE_HEADER_FIELDS
  
  int         op;
} lmdb_predicate_node_combiner;

lmdb_predicate_node*
lmdb_predicate_node_combiner_alloc(
  int         op
)
{
  lmdb_predicate_node_combiner  *new_node = malloc(sizeof(lmdb_predicate_node_combiner));
  
  if ( new_node ) {
    new_node->node_type = lmdb_predicate_node_type_combiner;
    new_node->next = NULL;
    
    new_node->op = op;
  }
  return (lmdb_predicate_node*)new_node;
}

//

typedef struct _lmdb_predicate_node_expression {
  LMDB_PREDICATE_NODE_HEADER_FIELDS
  
  struct _lmdb_predicate  *expression;
} lmdb_predicate_node_expression;

lmdb_predicate_node*
lmdb_predicate_node_expression_alloc(
  struct _lmdb_predicate    *expression
)
{
  lmdb_predicate_node_expression  *new_node = malloc(sizeof(lmdb_predicate_node_expression));
  
  if ( new_node ) {
    new_node->node_type = lmdb_predicate_node_type_expression;
    new_node->next = NULL;
    
    new_node->expression = lmdb_predicate_retain(expression);
  }
  return (lmdb_predicate_node*)new_node;
}

//

typedef struct _lmdb_predicate {
  unsigned int            ref_count;
  lmdb_predicate_node     *chain;
} lmdb_predicate;

//

lmdb_predicate_ref
lmdb_predicate_create_with_test(
  lmdb_predicate_field    field,
  lmdb_predicate_operator op,
  const char              *value
)
{
  if ( (field >= lmdb_predicate_field_undef && field < lmdb_predicate_field_max) && (op >= lmdb_predicate_operator_undef && op < lmdb_predicate_operator_max) ) {
    lmdb_predicate_node *new_test = lmdb_predicate_node_test_alloc(field, op, (value ? value : ""));
    
    if ( new_test ) {
      lmdb_predicate    *new_pred = malloc(sizeof(lmdb_predicate));
    
      if ( new_pred ) {
        new_pred->ref_count = 1;
        new_pred->chain = new_test;
        return (lmdb_predicate_ref)new_pred;
      } else {
        free((void*)new_test);
      }
    }
  }
  return NULL;
}

//

lmdb_predicate_ref
lmdb_predicate_retain(
  lmdb_predicate_ref  the_predicate
)
{
  the_predicate->ref_count++;
  return the_predicate;
}

//

void
lmdb_predicate_release(
  lmdb_predicate_ref  the_predicate
)
{
  if ( --the_predicate->ref_count == 0 ) {
    lmdb_predicate_node *p = the_predicate->chain;
    
    while ( p ) {
      lmdb_predicate_node *next = p->next;
      
      if ( p->node_type == lmdb_predicate_node_type_expression ) lmdb_predicate_release(((lmdb_predicate_node_expression*)p)->expression);
      free((void*)p);
      p = next;
    }
    free((void*)the_predicate);
  }
}

//

bool
lmdb_predicate_add_test(
  lmdb_predicate_ref      the_predicate,
  lmdb_predicate_combiner combiner,
  lmdb_predicate_field    field,
  lmdb_predicate_operator op,
  const char              *value
)
{
  bool                rc = false;
  
  if ( (field > lmdb_predicate_field_undef && field < lmdb_predicate_field_max) && (op > lmdb_predicate_operator_undef && op < lmdb_predicate_operator_max) && (combiner > lmdb_predicate_combiner_undef && combiner < lmdb_predicate_combiner_max) ) {
    lmdb_predicate_node *new_test = lmdb_predicate_node_test_alloc(field, op, (value ? value : ""));
    
    if ( new_test ) {
      lmdb_predicate_node *new_combiner = lmdb_predicate_node_combiner_alloc(combiner);
      
      if ( new_combiner ) {
        lmdb_predicate_node *last = the_predicate->chain;
        
        while ( last ) {
          if ( last->next == NULL ) break;
          last = last->next;
        }
        last->next = new_combiner;
        new_combiner->next = new_test;
      } else {
        free((void*)new_test);
      }
    }
  }
  return rc;
}

//

bool
lmdb_predicate_add_expression(
  lmdb_predicate_ref      the_predicate,
  lmdb_predicate_combiner combiner,
  lmdb_predicate_ref      other_predicate
)
{
  bool                rc = false;
  
  if ( other_predicate && (combiner > lmdb_predicate_combiner_undef && combiner < lmdb_predicate_combiner_max) ) {
    lmdb_predicate_node *new_expr = lmdb_predicate_node_expression_alloc(other_predicate);
    
    if ( new_expr ) {
      lmdb_predicate_node *new_combiner = lmdb_predicate_node_combiner_alloc(combiner);
      
      if ( new_combiner ) {
        lmdb_predicate_node *last = the_predicate->chain;
        
        while ( last ) {
          if ( last->next == NULL ) break;
          last = last->next;
        }
        last->next = new_combiner;
        new_combiner->next = new_expr;
      } else {
        lmdb_predicate_release(other_predicate);
        free((void*)new_expr);
      }
    }
  }
  return rc;
}

//

const char*
lmdb_predicate_get_string(
  lmdb_predicate_ref    the_predicate
)
{
  const char            *out = NULL;
  lmdb_predicate_node   *p = the_predicate->chain;
  
  while ( p ) {
    switch ( p->node_type ) {
    
      case lmdb_predicate_node_type_test: {
        lmdb_predicate_node_test        *node = (lmdb_predicate_node_test*)p;
        struct lmdb_field_descriptor    field_desc = lmdb_field_descriptors[node->field];
        struct lmdb_operator_descriptor op_desc = lmdb_operator_descriptors[node->operator];
        
        if ( op_desc.is_unary ) {
          if ( out ) {
            out = strcatm(out, field_desc.name, op_desc.name, NULL);
          } else {
            out = strcatm(field_desc.name, op_desc.name, NULL);
          }
        } else if ( field_desc.should_be_quoted ) {
          if ( out ) {
            out = strcatm(out, field_desc.name, op_desc.name, "'", node->value, "'", NULL);
          } else {
            out = strcatm(field_desc.name, op_desc.name, "'", node->value, "'", NULL);
          }
        } else {
          if ( out ) {
            out = strcatm(out, field_desc.name, op_desc.name, node->value, NULL);
          } else {
            out = strcatm(field_desc.name, op_desc.name, node->value, NULL);
          }
        }
        break;
      }
      
      case lmdb_predicate_node_type_combiner: {
        lmdb_predicate_node_combiner  *node = (lmdb_predicate_node_combiner*)p;
        
        if ( out ) {
          out = strcatm(out, lmdb_combiner_names[node->op], NULL);
        }
        break;
      }
      
      case lmdb_predicate_node_type_expression: {
        lmdb_predicate_node_expression  *node = (lmdb_predicate_node_expression*)p;
        const char                      *expression_string = lmdb_predicate_get_string(node->expression);
        
        if ( out ) {
          out = strcatm(out, " ( ", expression_string, " ) ", NULL);
        } else {
          out = strcatm(" ( ", expression_string, " ) ", NULL);
        }
        free((void*)expression_string);
        break;
      }
    }
    p = p->next;
  }
  return out;
}
