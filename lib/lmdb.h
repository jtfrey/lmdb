/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmdb.h
 *
 * lmcount database API
 *
 */

#ifndef __LMDB_H__
#define __LMDB_H__

#include "lmfeature.h"

/*!
  @typedef lmdb_predicate_field
  Enumerates the database fields which can be tested in
  lmdb_predicate objects.
*/
typedef enum {
  lmdb_predicate_field_undef = 0,
  //
  lmdb_predicate_field_feature_id,
  lmdb_predicate_field_feature,
  lmdb_predicate_field_vendor,
  lmdb_predicate_field_version,
  lmdb_predicate_field_in_use,
  lmdb_predicate_field_issued,
  lmdb_predicate_field_expiration,
  lmdb_predicate_field_checked,
  //
  lmdb_predicate_field_max
} lmdb_predicate_field;

/*!
  @typedef lmdb_predicate_operator
  Enumerates the operators that may applied to database
  fields in lmdb_predicate objects.
*/
typedef enum {
  lmdb_predicate_operator_undef = 0,
  //
  lmdb_predicate_operator_eq,
  lmdb_predicate_operator_ne,
  lmdb_predicate_operator_lt,
  lmdb_predicate_operator_le,
  lmdb_predicate_operator_gt,
  lmdb_predicate_operator_ge,
  lmdb_predicate_operator_like,
  lmdb_predicate_operator_not_like,
  lmdb_predicate_operator_glob,
  lmdb_predicate_operator_regexp,
  lmdb_predicate_operator_isnull,
  lmdb_predicate_operator_not_isnull,
  //
  lmdb_predicate_operator_max
} lmdb_predicate_operator;

/*!
  @typedef lmdb_predicate_combiner
  Enumerates the logical operators that can be used to
  combine two test expressions in lmdb_predicate objects.
*/
typedef enum {
  lmdb_predicate_combiner_undef = 0,
  //
  lmdb_predicate_combiner_and,
  lmdb_predicate_combiner_or,
  //
  lmdb_predicate_combiner_max
} lmdb_predicate_combiner;

/*!
  @typedef lmdb_predicate_ref
  Type of an opaque reference to an lmdb_predicate object.
*/
typedef struct _lmdb_predicate * lmdb_predicate_ref;

/*!
  @function lmdb_predicate_create_with_test
  Create a new lmdb_predicate object that tests the given database field against
  the given string value.  The operator, op, determines how the field (L.H.S.) is
  compared against the value (R.H.S.).
  
  Since the lmdb_predicate_operator_isnull and lmdb_predicate_operator_not_isnull
  operators are unary the value argument is ignored in those cases.
*/
lmdb_predicate_ref lmdb_predicate_create_with_test(lmdb_predicate_field field, lmdb_predicate_operator op, const char *value);

/*!
  @function lmdb_predicate_retain
  Increment the reference count of the_predicate.
*/
lmdb_predicate_ref lmdb_predicate_retain(lmdb_predicate_ref the_predicate);

/*!
  @function lmdb_predicate_release
  Decrement the reference count of the_predicate.  When the count
  reaches zero, the_predicate is deallocated.
*/
void lmdb_predicate_release(lmdb_predicate_ref the_predicate);

/*!
  @function lmdb_predicate_add_test
  Append a new test to the_predicate:
  
    [the_predicate tests] <combiner> ( <field> <op> {<value>} )
  
  Returns false if the_predicate could not be augmented.
*/
bool lmdb_predicate_add_test(lmdb_predicate_ref the_predicate, lmdb_predicate_combiner combiner, lmdb_predicate_field field, lmdb_predicate_operator op, const char *value);

/*!
  @function lmdb_predicate_add_expression
  Append another lmdb_predicate object to the_predicate:
  
    [the_predicate tests] <combiner> ( [other_predicate tests] )
  
  Returns false if the_predicate could not be augmented.
*/
bool lmdb_predicate_add_expression(lmdb_predicate_ref the_predicate, lmdb_predicate_combiner combiner, lmdb_predicate_ref other_predicate);

/*!
  @function lmdb_predicate_get_string
  Generates the SQL WHERE clause associated with the_predicate and
  returns as a C string.  The caller is reponsible for free'ing the
  non-NULL pointer returned.
*/
const char* lmdb_predicate_get_string(lmdb_predicate_ref the_predicate);


#if 0
#pragma mark -
#endif


/*!
  @function lmdb_get_schema
  Returns a string constant containing the SQLite database
  schema used by this library.
*/
const char* lmdb_get_schema(void);

