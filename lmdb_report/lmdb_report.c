/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmdb_report.c
 *
 * Generate reports of license usage.
 *
 */

#include "lmconfig.h"
#include "fscanln.h"
#include "lmdb.h"
#include "lmlog.h"
#include "util_fns.h"

#include <math.h>

//

typedef enum {
	display_field_feature_id						= 0,
	display_field_feature_string,
	display_field_vendor,
	display_field_version,
	display_field_in_use,
	display_field_issued,
	display_field_percentage,
	display_field_expiration_timestamp,
	display_field_check_timestamp,
	//
	display_field_max
} display_field;

const char* display_field_header[] = {
								"id",
								"feature",
								"vendor",
								"version",
								"in-use",
								"issued",
								"percent",
								"expire time",
								"check time"
							};

typedef struct {
	bool				disable[display_field_max];
	int					width[display_field_max];
} display_field_control;

//

static inline display_field_control
display_field_control_make(void)
{
	display_field_control			ctl;
	int												i;
	
	for ( i = display_field_feature_id; i < display_field_max; i++ ) {
		ctl.disable[i] = false;
		ctl.width[i] = strlen(display_field_header[i]);
	}
	return ctl;					
}

//

void
column_print_headers(
	display_field_control		*ctl,
	bool										is_ranged
)
{
	int						i;
	bool					got_one = false;
	
	for ( i = display_field_feature_id; i < display_field_max; i++ ) {
		if ( ! ctl->disable[i] ) {
			printf("%*s ", ctl->width[i], display_field_header[i]);
			got_one = true;
		}
	}
	fputc('\n', stdout);
	for ( i = display_field_feature_id; i < display_field_max; i++ ) {
		if ( ! ctl->disable[i] ) {
			int				j = ctl->width[i];
			
			while ( j-- ) fputc('-', stdout);
			fputc(' ', stdout);
		}
	}
	fputc('\n', stdout);
}

//

bool
column_calc_no_range_iterator(
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
	display_field_control			*ctl = (display_field_control*)context;
	int												l;
	int												pct = ceil((double)in_use.avg / (double)issued.avg);
	
	if ( ! ctl->disable[display_field_feature_id] ) {
		l = 1; while ( feature_id > 10 ) { l++; feature_id /= 10; }
		if ( l > ctl->width[display_field_feature_id] ) ctl->width[display_field_feature_id] = l;
	}
	
	if ( ! ctl->disable[display_field_feature_string] ) {
		l = strlen(feature_string);
		if ( l > ctl->width[display_field_feature_string] ) ctl->width[display_field_feature_string] = l;
	}
	
	if ( ! ctl->disable[display_field_vendor] ) {
		l = strlen(vendor);
		if ( l > ctl->width[display_field_vendor] ) ctl->width[display_field_vendor] = l;
	}
	
	if ( ! ctl->disable[display_field_version] ) {
		l = strlen(version);
		if ( l > ctl->width[display_field_version] ) ctl->width[display_field_version] = l;
	}
	
	if ( ! ctl->disable[display_field_in_use] ) {
		l = 1; while ( in_use.avg >= 10 ) { l++; in_use.avg /= 10; }
		if ( l > ctl->width[display_field_in_use] ) ctl->width[display_field_in_use] = l;
	}
	
	if ( ! ctl->disable[display_field_issued] ) {
		l = 1; while ( issued.avg >= 10 ) { l++; issued.avg /= 10; }
		if ( l > ctl->width[display_field_issued] ) ctl->width[display_field_issued] = l;
	}
	
	if ( ! ctl->disable[display_field_percentage] ) {
		l = 2; while ( pct >= 10 ) { l++; pct /= 10; }
		if ( l > ctl->width[display_field_percentage] ) ctl->width[display_field_percentage] = l;
	}
	
	if ( ! ctl->disable[display_field_expiration_timestamp] ) {
		l = ( expiration_timestamp == lmfeature_no_expiration ) ? strlen("permanent") : strlen("YYYY-mm-dd HH:MM:SS");
		if ( l > ctl->width[display_field_expiration_timestamp] ) ctl->width[display_field_expiration_timestamp] = l;
	}
	
	if ( ! ctl->disable[display_field_check_timestamp] ) {
		if ( 19 > ctl->width[display_field_check_timestamp] ) ctl->width[display_field_check_timestamp] = 19;
	}
	
  return true;
}

