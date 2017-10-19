/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmfeature.c
 *
 * wrapper for FLEXlm license features, sets of features
 *
 */

#include "lmfeature.h"

//

const int lmfeature_no_id = -1;

const time_t lmfeature_no_expiration = 0;

//

typedef struct _lmfeature {
  unsigned int  ref_count;
  
  bool          is_modified;
  
  int           feature_id;
  char          *feature_string;
  char          *vendor;
  char          *version;
  
  time_t        expiration_date;
  
  int           issued, in_use;
} lmfeature;

//

lmfeature*
__lmfeature_alloc(
  int               feature_id,
  const char        *feature_string,
  const char        *vendor,
  const char        *version,
  time_t            expiration_date,
  int               issued,
  int               in_use
)
{
  size_t            feature_string_len = 1 + strlen(feature_string);
  size_t            vendor_len = 1 + strlen(vendor);
  size_t            version_len = 1 + strlen(version);
  lmfeature         *new_feature = NULL;
  void              *new_obj_mem = malloc(sizeof(lmfeature) + feature_string_len + vendor_len + version_len);
  
  if ( new_obj_mem ) {
    new_feature = (lmfeature*)new_obj_mem;
    
    new_obj_mem += sizeof(lmfeature);
    
    new_feature->ref_count = 1;
    new_feature->is_modified = false;
    new_feature->feature_id = feature_id;
    
    new_feature->feature_string = (char*)new_obj_mem;
    new_obj_mem += feature_string_len;
    strcpy(new_feature->feature_string, feature_string);
    
    new_feature->vendor = (char*)new_obj_mem;
    new_obj_mem += vendor_len;
    strcpy(new_feature->vendor, vendor);
    
    new_feature->version = (char*)new_obj_mem;
    new_obj_mem += version_len;
    strcpy(new_feature->version, version);
    
    new_feature->expiration_date = expiration_date;
    
    new_feature->issued = issued;
    new_feature->in_use = in_use;
    
    return new_feature;
  }
  return new_feature;
}

//

void
__lmfeature_set_expiration_date(
  lmfeature   *the_feature,
  time_t      expiration_date
)
{
  the_feature->expiration_date = expiration_date;
  the_feature->is_modified = true;
}

//

void
__lmfeature_set_issued(
  lmfeature   *the_feature,
  int         issued
)
{
  the_feature->issued = issued;
  the_feature->is_modified = true;
}

//

void
__lmfeature_set_in_use(
  lmfeature   *the_feature,
  int         in_use
)
{
  the_feature->in_use = in_use;
  the_feature->is_modified = true;
}

//
#if 0
#pragma mark -
#endif
//

lmfeature_ref
lmfeature_create_with_stats(
  int               feature_id,
  const char        *feature_string,
  const char        *vendor,
  const char        *version,
  time_t            expiration_date,
  int               issued,
  int               in_use
)
{
  lmfeature_ref     new_feature = NULL;
  
  if ( feature_string && *feature_string && vendor && *vendor && version && *version ) {
    new_feature = (lmfeature_ref)__lmfeature_alloc(feature_id, feature_string, vendor, version, expiration_date, issued, in_use);
  }
  return new_feature;
}
                
//

lmfeature_ref
lmfeature_create(
  int               feature_id,
  const char        *feature_string,
  const char        *vendor,
  const char        *version
)
{
  return lmfeature_create_with_stats(feature_id, feature_string, vendor, version, lmfeature_no_expiration, 0, 0);
}

//

lmfeature_ref
lmfeature_retain(
  lmfeature_ref   the_feature
)
{
  ((lmfeature*)the_feature)->ref_count++;
  return the_feature;
}

//

void
lmfeature_release(
  lmfeature_ref   the_feature
)
{
  if ( --((lmfeature*)the_feature)->ref_count == 0 ) {
    free((void*)the_feature);
  }
}

//

bool
lmfeature_is_modified(
  lmfeature_ref   the_feature
)
{
  return the_feature->is_modified;
}

//

int
lmfeature_get_feature_id(
  lmfeature_ref   the_feature
)
{
  return the_feature->feature_id;
}

//

const char*
lmfeature_get_feature_string(
  lmfeature_ref   the_feature
)
{
  return the_feature->feature_string;
}

//

const char*
lmfeature_get_vendor(
  lmfeature_ref   the_feature
)
{
  return the_feature->vendor;
}

//

const char*
lmfeature_get_version(
  lmfeature_ref   the_feature
)
{
  return the_feature->version;
}

//

time_t
lmfeature_get_expiration_date(
  lmfeature_ref   the_feature
)
{
  return the_feature->expiration_date;
}

void
lmfeature_set_expiration_date(
  lmfeature_ref   the_feature,
  time_t          expiration_date
)
{
  __lmfeature_set_expiration_date((lmfeature*)the_feature, expiration_date);
}

//

int
lmfeature_get_issued(
  lmfeature_ref   the_feature
)
{
  return the_feature->issued;
}

void
lmfeature_set_issued(
  lmfeature_ref   the_feature,
  int             issued
)
{
  __lmfeature_set_issued((lmfeature*)the_feature, issued);
}

void
lmfeature_add_issued(
  lmfeature_ref   the_feature,
  int             issued
)
{
  __lmfeature_set_issued((lmfeature*)the_feature, the_feature->issued + issued);
}

//

int
lmfeature_get_in_use(
  lmfeature_ref   the_feature
)
{
  return the_feature->in_use;
}

void
lmfeature_set_in_use(
  lmfeature_ref   the_feature,
  int             in_use
)
{
  __lmfeature_set_in_use((lmfeature*)the_feature, in_use);
}

