/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * mempool.c
 *
 * Pooled memory allocator.
 *
 */
 
#include "mempool.h"

#include <stdio.h>
#include <errno.h>

//

typedef struct _mempool_bucket {
  struct _mempool_bucket      *next;
  size_t                      used, capacity;
  void                        *base, *current;
} mempool_bucket_t;

//

#ifndef MEMPOOL_BUCKET_MIN_CAPACITY
#define MEMPOOL_BUCKET_MIN_CAPACITY   (4096 - sizeof(mempool_bucket_t))
#endif

//

mempool_bucket_t*
mempool_bucket_alloc(
  size_t      capacity
)
{
  mempool_bucket_t  *new_bucket;
  
  if ( capacity < MEMPOOL_BUCKET_MIN_CAPACITY ) capacity = MEMPOOL_BUCKET_MIN_CAPACITY;
  
  new_bucket = malloc(sizeof(mempool_bucket_t) + capacity);
  if ( new_bucket ) {
    new_bucket->next          = NULL;
    new_bucket->used          = 0;
    new_bucket->capacity      = capacity;
    new_bucket->base          = (void*)new_bucket + sizeof(mempool_bucket_t);
    new_bucket->current       = new_bucket->base;
  }
  return new_bucket;
}

//

void
mempool_bucket_dealloc(
  mempool_bucket_t    *bucket
)
{
  free((void*)bucket);
}

//

void
mempool_bucket_debug_summary(
  mempool_bucket_t    *bucket
)
{
  fprintf(stderr, "mempool_bucket@%p (%lu/%lu; base: %p)\n", bucket, bucket->used, bucket->capacity, bucket->base);
}

//

void*
mempool_bucket_alloc_bytes(
  mempool_bucket_t    *bucket,
  size_t              bytes
)
{
  void                *out = NULL;
  
  while ( bucket ) {
    if ( bucket->used + bytes <= bucket->capacity ) {
      bucket->used += bytes;
      out = bucket->current;
      bucket->current += bytes;
      break;
    }
    bucket = bucket->next;
  }
  return out;
}

//

void
mempool_bucket_reset(
  mempool_bucket_t    *bucket
)
{
  while ( bucket ) {
    bucket->used = 0;
    bucket->current = bucket->base;
    bucket = bucket->next;
  }
}

//
#if 0
#pragma mark -
#endif
//

typedef struct _mempool_t {
  mempool_bucket_t      *buckets;
} mempool_t;

//

mempool_ref
mempool_alloc(void)
{
  mempool_t     *new_pool = malloc(sizeof(mempool_t));
  
  if ( new_pool ) {
    new_pool->buckets   = NULL;
  }
  return (mempool_ref)new_pool;
}

//

void
mempool_dealloc(
  mempool_ref   pool
)
{
  mempool_bucket_t    *b = pool->buckets;
  
  while ( b ) {
    mempool_bucket_t  *next = b->next;
    
    mempool_bucket_dealloc(b);
    b = next;
  }
  free((void*)pool);
}

//

void
mempool_debug_summary(
  mempool_ref   pool
)
{
  mempool_bucket_t    *b = pool->buckets;
  
  fprintf(stderr, "mempool@%p {\n", pool);
  while ( b ) {
    fprintf(stderr, "  ");
    mempool_bucket_debug_summary(b);
    b = b->next;
  }
  fprintf(stderr, "}\n");
}

//

void*
mempool_alloc_bytes(
  mempool_ref   pool,
  size_t        bytes
)
{
  void        *p = NULL;
  
  if ( pool->buckets ) p = mempool_bucket_alloc_bytes(pool->buckets, bytes);
  
  if ( ! p ) {
    // Create a new bucket:
    mempool_bucket_t    *new_bucket = mempool_bucket_alloc(bytes);
    
    if ( new_bucket ) {
      new_bucket->next = pool->buckets;
      ((mempool_t*)pool)->buckets = new_bucket;
      p = mempool_bucket_alloc_bytes(new_bucket, bytes);
    }
  }
  return p;
}

//

void*
mempool_alloc_bytes_clear(
  mempool_ref   pool,
  size_t        bytes
)
{
  void          *p = mempool_alloc_bytes(pool, bytes);
  
  if ( p ) memset(p, 0, bytes);
  return p;
}

//

const char*
mempool_strdup(
  mempool_ref   pool,
  const char  	*s
)
{
  if ( s ) {
    size_t      slen = 1 + strlen(s);
    char        *s_copy = (char*)mempool_alloc_bytes(pool, slen);
    
    if ( s_copy ) {
      strncpy(s_copy, s, slen);
      return (const char*)s_copy;
    }
  }
  return NULL;
}

//

void
mempool_reset(
  mempool_ref   pool
)
{
  mempool_bucket_t    *b = pool->buckets;
  
  while ( b ) {
    mempool_bucket_reset(b);
    b = b->next;
  }
}
