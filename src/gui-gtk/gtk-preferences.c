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


#include "gtk-preferences.h"

#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-games.h"
#include "gtk-named-vbox.h"
#include "gtk-gtp-client-interface.h"
#include "gtk-progress-dialog.h"
#include "gtk-utils.h"
#include "quarry-stock.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <string.h>


typedef struct _PreferencesDialogPage	PreferencesDialogPage;

struct _PreferencesDialogPage {
  GtkWidget * (* create_page) (void);

  const gchar		      *icon_stock_id;
  const gchar		      *title;
};


enum {
  CATEGORIES_PAGE_INDEX,
#if GTK_2_2_OR_LATER
  CATEGORIES_ICON,
#endif
  CATEGORIES_TEXT,
  CATEGORIES_NUM_COLUMNS
};


enum {
  ENGINES_DATA,
  ENGINES_NAME,
#if GTK_2_4_OR_LATER
  ENGINES_COMMAND_LINE,
#endif
  ENGINES_NUM_COLUMNS
};


typedef struct _GtkEngineSelectorData	GtkEngineSelectorData;

struct _GtkEngineSelectorData {
  GtkEngineChanged    callback;
  GtkWidget	     *selector;
  gpointer	     *user_data;

#if GTK_2_4_OR_LATER
  GtkCellRenderer    *pixbuf_cell;
#else
  GtkGameIndex	      selector_game_index;
  GtpEngineListItem  *last_selection;
#endif
};


typedef struct _GtkEngineDialogData	GtkEngineDialogData;

struct _GtkEngineDialogData {
  GtpEngineListItem  *engine_data;
  gboolean	      engine_deleted;

  GtkWindow	     *window;
  GtkEntry	     *command_line_entry;
  GtkEntry	     *name_entry;

  GtkWindow	     *browsing_dialog;

  GtpClient	     *client;
  GtkWidget	     *progress_dialog;
};


typedef struct _GtkChainEngineData	GtkChainEngineData;

struct _GtkChainEngineData {
  GtpEngineListItem  *engine_data;
  GtkEngineChain     *engine_chain;
};


static void	    handle_drag_and_drop(GtkTreeModel *gtp_engines_tree_model,
					 GtkTreePath *tree_path,
					 GtkTreeIter *iterator);

static GtkWidget *  create_gtp_engines_page(void);
static GtkWidget *  create_board_appearance_page(void);

static void	    gtk_preferences_dialog_change_page
		      (GtkTreeSelection *selection, GtkNotebook *notebook);
static void	    gtk_preferences_dialog_destroy(GtkWindow *window);

static void	    gtk_preferences_dialog_update_gtp_engine_info
		      (GtkTreeSelection *selection);

static void	    do_remove_gtp_engine(void);
static void	    do_move_gtp_engine(gpointer move_upwards);

static GSList *	    find_gtp_engine_dialog_by_engine_data
		      (GtpEngineListItem *engine_data);


static void	    gtk_gtp_engine_dialog_present(gpointer new_engine);

static void	    gtk_gtp_engine_dialog_destroy(GtkWindow *window,
						  GtkEngineDialogData *data);
static void	    gtk_gtp_engine_dialog_response(GtkWindow *window,
						   gint response_id,
						   GtkEngineDialogData *data);

static void	    browse_for_gtp_engine(GtkEngineDialogData *data);
static void	    browsing_dialog_response(GtkFileSelection *file_selection,
					     gint response_id,
					     GtkEngineDialogData *data);

static void	    client_initialized(GtpClient *client, void *user_data);
static void	    client_deleted(GtpClient *client, GError *shutdown_reason,
				   void *user_data);

static gboolean	    show_progress_dialog(GtkProgressDialog *progress_dialog,
					 gpointer user_data);
static gboolean	    cancel_engine_query(GtkProgressDialog *progress_dialog,
					GtkEngineDialogData *data);


static void	    engine_selector_changed(GtkWidget *selector,
					    GtkEngineSelectorData *data);
static void	    engine_selector_destroyed(GtkEngineSelectorData *data);

#if GTK_2_4_OR_LATER

#define prepare_to_rebuild_menus()
#define rebuild_all_menus()

static void	    set_pixbuf_cell_image(GtkCellLayout *cell_layout,
					  GtkCellRenderer *cell,
					  GtkTreeModel *gtp_engines_tree_model,
					  GtkTreeIter *iterator,
					  gpointer data);

#else

static void	    prepare_to_rebuild_menus(void);
static void	    rebuild_all_menus(void);
static void	    build_and_attach_menu(GtkWidget *option_menu,
					  GtkGameIndex game_index);

#endif


static void	    chain_client_initialized(GtpClient *client,
					     void *user_data);


static const PreferencesDialogPage preferences_dialog_pages[] = {
  { create_gtp_engines_page,
    GTK_STOCK_PREFERENCES,		"GTP Engines" },
  { create_board_appearance_page,
    GTK_STOCK_SELECT_COLOR,		"Board Appearance" }
};

static gint		  last_selected_page = 0;


static GtkListStore	 *gtp_engines_list_store;
static GSList		 *gtp_engine_selectors = NULL;


static GtkWindow	 *preferences_dialog = NULL;
static GtkTreeView	 *category_tree_view;

static GtkTreeView	 *gtp_engines_tree_view;
static GtkTreeSelection	 *gtp_engines_tree_selection;
static GtkWidget	 *modify_gtp_engine;
static GtkWidget	 *remove_gtp_engine;
static GtkWidget	 *move_gtp_engine_up;
static GtkWidget	 *move_gtp_engine_down;
static GtkWidget	 *gtp_engine_info;


