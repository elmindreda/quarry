/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004 Paul Pogonyshev.                       *
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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>


#define ITEM_DISPOSE(list, item)					\
  do {									\
    if ((list)->item_dispose)						\
      (list)->item_dispose(item);					\
    utils_free((item)->text);						\
    utils_free(item);							\
  } while (0)


#define MAKE_NEW_ITEM_FROM_STRING					\
  StringList *list = (StringList *) abstract_list;			\
  StringListItem *item;							\
  assert(list);								\
  item = utils_malloc(list->item_size);					\
  item->text = (string ? utils_duplicate_string(string) : NULL)

#define MAKE_NEW_ITEM_FROM_BUFFER					\
  StringList *list = (StringList *) abstract_list;			\
  StringListItem *item;							\
  assert(list);								\
  assert(buffer);							\
  item = utils_malloc(list->item_size);					\
  item->text = utils_duplicate_as_string(buffer, length)

#define MAKE_NEW_ITEM_FROM_READY_STRING					\
  StringList *list = (StringList *) abstract_list;			\
  StringListItem *item;							\
  assert(list);								\
  item = utils_malloc(list->item_size);					\
  item->text = allocated_string

#define CONVERT_READY_ITEM						\
  StringList *list = (StringList *) abstract_list;			\
  StringListItem *item = (StringListItem *) abstract_item;		\
  assert(list);								\
  assert(item)


#define ADD_ITEM							\
  item->next = NULL;							\
  if (list->last)							\
    list->last->next = item;						\
  else									\
    list->first = item;							\
  list->last = item

#define PREPEND_ITEM							\
  item->next = list->first;						\
  list->first = item;							\
  if (!list->last)							\
    list->last = item

#define INSERT_ITEM							\
  do {									\
    StringListItem *notch = (StringListItem *) abstract_notch;		\
    if (notch) {							\
      item->next = notch->next;						\
      notch->next = item;						\
    }									\
    else {								\
      item->next = list->first;						\
      list->first = item;						\
    }									\
    if (!item->next)							\
      list->last = item;						\
  } while (0)


void *
string_list_new_derived(int item_size, StringListItemDispose item_dispose)
{
  void *list = utils_malloc(sizeof(StringList));

  string_list_init_derived(list, item_size, item_dispose);
  return list;
}


void
string_list_delete(void *abstract_list)
{
  string_list_empty(abstract_list);
  utils_free(abstract_list);
}


void
string_list_init_derived(void *abstract_list,
			 int item_size, StringListItemDispose item_dispose)
{
  StringList *list = (StringList *) abstract_list;

  list->first	     = NULL;
  list->last	     = NULL;
  list->item_size    = item_size;
  list->item_dispose = item_dispose;
}


void
string_list_empty(void *abstract_list)
{
  StringList *list = (StringList *) abstract_list;
  StringListItem *this_item;

  assert(list);

  for (this_item = list->first; this_item;) {
    StringListItem *next_item = this_item->next;

    ITEM_DISPOSE(list, this_item);
    this_item = next_item;
  }

  list->first = NULL;
  list->last = NULL;
}


void
string_list_fill_from_string(void *abstract_list, const char *super_string)
{
  const char *buffer;

  assert(super_string);

  for (buffer = super_string; *buffer;) {
    int length = strlen(buffer);

    string_list_add_from_buffer(abstract_list, buffer, length);
    buffer += length + 1;
  }
}


void
string_list_duplicate_items(StringList *list, StringList *duplicate_from_list)
{
  StringListItem *item;
  StringListItem *last_duplicated_item = NULL;
  StringListItem **link = &list->first;

  assert(list);
  assert(list->first == NULL);
  assert(duplicate_from_list);

  for (item = duplicate_from_list->first; item; item = item->next) {
    last_duplicated_item = utils_malloc(sizeof(StringListItem));
    last_duplicated_item->text = utils_duplicate_string(item->text);

    *link = last_duplicated_item;
    link = &last_duplicated_item->next;
  }

  list->last = last_duplicated_item;
}


void
string_list_steal_items(void *abstract_list, void *steal_from)
{
  StringList *list = (StringList *) abstract_list;
  StringList *steal_from_list = (StringList *) steal_from;

  assert(list);
  assert(list->first == NULL);
  assert(steal_from_list);
  assert(steal_from_list->item_size == list->item_size);
  assert(steal_from_list->item_dispose == list->item_dispose);

  list->first = steal_from_list->first;
  list->last = steal_from_list->last;

  steal_from_list->first = NULL;
  steal_from_list->last = NULL;
}


void
string_list_add(void *abstract_list, const char *string)
{
  MAKE_NEW_ITEM_FROM_STRING;
  ADD_ITEM;
}


void
string_list_add_from_buffer(void *abstract_list,
			    const char *buffer, int length)
{
  MAKE_NEW_ITEM_FROM_BUFFER;
  ADD_ITEM;
}


void
string_list_add_ready(void *abstract_list, char *allocated_string)
{
  MAKE_NEW_ITEM_FROM_READY_STRING;
  ADD_ITEM;
}


void
string_list_add_ready_item(void *abstract_list, void *abstract_item)
{
  CONVERT_READY_ITEM;
  ADD_ITEM;
}


void
string_list_prepend(void *abstract_list, const char *string)
{
  MAKE_NEW_ITEM_FROM_STRING;
  PREPEND_ITEM;
}


void
string_list_prepend_from_buffer(void *abstract_list,
				const char *buffer, int length)
{
  MAKE_NEW_ITEM_FROM_BUFFER;
  PREPEND_ITEM;
}


void
string_list_prepend_ready(void *abstract_list, char *allocated_string)
{
  MAKE_NEW_ITEM_FROM_READY_STRING;
  PREPEND_ITEM;
}