/*!
  @typedef lmdb_ref
  Type of an opaque reference to an lmdb object.
*/
typedef struct _lmdb * lmdb_ref;

/*!
  @function lmdb_create
  Connect to (create if not present) the SQLite database at
  db_path.  The connection is opened with read-write capability.
  
  If no file existed at db_path, then the lmdb schema is
  executed to create the necessary tables and indices.
*/
lmdb_ref lmdb_create(const char *db_path);

/*!
  @function lmdb_create_read_only
  Connect to an extant SQLite database at db_path.  The
  connection is opened with read-only capability.
*/
lmdb_ref lmdb_create_read_only(const char *db_path);

/*!
  @function lmdb_retain
  Increase the reference count of the_db.
*/
lmdb_ref lmdb_retain(lmdb_ref the_db);

/*!
  @function lmdb_release
  Decrease the reference count of the_db.  When the
  reference count reaches zero the object is deallocated.
*/
void lmdb_release(lmdb_ref the_db);

/*!
  @function lmdb_get_path
  Returns a string constant that is the path to the SQLite
  database wrapped by the_db.
*/
const char* lmdb_get_path(lmdb_ref the_db);

#ifndef LMDB_DISABLE_RRDTOOL
/*!
  @function lmdb_get_rrd_repodir
  Returns a string constant that is the path of a directory
  where the_db can create and update RRD file(s) for all
  counted features.
*/
const char* lmdb_get_rrd_repodir(lmdb_ref the_db);

/*!
  @function lmdb_set_rrd_repodir
  Set the_db to use the directory at rrd_repodir to create and
  update RRD file(s) for all counted features.
*/
bool lmdb_set_rrd_repodir(lmdb_ref the_db, const char *rrd_repodir);
#endif

/*!
  @function lmdb_load_all_features
  Force the_db to load the entire list of defined feature tuples
  and add them all to its feature set.
*/
void lmdb_load_all_features(lmdb_ref the_db);

/*!
  @function lmdb_lookup_features
  Return an lmfeatureset containing feature tuples present in the_db.
  If predicate is NULL, all defined features are present.  Otherwise,
  an lmdb_predicate expression containing references to:
  
    - lmdb_predicate_field_feature_id
    - lmdb_predicate_field_feature
    - lmdb_predicate_field_vendor
    - lmdb_predicate_field_version
  
  is used to limit the features returned.
  
  The caller is responsible for releasing the returned lmfeatureset.
*/
lmfeatureset_ref lmdb_lookup_features(lmdb_ref the_db, lmdb_predicate_ref predicate);

/*!
  @function lmdb_get_features
  Returns the set of license features currently loaded for
  the_db.
*/
lmfeatureset_ref lmdb_get_features(lmdb_ref the_db);

/*!
  @function lmdb_get_feature_by_feature_id
  Check the_db for a feature given a feature id (the internal database
  identifier for the feature).  If a matching id is not found in the
  lmfeatureset for the_db, the database is queried and the newly
  loaded lmfeature is cached in its lmfeatureset and returned.
*/
lmfeature_ref lmdb_get_feature_by_feature_id(lmdb_ref the_db, int feature_id);

/*!
  @function lmdb_get_feature_by_name
  Check the_db for a feature given a feature string, vendor, and version.
  If a matching feature is not found in the lmfeatureset for the_db, the
  database is queried and the newly loaded lmfeature is cached in its
  lmfeatureset and returned.
  
  Note that if the requested (feature_string,vendor,version) tuple is not
  present in the database, it will be added, so the_db must not have been
  opened read-only.
*/
lmfeature_ref lmdb_get_feature_by_name(lmdb_ref the_db, const char *feature_string, const char *vendor, const char *version);

/*!
  @function lmdb_add_feature
  Given an externally-created lmfeature object new_feature, attempt to add
  that feature to the database.
  
  Returns NULL if new_feature has an id other than lmfeature_no_id or its
  (feature_string,vendor,version) tuple is already present in the database.
  Returns the newly-cached lmfeature object if successful.
*/
lmfeature_ref lmdb_add_feature(lmdb_ref the_db, lmfeature_ref new_feature);

/*!
  @constant lmdb_check_timestamp_now
  Constant Unix timestamp used to indicate that license count updates
  should have a single timestamp equal to the time when the
  lmdb_commit_counts() function was called.
*/
extern const time_t lmdb_check_timestamp_now;