static GSList		 *gtp_engine_dialogs = NULL;


void
gtk_preferences_init(void)
{
  GtpEngineListItem *engine_data;

#if GTK_2_4_OR_LATER
  gtp_engines_list_store = gtk_list_store_new(ENGINES_NUM_COLUMNS,
					      G_TYPE_POINTER, G_TYPE_STRING,
					      G_TYPE_STRING);
#else
  gtp_engines_list_store = gtk_list_store_new(ENGINES_NUM_COLUMNS,
					      G_TYPE_POINTER, G_TYPE_STRING);
#endif

  for (engine_data = gtp_engines.first; engine_data;
       engine_data = engine_data->next) {
    GtkTreeIter iterator;

    gtk_list_store_append(gtp_engines_list_store, &iterator);

#if GTK_2_4_OR_LATER
    gtk_list_store_set(gtp_engines_list_store, &iterator,
		       ENGINES_DATA, engine_data,
		       ENGINES_NAME, engine_data->screen_name,
		       ENGINES_COMMAND_LINE, engine_data->command_line, -1);
#else
    gtk_list_store_set(gtp_engines_list_store, &iterator,
		       ENGINES_DATA, engine_data,
		       ENGINES_NAME, engine_data->screen_name, -1);
#endif
  }

  g_signal_connect(GTK_TREE_MODEL(gtp_engines_list_store), "row-changed",
		   G_CALLBACK(handle_drag_and_drop), NULL);
}


static void
handle_drag_and_drop(GtkTreeModel *gtp_engines_tree_model,
		     GtkTreePath *tree_path, GtkTreeIter *iterator)
{
  GtpEngineListItem *engine_data;
  GtpEngineListItem *notch = NULL;

  gtk_tree_model_get(gtp_engines_tree_model, iterator,
		     ENGINES_DATA, &engine_data, -1);

  if (gtk_tree_path_prev(tree_path)) {
    GtkTreeIter notch_iterator;

    gtk_tree_model_get_iter(gtp_engines_tree_model,
			    &notch_iterator, tree_path);
    gtk_tree_model_get(gtp_engines_tree_model, &notch_iterator,
		       ENGINES_DATA, &notch, -1);

    /* Restore tree path just in case GTK+ needs it. */
    gtk_tree_path_next(tree_path);
  }

  prepare_to_rebuild_menus();
  string_list_move(&gtp_engines, engine_data, notch);
  rebuild_all_menus();
}


void
gtk_preferences_finalize(void)
{
  g_object_unref(gtp_engines_list_store);
}


void
gtk_preferences_dialog_present(gint page_to_select)
{
  int k;
  GtkTreePath *tree_path;

  if (!preferences_dialog) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Preferences", NULL, 0,
						    GTK_STOCK_CLOSE,
						    GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *label;
    GtkListStore *categories;
    GtkWidget *category_list;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *notebook_widget;
    GtkNotebook *notebook;
    GtkWidget *vbox;
    GtkWidget *hbox;

    preferences_dialog = GTK_WINDOW(dialog);
    gtk_control_center_window_created(preferences_dialog);
    gtk_dialog_set_default_response(GTK_DIALOG(preferences_dialog),
				    GTK_RESPONSE_CLOSE);

    g_signal_connect(preferences_dialog, "destroy",
		     G_CALLBACK(gtk_preferences_dialog_destroy), NULL);
    g_signal_connect(preferences_dialog, "response",
		     G_CALLBACK(gtk_widget_destroy), NULL);

    label = gtk_label_new_with_mnemonic("Cat_egory:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

#if GTK_2_2_OR_LATER
    categories = gtk_list_store_new(CATEGORIES_NUM_COLUMNS,
				    G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
#else
    categories = gtk_list_store_new(CATEGORIES_NUM_COLUMNS,
				    G_TYPE_INT, G_TYPE_STRING);
#endif

    category_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(categories));
    category_tree_view = GTK_TREE_VIEW(category_list);
    gtk_tree_view_set_headers_visible(category_tree_view, FALSE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), category_list);
    g_object_unref(categories);

    vbox = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				 label, GTK_UTILS_FILL,
				 gtk_utils_sink_widget(category_list),
				 GTK_UTILS_PACK_DEFAULT, NULL);

#if GTK_2_2_OR_LATER
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
						      "stock-id",
						      CATEGORIES_ICON,
						      NULL);
    gtk_tree_view_append_column(category_tree_view, column);
#endif

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
						      "text", CATEGORIES_TEXT,
						      NULL);
    gtk_tree_view_append_column(category_tree_view, column);

    notebook_widget = gtk_notebook_new();
    notebook = GTK_NOTEBOOK(notebook_widget);
    gtk_notebook_set_show_tabs(notebook, FALSE);
    gtk_notebook_set_show_border(notebook, FALSE);

    g_signal_connect(gtk_tree_view_get_selection(category_tree_view),
		     "changed", G_CALLBACK(gtk_preferences_dialog_change_page),
		     notebook);

    for (k = 0;
	 k < sizeof(preferences_dialog_pages) / sizeof(PreferencesDialogPage);
	 k++) {
      GtkWidget *contents = preferences_dialog_pages[k].create_page();
      const gchar *icon_stock_id = preferences_dialog_pages[k].icon_stock_id;
      const gchar *title = preferences_dialog_pages[k].title;
      GtkWidget *page = gtk_utils_create_titled_page(contents,
						     icon_stock_id, title);
      GtkTreeIter iterator;

      gtk_notebook_append_page(notebook, page, NULL);

      gtk_list_store_append(categories, &iterator);

#if GTK_2_2_OR_LATER
      gtk_list_store_set(categories, &iterator,
			 CATEGORIES_PAGE_INDEX, k,
			 CATEGORIES_ICON, icon_stock_id,
			 CATEGORIES_TEXT, title, -1);
#else
      gtk_list_store_set(categories, &iterator,
			 CATEGORIES_PAGE_INDEX, k, CATEGORIES_TEXT, title, -1);
#endif
    }

    hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
				 vbox, GTK_UTILS_FILL,
				 gtk_vseparator_new(), GTK_UTILS_FILL,
				 notebook_widget, GTK_UTILS_PACK_DEFAULT,
				 NULL);
    gtk_utils_standardize_dialog(GTK_DIALOG(dialog), hbox);
    gtk_widget_show_all(hbox);
  }

  if (page_to_select < 0 && !GTK_WIDGET_VISIBLE(preferences_dialog))
    page_to_select = last_selected_page;

  assert(0 <= page_to_select && page_to_select < NUM_PREFERENCES_DIALOG_PAGES);

