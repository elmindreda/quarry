/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005, 2006 Paul Pogonyshev.           *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of the GNU General Public License as  *
 * published by the Free Software Foundation; either version 2 of  *
 * the License, or (at your option) any later version.             *
 *                                                                 *
 * This program is distributed in the hope that it will be useful, *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of  *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the   *
 * GNU General Public License for more details.                    *
 *                                                                 *
 * You should have received a copy of the GNU General Public       *
 * License along with this program; if not, write to the Free      *
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,     *
 * Boston, MA 02110-1301, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Memory pools are used for storing large numbers of small items of
 * same size (e.g. SGF nodes and properties).  They store the items in
 * chunks, each consisting of NUM_ITEMS_IN_CHUNK items.
 *
 * The advantages are:
 *
 * - way less calls to malloc() and free() (faster memory management);
 *
 * - smaller memory footprint (the smaller the items, the better);
 *
 * - fast traversing of all allocated items (in no particular order,
 *   though; the callback must not make any assumptions);
 *
 * - very fast "flushing" of pools which frees all items stored.
 *
 * The disadvantage is that all items must include a field of
 * `ItemIndex' type (unsigned char) as their first field.
 *
 *
 * Chunks in memory pools are kept in double-linked list.  Non-full
 * chunks (with at least one free item) are kept together in the
 * list's head.  At least one non-full chunk must always be present.
 *
 * Each item's first field must be of `ItemIndex' type.  This field is
 * private to memory pool and must not be used from outside.
 *
 * When an item is allocated, this field contains item's index in
 * chunk and is used in memory_pool_free() for finding chunk structure
 * in memory.  When an item is free, the field contains index of
 * chunk's next free item.
 *
 * Knowing the above, it is possible to say if an item in chunk is
 * free (provided that you have a pointer to chunk).  This fact is
 * used in item traversing.
 */


#include "utils.h"


#if ENABLE_MEMORY_POOLS


#include <stdlib.h>
#include <assert.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif


static MemoryChunk *  memory_chunk_new (int item_size);


#if ENABLE_MEMORY_PROFILING


#include <stdio.h>


int		num_pools_initialized = 0;
int		num_pools_flushed = 0;


#endif	/* ENABLE_MEMORY_PROFILING */


/* Initialize a MemoryPool structure to store items of given size. */
void
memory_pool_init (MemoryPool *pool, int item_size)
{
  MemoryChunk *chunk;

  assert (pool);
  assert (item_size > 0);

  pool->item_size = item_size;

  chunk = memory_chunk_new (item_size);
  chunk->next = NULL;
  chunk->previous = NULL;

  pool->first_chunk= chunk;
  pool->last_chunk = chunk;

#if ENABLE_MEMORY_PROFILING

  pool->number = ++num_pools_initialized;
  fprintf (stderr, ("Memory pool number %d initialized:\n"
		    "  size of item:  %6d bytes\n"
		    "  size of chunk: %6d bytes\n\n"),
	   pool->number, item_size,
	   (sizeof (MemoryChunk) - sizeof (int)
	    + NUM_ITEMS_IN_CHUNK * item_size));

  pool->num_chunks_allocated = 1;
  pool->num_chunks_freed = 0;

  pool->num_items_allocated = 0;
  pool->num_items_freed = 0;

#endif	/* ENABLE_MEMORY_PROFILING */
}


/* Allocate an item from given memory pool. */
void *
memory_pool_alloc (MemoryPool *pool)
{
  MemoryChunk *chunk = pool->first_chunk;
  int item_index;
  void *item;

  assert (pool->item_size > 0);

  item_index = chunk->first_free_item;
  item = (char *) chunk->memory + item_index * pool->item_size;

  if (--chunk->num_free_items)
    chunk->first_free_item = * (ItemIndex *) item;
  else {
    if (chunk->next && chunk->next->num_free_items > 0) {
      /* The chunk is full now, but is followed by at least one
       * non-full chunk.  We have to move this one to the tail of
       * pool's list of chunks.
       */
      chunk->next->previous = NULL;

      pool->first_chunk = chunk->next;

      chunk->next = NULL;
      chunk->previous = pool->last_chunk;

      pool->last_chunk = chunk;
    }
    else {
      /* We need at least one non-full chunk. */
      chunk = memory_chunk_new (pool->item_size);
      chunk->next = pool->first_chunk;
      chunk->previous = NULL;

      pool->first_chunk->previous = chunk;
      pool->first_chunk = chunk;

#if ENABLE_MEMORY_PROFILING
      pool->num_chunks_allocated++;
#endif
    }
  }

  * (ItemIndex *) item = item_index;

#if ENABLE_MEMORY_PROFILING
  pool->num_items_allocated++;
#endif

  return item;
}


/* Free an item which was allocated from given pool.  Note that even
 * if all items are freed, you still have to call memory_pool_flush()
 * to free all memory allocated by the pool.
 */
