/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmdb_nagios_check.c
 *
 * Check a FLEXlm license database for expired licenses or usage beyond
 * thresholds.
 *
 */

#include "lmconfig.h"
#include "fscanln.h"
#include "lmdb.h"
#include "lmlog.h"
#include "util_fns.h"

//

enum {
	nagios_exit_code_ok				= 0,
	nagios_exit_code_warning	= 1,
	nagios_exit_code_critical = 2,
	nagios_exit_code_unknown 	= 3
};
static int						nagios_exit_code = nagios_exit_code_ok;
static const char			*nagios_exit_code_strings[] = {
														"OK",
              							"WARNING",
                     				"CRITICAL",
                         		"UNKNOWN"
                        };

static const char			*nagios_messages = NULL;

//

typedef struct {
	nagios_rules_ref					rules;
	nagios_threshold					default_warn, default_crit;
  time_t                    max_check_timestamp;
} nagios_usage_context;

//

void
nagios_messages_append(
	int						exit_status,
	const char		*s
)
{
	if ( exit_status > nagios_exit_code ) nagios_exit_code = exit_status;
	if ( nagios_messages ) {
		nagios_messages = strappendm(nagios_messages, "; ", s, NULL);
  } else {
    nagios_messages = strappendm(nagios_messages, s, NULL);
	}
}