#if GTK_2_2_OR_LATER
  tree_path = gtk_tree_path_new_from_indices(page_to_select, -1);
#else
  tree_path = gtk_tree_path_new();

  gtk_tree_path_append_index(tree_path, page_to_select);
#endif

  gtk_tree_view_set_cursor(category_tree_view, tree_path, NULL, FALSE);
  gtk_tree_path_free(tree_path);

  gtk_window_present(preferences_dialog);
}


static GtkWidget *
create_gtp_engines_page(void)
{
  GtkWidget *label;
  GtkWidget *gtp_engines_widget;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *scrolled_window;
  GtkWidget *add_gtp_engine;
  GtkWidget *button_box;
  GtkWidget *hbox;
  GtkWidget *vbox;

  label = gtk_label_new_with_mnemonic("_List of GTP engines:");
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

  gtp_engines_widget
    = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtp_engines_list_store));
  gtp_engines_tree_view = GTK_TREE_VIEW(gtp_engines_widget);
  gtk_tree_view_set_headers_visible(gtp_engines_tree_view, FALSE);
  gtk_tree_view_set_reorderable(gtp_engines_tree_view, TRUE);
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), gtp_engines_widget);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
						    "text", ENGINES_NAME,
						    NULL);
  gtk_tree_view_append_column(gtp_engines_tree_view, column);

  scrolled_window = gtk_utils_make_widget_scrollable(gtp_engines_widget,
						     GTK_POLICY_AUTOMATIC,
						     GTK_POLICY_AUTOMATIC);

  gtk_utils_create_size_group(GTK_SIZE_GROUP_HORIZONTAL,
			      label, scrolled_window, NULL);

  add_gtp_engine = gtk_button_new_from_stock(GTK_STOCK_ADD);
  g_signal_connect_swapped(add_gtp_engine, "clicked",
			   G_CALLBACK(gtk_gtp_engine_dialog_present),
			   GINT_TO_POINTER(TRUE));

  modify_gtp_engine = gtk_button_new_with_mnemonic("_Modify");
  g_signal_connect_swapped(modify_gtp_engine, "clicked",
			   G_CALLBACK(gtk_gtp_engine_dialog_present),
			   GINT_TO_POINTER(FALSE));

  remove_gtp_engine = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
  g_signal_connect(remove_gtp_engine, "clicked",
		   G_CALLBACK(do_remove_gtp_engine), NULL);

  move_gtp_engine_up = gtk_button_new_from_stock(QUARRY_STOCK_MOVE_UP);
  g_signal_connect_swapped(move_gtp_engine_up, "clicked",
			   G_CALLBACK(do_move_gtp_engine),
			   GINT_TO_POINTER(TRUE));

  move_gtp_engine_down = gtk_button_new_from_stock(QUARRY_STOCK_MOVE_DOWN);
  g_signal_connect_swapped(move_gtp_engine_down, "clicked",
			   G_CALLBACK(do_move_gtp_engine),
			   GINT_TO_POINTER(FALSE));

  button_box = gtk_utils_pack_in_box(GTK_TYPE_VBUTTON_BOX,
				     QUARRY_SPACING_SMALL,
				     add_gtp_engine, 0, modify_gtp_engine, 0,
				     remove_gtp_engine, 0,
				     move_gtp_engine_up, 0,
				     move_gtp_engine_down, 0, NULL);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_START);

  gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(button_box),
				     move_gtp_engine_up, TRUE);
  gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(button_box),
				     move_gtp_engine_down, TRUE);

  hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING,
			       scrolled_window, GTK_UTILS_PACK_DEFAULT,
			       button_box, GTK_UTILS_FILL, NULL);

  vbox = gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
			       label, GTK_UTILS_FILL,
			       hbox, GTK_UTILS_PACK_DEFAULT, NULL);

  gtp_engine_info = gtk_named_vbox_new("GTP Engine Information",
				       FALSE, QUARRY_SPACING_SMALL);

  gtp_engines_tree_selection
    = gtk_tree_view_get_selection(gtp_engines_tree_view);
  g_signal_connect(gtp_engines_tree_selection, "changed",
		   G_CALLBACK(gtk_preferences_dialog_update_gtp_engine_info),
		   NULL);
  gtk_preferences_dialog_update_gtp_engine_info(gtp_engines_tree_selection);

  return gtk_utils_pack_in_box(GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
			       vbox, GTK_UTILS_PACK_DEFAULT,
			       gtp_engine_info, GTK_UTILS_FILL, NULL);
}


static GtkWidget *
create_board_appearance_page(void)
{
  return gtk_label_new("Not implemented yet");
}


