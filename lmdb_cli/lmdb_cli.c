/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmdb_cli.c
 *
 * Main program for CLI access to a FLEXlm license count database
 *
 */

#include "lmconfig.h"
#include "fscanln.h"
#include "lmdb.h"
#include "lmlog.h"
#include "util_fns.h"

//

extern char **environ;

//

static const char   *flexlm_feature_regex = "^[[:space:]]*(FEATURE|INCREMENT)[[:space:]]+([^[:space:]]+)[[:space:]]+([^[:space:]]+)[[:space:]]+([^[:space:]]+)[[:space:]]+([[:digit:]]{2}-[[:alpha:]]{3}-([[:digit:]]{1,4})|permanent)[[:space:]]+([[:digit:]]+)";
static int          flexlm_feature_regex_flags = REG_ICASE | REG_EXTENDED;
static int          flexlm_feature_match_count = 8;

//

static const char   *lmstat_feature_regex = "(Users of ([^:]+):.*Total of ([0-9]+) licenses? issued;[[:space:]]+Total of ([0-9]+) licenses? in use|\"([^\"]+)\"[[:space:]]+v([^,]+),[[:space:]]+vendor:[[:space:]]+([^,]+),[[:space:]]+expiry:[[:space:]]+([[:digit:]]{1,2}-[[:alpha:]]{3}-([[:digit:]]{1,4})|permanent))";
static int          lmstat_feature_regex_flags = REG_ICASE | REG_EXTENDED;
static int          lmstat_feature_match_count = 9;

//

bool
lmdb_usage_iterator(
	const void				*context,
	int								feature_id,
	const char 				*vendor,
	const char 				*version,
	const char 				*feature_string,
	lmdb_int_range_t	in_use,
	lmdb_int_range_t	issued,
	time_t					  expiration_timestamp,
	lmdb_time_range_t check_timestamp
)
{
	double						usage_pct = (double)in_use.avg / (double)issued.avg;
	
	if ( usage_pct >= 0.900 ) {
		if ( usage_pct >= 0.990 ) {
  		printf("%s (%s v%s): %d / %d (%.1f%%)\n", 
    			feature_string,
       		vendor,
         	version,
          in_use.avg,
          issued.avg,
          100.0 * usage_pct
				);
		}
	}
	return true;
}

//