//

bool
column_calc_range_iterator(
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
	display_field_control			*ctl = (display_field_control*)context;
	int												l, l2;
	int												pct_avg = ceil(100.0 * (double)in_use.avg / (double)issued.avg);
	int												pct_min = ceil(100.0 * (double)in_use.min / (double)issued.min);
	int												pct_max = ceil(100.0 * (double)in_use.max / (double)issued.max);
	
	if ( ! ctl->disable[display_field_feature_id] ) {
		l = 1; while ( feature_id > 10 ) { l++; feature_id /= 10; }
		if ( l > ctl->width[display_field_feature_id] ) ctl->width[display_field_feature_id] = l;
	}
	
	if ( ! ctl->disable[display_field_feature_string] ) {
		l = strlen(feature_string);
		if ( l > ctl->width[display_field_feature_string] ) ctl->width[display_field_feature_string] = l;
	}
	
	if ( ! ctl->disable[display_field_vendor] ) {
		l = strlen(vendor);
		if ( l > ctl->width[display_field_vendor] ) ctl->width[display_field_vendor] = l;
	}
	
	if ( ! ctl->disable[display_field_version] ) {
		l = strlen(version);
		if ( l > ctl->width[display_field_version] ) ctl->width[display_field_version] = l;
	}
	
	if ( ! ctl->disable[display_field_in_use] ) {
		l = 1; while ( in_use.avg >= 10 ) { l++; in_use.avg /= 10; }
		l2 = 1; while ( in_use.min >= 10 ) { l2++; in_use.min /= 10; }; if ( l2 > l ) l = l2;
		l2 = 1; while ( in_use.max >= 10 ) { l2++; in_use.max /= 10; }; if ( l2 > l ) l = l2;
		
		l = 3 * l + 2;
		
		if ( l > ctl->width[display_field_in_use] ) ctl->width[display_field_in_use] = l;
	}
	
	if ( ! ctl->disable[display_field_issued] ) {
		l = 1; while ( issued.avg >= 10 ) { l++; issued.avg /= 10; }
		l2 = 1; while ( issued.min >= 10 ) { l2++; issued.min /= 10; }; if ( l2 > l ) l = l2;
		l2 = 1; while ( issued.max >= 10 ) { l2++; issued.max /= 10; }; if ( l2 > l ) l = l2;
		
		l = 3 * l + 2;
		
		if ( l > ctl->width[display_field_issued] ) ctl->width[display_field_issued] = l;
	}
	
	if ( ! ctl->disable[display_field_percentage] ) {
		l = 2; while ( pct_min >= 10 ) { l++; pct_min /= 10; }
		l2 = 2; while ( pct_max >= 10 ) { l2++; pct_max /= 10; }; if ( l2 > l ) l = l2;
		l2 = 2; while ( pct_avg >= 10 ) { l2++; pct_avg /= 10; }; if ( l2 > l ) l = l2;
		
		l = 3 * l + 2;
		
		if ( l > ctl->width[display_field_percentage] ) ctl->width[display_field_percentage] = l;
	}
	
	if ( ! ctl->disable[display_field_expiration_timestamp] ) {
		l = ( expiration_timestamp == lmfeature_no_expiration ) ? strlen("permanent") : strlen("YYYY-mm-dd HH:MM:SS");
		if ( l > ctl->width[display_field_expiration_timestamp] ) ctl->width[display_field_expiration_timestamp] = l;
	}
	
	if ( ! ctl->disable[display_field_check_timestamp] ) {
		if ( 41 > ctl->width[display_field_check_timestamp] ) ctl->width[display_field_check_timestamp] = 41;
	}
	
  return true;
}

//