static void
gtk_preferences_dialog_change_page (GtkTreeSelection *selection,
				    GtkNotebook *notebook)
{
  GtkTreeIter iterator;
  GtkTreeModel *categories_tree_model;

  if (gtk_tree_selection_get_selected(selection, &categories_tree_model,
				      &iterator)) {
    gtk_tree_model_get(categories_tree_model, &iterator,
		       CATEGORIES_PAGE_INDEX, &last_selected_page, -1);
    gtk_notebook_set_current_page(notebook, last_selected_page);
  }
}


static void
gtk_preferences_dialog_destroy(GtkWindow *window)
{
  assert(window == preferences_dialog);

  if (gtk_control_center_window_destroyed(window))
    preferences_dialog = NULL;
}


static void
gtk_preferences_dialog_update_gtp_engine_info(GtkTreeSelection *selection)
{
  GtkTreeModel *gtp_engines_tree_model;
  GtkTreeIter iterator;
  GtpEngineListItem *engine_data = NULL;

  if (gtk_tree_selection_get_selected(selection, &gtp_engines_tree_model,
				      &iterator)) {
    gtk_tree_model_get(gtp_engines_tree_model, &iterator,
		       ENGINES_DATA, &engine_data, -1);
  }

  gtk_widget_set_sensitive(modify_gtp_engine, engine_data != NULL);
  gtk_widget_set_sensitive(remove_gtp_engine, engine_data != NULL);

  gtk_widget_set_sensitive(move_gtp_engine_up,
			   (engine_data != NULL
			    && engine_data != gtp_engines.first));
  gtk_widget_set_sensitive(move_gtp_engine_down,
			   (engine_data != NULL
			    && engine_data != gtp_engines.last));

  gtk_widget_set_sensitive(gtp_engine_info, engine_data != NULL);
}


static void
do_remove_gtp_engine(void)
{
  GtkTreeModel *gtp_engines_tree_model;
  GtkTreeIter iterator;
  GtpEngineListItem *engine_data;
  GSList *item;
  GtkTreePath *tree_path;

  assert(gtk_tree_selection_get_selected(gtp_engines_tree_selection,
					 &gtp_engines_tree_model,
					 &iterator));
  gtk_tree_model_get(gtp_engines_tree_model, &iterator,
		     ENGINES_DATA, &engine_data, -1);

  item = find_gtp_engine_dialog_by_engine_data(engine_data);
  if (item) {
    GtkEngineDialogData *data= (GtkEngineDialogData *) (item->data);

    if (data->progress_dialog) {
      data->engine_deleted = TRUE;
      gtk_schedule_gtp_client_deletion(data->client);
    }

    gtk_widget_destroy(GTK_WIDGET(data->window));
  }

  prepare_to_rebuild_menus();
  string_list_delete_item(&gtp_engines, engine_data);
  rebuild_all_menus();

#if !GTK_2_4_OR_LATER

  if (string_list_is_empty(&gtp_engines)) {
    for (item = gtp_engine_selectors; item; item = item->next) {
      GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

      data->callback(data->selector, data->user_data);
    }
  }

#endif

  tree_path = gtk_tree_model_get_path(gtp_engines_tree_model, &iterator);
  if (!gtk_list_store_remove(gtp_engines_list_store, &iterator))
    gtk_tree_path_prev(tree_path);

  gtk_tree_view_set_cursor(gtp_engines_tree_view, tree_path, NULL, FALSE);
  gtk_tree_path_free(tree_path);
}


static void
do_move_gtp_engine(gpointer move_upwards)
{
  GtkTreeModel *gtp_engines_tree_model;
  GtkTreeIter first_iterator;
  GtkTreeIter second_iterator;
  GtpEngineListItem *first_engine_data;
  GtpEngineListItem *second_engine_data;
  GtkTreePath *tree_path;

  assert(gtk_tree_selection_get_selected(gtp_engines_tree_selection,
					 &gtp_engines_tree_model,
					 &first_iterator));
  gtk_tree_model_get(gtp_engines_tree_model, &first_iterator,
		     ENGINES_DATA, &first_engine_data, -1);

  tree_path = gtk_tree_model_get_path(gtp_engines_tree_model, &first_iterator);

  prepare_to_rebuild_menus();

  if (GPOINTER_TO_INT(move_upwards)) {
    string_list_swap_with_previous(&gtp_engines, first_engine_data);
    second_engine_data = first_engine_data->next;

    gtk_tree_path_prev(tree_path);
  }
  else {
    second_engine_data = first_engine_data->next;
    string_list_swap_with_next(&gtp_engines, first_engine_data);

    gtk_tree_path_next(tree_path);
  }

  rebuild_all_menus();

  gtk_tree_model_get_iter(gtp_engines_tree_model, &second_iterator, tree_path);

#if GTK_2_2_OR_LATER

  gtk_tree_path_free(tree_path);

  gtk_list_store_swap(gtp_engines_list_store,
		      &first_iterator, &second_iterator);
  gtk_preferences_dialog_update_gtp_engine_info(gtp_engines_tree_selection);

#else /* not GTK_2_2_OR_LATER */

  g_signal_handlers_block_by_func(gtp_engines_list_store,
				  handle_drag_and_drop, NULL);

  gtk_list_store_set(gtp_engines_list_store, &first_iterator,
		     ENGINES_DATA, second_engine_data,
		     ENGINES_NAME, second_engine_data->screen_name, -1);
  gtk_list_store_set(gtp_engines_list_store, &second_iterator,
		     ENGINES_DATA, first_engine_data,
		     ENGINES_NAME, first_engine_data->screen_name, -1);

  g_signal_handlers_unblock_by_func(gtp_engines_list_store,
				    handle_drag_and_drop, NULL);

  gtk_tree_view_set_cursor(gtp_engines_tree_view, tree_path, NULL, FALSE);
  gtk_tree_path_free(tree_path);

#endif /* not GTK_2_2_OR_LATER */
}