void
string_list_prepend_ready_item(void *abstract_list, void *abstract_item)
{
  CONVERT_READY_ITEM;
  PREPEND_ITEM;
}


void
string_list_insert(void *abstract_list, void *abstract_notch,
		   const char *string)
{
  MAKE_NEW_ITEM_FROM_STRING;
  INSERT_ITEM;
}


void
string_list_insert_from_buffer(void *abstract_list, void *abstract_notch,
			       const char *buffer, int length)
{
  MAKE_NEW_ITEM_FROM_BUFFER;
  INSERT_ITEM;
}


void
string_list_insert_ready(void *abstract_list, void *abstract_notch,
			 char *allocated_string)
{
  MAKE_NEW_ITEM_FROM_READY_STRING;
  INSERT_ITEM;
}


void
string_list_insert_ready_item(void *abstract_list, void *abstract_notch,
			      void *abstract_item)
{
  CONVERT_READY_ITEM;
  INSERT_ITEM;
}


void
string_list_delete_item(void *abstract_list, void *abstract_item)
{
  StringList *list = (StringList *) abstract_list;
  StringListItem *item = (StringListItem *) abstract_item;
  StringListItem *previous_item;
  StringListItem **link = &list->first;

  assert(list);
  assert(item);

  for (link = &list->first, previous_item = NULL;
       *link != item; link = &previous_item->next) {
    previous_item = *link;
    assert(previous_item);
  }

  *link = item->next;
  if (!item->next)
    list->last = previous_item;

  ITEM_DISPOSE(list, item);
}


void
string_list_delete_first_item(void *abstract_list)
{
  StringList *list = (StringList *) abstract_list;
  StringListItem *first;

  assert(list);
  assert(list->first);

  first = list->first;
  list->first = first->next;
  if (list->first == NULL)
    list->last = NULL;

  ITEM_DISPOSE(list, first);
}


int
string_list_get_item_index(void *abstract_list, void *abstract_item)
{
  StringList *list = (StringList *) abstract_list;
  StringListItem *item = (StringListItem *) abstract_item;
  StringListItem *this_item;
  int item_index;

  assert(list);
  assert(item);

  for (item_index = 0, this_item = list->first; this_item;
       this_item = this_item->next, item_index++) {
    if (this_item == item)
      return item_index;
  }

  return -1;
}


StringListItem *
string_list_get_item(void *abstract_list, int item_index)
{
  StringList *list = (StringList *) abstract_list;

  assert(list);

  if (item_index >= 0) {
    StringListItem *item;

    for (item = list->first; item; item = item->next) {
      if (!item_index--)
	return item;
    }
  }

  return NULL;
}


StringListItem *
string_list_find(const void *abstract_list, const char *text)
{
  const StringList *list = (const StringList *) abstract_list;
  StringListItem *item;

  assert(list);
  assert(text);

  for (item = list->first; item; item = item->next) {
    if (strcmp(item->text, text) == 0)
      break;
  }

  return item;
}


StringListItem *
string_list_find_after_notch(const void *abstract_list, const char *text,
			     const void *abstract_notch)
{
  const StringList *list = (const StringList *) abstract_list;
  const StringListItem *notch = (const StringListItem *) abstract_notch;
  StringListItem *item;

  assert(list);
  assert(text);

  for (item = notch ? notch->next : list->first; item; item = item->next) {
    if (strcmp(item->text, text) == 0)
      break;
  }

  return item;
}


void
string_list_swap_with_next(void *abstract_list, void *abstract_item)
{
  StringList *list = (StringList *) abstract_list;
  StringListItem *item = (StringListItem *) abstract_item;
  StringListItem *next_item = item->next;
  StringListItem **link;

  assert(list);
  assert(item);
  assert(next_item);

  for (link = &list->first; *link != item; link = &(*link)->next)
    assert(*link);

  *link		  = next_item;
  item->next	  = next_item->next;
  next_item->next = item;

  if (!item->next)
    list->last = item;
}


void
string_list_swap_with_previous(void *abstract_list, void *abstract_item)
{
  StringList *list = (StringList *) abstract_list;
  StringListItem *item = (StringListItem *) abstract_item;
  StringListItem *previous_item;
  StringListItem **link;

  assert(list);
  assert(item);
  assert(item != list->first);

  for (link = &list->first; ; link = &previous_item->next) {
    previous_item = *link;
    assert(previous_item);

    if (previous_item->next == item)
      break;
  }

  *link		      = item;
  previous_item->next = item->next;
  item->next	      = previous_item;

  if (!previous_item->next)
    list->last = previous_item;
}


void
string_list_move(void *abstract_list, void *abstract_item,
		 void *abstract_notch)
{
  StringList *list = (StringList *) abstract_list;
  StringListItem *item = (StringListItem *) abstract_item;
  StringListItem *notch = (StringListItem *) abstract_notch;
  StringListItem *previous_item;
  StringListItem **link;

  assert(list);
  assert(item);
  assert(item != notch);

  for (link = &list->first, previous_item = NULL;
       *link != item; link = &previous_item->next) {
    previous_item = *link;
    assert(previous_item);
  }

  *link = item->next;
  if (!item->next)
    list->last = previous_item;

  if (notch) {
    item->next = notch->next;
    notch->next = item;
  }
  else {
    item->next = list->first;
    list->first = item;
  }

  if (!item->next)
    list->last = item;
}



void
association_list_item_dispose(AssociationListItem *item)
{
  utils_free(item->association);
}


inline char *
association_list_find_association(AssociationList *list, const char *key)
{
  AssociationListItem *item = association_list_find(list, key);

  return item ? item->association : NULL;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