void
memory_pool_free (MemoryPool *pool, void *item)
{
  MemoryChunk *chunk;
  ItemIndex item_index = * (ItemIndex *) item;

  assert (pool->item_size > 0);

  chunk = (MemoryChunk *) ((char *) item - item_index * pool->item_size
			   - (sizeof (MemoryChunk) - sizeof (int)));

  if (chunk->num_free_items < NUM_ITEMS_IN_CHUNK - 1) {
    if (chunk->num_free_items == 0 && chunk->previous->num_free_items == 0) {
      /* The chunk is not full now, but is not in "non-full" head of
       * the pool's chunk list.  We have to move it to the head.
       */
      chunk->previous->next = chunk->next;

      if (chunk->next)
	chunk->next->previous = chunk->previous;
      else
	pool->last_chunk = chunk->previous;

      chunk->previous = NULL;
      chunk->next = pool->first_chunk;

      pool->first_chunk = chunk;
    }
  }
  else {
    if (chunk->previous
	|| (chunk->next && chunk->next->num_free_items > 0)) {
      /* The chunk is not the only non-full chunk in the pool.  There
       * is no reason to keep it.
       */
      if (chunk->next)
	chunk->next->previous = chunk->previous;
      else
	pool->last_chunk = chunk->previous;

      if (chunk->previous)
	chunk->previous->next = chunk->next;
      else
	pool->first_chunk = chunk->next;

      utils_free (chunk);

#if ENABLE_MEMORY_PROFILING
      pool->num_items_freed++;
      pool->num_chunks_freed++;
#endif

      /* Since the whole chunk is freed, we don't need to free the
       * item itself.  Return now.
       */
      return;
    }

    /* We cannot free the chunk because it is the only "non-full" one.
     * Just fall through and free the item, leaving the chunk around.
     */
  }

  chunk->num_free_items++;

  * (ItemIndex *) item = chunk->first_free_item;
  chunk->first_free_item = item_index;

#if ENABLE_MEMORY_PROFILING
  pool->num_items_freed++;
#endif
}


int
memory_pool_count_items (const MemoryPool *pool)
{
  int num_items = 0;
  MemoryChunk *chunk;

  assert (pool);

  if (pool->item_size > 0) {
    /* Traverse chunks in "backward" direction, because it would mean
     * traversing chunks in order of allocation in the most common
     * case.
     */
    for (chunk = pool->last_chunk; chunk; chunk = chunk->previous)
      num_items += NUM_ITEMS_IN_CHUNK - chunk->num_free_items;
  }

  return num_items;
}


void
memory_pool_traverse (const MemoryPool *pool, MemoryPoolCallback callback)
{
  MemoryChunk *chunk;
  int item_size = pool->item_size;
  char *memory;
  int k;

  assert (item_size > 0);

  /* Traverse full chunks.  We do this in "backward" direction,
   * because it would mean traversing chunks in order of allocation in
   * the most common case.
   */
  for (chunk = pool->last_chunk; chunk->num_free_items == 0;
       chunk = chunk->previous) {
    for (memory = (char *) chunk->memory, k = 0; k < NUM_ITEMS_IN_CHUNK;
	 memory += pool->item_size, k++)
      callback (memory);
  }

  /* Traverse non-full chunks.  In this case we need to test if an
   * item is allocated before invoking callback on it.
   */
  do {
    for (memory = (char *) chunk->memory, k = 0; k < NUM_ITEMS_IN_CHUNK;
	 memory += pool->item_size, k++) {
      if (* (ItemIndex *) memory == k)
	callback (memory);
    }

    chunk = chunk->previous;
  } while (chunk);
}


void
memory_pool_traverse_data (const MemoryPool *pool,
			   MemoryPoolDataCallback callback, void *data)
{
  MemoryChunk *chunk = pool->first_chunk;
  int item_size = pool->item_size;
  char *memory;
  int k;

  assert (item_size > 0);

  /* Traverse full chunks.  We do this in "backward" direction,
   * because it would mean traversing chunks in order of allocation in
   * the most common case.
   */
  for (chunk = pool->last_chunk; chunk->num_free_items == 0;
       chunk = chunk->previous) {
    for (memory = (char *) chunk->memory, k = 0; k < NUM_ITEMS_IN_CHUNK;
	 memory += pool->item_size, k++)
      callback (memory, data);
  }

  /* Traverse non-full chunks.  In this case we need to test if an
   * item is allocated before invoking callback on it.
   */
  do {
    for (memory = (char *) chunk->memory, k = 0; k < NUM_ITEMS_IN_CHUNK;
	 memory += pool->item_size, k++) {
      if (* (ItemIndex *) memory == k)
	callback (memory, data);
    }

    chunk = chunk->previous;
  } while (chunk);
}


/* Flush given memory pool.  This frees all pool's items and allocated
 * memory.  The pool becomes uninitialized and to use it again, you
 * must call memory_pool_init().
 *
 * Note that you don't have to free each item individually before
 * calling this function.
 */
void
memory_pool_flush (MemoryPool *pool)
{
  MemoryChunk *chunk;

  assert (pool->item_size > 0);

  for (chunk = pool->first_chunk; chunk;) {
    MemoryChunk *next = chunk->next;

    utils_free (chunk);
    chunk = next;
  }

  pool->item_size = 0;

  pool->first_chunk = NULL;
  pool->last_chunk = NULL;

#if ENABLE_MEMORY_PROFILING

  num_pools_flushed++;
  fprintf (stderr, ("Memory pool %d flushed; detailed information:\n"
		    "  number of chunks allocated: %10u\n"
		    "  number of chunks freed:     %10u\n"
		    "  number of items allocated:  %10u\n"
		    "  number of items freed:      %10u\n\n"),
	   pool->number,
	   pool->num_chunks_allocated, pool->num_chunks_freed,
	   pool->num_items_allocated, pool->num_items_freed);

#endif
}


/* Allocate a new MemoryChunk structure with all items being free. */
static MemoryChunk *
memory_chunk_new (int item_size)
{
  MemoryChunk *chunk = utils_malloc (sizeof (MemoryChunk) - sizeof (int)
				     + NUM_ITEMS_IN_CHUNK * item_size);
  char *memory;
  int k;

  for (memory = (char *) chunk->memory, k = 0; k < NUM_ITEMS_IN_CHUNK;
       memory += item_size, k++)
    * (ItemIndex *) memory = k + 1;

  chunk->first_free_item = 0;
  chunk->num_free_items = NUM_ITEMS_IN_CHUNK;

  return chunk;
}


#endif /* ENABLE_MEMORY_POOLS */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