static GSList *
find_gtp_engine_dialog_by_engine_data(GtpEngineListItem *engine_data)
{
  GSList *item;

  for (item = gtp_engine_dialogs; item; item = item->next) {
    if (((GtkEngineDialogData *) (item->data))->engine_data == engine_data)
      break;
  }

  return item;
}


static void
gtk_gtp_engine_dialog_present(gpointer new_engine)
{
  static const gchar *hint_text
    = ("You can use `%n' and `%v' strings in <i>Screen name</i> field. "
       "They will substituted with name and version of the engine "
       "correspondingly. By default, `%n %v' is used.");

  GtpEngineListItem *engine_data = NULL;
  GtkEngineDialogData *data;
  GSList *item;

  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *table_widget;
  GtkTable *table;
  GtkWidget *label;
  GtkWidget *entry;

  if (!GPOINTER_TO_INT(new_engine)) {
    /* FIXME */
    assert(0);
  }

  item = find_gtp_engine_dialog_by_engine_data(engine_data);
  if (item) {
    data = (GtkEngineDialogData *) (item->data);

    gtk_window_present(data->window);
    if (data->progress_dialog)
      gtk_window_present(GTK_WINDOW(data->progress_dialog));

    return;
  }

  data = g_malloc(sizeof(GtkEngineDialogData));
  data->engine_data	= engine_data;
  data->engine_deleted	= FALSE;
  data->browsing_dialog = NULL;
  data->progress_dialog = NULL;

  gtp_engine_dialogs = g_slist_prepend(gtp_engine_dialogs, data);

  dialog = gtk_dialog_new_with_buttons((GPOINTER_TO_INT(new_engine)
					? "New GTP Engine"
					: "Modify GTP Engine Information"),
				       NULL, 0,
				       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				       NULL);
  data->window = GTK_WINDOW(dialog);
  gtk_control_center_window_created(data->window);

  gtk_utils_make_window_only_horizontally_resizable(data->window);

  button = gtk_dialog_add_button(GTK_DIALOG(dialog),
				 (GPOINTER_TO_INT(new_engine)
				  ? GTK_STOCK_ADD : "_Modify"),
				 GTK_RESPONSE_OK);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

  g_signal_connect(dialog, "destroy",
		   G_CALLBACK(gtk_gtp_engine_dialog_destroy), data);
  g_signal_connect(dialog, "response",
		   G_CALLBACK(gtk_gtp_engine_dialog_response), data);

  table_widget = gtk_table_new(3, 3, FALSE);
  table = GTK_TABLE(table_widget);
  gtk_table_set_row_spacing(table, 0, QUARRY_SPACING);
  gtk_table_set_row_spacing(table, 1, QUARRY_SPACING_SMALL);
  gtk_table_set_col_spacing(table, 0, QUARRY_SPACING);
  gtk_table_set_col_spacing(table, 1, QUARRY_SPACING_SMALL);

  label = gtk_label_new_with_mnemonic("Command _line:");
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(table, label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

#if GTK_2_4_OR_LATER

  {
    GtkTreeModel *gtp_engines_tree_model
      = GTK_TREE_MODEL(gtp_engines_list_store);

    entry = gtk_combo_box_entry_new_with_model(gtp_engines_tree_model,
					       ENGINES_COMMAND_LINE);
    data->command_line_entry = GTK_ENTRY(GTK_BIN(entry)->child);
    gtk_entry_set_activates_default(data->command_line_entry, TRUE);
  }

#else
  entry = gtk_utils_create_entry(NULL);
  data->command_line_entry = GTK_ENTRY(entry);
#endif

  gtk_utils_set_sensitive_on_input(data->command_line_entry, button);
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
  gtk_table_attach(table, entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  button = gtk_button_new_from_stock(QUARRY_STOCK_BROWSE);
  gtk_table_attach(table, button, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);

  g_signal_connect_swapped(button, "clicked",
			   G_CALLBACK(browse_for_gtp_engine), data);

  label = gtk_label_new_with_mnemonic("Screen _name:");
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(table, label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

  entry = gtk_utils_create_entry("%n %v");
  data->name_entry = GTK_ENTRY(entry);
  gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
  gtk_table_attach(table, entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  label = gtk_label_new(NULL);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_markup(GTK_LABEL(label), hint_text);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(table, label, 0, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gtk_utils_standardize_dialog(GTK_DIALOG(dialog), table_widget);

  gtk_widget_show_all(dialog);
}


static void
gtk_gtp_engine_dialog_destroy(GtkWindow *window, GtkEngineDialogData *data)
{
  if (gtk_control_center_window_destroyed(window)) {
    GSList *item = find_gtp_engine_dialog_by_engine_data(data->engine_data);

    assert(item);
    gtp_engine_dialogs = g_slist_delete_link(gtp_engine_dialogs, item);
    g_free(data);
  }
}


static void
gtk_gtp_engine_dialog_response(GtkWindow *window, gint response_id,
			       GtkEngineDialogData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *command_line;
    GError *error = NULL;

    command_line = gtk_entry_get_text(data->command_line_entry);

    data->client
      = gtk_create_gtp_client(command_line, client_initialized, client_deleted,
			      data, &error);
    if (data->client) {
      data->progress_dialog
	= gtk_progress_dialog_new(window, "Quarry",
				  ("Querying engine's "
				   "name, version and known commands..."),
				  show_progress_dialog,
				  ((GtkProgressDialogCallback)
				   cancel_engine_query),
				  data);
      g_object_ref(data->progress_dialog);

      gtp_client_setup_connection(data->client);
    }
    else {
      gtk_utils_create_message_dialog(window, GTK_STOCK_DIALOG_ERROR,
				      GTK_UTILS_BUTTONS_OK,
				      G_CALLBACK(gtk_widget_destroy),
				      ("Please make sure you typed engine's "
				       "filename correctly and that you have "
				       "permission to execute it."),
				      error->message);
      g_error_free(error);

      gtk_widget_grab_focus(GTK_WIDGET(data->command_line_entry));
    }
  }
  else if (response_id == GTK_RESPONSE_CANCEL)
    gtk_widget_destroy(GTK_WIDGET(window));
}


static void
browse_for_gtp_engine(GtkEngineDialogData *data)
{
  if (!data->browsing_dialog) {
    const gchar *command_line;
    gchar **argv;

    data->browsing_dialog
      = GTK_WINDOW(gtk_file_selection_new("Choose GTP Engine"));
    gtk_window_set_transient_for(data->browsing_dialog, data->window);
    gtk_window_set_destroy_with_parent(data->browsing_dialog, TRUE);

    command_line = gtk_entry_get_text(data->command_line_entry);
    if (*command_line && g_shell_parse_argv(command_line, NULL, &argv, NULL)) {
      gtk_file_selection_set_filename(GTK_FILE_SELECTION(data->browsing_dialog),
				      argv[0]);
      g_strfreev(argv);
    }

    g_signal_connect(data->browsing_dialog, "response",
		     G_CALLBACK(browsing_dialog_response), data);
  }

  gtk_window_present(data->browsing_dialog);
}


static void
browsing_dialog_response(GtkFileSelection *file_selection, gint response_id,
			 GtkEngineDialogData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    gtk_entry_set_text(data->command_line_entry,
		       gtk_file_selection_get_filename(file_selection));
    gtk_widget_grab_focus(GTK_WIDGET(data->command_line_entry));
  }

  gtk_widget_destroy(GTK_WIDGET(file_selection));
  data->browsing_dialog = NULL;
}


static void
client_initialized(GtpClient *client, void *user_data)
{
  GtkWidget *progress_dialog
    = ((GtkEngineDialogData *) user_data)->progress_dialog;

  gtp_client_quit(client);
  gtk_label_set_text(GTK_PROGRESS_DIALOG(progress_dialog)->label,
		     "Waiting for the engine to quit...");
}


static void
client_deleted(GtpClient *client, GError *shutdown_reason, void *user_data)
{
  GtkEngineDialogData *data = (GtkEngineDialogData *) user_data;
  GtkWidget *progress_dialog = data->progress_dialog;

  if (!data->engine_deleted) {
    if (client->operation_stage == GTP_CLIENT_QUIT) {
      static const ConfigurationSection *gtp_engines_section
	= &gtk_configuration_sections[SECTION_GTP_ENGINES];

      const gchar *command_line;
      const gchar *name;
      char *screen_name;
      GtkTreeIter iterator;
      GSList *item;

      command_line = gtk_entry_get_text(data->command_line_entry);
      name = gtk_entry_get_text(data->name_entry);
      if (! *name)
	name = "%n %v";

      screen_name = utils_special_printf(name,
					 'n', client->engine_name,
					 'v', client->engine_version, 0);
      string_list_add_ready(&gtp_engines, screen_name);
      configuration_init_repeatable_section(gtp_engines_section,
					    gtp_engines.last);

      configuration_set_section_values(gtp_engines_section, gtp_engines.last,
				       GTP_ENGINE_SCREEN_NAME_FORMAT, name,
				       GTP_ENGINE_NAME, client->engine_name,
				       GTP_ENGINE_VERSION,
				       client->engine_version,
				       GTP_ENGINE_COMMAND_LINE, command_line,
				       GTP_ENGINE_SUPPORTED_GAMES,
				       &client->supported_games, 1,
				       -1);

      g_signal_handlers_block_by_func(gtp_engines_list_store,
				      handle_drag_and_drop, NULL);

#if GTK_2_4_OR_LATER

      gtk_list_store_append(gtp_engines_list_store, &iterator);
      gtk_list_store_set(gtp_engines_list_store, &iterator,
			 ENGINES_DATA, gtp_engines.last,
			 ENGINES_NAME, screen_name,
			 ENGINES_COMMAND_LINE, gtp_engines.last->command_line,
			 -1);

#else

      gtk_list_store_append(gtp_engines_list_store, &iterator);
      gtk_list_store_set(gtp_engines_list_store, &iterator,
			 ENGINES_DATA, gtp_engines.last,
			 ENGINES_NAME, screen_name, -1);

#endif

      g_signal_handlers_unblock_by_func(gtp_engines_list_store,
					handle_drag_and_drop, NULL);

      prepare_to_rebuild_menus();
      rebuild_all_menus();

      if (string_list_is_single_string(&gtp_engines)) {
	for (item = gtp_engine_selectors; item; item = item->next) {
	  GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

	  data->callback(data->selector, data->user_data);

#if GTK_2_4_OR_LATER
	  gtk_combo_box_set_active(GTK_COMBO_BOX(data->selector), 0);
#endif
	}
      }

      gtk_widget_destroy(GTK_WIDGET(data->window));
    }
    else {
      static const gchar *hint
	= ("The engine might have crashed, quit prematurely or disconnected. "
	   "Please verify command line, including options, and consult "
	   "engine's documentation if needed.");

      gtk_progress_dialog_recover_parent(GTK_PROGRESS_DIALOG(progress_dialog));
      data->progress_dialog = NULL;

      gtk_widget_grab_focus(GTK_WIDGET(data->command_line_entry));

      if (shutdown_reason) {
	gtk_utils_create_message_dialog(data->window, GTK_STOCK_DIALOG_ERROR,
					GTK_UTILS_BUTTONS_OK,
					G_CALLBACK(gtk_widget_destroy),
					hint,
					"Lost connection to GTP Engine (%s).",
					shutdown_reason->message);
      }
    }
  }

  gtk_widget_destroy(progress_dialog);
  g_object_unref(progress_dialog);
}


static gboolean
show_progress_dialog(GtkProgressDialog *progress_dialog, gpointer user_data)
{
  UNUSED(user_data);

  gtk_widget_show(GTK_WIDGET(progress_dialog));
  return FALSE;
}


static gboolean
cancel_engine_query(GtkProgressDialog *progress_dialog,
		    GtkEngineDialogData *data)
{
  UNUSED(progress_dialog);

  gtk_schedule_gtp_client_deletion(data->client);

  return TRUE;
}


GtkWidget *
gtk_preferences_create_engine_selector(GtkGameIndex game_index,
				       const gchar *engine_name,
				       GtkEngineChanged callback,
				       gpointer user_data)
{
  GtkEngineSelectorData *data = g_malloc(sizeof(GtkEngineSelectorData));
  GtpEngineListItem *engine_data = (engine_name
				    ? gtp_engine_list_find(&gtp_engines,
							   engine_name)
				    : NULL);
#if GTK_2_4_OR_LATER

  GtkWidget *widget
    = gtk_combo_box_new_with_model(GTK_TREE_MODEL(gtp_engines_list_store));
  GtkCellRenderer *cell = gtk_cell_renderer_text_new();

  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widget), cell, FALSE);
  gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(widget), cell,
				"text", ENGINES_NAME);

  data->pixbuf_cell = gtk_cell_renderer_pixbuf_new();
  g_object_set(data->pixbuf_cell, "xpad", QUARRY_SPACING_SMALL, NULL);
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widget), data->pixbuf_cell,
			     FALSE);

  gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(widget),
				     data->pixbuf_cell,
				     set_pixbuf_cell_image,
				     GINT_TO_POINTER(game_index), NULL);

