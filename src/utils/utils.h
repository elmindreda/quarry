/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003 Paul Pogonyshev.                             *
 * Copyright (C) 2004 Paul Pogonyshev and Martin Holters.          *
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


#ifndef QUARRY_UTILS_H
#define QUARRY_UTILS_H


#include "quarry.h"

#include <iconv.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


/* Set to zero to disable memory pools. */
#define ENABLE_MEMORY_POOLS	1

/* Set to 1 to get lots of information about memory allocation. */
#define ENABLE_MEMORY_PROFILING	0


/* FIXME: proper `#ifdef's to make this work under Windows. */
#define DIRECTORY_SEPARATOR	'/'


/* Just a useful structure to have around. */
typedef struct _QuarryColor	QuarryColor;

struct _QuarryColor {
  unsigned char	  red;
  unsigned char	  green;
  unsigned char	  blue;
};


#define QUARRY_COLORS_ARE_EQUAL(first_color, second_color)	\
  ((first_color).red == (second_color).red			\
   && (first_color).green == (second_color).green		\
   && (first_color).blue == (second_color).blue)



/* `utils.c' global functions. */

void *		utils_malloc(size_t size);
void *		utils_malloc0(size_t size);
void *		utils_realloc(void *pointer, size_t size);


#if ENABLE_MEMORY_PROFILING

void		utils_free(void *pointer);

void		utils_print_memory_profiling_info(void);

#else  /* not ENABLE_MEMORY_PROFILING */

#define utils_free		free

#endif	/* not ENABLE_MEMORY_PROFILING */


char *		utils_duplicate_string(const char *string);
void *		utils_duplicate_buffer(const void *buffer, int length);
char *		utils_duplicate_as_string(const char *buffer, int length);

char *		utils_cat_string(char *string, const char *to_cat);
char *		utils_cat_strings(char *string, ...);

char *		utils_cat_as_string(char *string, const char *buffer,
				    int length);
char *		utils_cat_as_strings(char *string, ...);


void		utils_remember_program_name(const char *argv0);
void		utils_free_program_name_strings(void);


char *		utils_printf(const char *format_string, ...);
char *		utils_vprintf(const char *format_string, va_list arguments);

char *		utils_special_printf(const char *format_string, ...);
char *		utils_special_vprintf(const char *format_string,
				      va_list arguments);


char *		utils_fgets(FILE *file, int *length);


int		utils_compare_ints(const void *first_int,
				   const void *second_int);


const char *	utils_format_double(double value);

int             utils_parse_double(const char *float_string, 
                                   double *result);
int		utils_parse_time(const char *time_string);


extern char    *full_program_name;
extern char    *short_program_name;
extern char    *program_directory;



/* `memory-pool.c' declarations and global functions. */

#if ENABLE_MEMORY_POOLS


#define NUM_ITEMS_IN_CHUNK	128

/* NOTE: this field is private to memory pool, it should never be
 *	 accessed from other code, especially, it must _never_ be
 *	 written to.
 */
#define MEMORY_POOL_ITEM_INDEX	ItemIndex	item_index


typedef char			ItemIndex;

typedef struct _MemoryChunk	MemoryChunk;
typedef struct _MemoryPool	MemoryPool;

struct _MemoryChunk {
  MemoryChunk	 *next;
  MemoryChunk	 *previous;

  ItemIndex	  first_free_item;
  unsigned char	  num_free_items;

  /* We use `int' here to force proper memory alignment. */
  int		  memory[1];
};

struct _MemoryPool {
  int		 item_size;

  MemoryChunk	 *first_chunk;
  MemoryChunk	 *last_chunk;

#if ENABLE_MEMORY_PROFILING

  int		  number;

  unsigned int	  num_chunks_allocated;
  unsigned int	  num_chunks_freed;

  unsigned int	  num_items_allocated;
  unsigned int	  num_items_freed;

#endif
};


typedef void (* MemoryPoolCallback) (void *item);
typedef void (* MemoryPoolDataCallback) (void *item, void *data);


void		memory_pool_init(MemoryPool *pool, int item_size);

void *		memory_pool_alloc(MemoryPool *pool);
void		memory_pool_free(MemoryPool *pool, void *item);

int		memory_pool_count_items(const MemoryPool *pool);

void		memory_pool_traverse(const MemoryPool *pool,
				     MemoryPoolCallback callback);
void		memory_pool_traverse_data(const MemoryPool *pool,
					  MemoryPoolDataCallback callback,
					  void *data);

void		memory_pool_flush(MemoryPool *pool);


#if ENABLE_MEMORY_PROFILING

extern int	num_pools_initialized;
extern int	num_pools_flushed;

#endif


#else /* not ENABLE_MEMORY_POOLS */


#define MEMORY_POOL_ITEM_INDEX


typedef struct _MemoryPool	MemoryPool;

struct _MemoryPool {
  int		 item_size;
};


#define memory_pool_init(pool, _item_size)	\
  ((pool)->item_size = (_item_size))

#define memory_pool_alloc(pool)			\
  utils_malloc((pool)->item_size)

