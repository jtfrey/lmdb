/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmdb_ls.c
 *
 * List license features present in a database.
 *
 */

#include "lmconfig.h"
#include "lmdb.h"
#include "lmlog.h"
#include "util_fns.h"

#include <math.h>

//

bool feature_iterator(
  const void      *context,
  lmfeature_ref   a_feature
)
{
  lmconfig        *the_conf = (lmconfig*)context;
  
  if ( the_conf->name_only ) {
    printf("%s\n",
        lmfeature_get_feature_string(a_feature)
      );
  } else {
    printf("%-5d %s (%s %s)\n",
        lmfeature_get_feature_id(a_feature),
        lmfeature_get_feature_string(a_feature),
        lmfeature_get_vendor(a_feature),
        lmfeature_get_version(a_feature)
      );
  }
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
      lmdb_predicate_ref  predicate = NULL;
      lmfeatureset_ref    the_features;
      
      //
      // Id provided?
      //
      if ( the_conf->match_id != lmfeature_no_id ) {
        char              id_str[24];
        
        snprintf(id_str, sizeof(id_str), "%d", the_conf->match_id);
        predicate = lmdb_predicate_create_with_test(lmdb_predicate_field_feature_id, lmdb_predicate_operator_eq, id_str);
      } else {
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
      }
      
      the_features = lmdb_lookup_features(the_database, predicate);
      if ( the_features ) {
        lmfeatureset_iterate(the_features, feature_iterator, the_conf);
        lmfeatureset_release(the_features);
      }
      lmdb_release(the_database);
    }
  } else {
    lmlog(lmlog_level_error, "No license database configured");
  }
  return rc;
}