#else /* not GTK_2_4_OR_LATER */

  GtkWidget *widget = gtk_option_menu_new();

  build_and_attach_menu(widget, game_index);
  data->selector_game_index = game_index;

#endif /* not GTK_2_4_OR_LATER */

  data->callback  = callback;
  data->selector  = widget;
  data->user_data = user_data;
  gtp_engine_selectors = g_slist_prepend(gtp_engine_selectors, data);

  gtk_preferences_set_engine_selector_selection(widget, engine_data);

  g_signal_connect(widget, "changed",
		   G_CALLBACK(engine_selector_changed), data);
  g_signal_connect_swapped(widget, "destroy",
			   G_CALLBACK(engine_selector_destroyed), data);

  return widget;
}


void
gtk_preferences_set_engine_selector_game_index(GtkWidget *selector,
					       GtkGameIndex game_index)
{
  GSList *item;

  assert(0 <= game_index && game_index < NUM_SUPPORTED_GAMES);

  for (item = gtp_engine_selectors; ; item = item->next) {
    GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

    assert(item);

    if (data->selector == selector) {
#if GTK_2_4_OR_LATER
      gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(selector),
					 data->pixbuf_cell,
					 set_pixbuf_cell_image,
					 GINT_TO_POINTER(game_index), NULL);
#else
      if (game_index != data->selector_game_index) {
	g_signal_handlers_block_by_func(data->selector,
					engine_selector_changed, data);

	build_and_attach_menu(data->selector, game_index);

	g_signal_handlers_unblock_by_func(data->selector,
					  engine_selector_changed, data);
      }

      data->selector_game_index = game_index;
#endif

      break;
    }
  }
}