bool
column_display_no_range_iterator(
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
	display_field_control			*ctl = (display_field_control*)context;
	char											check_ts[20], expiration_ts[20];
	double										pct = 100.0 *  (double)in_use.avg / (double)issued.avg;
	
	if ( ! ctl->disable[display_field_check_timestamp] ) {
		struct tm								when;
		
		localtime_r(&check_timestamp.start, &when); strftime(check_ts, sizeof(check_ts), "%Y-%m-%d %H:%M:%S", &when);
	}
	if ( ! ctl->disable[display_field_expiration_timestamp] ) {
		if ( expiration_timestamp == lmfeature_no_expiration ) {
			strncpy(expiration_ts, "permanent", sizeof(expiration_ts));
		} else {
			struct tm							when;
			
			localtime_r(&expiration_timestamp, &when); strftime(expiration_ts, sizeof(expiration_ts), "%Y-%m-%d %H:%M:%S", &when);
		}
	}
	if ( ! ctl->disable[display_field_feature_id] ) printf("%*d ", ctl->width[display_field_feature_id], feature_id);
	if ( ! ctl->disable[display_field_feature_string] ) printf("%*s ", ctl->width[display_field_feature_string], feature_string);
	if ( ! ctl->disable[display_field_vendor] ) printf("%*s ", ctl->width[display_field_vendor], vendor);
	if ( ! ctl->disable[display_field_version] ) printf("%*s ", ctl->width[display_field_version], version);
	if ( ! ctl->disable[display_field_in_use] ) printf("%*d ", ctl->width[display_field_in_use], in_use.avg);
	if ( ! ctl->disable[display_field_issued] ) printf("%*d ", ctl->width[display_field_issued], issued.avg);
	if ( ! ctl->disable[display_field_percentage] ) printf("%*d%% ", ctl->width[display_field_percentage] - 1, (int)ceil(pct));
	if ( ! ctl->disable[display_field_expiration_timestamp] ) printf("%*s ", ctl->width[display_field_expiration_timestamp], expiration_ts);
	if ( ! ctl->disable[display_field_check_timestamp] ) printf("%*s ", ctl->width[display_field_check_timestamp], check_ts);
	
	fputc('\n', stdout);
	
  return true;
}

//

bool
column_display_range_iterator(
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
	display_field_control			*ctl = (display_field_control*)context;
	char											check_ts[42], expiration_ts[20];
	int                       pct_avg = (int)ceil(100.0 *  (double)in_use.avg / (double)issued.avg);
	int                       pct_min = (int)ceil(100.0 *  (double)in_use.min / (double)issued.min);
	int                       pct_max = (int)ceil(100.0 *  (double)in_use.max / (double)issued.max);
	
	if ( ! ctl->disable[display_field_check_timestamp] ) {
		struct tm								when;
		
		localtime_r(&check_timestamp.start, &when); strftime(check_ts, sizeof(check_ts), "%Y-%m-%d %H:%M:%S", &when);
		strncpy(check_ts + strlen(check_ts), " - ", sizeof(check_ts) - strlen(check_ts));
		localtime_r(&check_timestamp.end, &when); strftime(check_ts + strlen(check_ts), sizeof(check_ts) - strlen(check_ts), "%Y-%m-%d %H:%M:%S", &when);
	}
	if ( ! ctl->disable[display_field_expiration_timestamp] ) {
		if ( expiration_timestamp == lmfeature_no_expiration ) {
			strncpy(expiration_ts, "permanent", sizeof(expiration_ts));
		} else {
			struct tm							when;
			
			localtime_r(&expiration_timestamp, &when); strftime(expiration_ts, sizeof(expiration_ts), "%Y-%m-%d %H:%M:%S", &when);
		}
	}
	if ( ! ctl->disable[display_field_feature_id] ) printf("%*d ", ctl->width[display_field_feature_id], feature_id);
	if ( ! ctl->disable[display_field_feature_string] ) printf("%*s ", ctl->width[display_field_feature_string], feature_string);
	if ( ! ctl->disable[display_field_vendor] ) printf("%*s ", ctl->width[display_field_vendor], vendor);
	if ( ! ctl->disable[display_field_version] ) printf("%*s ", ctl->width[display_field_version], version);
	if ( ! ctl->disable[display_field_in_use] ) {
    if ( in_use.min == in_use.max && in_use.min == in_use.avg ) {
      printf("%*d ", ctl->width[display_field_in_use], in_use.min);
    } else {
      int				w = (ctl->width[display_field_in_use] - 2) / 3;
      
      printf("%*d/%*d/%*d ", w, in_use.min, w, in_use.max, w, in_use.avg);
    }
	}
	if ( ! ctl->disable[display_field_issued] ) {
    if ( issued.min == issued.max && issued.min == issued.avg ) {
      printf("%*d ", ctl->width[display_field_issued], issued.min);
    } else {
      int				w = (ctl->width[display_field_issued] - 2) / 3;
      
      printf("%*d/%*d/%*d ", w, issued.min, w, issued.max, w, issued.avg);
    }
	}
	if ( ! ctl->disable[display_field_percentage] ) {
    if ( pct_min == pct_max && pct_min == pct_avg ) {
      printf("%*d%% ", ctl->width[display_field_percentage] - 1, pct_min);
    } else {
      int				w = ((ctl->width[display_field_percentage] - 2) / 3) - 1;
      
      printf("%*d%%/%*d%%/%*d%% ", w, (int)ceil(pct_min), w, (int)ceil(pct_max), w, (int)ceil(pct_avg));
    }
	}
	if ( ! ctl->disable[display_field_expiration_timestamp] ) printf("%*s ", ctl->width[display_field_expiration_timestamp], expiration_ts);
	if ( ! ctl->disable[display_field_check_timestamp] ) printf("%*s ", ctl->width[display_field_check_timestamp], check_ts);
	
	fputc('\n', stdout);
	
  return true;
}