#define memory_pool_free(pool, item)		\
  (UNUSED(pool), utils_free(item))

/* Functions memory_pool_count_items(), memory_pool_traverse(),
 * memory_pool_traverse_data() and memory_pool_flush() cannot be
 * emulated.  They must not be used if ENABLE_MEMORY_POOLS is zero.
 */


#endif /* not ENABLE_MEMORY_POOLS */



/* `string-list.c' declarations and global functions. */

/* Note that string lists and notches (items) are passed to functions
 * as `void *'.  Creating useful derived string lists seems impossible
 * otherwise.  Either it involves lots of ugly typecasts which remove
 * all advantages of type checking, or forces to reimplement all list
 * functionality with new types, thus making "derivation" senseless.
 *
 * The only thing to remember when using string lists is to _always_
 * double check what you actually pass as function parameters.
 */

typedef void (* StringListItemDispose) (void *abstract_item);

typedef struct _StringListItem	StringListItem;
typedef struct _StringList	StringList;

struct _StringListItem {
  StringListItem	 *next;
  char			 *text;
};

struct _StringList {
  StringListItem	 *first;
  StringListItem	 *last;

  int			  item_size;
  StringListItemDispose	  item_dispose;
};


#define string_list_new()						\
  ((StringList *)							\
   string_list_new_derived(sizeof(StringListItem), NULL))

#define string_list_init(list)						\
  string_list_init_derived((list), sizeof(StringListItem), NULL)

#define STATIC_STRING_LIST						\
  STATIC_STRING_LIST_DERIVED(StringListItem, NULL)

#define STATIC_STRING_LIST_DERIVED(ItemType, item_dispose)		\
  { NULL, NULL,								\
    sizeof(ItemType), (StringListItemDispose) (item_dispose) }


void *		  string_list_new_derived(int item_size,
					  StringListItemDispose item_dispose);
void		  string_list_init_derived(void *abstract_list,
					   int item_size,
					   StringListItemDispose item_dispose);
void		  string_list_delete(void *abstract_list);
void		  string_list_empty(void *abstract_list);

#define string_list_is_empty(abstract_list)				\
  ((abstract_list)->first == NULL)

#define string_list_is_single_string(abstract_list)			\
  ((abstract_list)->first != NULL					\
   && (abstract_list)->first == (abstract_list)->last)

int		  string_list_count_items(void *abstract_list);

void		  string_list_fill_from_string(void *abstract_list,
					       const char *super_string);

/* Note that this function only operates on non-derived StringLists.
 * I don't want to introduce copy constructor for list items unless
 * really needed.
 */
void		  string_list_duplicate_items
		    (StringList *list, const StringList *duplicate_from_list);

void		  string_list_steal_items(void *abstract_list,
					  void *steal_from);


void		  string_list_add(void *abstract_list, const char *string);
void		  string_list_add_from_buffer(void *abstract_list,
					      const char *buffer, int length);
void		  string_list_add_ready(void *abstract_list,
					char *allocated_string);
void		  string_list_add_ready_item(void *abstract_list,
					     void *abstract_item);

void		  string_list_prepend(void *abstract_list, const char *string);
void		  string_list_prepend_from_buffer(void *abstract_list,
						  const char *buffer,
						  int length);
void		  string_list_prepend_ready(void *abstract_list,
					    char *allocated_string);
void		  string_list_prepend_ready_item(void *abstract_list,
						 void *abstract_item);

void		  string_list_insert(void *abstract_list, void *abstract_notch,
				     const char *string);
void		  string_list_insert_from_buffer(void *abstract_list,
						 void *abstract_notch,
						 const char *buffer,
						 int length);
void		  string_list_insert_ready(void *abstract_list,
					   void *abstract_notch,
					   char *allocated_string);
void		  string_list_insert_ready_item(void *abstract_list,
						void *abstract_notch,
						void *abstract_item);

void		  string_list_delete_item(void *abstract_list,
					  void *abstract_item);
void		  string_list_delete_first_item(void *abstract_list);

int		  string_list_get_item_index(void *abstract_list,
					     void *abstract_item);
StringListItem *  string_list_get_item(void *abstract_list, int item_index);

StringListItem *  string_list_find(const void *abstract_list,
				   const char *text);
StringListItem *  string_list_find_after_notch(const void *abstract_list,
					       const char *text,
					       const void *abstract_notch);

void		  string_list_swap_with_next(void *abstract_list,
					     void *abstract_item);
void		  string_list_swap_with_previous(void *abstract_list,
						 void *abstract_item);
void		  string_list_move(void *abstract_list, void *abstract_item,
				   void *abstract_notch);

char *		  string_list_implode(const void *abstract_list,
				      const char *separator);


/* A type derived from string list. */
typedef struct _AssociationListItem	AssociationListItem;
typedef struct _AssociationList		AssociationList;

struct _AssociationListItem {
  AssociationListItem	 *next;
  char			 *key;

  char			 *association;
};

struct _AssociationList {
  AssociationListItem	 *first;
  AssociationListItem	 *last;

  int			  item_size;
  StringListItemDispose	  item_dispose;
};