void
gtk_preferences_set_engine_selector_selection(GtkWidget *selector,
					      GtpEngineListItem *engine_data)
{
  gint engine_to_select;

  if (engine_data) {
    engine_to_select = string_list_get_item_index(&gtp_engines, engine_data);

    if (engine_to_select == -1)
      engine_to_select = 0;
  }
  else
    engine_to_select = 0;

#if GTK_2_4_OR_LATER
  gtk_combo_box_set_active(GTK_COMBO_BOX(selector), engine_to_select);
#else
  gtk_option_menu_set_history(GTK_OPTION_MENU(selector), engine_to_select);
#endif
}


GtpEngineListItem *
gtk_preferences_get_engine_selector_selection(GtkWidget *selector)
{
#if GTK_2_4_OR_LATER
  gint selected_engine = gtk_combo_box_get_active(GTK_COMBO_BOX(selector));
#else
  gint selected_engine
    = gtk_option_menu_get_history(GTK_OPTION_MENU(selector));
#endif

  return gtp_engine_list_get_item(&gtp_engines, selected_engine);
}


static void
engine_selector_changed(GtkWidget *selector, GtkEngineSelectorData *data)
{
  data->callback(selector, data->user_data);
}


static void
engine_selector_destroyed(GtkEngineSelectorData *data)
{
  gtp_engine_selectors = g_slist_remove(gtp_engine_selectors, data);
}


#if GTK_2_4_OR_LATER