/*!
  @function lmdb_commit_counts
  Attempt to commit all updated license is-use counts to the database
  with check_timestamp as the time logged in the database.
  
  Returns true if all counts were commited successully.
*/
bool lmdb_commit_counts(lmdb_ref the_db, time_t check_timestamp);


#if 0
#pragma mark -
#endif

/*!
  @typedef lmdb_usage_report_aggregate
  Enumerates the temporal "bucket size" used to aggregate selected
  license counts.  Most values are self-explanatory, except:
  
    lmdb_usage_report_aggregate_none
      return raw data without aggregating
    
    lmdb_usage_report_aggregate_total
      min/max/average of all selected rows with matching feature
      tuple
*/
typedef enum {
  lmdb_usage_report_aggregate_undef = 0,
  //
  lmdb_usage_report_aggregate_none,
  lmdb_usage_report_aggregate_hourly,
  lmdb_usage_report_aggregate_daily,
  lmdb_usage_report_aggregate_weekly,
  lmdb_usage_report_aggregate_monthly,
  lmdb_usage_report_aggregate_yearly,
  lmdb_usage_report_aggregate_total,
  //
  lmdb_usage_report_aggregate_max
} lmdb_usage_report_aggregate;

/*!
  @typedef lmdb_usage_report_range
  Enumerates the (relative) temporal range of rows that should be included
  in the report.  Most values are self-explanatory, except:
  
    lmdb_usage_report_range_last_check
      find the maximum check timestamp in the database and only return counts
      made for that timestamp
*/
typedef enum {
	lmdb_usage_report_range_undef = 0,
	//
	lmdb_usage_report_range_none,
	lmdb_usage_report_range_last_check,
	lmdb_usage_report_range_last_hour,
  lmdb_usage_report_range_last_day,
  lmdb_usage_report_range_last_week,
  lmdb_usage_report_range_last_month,
  lmdb_usage_report_range_last_year,
  //
  lmdb_usage_report_range_max
} lmdb_usage_report_range;

/*!
  @typedef lmdb_usage_report_ref
  Type of an opaque reference to an lmdb_usage_report object.
*/
typedef struct _lmdb_usage_report * lmdb_usage_report_ref;

/*!
  @typedef lmdb_usage_report_create
  Allocate and initialize a new lmdb_usage_report that uses the
  given aggregate function to coallesce rows into temporal "buckets"
  and limits the range of rows by their check timestamp.  An
  lmdb_predicate object may be passed to further limit the scope of
  the selected rows.
*/
lmdb_usage_report_ref lmdb_usage_report_create(lmdb_ref the_db, lmdb_usage_report_aggregate aggregate, lmdb_usage_report_range range, lmdb_predicate_ref predicate);

/*!
  @function lmdb_usage_report_release
  Decrement the reference count of the_query.  When the count reaches
  zero, the_query is deallocated.
*/
void lmdb_usage_report_release(lmdb_usage_report_ref the_query);

/*!
  @typedef lmdb_int_range_t
  Data structure that holds the statistical value of an integer field
  that's been aggregated.  If no aggregate was involved, all three values
  will be the same.
*/
typedef struct {
  int       min, max, avg;
} lmdb_int_range_t;

/*!
  @typedef lmdb_time_range_t
  Data structure that holds a time span in terms of a start and end Unix
  timestamp.
*/
typedef struct {
  time_t    start, end;
} lmdb_time_range_t;

/*!
  @typedef lmdb_iterator_fn
  Type of a callback function passed to lmdb_usage_report_iterate() to
  process results of a query.  The context is a caller-defined pointer-sized
  value passed to lmdb_usage_report_iterate() to provide communication between
  the iterator and the caller.  All other fields are data coming from the
  database.
  
  The function should return false to terminate iteration or true to continue.
*/
typedef bool (*lmdb_iterator_fn)(const void *context, int feature_id, const char *vendor, const char *version, const char *feature_string, lmdb_int_range_t in_use, lmdb_int_range_t issued, time_t expiration_timestamp, lmdb_time_range_t check_timestamp);

/*!
  @function lmdb_usage_report_iterate
  Iterate over the result rows returned for the_query.  The context is a caller-defined
  pointer-sized value that will be passed to the interator_fn each time it is called.
  
  Returns true if all rows were successfully enumerated.
*/
bool lmdb_usage_report_iterate(lmdb_usage_report_ref the_query, lmdb_iterator_fn iterator_fn, const void *context);

#endif /* __LMDB_H__ */
