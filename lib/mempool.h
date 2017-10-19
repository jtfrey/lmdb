/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * mempool.h
 *
 * Pooled memory allocator; functions like a collection of fixed-size
 * allocation stacks (buckets)
 *
 */
 
#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <stdlib.h>
#include <string.h>

/*!
  @typedef mempool_ref
  
  Type of a reference to a memory pool object.
*/
typedef const struct _mempool_t * mempool_ref;

/*!
  @function mempool_alloc
  
  Create a new (empty) memory pool.
*/
mempool_ref mempool_alloc(void);

/*!
  @function mempool_dealloc
  
  Dispose of the given memory pool.  All allocated heap space
  used by the pool is also free'd, so anything referencing allocations
  made using the pool becomes invalid.
*/
void mempool_dealloc(mempool_ref pool);

/*!
  @function mempool_debug_summary
  
  Writes a summary of the memory pool (and all its allocation buckets)
  to stderr.
*/
void mempool_debug_summary(mempool_ref pool);

/*!
  @function mempool_alloc_bytes
  
  Attempt to allocate the given number of bytes from the pool.  If an
  existing allocation bucket has enough room, the allocation will come
  from it.  Otherwise, a new bucket will be created (sized according to
  the MAX(MEMPOOL_BUCKET_MIN_CAPACITY, bytes)) and used for the
  allocation.
*/
void* mempool_alloc_bytes(mempool_ref pool, size_t bytes);

/*!
  @function mempool_alloc_bytes_clear
  
  Calls mempool_alloc_bytes() and if a memory region is returned it is
  initialized to all zero bytes before being returned.
*/
void* mempool_alloc_bytes_clear(mempool_ref pool, size_t bytes);

/*!
  @function mempool_strdup
  
  Uses mempool_alloc_bytes() to make a copy of the given C string.
*/
const char* mempool_strdup(mempool_ref pool, const char *s);

/*!
  @function mempool_reset
  
  Zero the allocation count on all allocation buckets in the pool.
*/
void mempool_reset(mempool_ref pool);

#endif /* __MEMPOOL_H__ */