static void
set_pixbuf_cell_image(GtkCellLayout *cell_layout, GtkCellRenderer *cell,
		      GtkTreeModel *gtp_engines_tree_model,
		      GtkTreeIter *iterator,
		      gpointer data)
{
  GtpEngineListItem *engine_data;
  const gchar *stock_id;

  UNUSED(cell_layout);

  gtk_tree_model_get(gtp_engines_tree_model, iterator,
		     ENGINES_DATA, &engine_data, -1);
  stock_id = (gtk_games_engine_supports_game(engine_data,
					     GPOINTER_TO_INT(data))
	      ? GTK_STOCK_YES : GTK_STOCK_NO);

  g_object_set(cell, "stock-id", stock_id, NULL);
}


#else /* not GTK_2_4_OR_LATER */


static void
prepare_to_rebuild_menus(void)
{
  GSList *item;

  for (item = gtp_engine_selectors; item; item = item->next) {
    GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

    data->last_selection
      = gtk_preferences_get_engine_selector_selection(data->selector);
  }
}


static void
rebuild_all_menus(void)
{
  GSList *item;

  for (item = gtp_engine_selectors; item; item = item->next) {
    GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

    g_signal_handlers_block_by_func(data->selector,
				    engine_selector_changed, data);

    build_and_attach_menu(data->selector, data->selector_game_index);
    gtk_preferences_set_engine_selector_selection(data->selector,
						  data->last_selection);

    g_signal_handlers_unblock_by_func(data->selector,
				      engine_selector_changed, data);
  }
}


static void
build_and_attach_menu(GtkWidget *option_menu, GtkGameIndex game_index)
{
  GtkWidget *menu = gtk_menu_new();
  GtpEngineListItem *engine_data;

  for (engine_data = gtp_engines.first; engine_data;
       engine_data = engine_data->next) {
    gboolean engine_supports_game = gtk_games_engine_supports_game(engine_data,
								   game_index);
    GtkWidget *menu_item = gtk_menu_item_new();
    GtkWidget *icon;
    GtkWidget *hbox;

    icon = gtk_image_new_from_stock((engine_supports_game
				     ? GTK_STOCK_YES : GTK_STOCK_NO),
				    GTK_ICON_SIZE_MENU);

    hbox = gtk_utils_pack_in_box(GTK_TYPE_HBOX, QUARRY_SPACING_SMALL,
				 gtk_label_new(engine_data->screen_name),
				 GTK_UTILS_FILL,
				 icon, GTK_UTILS_FILL, NULL);

    gtk_container_add(GTK_CONTAINER(menu_item), hbox);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  }

  gtk_widget_show_all(menu);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
}


#endif /* not GTK_2_4_OR_LATER */


/* FIXME: the functions below need to be written properly. */

GtkEngineChain *
gtk_preferences_create_engines_instantiation_chain
		(GtkWindow *parent_window,
		 GtkEnginesInstantiated callback, gpointer user_data)
{
  GtkEngineChain *engine_chain = g_malloc(sizeof(GtkEngineChain));

  engine_chain->parent_window = parent_window;

  engine_chain->instantiation_callback = callback;
  engine_chain->user_data = user_data;

  engine_chain->chain_engine_datum = NULL;
  engine_chain->have_error = FALSE;

  return engine_chain;
}


void
gtk_preferences_instantiate_selected_engine(GtkEngineChain *engine_chain,
					    GtkWidget *selector,
					    GtpClient **gtp_client)
{
  assert(engine_chain);
  assert(selector);
  assert(gtp_client);

  if (!engine_chain->have_error) {
    GtpEngineListItem *engine_data
      = gtk_preferences_get_engine_selector_selection(selector);
    GError *error = NULL;
    GtkChainEngineData *chain_engine_data
      = g_malloc(sizeof(GtkChainEngineData));

    assert(engine_data);

    chain_engine_data->engine_data = engine_data;
    chain_engine_data->engine_chain = engine_chain;

    *gtp_client = gtk_create_gtp_client(engine_data->command_line,
					((GtpClientInitializedCallback)
					 chain_client_initialized),
					NULL, chain_engine_data, &error);

    if (*gtp_client) {
      gtp_client_setup_connection(*gtp_client);

      engine_chain->chain_engine_datum
	= g_slist_prepend(engine_chain->chain_engine_datum, chain_engine_data);
    }
    else {
      g_free(chain_engine_data);

      gtk_utils_create_message_dialog(engine_chain->parent_window,
				      GTK_STOCK_DIALOG_ERROR,
				      GTK_UTILS_BUTTONS_OK,
				      G_CALLBACK(gtk_widget_destroy),
				      ("Perhaps engine's binary has been "
				       "deleted or changed. You will probably "
				       "need to alter engine's command line "
				       "in preferences dialog"),
				      error->message);
      g_error_free(error);

      engine_chain->have_error = TRUE;
    }
  }
}


void
gtk_preferences_do_instantiate_engines(GtkEngineChain *engine_chain)
{
  if (engine_chain->chain_engine_datum != NULL) {
  }
  else {
    if (engine_chain->instantiation_callback) {
      engine_chain->instantiation_callback(ENGINES_INSTANTIATED,
					   engine_chain->user_data);
    }

    g_free(engine_chain);
  }
}


static void
chain_client_initialized(GtpClient *client, void *user_data)
{
  GtkChainEngineData *chain_engine_data = (GtkChainEngineData *) user_data;
  GtkEngineChain *engine_chain = chain_engine_data->engine_chain;

  UNUSED(client);

  engine_chain->chain_engine_datum
    = g_slist_remove(engine_chain->chain_engine_datum, chain_engine_data);
  g_free(chain_engine_data);

  if (engine_chain->chain_engine_datum == NULL) {
    if (engine_chain->instantiation_callback) {
      engine_chain->instantiation_callback(ENGINES_INSTANTIATED,
					   engine_chain->user_data);
    }

    g_free(engine_chain);
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