//

void
csv_print_headers(
	display_field_control		*ctl,
	bool										is_ranged
)
{
	int						i;
	const char		*comma = "";
	
	if ( is_ranged ) {
		for ( i = display_field_feature_id; i < display_field_max; i++ ) {
			if ( ! ctl->disable[i] ) {
				switch ( i ) {
					case display_field_in_use:
					case display_field_issued:
					case display_field_percentage:
						printf("%1$s\"%2$s min\",\"%2$s max\",\"%2$s avg\"", comma, display_field_header[i]);
						break;
					case display_field_check_timestamp:
						printf("%1$s\"%2$s start\",\"%2$s end\"", comma, display_field_header[i]);
						break;
					default:
						printf("%s\"%s\"", comma, display_field_header[i]);
						break;
				}
				comma = ",";
			}
		}
	} else {
		for ( i = display_field_feature_id; i < display_field_max; i++ ) {
			if ( ! ctl->disable[i] ) {
				printf("%s\"%s\"", comma, display_field_header[i]);
				comma = ",";
			}
		}
	}
	fputc('\n', stdout);
}

//

bool
csv_display_no_range_iterator(
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
	display_field_control			*ctl = (display_field_control*)context;
	char											check_ts[20], expiration_ts[20];
	double										pct = (double)in_use.avg / (double)issued.avg;
	const char								*comma = "";
	
	if ( ! ctl->disable[display_field_check_timestamp] ) {
		struct tm								when;
		
		localtime_r(&check_timestamp.start, &when); strftime(check_ts, sizeof(check_ts), "%Y-%m-%d %H:%M:%S", &when);
	}
	if ( ! ctl->disable[display_field_expiration_timestamp] ) {
		if ( expiration_timestamp == lmfeature_no_expiration ) {
			strncpy(expiration_ts, "permanent", sizeof(expiration_ts));
		} else {
			struct tm							when;
			
			localtime_r(&expiration_timestamp, &when); strftime(expiration_ts, sizeof(expiration_ts), "%Y-%m-%d %H:%M:%S", &when);
		}
	}
	if ( ! ctl->disable[display_field_feature_id] ) {
		printf("\"%d\"", feature_id); comma = ",";
	}
	if ( ! ctl->disable[display_field_feature_string] ) {
		printf("%s\"%s\"", comma, feature_string); comma = ",";
	}
	if ( ! ctl->disable[display_field_vendor] ) {
		printf("%s\"%s\"", comma, vendor); comma = ",";
	}
	if ( ! ctl->disable[display_field_version] ) {
		printf("%s\"%s\"", comma, version); comma = ",";
	}
	if ( ! ctl->disable[display_field_in_use] ) {
		printf("%s%d", comma, in_use.avg); comma = ",";
	}
	if ( ! ctl->disable[display_field_issued] ) {
		printf("%s%d", comma, issued.avg); comma = ",";
	}
	if ( ! ctl->disable[display_field_percentage] ) {
		printf("%s%.3f", comma, pct); comma = ",";
	}
	if ( ! ctl->disable[display_field_expiration_timestamp] ) {
		printf("%s\"%s\"", comma, expiration_ts); comma = ",";
	}
	if ( ! ctl->disable[display_field_check_timestamp] ) {
		printf("%s\"%s\"", comma, check_ts);
	}
	fputc('\n', stdout);
	
  return true;
}