void
nagios_messages_appendf(
  int          	exit_status,
	const char		*format,
	...
)
{
	va_list				vargs;
	int						slen;
	char					static_buffer[4096];
	
	va_start(vargs, format);
	slen = vsnprintf(static_buffer, sizeof(static_buffer), format, vargs);
	va_end(vargs);
	
	if ( slen < sizeof(static_buffer) ) {
		nagios_messages_append(exit_status, static_buffer);
	} else {
		char				*s = malloc(++slen);
  
  	if ( s ) {
      va_start(vargs, format);
      slen = vsnprintf(s, slen, format, vargs);
      va_end(vargs);
      nagios_messages_append(exit_status, s);
      free((void*)s);
    }
	}
}

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
	nagios_usage_context	*usage_conf = (nagios_usage_context*)context;
	double								usage_pct = (double)in_use.avg / (double)issued.avg;
	int										rc = nagios_exit_code_ok;
	bool									should_test = true;
	nagios_threshold			warn = nagios_threshold_default, crit = nagios_threshold_default;
  
  if ( check_timestamp.start > usage_conf->max_check_timestamp ) usage_conf->max_check_timestamp = check_timestamp.start;
  if ( check_timestamp.end > usage_conf->max_check_timestamp ) usage_conf->max_check_timestamp = check_timestamp.end;
	
	if ( usage_conf->rules ) {
		const char					*feature_tuple = strcatm(feature_string, ":", vendor, ":", version, NULL);
		
		if ( feature_tuple ) {
			nagios_rule_result	rc = nagios_rules_apply(usage_conf->rules, feature_tuple, &warn, &crit);
			
			LMDEBUG("nagios_rules_apply(%s) = %d (w=%f c=%f)", feature_tuple, rc, warn.value, crit.value);
			should_test = (rc == nagios_rule_result_include) ? true : false;
			free((void*)feature_tuple);
		}
	}
	if ( should_test ) {
		if ( nagios_threshold_is_default(&warn) ) warn = usage_conf->default_warn;
		if ( nagios_threshold_match(&warn, in_use.avg, issued.avg) ) {
			if ( nagios_threshold_is_default(&crit) ) crit = usage_conf->default_crit;
			rc = nagios_threshold_match(&crit, in_use.avg, issued.avg) ? nagios_exit_code_critical : nagios_exit_code_warning;
			nagios_messages_appendf(rc, "%s (%s v%s) %d/%d (%.1f%%)",
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

bool
lmdb_expiration_iterator(
  const void        *context,
  int               feature_id,
  const char        *vendor,
  const char        *version,
  const char        *feature_string,
  lmdb_int_range_t  in_use,
  lmdb_int_range_t  issued,
  time_t            expiration_timestamp,
  lmdb_time_range_t check_timestamp
)
{
	if ( expiration_timestamp > 0 ) {
		time_t			now = time(NULL);
  	int64_t			seconds = (expiration_timestamp - now);
    
  	if ( seconds < 0 ) {
   		nagios_messages_appendf(nagios_exit_code_critical, "%s (%s v%s) has expired", feature_string, vendor, version);
    } else {
     	int				minutes, hours, days;
      int				rc = nagios_exit_code_ok;
      
      days = seconds / 86400; seconds -= 86400 * days;
      hours = seconds / 3600; seconds -= 3600 * hours;
      minutes = seconds / 60; seconds -= 60 * minutes;
      
      if ( days < 30 ) {
      	rc = (days < 7) ? nagios_exit_code_critical : nagios_exit_code_warning;
        nagios_messages_appendf(rc, "%s (%s v%s) will expire in %d %02d:%02d:%02lld",
              feature_string, vendor, version, days, hours, minutes, seconds
            );
      }
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
  
  if ( ! the_conf ) exit(EINVAL);
  
  //
  // Now update from whatever configuration file we're supposed to be
  // using:
  //
  if ( file_exists(the_conf->base_config_path) ) the_conf = lmconfig_update_with_file(the_conf, the_conf->base_config_path);
  
  if ( ! the_conf ) exit(EINVAL);
  
  //
  // Command line arguments also override whatever may have been in a
  // configure file:
  //
  the_conf = lmconfig_update_with_options(the_conf, argc, argv);
  
  if ( the_conf && the_conf->license_db_path ) {
    lmdb_ref          the_database = NULL;
    
    //
    // If a database file was present, get it opened.  If there was
    // no file, at least open an in-memory database:
    //
    the_database = lmdb_create_read_only(the_conf->license_db_path);
    if ( the_database ) {
      lmdb_usage_report_ref		the_report;
      
      the_report = lmdb_usage_report_create(
															the_database,
															lmdb_usage_report_aggregate_none,
															lmdb_usage_report_range_last_check,
															NULL
														);
      if ( the_report ) {
      	nagios_usage_context		usage_conf = {
      															.rules = the_conf->nagios_rules,
      															.default_warn = the_conf->nagios_default_warn,
      															.default_crit = the_conf->nagios_default_crit,
                                    .max_check_timestamp = 0
      														};
      	time_t                  age;
        
      	lmdb_usage_report_iterate(the_report, lmdb_usage_iterator, (const void*)&usage_conf);
        if ( usage_conf.max_check_timestamp == 0 ) {
          nagios_messages_append(nagios_exit_code_critical, "no feature counts found in database");
        }
        else if ( (age = (time(NULL) - usage_conf.max_check_timestamp)) > the_conf->maximum_data_age ) {
          const char            *age_unit = "second";
          
          if ( age > 60 ) {
            age /= 60;
            if ( age > 60 ) {
              age /= 60;
              if ( age > 24 ) {
                age /= 24;
                age_unit = "day";
              } else {
                age_unit = "hour";
              }
            } else {
              age_unit = "minute";
            }
          }
          nagios_messages_appendf(nagios_exit_code_critical, "usage counts data is %lld %s%s old", (long long int)age, age_unit, (age == 1) ? "" : "s");
        } else {
          lmlogf(lmlog_level_info, "usage counts are %lld second%s old", age, (age == 1) ? "" : "s");
        }
        
      	lmdb_usage_report_release(the_report);
      }
      
      the_report = lmdb_usage_report_create(
                            the_database,
                            lmdb_usage_report_aggregate_total,
                            lmdb_usage_report_range_none,
                            NULL
                          );
      if ( the_report ) {
        lmdb_usage_report_iterate(the_report, lmdb_expiration_iterator, NULL);
        lmdb_usage_report_release(the_report);
      }
      
      lmdb_release(the_database);
    }
  } else {
    nagios_messages_append(nagios_exit_code_unknown, "No license database configured");
  }
  switch ( nagios_exit_code ) {
  
  	case nagios_exit_code_ok:
   		printf("OK: no expired licenses or usage threshold problems\n");
      break;
  
    case nagios_exit_code_warning:
      printf("WARNING: %s\n", nagios_messages ? nagios_messages : "no messages, that's odd");
      break;
  
    case nagios_exit_code_critical:
      printf("CRITICAL: %s\n", nagios_messages ? nagios_messages : "no messages, that's odd");
      break;
  
    case nagios_exit_code_unknown:
      printf("UNKNOWN: %s\n", nagios_messages ? nagios_messages : "generic problem with lmdb_nagios_check");
      break;
  
  }
  return nagios_exit_code;
}