#define association_list_new()						\
  ((AssociationList *)							\
   string_list_new_derived(sizeof(AssociationListItem),			\
			   ((StringListItemDispose)			\
			    association_list_item_dispose)))

#define association_list_init(list)					\
  string_list_init_derived((list), sizeof(AssociationListItem),		\
			   ((StringListItemDispose)			\
			    association_list_item_dispose))

#define STATIC_ASSOCIATION_LIST						\
  STATIC_STRING_LIST_DERIVED(AssociationListItem,			\
			     association_list_item_dispose)

void		association_list_item_dispose(AssociationListItem *item);


#define association_list_get_item(list, item_index)			\
  ((AssociationListItem *) string_list_get_item((list), (item_index)))

#define association_list_find(list, key)				\
  ((AssociationListItem *) string_list_find((list), (key)))

#define association_list_find_after_notch(list, key, notch)		\
  ((AssociationListItem *) string_list_find_after_notch((list), (key),	\
							(notch)))


inline char *	association_list_find_association(AssociationList *list,
						  const char *key);



/* `string-buffer.c' declarations and global functions. */

typedef struct _StringBuffer	StringBuffer;

struct _StringBuffer {
  char	       *string;
  int		length;

  int		current_size;
  int		size_increment;
};


StringBuffer *	string_buffer_new(int initial_size, int size_increment);
void		string_buffer_init(StringBuffer *string_buffer,
				   int initial_size, int size_increment);

void		string_buffer_delete(StringBuffer *string_buffer);
void		string_buffer_dispose(StringBuffer *string_buffer);

void		string_buffer_empty(StringBuffer *string_buffer);


#define string_buffer_add_character(string_buffer, character)	\
  string_buffer_add_characters((string_buffer), (character), 1)

void		string_buffer_add_characters(StringBuffer *string_buffer,
					     char character,
					     int num_characters);

void		string_buffer_cat_string(StringBuffer *string_buffer,
					 const char *string);
void		string_buffer_cat_strings(StringBuffer *string_buffer, ...);

void		string_buffer_cat_as_string(StringBuffer *string_buffer,
					    const char *buffer, int length);
void		string_buffer_cat_as_strings(StringBuffer *string_buffer, ...);

void		string_buffer_printf(StringBuffer *string_buffer,
				     const char *format_string, ...);
void		string_buffer_vprintf(StringBuffer *string_buffer,
				      const char *format_string,
				      va_list arguments);



/* `buffered-writer.c' declarations and global functions. */

typedef struct _BufferedWriter	BufferedWriter;

struct _BufferedWriter {
  FILE	       *file;

  char	       *buffer;
  char	       *buffer_pointer;
  char	       *buffer_end;

  iconv_t	iconv_handle;

  int		column;

  int		successful;
};


int		buffered_writer_init(BufferedWriter *writer,
				     const char *filename, int buffer_size);
int		buffered_writer_dispose(BufferedWriter *writer);

#define buffered_writer_set_iconv_handle(writer, handle)	\
  ((writer)->iconv_handle = (handle))


void		buffered_writer_add_character(BufferedWriter *writer,
					      char character);
void		buffered_writer_add_newline(BufferedWriter *writer);

void		buffered_writer_cat_string(BufferedWriter *writer,
					   const char *string);
void		buffered_writer_cat_strings(BufferedWriter *writer, ...);

void		buffered_writer_cat_as_string(BufferedWriter *writer,
					      const char *buffer, int length);
void		buffered_writer_cat_as_strings(BufferedWriter *writer, ...);

void		buffered_writer_printf(BufferedWriter *writer,
				       const char *format_string, ...);
void		buffered_writer_vprintf(BufferedWriter *writer,
					const char *format_string,
					va_list arguments);



/* `object-cache.c' declarations and global functions. */

typedef int (* ObjectCacheCompareKeys) (const void *first_key,
					const void *second_key);

typedef void * (* ObjectCacheCreate) (const void *key);
typedef void (* ObjectCacheDelete) (void *object);

typedef struct _ObjectCacheEntry	ObjectCacheEntry;
typedef struct _ObjectCache		ObjectCache;

struct _ObjectCacheEntry {
  int			   reference_counter;
  ObjectCacheEntry	  *next;

  void			  *key;
  void			  *object;
};

struct _ObjectCache {
  ObjectCacheEntry	  *first_stock_entry;

  ObjectCacheEntry	  *first_dump_entry;
  int			   current_dump_size;
  int			   max_dump_size;

  ObjectCacheCompareKeys   compare_keys;

  ObjectCacheCreate	   duplicate_key;
  ObjectCacheCreate	   create_object;

  ObjectCacheDelete	   delete_key;
  ObjectCacheDelete	   delete_object;
};


void *		object_cache_create_or_reuse_object(ObjectCache *cache,
						    const void *key);
void		object_cache_unreference_object(ObjectCache *cache,
						void *object);

void		object_cache_recycle_dump(ObjectCache *cache,
					  int lazy_recycling);

void		object_cache_free(ObjectCache *cache);


#endif /* QUARRY_UTILS_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
