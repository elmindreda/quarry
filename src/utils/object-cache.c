/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004 Paul Pogonyshev.                             *
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
 * Software Foundation, Inc., 59 Temple Place - Suite 330,         *
 * Boston, MA 02111-1307, USA.                                     *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include "utils.h"

#include <assert.h>


/* Return an object corresponding to given `key'.  If such an object
 * is not present in the cache yet, it is created.
 */
void *
object_cache_create_or_reuse_object(ObjectCache *cache, const void *key)
{
  ObjectCacheEntry *stock_entry;
  ObjectCacheEntry **link;

  assert(cache);
  assert(key);

  /* Check if we have required object in stock. */
  for (stock_entry = cache->first_stock_entry;
       stock_entry; stock_entry = stock_entry->next) {
    if (cache->compare_keys(key, stock_entry->key)) {
      stock_entry->reference_counter++;
      return stock_entry->object;
    }
  }

  /* Maybe such an object is present in the dump, pending for
   * deletion?  Then we move it to stock and avoid recreating it.
   */
  link = &cache->first_dump_entry;
  while (*link) {
    ObjectCacheEntry *dump_entry = *link;

    if (cache->compare_keys(key, dump_entry->key)) {
      *link = dump_entry->next;

      dump_entry->next = cache->first_stock_entry;
      cache->first_stock_entry = dump_entry;

      dump_entry->reference_counter = 1;
      cache->current_dump_size--;

      return dump_entry->object;
    }

    link = &dump_entry->next;
  }

  stock_entry = utils_malloc(sizeof(ObjectCacheEntry));

  stock_entry->reference_counter = 1;
  stock_entry->next = cache->first_stock_entry;

  stock_entry->key = cache->duplicate_key(key);
  stock_entry->object = cache->create_object(key);

  cache->first_stock_entry = stock_entry;

  return stock_entry->object;
}


/* Unreference given object.  If the object is referenced only once,
 * then it is either moved to dump or deleted instantly (depending on
 * whether the dump exists at all).
 */
void
object_cache_unreference_object(ObjectCache *cache, void *object)
{
  ObjectCacheEntry **link;

  assert(cache);

  for (link = &cache->first_stock_entry; *link; ) {
    ObjectCacheEntry *stock_entry = *link;

    if (stock_entry->object == object) {
      if (--stock_entry->reference_counter == 0) {
	*link = stock_entry->next;

	if (cache->max_dump_size > 0) {
	  stock_entry->next = cache->first_dump_entry;
	  cache->first_dump_entry = stock_entry;

	  if (++cache->current_dump_size > cache->max_dump_size)
	    object_cache_recycle_dump(cache, 1);
	}
	else {
	  cache->delete_key(stock_entry->key);
	  cache->delete_object(stock_entry->object);
	  utils_free(stock_entry);
	}
      }

      return;
    }

    link = &stock_entry->next;
  }

  assert(0);
}


/* Recycle an object cache.  On first recycle, objects' reference
 * counters are set to -1.  On second recycle objects are deleted
 * unless they have been requested again with
 * object_cache_create_or_reuse_object().
 *
 * If `lazy_recycling' is set, this function recycles only one object
 * set.  This feature is probably not useful outside this module.
 */
void
object_cache_recycle_dump(ObjectCache *cache, int lazy_recycling)
{
  ObjectCacheEntry **link;

  assert(cache);

  for (link = &cache->first_dump_entry; *link; ) {
    ObjectCacheEntry *dump_entry = *link;

    if (dump_entry->reference_counter == 0 && !lazy_recycling)
      dump_entry->reference_counter = -1;
    else if (dump_entry->reference_counter == -1
	     || (lazy_recycling && !dump_entry->next)) {
      *link = dump_entry->next;

      cache->delete_key(dump_entry->key);
      cache->delete_object(dump_entry->object);
      utils_free(dump_entry);

      cache->current_dump_size--;

      if (lazy_recycling)
	return;

      continue;
    }

    link = &dump_entry->next;
  }
}


/* Unconditionally delete all objects stored in cache, including
 * dumped ones.  This frees all memory allocated by the cache itself
 * too.  After a call to this function the cache becomes unusable.
 */
void
object_cache_free(ObjectCache *cache)
{
  ObjectCacheEntry *entry;

  assert(cache);

  for (entry = cache->first_stock_entry; entry; ) {
    ObjectCacheEntry *next_entry = entry->next;

    cache->delete_key(entry->key);
    cache->delete_object(entry->object);
    utils_free(entry);

    entry = next_entry;
  }

  for (entry = cache->first_dump_entry; entry; ) {
    ObjectCacheEntry *next_entry = entry->next;

    cache->delete_key(entry->key);
    cache->delete_object(entry->object);
    utils_free(entry);

    entry = next_entry;
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