//

bool
csv_display_range_iterator(
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
	display_field_control			*ctl = (display_field_control*)context;
	char											check_start_ts[20], check_end_ts[20], expiration_ts[20];
	double										pct_avg = 100.0 *  (double)in_use.avg / (double)issued.avg;
	double										pct_min = 100.0 *  (double)in_use.min / (double)issued.min;
	double										pct_max = 100.0 *  (double)in_use.max / (double)issued.max;
	const char								*comma = "";
	
	if ( ! ctl->disable[display_field_check_timestamp] ) {
		struct tm								when;
		
		localtime_r(&check_timestamp.start, &when); strftime(check_start_ts, sizeof(check_start_ts), "%Y-%m-%d %H:%M:%S", &when);
		localtime_r(&check_timestamp.end, &when); strftime(check_end_ts, sizeof(check_end_ts), "%Y-%m-%d %H:%M:%S", &when);
	}
	if ( ! ctl->disable[display_field_expiration_timestamp] ) {
		if ( expiration_timestamp == lmfeature_no_expiration ) {
			strncpy(expiration_ts, "permanent", sizeof(expiration_ts));
		} else {
			struct tm							when;
			
			localtime_r(&expiration_timestamp, &when); strftime(expiration_ts, sizeof(expiration_ts), "%Y-%m-%d %H:%M:%S", &when);
		}
	}
	if ( ! ctl->disable[display_field_feature_id] ) {
		printf("%d", feature_id); comma = ",";
	}
	if ( ! ctl->disable[display_field_feature_string] ) {
		printf("%s\"%s\"", comma, feature_string); comma = ",";
	}
	if ( ! ctl->disable[display_field_vendor] ) {
		printf("%s\"%s\"", comma, vendor); comma = ",";
	}
	if ( ! ctl->disable[display_field_version] ) {
		printf("%s\"%s\"", comma, version); comma = ",";
	}
	if ( ! ctl->disable[display_field_in_use] ) {
		printf("%s%d,%d,%d", comma, in_use.min, in_use.max, in_use.avg); comma = ",";
	}
	if ( ! ctl->disable[display_field_issued] ) {
		printf("%s%d,%d,%d", comma, issued.min, issued.max, issued.avg); comma = ",";
	}
	if ( ! ctl->disable[display_field_percentage] ) {
		printf("%s%.3f,%.3f,%.3f", comma, pct_min, pct_max, pct_avg); comma = ",";
	}
	if ( ! ctl->disable[display_field_expiration_timestamp] ) {
		printf("%s\"%s\"", comma, expiration_ts); comma = ",";
	}
	if ( ! ctl->disable[display_field_check_timestamp] ) {
		printf("%s\"%s\",\"%s\"", comma, check_start_ts, check_end_ts);
	}
	fputc('\n', stdout);
	
  return true;
}

//