int
main(
  int           argc,
  char * const  argv[]
)
{
  lmconfig      *the_conf = lmconfig_update_with_options(NULL, argc, argv);
  int           rc = 0;
  
  //
  // Now update from whatever configuration file we're supposed to be
  // using:
  //
  if ( file_exists(the_conf->base_config_path) ) the_conf = lmconfig_update_with_file(the_conf, the_conf->base_config_path);
  
  //
  // Command line arguments also override whatever may have been in a
  // configure file:
  //
  the_conf = lmconfig_update_with_options(the_conf, argc, argv);
  
  if ( the_conf ) {
    lmdb_ref          the_database = NULL;
    
    //
    // If a database file was present, get it opened.  If there was
    // no file, at least open an in-memory database:
    //
    the_database = lmdb_create(the_conf->license_db_path ?  : ":memory:");
    if ( the_database ) {
#ifndef LMDB_DISABLE_RRDTOOL
      if ( the_conf->rrd_repodir && the_conf->should_update_rrds ) {
        lmdb_set_rrd_repodir(the_database, the_conf->rrd_repodir);
      }
#endif
      //
      // If a FLEXlm license file was present, then scan it for features:
      //
      if ( the_conf->flexlm_license_path ) {
        fscanln_ref   flexlm_scanner = fscanln_create_with_file(the_conf->flexlm_license_path);
        
        if ( flexlm_scanner ) {
          LMDEBUG("opened FLEXlm license file %s for scanning", the_conf->flexlm_license_path);
          if ( fscanln_set_line_regex(flexlm_scanner, flexlm_feature_regex, flexlm_feature_regex_flags, flexlm_feature_match_count) ) {
            const char      *next_line;
            
            while ( fscanln_get_line(flexlm_scanner, &next_line, NULL) ) {
              const char  *vendor, *version, *feature_string, *expiration, *count;
              time_t      expire_ts = lmfeature_no_expiration;
              
              feature_string = fscanln_get_sub_match_string(flexlm_scanner, 2);
              vendor = fscanln_get_sub_match_string(flexlm_scanner, 3);
              version = fscanln_get_sub_match_string(flexlm_scanner, 4);
              expiration = fscanln_get_sub_match_string(flexlm_scanner, 5);
              count = fscanln_get_sub_match_string(flexlm_scanner, 7);
                
              if ( expiration ) {
                if ( strcasecmp(expiration, "permanent") != 0 ) {
                	const char	*year = expiration;
                	int					dashes = 0;
                	
                	while ( *year && (dashes < 2) ) {
                		if ( *year == '-' ) dashes++;
                		year++;
                	}
                	if ( dashes == 2 ) {
										size_t    zero_run = strspn(year, "0");
									
										if ( zero_run && (*(year + zero_run) == '\0') ) {
											// A year that's all zeroes => permanent
										} else {
											struct tm   expire_conv;
										
											memset(&expire_conv, 0, sizeof(expire_conv));
											expire_conv.tm_isdst = -1;
											if ( strptime(expiration, "%d-%b-%Y", &expire_conv) ) {
												expire_ts = mktime(&expire_conv);
											}
										}
									}
                }
              }
        
              if ( feature_string && vendor && version ) {
                lmfeature_ref   feature = lmdb_get_feature_by_name(the_database, feature_string, vendor, version);
                
                if ( feature ) {
                  long    issued = strtol(count, NULL, 10);
                  
                  if ( issued >= 0 ) {
                    LMDEBUG("%s (%s %s), incrementing seat count by %ld", feature_string, vendor, version, issued);
                    lmfeature_add_issued(feature, issued);
                  }
                  if ( expire_ts != lmfeature_get_expiration_date(feature) ) {
                    LMDEBUG("%s (%s %s), setting expiration timestamp %lld", feature_string, vendor, version, (long long int)expire_ts);
                    lmfeature_set_expiration_date(feature, expire_ts);
                  }
                }
              }
            }
          }
          fscanln_release(flexlm_scanner);
        } else {
          lmlogf(lmlog_level_error, "failed to create file scanner for '%s'", the_conf->flexlm_license_path);
        }
      
				//
				// If any lmstat stuff was configured, handle it now:
				//
				if ( the_conf->lmstat_interface_kind != lmstat_interface_kind_unset ) {
					fscanln_ref     lmstat_scanner = NULL;
				
					switch ( the_conf->lmstat_interface_kind ) {
				
						default:
						case lmstat_interface_kind_unset:
							break;
					
						case lmstat_interface_kind_static_output:
							LMDEBUG("attempting to open %s lmstat output", the_conf->lmstat_interface.static_output);
							lmstat_scanner = fscanln_create_with_file(the_conf->lmstat_interface.static_output);
							break;
					
						case lmstat_interface_kind_command:
							LMDEBUG("attempting to execute \"%s\" lmstat command in shell", the_conf->lmstat_interface.command);
							lmstat_scanner = fscanln_create_with_command(the_conf->lmstat_interface.command);
							break;
					
						case lmstat_interface_kind_exec:
							LMDEBUG("attempting to execute \"%s\"", the_conf->lmstat_interface.exec[0]);
							lmstat_scanner = fscanln_create_with_execve(the_conf->lmstat_interface.exec[0], the_conf->lmstat_interface.exec, environ, true);
							break;
					
					}
					if ( lmstat_scanner ) {
						LMDEBUG("lmstat source opened and ready to scan");
						if ( fscanln_set_line_regex(lmstat_scanner, lmstat_feature_regex, lmstat_feature_regex_flags, lmstat_feature_match_count) ) {
							const char    *next_line;
							const char    *vendor = NULL, *version = NULL, *feature_string = NULL, *expiration = NULL;
							time_t        expire_ts = lmfeature_no_expiration;
							int           in_use = -1, issued = -1;
						
							while ( fscanln_get_line(lmstat_scanner, &next_line, NULL) ) {
								//
								// What did we match, a "Users of" line or a vendor info line?
								//
								if ( strstr(next_line, "Users of") ) {
								  const char		*issued_string = fscanln_get_sub_match_string(lmstat_scanner, 3);
									const char    *in_use_string = fscanln_get_sub_match_string(lmstat_scanner, 4);
                  const char    *feature_scanned = fscanln_get_sub_match_string(lmstat_scanner, 2);
                  
                  lmlogf(lmlog_level_debug, "Found Users of line: %s %s of %s", feature_scanned ? feature_scanned : "<unknown>", in_use_string ? in_use_string : "<unknown>", issued_string ? issued_string : "<unknown>");
									if ( feature_string ) {
                    free((void*)feature_string);
                    feature_string= NULL;
                  }
                  in_use = -1; issued = -1;
                  if ( feature_scanned ) {
                    feature_string = strdup(feature_scanned);
										in_use = in_use_string ? strtol(in_use_string, NULL, 10) : 0;
										issued = issued_string ? strtol(issued_string, NULL, 10) : 0;
									}
									// Reset other pieces that will come from a vendor info line:
									vendor = version = expiration = NULL;
									expire_ts = lmfeature_no_expiration;
								} else if ( feature_string && (in_use >= 0) ) {
									//
									// Vendor info line:
									//
									const char    *feature_string_check = fscanln_get_sub_match_string(lmstat_scanner, 5);
                  
                  lmlogf(lmlog_level_debug, "Found vendor line: %s", feature_string_check ? feature_string_check : "<unknown>");
								
									if ( strcmp(feature_string, feature_string_check) == 0 ) {
										version = fscanln_get_sub_match_string(lmstat_scanner, 6);
										vendor = fscanln_get_sub_match_string(lmstat_scanner, 7);
										expiration = fscanln_get_sub_match_string(lmstat_scanner, 8);
									
										if ( expiration ) {
											if ( strcasecmp(expiration, "permanent") != 0 ) {
												const char	*year = expiration;
												int					dashes = 0;
									
												while ( *year && (dashes < 2) ) {
													if ( *year == '-' ) dashes++;
													year++;
												}
												if ( dashes == 2 ) {
													size_t    zero_run = strspn(year, "0");
											
													if ( zero_run && (*(year + zero_run) == '\0') ) {
														// A year that's all zeroes => permanent
													} else {
														struct tm   expire_conv;
												
														memset(&expire_conv, 0, sizeof(expire_conv));
														expire_conv.tm_isdst = -1;
														if ( strptime(expiration, "%d-%b-%Y", &expire_conv) ) {
															expire_ts = mktime(&expire_conv);
														}
													}
												}
											}
										}
										LMDEBUG("from lmstat => %s (%s %s) expires %lld = %d", feature_string, vendor, version, (long long int)expire_ts, in_use);
										if ( (in_use >= 0) && feature_string && vendor && version ) {
											lmfeature_ref   feature = lmdb_get_feature_by_name(the_database, feature_string, vendor, version);
										
											if ( feature ) {
												LMDEBUG("%s (%s %s), incrementing in-use by %d (issued = %d)", feature_string, vendor, version, in_use, issued);
												lmfeature_add_in_use(feature, in_use);
												if ( issued > 0 ) lmfeature_set_issued(feature, issued);
											
												if ( expire_ts != lmfeature_get_expiration_date(feature) ) {
													LMDEBUG("%s (%s %s), setting expiration timestamp %lld", feature_string, vendor, version, (long long int)expire_ts);
													lmfeature_set_expiration_date(feature, expire_ts);
												}
											}
										}
									}
								}
							}
						}
						fscanln_release(lmstat_scanner);
					}
				}
			
				//
				// Save any updates:
				//
				lmdb_commit_counts(the_database, lmdb_check_timestamp_now);
			}
      lmdb_release(the_database);
    }
  } else {
    rc = EINVAL;
  }
  return rc;
}
