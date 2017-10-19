/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmfeature.h
 *
 * wrapper for FLEXlm license features, sets of features
 *
 */

#ifndef __LMFEATURE_H__
#define __LMFEATURE_H__

#include "config.h"

/*!
  @constant lmfeature_no_id
  Feature id that correponds to a feature that has not been added
  to the database.
*/
extern const int lmfeature_no_id;

/*!
  @constant lmfeature_no_expiration
  Unix timestamp associated with features that are permanent (have
  no expiration date and time).
*/
extern const time_t lmfeature_no_expiration;

/*!
  @typedef lmfeature_ref
  Type of an opaque reference to an lmfeature object.
*/
typedef const struct _lmfeature * lmfeature_ref;

/*!
  @function lmfeature_create_with_stats
  Allocate and initialize an lmfeature object with the given 
  idenfication and status values.
*/
lmfeature_ref lmfeature_create_with_stats(
                  int               feature_id,
                  const char        *feature_string,
                  const char        *vendor,
                  const char        *version,
                  time_t            expiration_date,
                  int               issued,
                  int               in_use
                );

/*!
  @function lmfeature_create
  Allocate and initialize an lmfeature object with the given 
  identification values.
*/
lmfeature_ref lmfeature_create(
                  int               feature_id,
                  const char        *feature_string,
                  const char        *vendor,
                  const char        *version
                );

/*!
  @function lmfeature_retain
  Increment the reference count of the_feature.
*/
lmfeature_ref lmfeature_retain(lmfeature_ref the_feature);

/*!
  @function lmfeature_release
  Decrement the reference count of the_feature.  When the
  reference count reaches zero the_feature is deallocated.
*/
void lmfeature_release(lmfeature_ref the_feature);

/*!
  @function lmfeature_is_modified
  Returns true if the_feature has had an status values
  altered.
*/
bool lmfeature_is_modified(lmfeature_ref the_feature);

/*!
  @function lmfeature_get_feature_id
  Returns the feature id (internal database identifier)
  of the_feature.
*/
int lmfeature_get_feature_id(lmfeature_ref the_feature);

/*!
  @function lmfeature_get_feature_string
  Returns the feature identifier string of the_feature.
*/
const char* lmfeature_get_feature_string(lmfeature_ref the_feature);

/*!
  @function lmfeature_get_vendor
  Returns the license vendor string of the_feature.
*/
const char* lmfeature_get_vendor(lmfeature_ref the_feature);

/*!
  @function lmfeature_get_version
  Returns the license version string of the_feature.
*/
const char* lmfeature_get_version(lmfeature_ref the_feature);

/*!
  @function lmfeature_get_expiration_date
  Returns the expiration (as a Unix timestamp) of the_feature.
*/
time_t lmfeature_get_expiration_date(lmfeature_ref the_feature);

/*!
  @function lmfeature_set_expiration_date
  Set the expiration (as a Unix timestamp) of the_feature to
  expiration_date.  If expiration_date is lmfeature_no_expiration
  the feature is permanent.
*/
void lmfeature_set_expiration_date(lmfeature_ref the_feature, time_t expiration_date);

/*!
  @function lmfeature_get_issued
  Returns the number of license seats issued for the_feature.
*/
int lmfeature_get_issued(lmfeature_ref the_feature);

/*!
  @function lmfeature_set_issued
  Set the number of license seats issued for the_feature to issued.
*/
void lmfeature_set_issued(lmfeature_ref the_feature, int issued);

/*!
  @function lmfeature_add_issued
  Set the number of license seats issued for the_feature to the
  current value plus issued.
*/
void lmfeature_add_issued(lmfeature_ref the_feature, int issued);

/*!
  @function lmfeature_get_in_use
  Returns the number of license seats in use for the_feature.
*/
int lmfeature_get_in_use(lmfeature_ref the_feature);

/*!
  @function lmfeature_set_in_use
  Set the number of license seats in_use for the_feature to in_use.
*/
void lmfeature_set_in_use(lmfeature_ref the_feature, int in_use);

/*!
  @function lmfeature_add_in_use
  Set the number of license seats in use for the_feature to the
  current value plus in_use.
*/
void lmfeature_add_in_use(lmfeature_ref the_feature, int in_use);

/*!
  @typedef lmfeatureset_ref
  Type of an opaque reference to a set of zero of more lmfeature
  objects.
*/
typedef const struct _lmfeatureset * lmfeatureset_ref;

/*!
  @function lmfeatureset_create
  Allocate an initialize an empty feature set.
*/
lmfeatureset_ref lmfeatureset_create(void);

/*!
  @function lmfeatureset_retain
  Increment the reference count of the_featureset.
*/
lmfeatureset_ref lmfeatureset_retain(lmfeatureset_ref the_featureset);

/*!
  @function lmfeatureset_release
  Decrement the reference count of the_featureset.  When the
  reference count reaches zero the_featureset is deallocated.
*/
void lmfeatureset_release(lmfeatureset_ref the_featureset);

/*!
  @function lmfeatureset_add_feature
  If the_featureset does not possess a feature with the same feature id or
  tuple (feature string,vendor,version), retain the_feature and add it
  to the_featureset.
  
  Returns true if the_feature was successfully added.
*/
bool lmfeatureset_add_feature(lmfeatureset_ref the_featureset, lmfeature_ref the_feature);

/*!
  @function lmfeatureset_get_feature_by_id
  If a feature with the given feature id (internal database idenfifier)
  is present in the_featureset, its reference is returned.  Otherwise,
  NULL is returned.
*/
lmfeature_ref lmfeatureset_get_feature_by_id(lmfeatureset_ref the_featureset, int feature_id);

/*!
  @function lmfeatureset_get_feature_by_name
  If a feature with the given tuple (feature string,vendor,version) is
  present in the_featureset, its reference is returned.  Otherwise,
  NULL is returned.
  
  Providing NULL for any of the identification components (feature_string,
  vendor, version) implies a wildcard.  So:
  
      f = lmfeatureset_get_feature_by_name(the_set,
              "MATLAB", "MLM", NULL
            );
  
  finds the first instance of "MATLAB" from vendor "MLM" with any version.
*/
lmfeature_ref lmfeatureset_get_feature_by_name(lmfeatureset_ref the_featureset, const char *feature_string, const char *vendor, const char *version);

/*!
  @typedef lmfeatureset_iterator
  Type of a pointer to a function that is called when iterating over the
  features present in a feature set.  The context is a application-specific
  pointer used to provide data to the lmfeatureset_iterator function.
*/
typedef bool (*lmfeatureset_iterator)(const void *context, lmfeature_ref a_feature);

/*!
  @function lmfeatureset_iterate
  Call the iterator function on each lmfeature object present in the_featureset.
  The context is an application-specific pointer passed to the iterator
  function.
*/
void lmfeatureset_iterate(lmfeatureset_ref the_featureset, lmfeatureset_iterator iterator, const void *context);

#endif /* __LMFEATURE_H__ */