lmdb_predicate_operator
finish_parsing_matching_option(
  const char*     *option
)
{
  if ( strncasecmp(*option, "{R}", 3) == 0 ) {
    *option = *option + 3;
    return lmdb_predicate_operator_regexp;
  }
  if ( strncasecmp(*option, "{L}", 3) == 0 ) {
    *option = *option + 3;
    return lmdb_predicate_operator_like;
  }
  if ( strncasecmp(*option, "{G}", 3) == 0 ) {
    *option = *option + 3;
    return lmdb_predicate_operator_glob;
  }
  return lmdb_predicate_operator_eq;
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
  
  if ( the_conf && the_conf->license_db_path ) {
    lmdb_ref          the_database = NULL;
    
    //
    // If a database file was present, get it opened.  If there was
    // no file, at least open an in-memory database:
    //
    the_database = lmdb_create_read_only(the_conf->license_db_path);
    if ( the_database ) {
      lmdb_usage_report_ref         the_report;
      display_field_control         column_ctl = display_field_control_make();
      lmdb_usage_report_aggregate   aggregate = the_conf->report_aggregate;
      lmdb_usage_report_range       range = the_conf->report_range;
      lmdb_predicate_ref            predicate = NULL;
      
      //
      // Disable/enable columns according to CLI options:
      //
      column_ctl.disable[display_field_feature_id] = (the_conf->fields_for_display & field_selection_feature_id) ? false : true;
      column_ctl.disable[display_field_in_use] = (the_conf->fields_for_display & field_selection_counts) ? false : true;
      column_ctl.disable[display_field_issued] = (the_conf->fields_for_display & field_selection_counts) ? false : true;
      column_ctl.disable[display_field_percentage] = (the_conf->fields_for_display & field_selection_percentage) ? false : true;
      column_ctl.disable[display_field_expiration_timestamp] = (the_conf->fields_for_display & field_selection_expire_timestamp) ? false : true;
      column_ctl.disable[display_field_check_timestamp] = (the_conf->fields_for_display & field_selection_check_timestamp) ? false : true;
      
      //
      // Any date range stuff supplied?
      //
      if ( the_conf->checked_time[0] || the_conf->checked_time[1] ) {
        if ( the_conf->checked_time[0] ) {
          if ( the_conf->checked_time[1] ) {
            //
            // Both times provided:
            //
            char                  time_str[24];
            time_t                delta;
            
            snprintf(time_str, sizeof(time_str), "%lld", (long long int)the_conf->checked_time[0]);
            predicate = lmdb_predicate_create_with_test(lmdb_predicate_field_checked, lmdb_predicate_operator_ge, time_str);
            snprintf(time_str, sizeof(time_str), "%lld", (long long int)the_conf->checked_time[1]);
            lmdb_predicate_add_test(predicate, lmdb_predicate_combiner_and, lmdb_predicate_field_checked, lmdb_predicate_operator_le, time_str);
          } else {
            //
            // Start time alone, use it plus the range requested:
            //
            char                  time_str[24];
            time_t                delta;
            
            snprintf(time_str, sizeof(time_str), "%lld", (long long int)the_conf->checked_time[0]);
            predicate = lmdb_predicate_create_with_test(lmdb_predicate_field_checked, lmdb_predicate_operator_ge, time_str);
            if ( range != lmdb_usage_report_range_none ) {
              switch ( range ) {
                case lmdb_usage_report_range_last_year:
                  delta = 365 * 86400;
                  break;
                case lmdb_usage_report_range_last_month:
                  delta = 30 * 86400;
                  break;
                case lmdb_usage_report_range_last_week:
                  delta = 7 * 86400;
                  break;
                case lmdb_usage_report_range_last_day:
                  delta = 86400;
                  break;
                case lmdb_usage_report_range_last_hour:
                  delta = 3600;
                  break;
                case lmdb_usage_report_range_none:
                case lmdb_usage_report_range_undef:
                case lmdb_usage_report_range_last_check:
                case lmdb_usage_report_range_max:
                  // Should never get here:
                  break;
              }
              snprintf(time_str, sizeof(time_str), "%lld", (long long int)the_conf->checked_time[0] + delta);
              lmdb_predicate_add_test(predicate, lmdb_predicate_combiner_and, lmdb_predicate_field_checked, lmdb_predicate_operator_le, time_str);
            }
          }
        } else {
          //
          // End time alone, use it plus the range requested:
          //
          char                  time_str[24];
          time_t                delta;
          
          snprintf(time_str, sizeof(time_str), "%lld", (long long int)the_conf->checked_time[1]);
          predicate = lmdb_predicate_create_with_test(lmdb_predicate_field_checked, lmdb_predicate_operator_le, time_str);
          if ( range != lmdb_usage_report_range_none ) {
            switch ( range ) {
              case lmdb_usage_report_range_last_year:
                delta = 365 * 86400;
                break;
              case lmdb_usage_report_range_last_month:
                delta = 30 * 86400;
                break;
              case lmdb_usage_report_range_last_week:
                delta = 7 * 86400;
                break;
              case lmdb_usage_report_range_last_day:
                delta = 86400;
                break;
              case lmdb_usage_report_range_last_hour:
                delta = 3600;
                break;
              case lmdb_usage_report_range_none:
              case lmdb_usage_report_range_undef:
              case lmdb_usage_report_range_last_check:
              case lmdb_usage_report_range_max:
                // Should never get here:
                break;
            }
            snprintf(time_str, sizeof(time_str), "%lld", (long long int)the_conf->checked_time[1] - delta);
            lmdb_predicate_add_test(predicate, lmdb_predicate_combiner_and, lmdb_predicate_field_checked, lmdb_predicate_operator_ge, time_str);
          }
        }
        range = lmdb_usage_report_range_none;
      }
      
      //
      // Feature string matching?
      //
      if ( the_conf->match_feature ) {
        const char                *pattern = the_conf->match_feature;
        lmdb_predicate_operator   operator = finish_parsing_matching_option(&pattern);
        
        if ( predicate ) {
          lmdb_predicate_add_test(predicate, lmdb_predicate_combiner_and, lmdb_predicate_field_feature, operator, pattern);
        } else {
          predicate = lmdb_predicate_create_with_test(lmdb_predicate_field_feature, operator, pattern);
        }
      }
      
      //
      // Vendor string matching?
      //
      if ( the_conf->match_vendor ) {
        const char                *pattern = the_conf->match_feature;
        lmdb_predicate_operator   operator = finish_parsing_matching_option(&pattern);
        
        if ( predicate ) {
          lmdb_predicate_add_test(predicate, lmdb_predicate_combiner_and, lmdb_predicate_field_vendor, operator, pattern);
        } else {
          predicate = lmdb_predicate_create_with_test(lmdb_predicate_field_vendor, operator, pattern);
        }
      }
      
      //
      // Version string matching?
      //
      if ( the_conf->match_version ) {
        const char                *pattern = the_conf->match_version;
        lmdb_predicate_operator   operator = finish_parsing_matching_option(&pattern);
        
        if ( predicate ) {
          lmdb_predicate_add_test(predicate, lmdb_predicate_combiner_and, lmdb_predicate_field_version, operator, pattern);
        } else {
          predicate = lmdb_predicate_create_with_test(lmdb_predicate_field_version, operator, pattern);
        }
      }
      
      //
      // Generate report results:
      //
      the_report = lmdb_usage_report_create(
															the_database,
															aggregate,
															range,
															predicate
														);
      if ( the_report ) {
      	bool			is_ranged = (the_conf->report_aggregate == lmdb_usage_report_aggregate_none) ? false : true;
      	
      	switch ( the_conf->report_format ) {
      	
      		case report_format_column:
		        lmdb_usage_report_iterate(the_report, is_ranged ? column_calc_range_iterator : column_calc_no_range_iterator, (const void*)&column_ctl);
						if ( the_conf->should_show_headers ) column_print_headers(&column_ctl, is_ranged);
						lmdb_usage_report_iterate(the_report, is_ranged ? column_display_range_iterator : column_display_no_range_iterator, (const void*)&column_ctl);
						break;
					
					case report_format_csv: {
		        if ( the_conf->should_show_headers ) csv_print_headers(&column_ctl, is_ranged);
    		    lmdb_usage_report_iterate(the_report, is_ranged ? csv_display_range_iterator : csv_display_no_range_iterator, (const void*)&column_ctl);
    		    break;
    		  }
          
          case report_format_max:
            break;
    		  
    		}
        lmdb_usage_report_release(the_report);
      }
      
      lmdb_release(the_database);
    }
  } else {
    lmlog(lmlog_level_error, "No license database configured");
  }
  return rc;
}