void
lmfeature_add_in_use(
  lmfeature_ref   the_feature,
  int             in_use
)
{
  __lmfeature_set_in_use((lmfeature*)the_feature, the_feature->in_use + in_use);
}

//
#if 0
#pragma mark -
#endif
//

typedef struct _lmfeatureset_node {
  lmfeature_ref               feature;
  struct _lmfeatureset_node   *next;
} lmfeatureset_node;

//

lmfeatureset_node*
__lmfeatureset_node_alloc(
  lmfeature_ref       feature
)
{
  lmfeatureset_node   *new_node = malloc(sizeof(lmfeatureset_node));
  
  if ( new_node ) {
    new_node->feature = feature ? lmfeature_retain(feature) : NULL;
    new_node->next = NULL;
  }
  return new_node;
}

//

void
__lmfeatureset_node_dealloc(
  lmfeatureset_node   *node
)
{
  if ( node->feature ) lmfeature_release(node->feature);
  free((void*)node);
}

//

typedef struct _lmfeatureset {
  unsigned int        ref_count;
  lmfeatureset_node   *features;
} lmfeatureset;

//

lmfeatureset*
__lmfeatureset_alloc()
{
  lmfeatureset      *new_set = malloc(sizeof(lmfeatureset));

  if ( new_set ) {
    new_set->ref_count = 1;
    new_set->features = NULL;
  }
  return new_set;
}

//

void
__lmfeatureset_dealloc(
  lmfeatureset      *the_featureset
)
{
  lmfeatureset_node *node, *next;
  
  node = the_featureset->features;
  while ( node ) {
    next = node->next;
    __lmfeatureset_node_dealloc(node);
    node = next;
  }
}

//
#if 0
#pragma mark -
#endif
//

lmfeatureset_ref
lmfeatureset_create()
{
  return (lmfeatureset_ref)__lmfeatureset_alloc();
}

//

void
lmfeatureset_release(
  lmfeatureset_ref  the_featureset
)
{
  __lmfeatureset_dealloc((lmfeatureset*)the_featureset);
}

//

bool
lmfeatureset_add_feature(
  lmfeatureset_ref  the_featureset,
  lmfeature_ref     the_feature
)
{
  if ( ! the_feature ) return false;
  
  if ( ! the_featureset->features ) {
    // Empty list means we just add it, period:
    ((lmfeatureset*)the_featureset)->features = __lmfeatureset_node_alloc(the_feature);
  } else {
    int               feature_id = lmfeature_get_feature_id(the_feature);
    lmfeatureset_node *node, *node_prev, *new_node;
    
    // First, figure out if we already have this id, or where in the chain we want to insert it:
    node = the_featureset->features;
    node_prev = NULL;
    while ( node ) {
      // We want to insert before this node if it's id exceeds the new one:
      if ( lmfeature_get_feature_id(node->feature) > feature_id ) break;
        
      // Hmm...same feature id?  Only time that isn't bad is if the id is not set yet:
      if ( lmfeature_get_feature_id(node->feature) == feature_id ) {
        // Ah, if it's a feature with no id, we should go ahead and add it:
        if ( feature_id == lmfeature_no_id ) break;
        return false;
      }
      
      node_prev = node;
      node = node->next;
    }
    
    // Second, ensure no feature with matchine name/vendor/version are present:
    if ( lmfeatureset_get_feature_by_name(the_featureset, lmfeature_get_feature_string(the_feature), lmfeature_get_vendor(the_feature), lmfeature_get_version(the_feature)) ) return false;
    
    new_node = __lmfeatureset_node_alloc(the_feature);
    if ( ! new_node ) return false;
    
    // If node is NULL, we went through the entire list and node_prev is the node prior to the new_node
    // If node is not NULL, then node should be linked to the new_node and node_prev should link to the
    //    new_node instead of node (except if node_prev is NULL, in which case we need to replace the
    //    head of the linked list with new_node)
    if ( ! node ) {
      node_prev->next = new_node;
    } else {
      new_node->next = node;
      if ( ! node_prev ) {
        ((lmfeatureset*)the_featureset)->features = new_node;
      } else {
        node_prev->next = new_node;
      }
    }
  }
  return true;
}

//

lmfeature_ref
lmfeatureset_get_feature_by_id(
  lmfeatureset_ref  the_featureset,
  int               feature_id
)
{
  lmfeature_ref     f = NULL;
  lmfeatureset_node *node = the_featureset->features;
  
  while ( node ) {
    if ( lmfeature_get_feature_id(node->feature) == feature_id ) {
      f = node->feature;
      break;
    }
    node = node->next;
  }
  return f;
}

//

lmfeature_ref
lmfeatureset_get_feature_by_name(
  lmfeatureset_ref  the_featureset,
  const char        *feature_string,
  const char        *vendor,
  const char        *version
)
{
  lmfeature_ref     f = NULL;
  lmfeatureset_node *node = the_featureset->features;
  
  while ( node ) {
    if ( (! feature_string || ((feature_string && strcmp(feature_string, lmfeature_get_feature_string(node->feature)) == 0))) &&
         (! vendor || ((vendor && strcmp(vendor, lmfeature_get_vendor(node->feature)) == 0))) &&
         (! version || ((version && strcmp(version, lmfeature_get_version(node->feature)) == 0)))
       )
    {
      f = node->feature;
      break;
    }
    node = node->next;
  }
  return f;
}

//

void
lmfeatureset_iterate(
  lmfeatureset_ref        the_featureset,
  lmfeatureset_iterator   iterator,
  const void              *context
)
{
  lmfeatureset_node *node = the_featureset->features;
  
  while ( node ) {
    if ( ! iterator(context, node->feature) ) break;
    node = node->next;
  }
}
