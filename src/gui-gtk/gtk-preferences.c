/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2003, 2004, 2005 Paul Pogonyshev                  *
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

#include "gtk-color-button.h"
#include "gtk-configuration.h"
#include "gtk-control-center.h"
#include "gtk-file-selector.h"
#include "gtk-games.h"
#include "gtk-goban-base.h"
#include "gtk-help.h"
#include "gtk-named-vbox.h"
#include "gtk-gtp-client-interface.h"
#include "gtk-progress-dialog.h"
#include "gtk-utils.h"
#include "quarry-stock.h"
#include "markup-theme-configuration.h"
#include "utils.h"

#include <assert.h>
#include <gtk/gtk.h>
#include <string.h>


typedef struct _PreferencesDialogCategory	PreferencesDialogCategory;

struct _PreferencesDialogCategory {
  GtkWidget * (* create_page) (void);

  const gchar		      *tree_icon_stock_id;
  const gchar		      *tree_title;

  const gchar		      *page_icon_stock_id;
  const gchar		      *page_title;
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
  ENGINES_IS_VISIBLE,
  ENGINES_NAME,
#if GTK_2_4_OR_LATER
  ENGINES_COMMAND_LINE,
#endif
  ENGINES_NUM_COLUMNS
};


typedef struct _UseThemeDefaultsData	UseThemeDefaultsData;

struct _UseThemeDefaultsData {
  Game		      game;

  GtkWidget	     *vbox;

  GtkRange	     *size_range;
  GtkToggleButton    *size_is_relative_toggle_button;
  GtkRange	     *opacity_range;
};


typedef struct _GtkEngineSelectorData	GtkEngineSelectorData;

struct _GtkEngineSelectorData {
  GtkEngineChanged    callback;
  GtkWidget	     *selector;
  gpointer	     *user_data;

  GtkGameIndex	      selector_game_index;

#if GTK_2_4_OR_LATER
  GtkCellRenderer    *pixbuf_cell;
#else
  gboolean	      only_this_game;
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

  GtpClient	     *client;
  GtkWidget	     *progress_dialog;
};


typedef struct _GtkChainEngineData	GtkChainEngineData;

struct _GtkChainEngineData {
  GtpEngineListItem  *engine_data;
  GtkEngineChain     *engine_chain;
};


static void	    handle_drag_and_drop (GtkTreeModel *gtp_engines_tree_model,
					  GtkTreePath *tree_path,
					  GtkTreeIter *iterator);


static GtkWidget *  create_gtp_engines_page (void);
static GtkWidget *  create_game_tree_page (void);
static GtkWidget *  create_saving_sgf_page (void);
static GtkWidget *  create_go_board_appearance_page (void);
static GtkWidget *  create_amazons_board_appearance_page (void);
static GtkWidget *  create_othello_board_appearance_page (void);

static GtkWidget *  create_background_table (GtkGameIndex game_index,
					     gint num_table_rows);
static GtkWidget *  create_markup_box (GtkGameIndex game_index,
				       const gchar *labels[4]);
static GtkWidget *  create_board_appearance_notebook_page
		      (GtkWidget *background_widget, GtkWidget *markup_widget);


static void	    gtk_preferences_dialog_change_page
		      (GtkTreeSelection *selection, GtkNotebook *notebook);
static void	    gtk_preferences_dialog_response (GtkWindow *window,
						     gint response_id);

static void	    gtk_preferences_dialog_update_gtp_engine_info
		      (GtkTreeSelection *selection);

static void	    show_or_hide_gtp_engine
		      (GtkCellRendererToggle *cell, gchar *path_string,
		       gpointer user_data);
static void	    do_remove_gtp_engine (void);
static void	    do_move_gtp_engine (gpointer move_upwards);

static GSList *	    find_gtp_engine_dialog_by_engine_data
		      (GtpEngineListItem *engine_data);


static void	    gtk_gtp_engine_dialog_present (gpointer new_engine);

static void	    gtk_gtp_engine_dialog_destroy (GtkWindow *window,
						   GtkEngineDialogData *data);
static void	    gtk_gtp_engine_dialog_response (GtkWindow *window,
						    gint response_id,
						    GtkEngineDialogData *data);

static void	    client_initialized (GtpClient *client, void *user_data);
static void	    client_deleted (GtpClient *client, GError *shutdown_reason,
				    void *user_data);

static gboolean	    show_progress_dialog (GtkProgressDialog *progress_dialog,
					  gpointer user_data);
static gboolean	    cancel_engine_query (GtkProgressDialog *progress_dialog,
					 GtkEngineDialogData *data);

static void	    store_toggle_setting (GtkToggleButton *toggle_button,
					  int *value_storage);
static void	    store_radio_button_setting (GtkRadioButton *radio_button,
						int *value_storage);

static gboolean	    update_board_background_texture (GtkEntry *entry,
						     GdkEventFocus *event,
						     gpointer game_index);

#ifdef GTK_TYPE_FILE_SELECTOR
static void	    board_background_texture_changed
		      (GtkFileSelector *file_selector, gpointer game_index);
#endif

static void	    update_board_appearance (GtkWidget *widget,
					     gpointer value_storage);
static void	    update_board_markup_theme (GtkWidget *widget,
					       UseThemeDefaultsData *data);
static void	    update_markup_theme_defaults_usage
		      (GtkToggleButton *toggle_button,
		       UseThemeDefaultsData *data);

static inline BoardAppearance *
		    game_index_to_board_appearance_structure
		      (GtkGameIndex game_index);


static void	    engine_selector_changed (GtkWidget *selector,
					     GtkEngineSelectorData *data);
static void	    engine_selector_destroyed (GtkEngineSelectorData *data);

#if GTK_2_4_OR_LATER

#define prepare_to_rebuild_menus()
#define rebuild_all_menus()

static void	    set_pixbuf_cell_image
		      (GtkCellLayout *cell_layout, GtkCellRenderer *cell,
		       GtkTreeModel *gtp_engines_tree_model,
		       GtkTreeIter *iterator, gpointer data);

static gboolean	    engine_is_visible_and_supports_game
		      (GtkTreeModel *tree_model, GtkTreeIter *iterator,
		       gpointer game_index);

#else

static void	    prepare_to_rebuild_menus (void);
static void	    rebuild_all_menus (void);
static void	    build_and_attach_menu (GtkWidget *option_menu,
					   GtkGameIndex game_index,
					   gboolean only_this_game);

#endif


static void	    chain_client_initialized (GtpClient *client,
					      void *user_data);
static void	    update_engine_screen_name (GtpEngineListItem *engine_data,
					       gboolean update_tree_model);
static void	    find_gtp_tree_model_iterator_by_engines_data
		      (const GtpEngineListItem *engine_data,
		       GtkTreeIter *iterator);

static gint		  last_selected_page = 0;


static GtkListStore	 *gtp_engines_list_store;
static GSList		 *gtp_engine_selectors = NULL;

#if GTK_2_4_OR_LATER
static GtkTreeModel	 *non_hidden_gtp_engines_tree_model;
static GtkTreeModel *only_one_game_gtp_engines_tree_model[NUM_SUPPORTED_GAMES];
#endif


static GtkWindow	 *preferences_dialog = NULL;
static GtkTreeView	 *category_tree_view;

static GtkTreeView	 *gtp_engines_tree_view;
static GtkTreeSelection	 *gtp_engines_tree_selection;
static GtkWidget	 *modify_gtp_engine;
static GtkWidget	 *remove_gtp_engine;
static GtkWidget	 *move_gtp_engine_up;
static GtkWidget	 *move_gtp_engine_down;
static GtkWidget	 *gtp_engine_info;
static GtkLabel		 *gtp_engine_name;
static GtkLabel		 *gtp_engine_version;
static GtkLabel		 *gtp_engine_supported_games;
static GtkLabel		 *gtp_engine_command_line;
static GtkLabel		 *gtp_engine_additional_info;


static GSList		 *gtp_engine_dialogs = NULL;


void
gtk_preferences_init (void)
{
  GtpEngineListItem *engine_data;

#if GTK_2_4_OR_LATER
  int k;

  gtp_engines_list_store = gtk_list_store_new (ENGINES_NUM_COLUMNS,
					       G_TYPE_POINTER, G_TYPE_BOOLEAN,
					       G_TYPE_STRING, G_TYPE_STRING);
#else
  gtp_engines_list_store = gtk_list_store_new (ENGINES_NUM_COLUMNS,
					       G_TYPE_POINTER, G_TYPE_BOOLEAN,
					       G_TYPE_STRING);
#endif

  for (engine_data = gtp_engines.first; engine_data;
       engine_data = engine_data->next) {
    GtkTreeIter iterator;

    /* Force name specified by `screen_name'. */
    update_engine_screen_name (engine_data, FALSE);

    gtk_list_store_append (gtp_engines_list_store, &iterator);

#if GTK_2_4_OR_LATER
    gtk_list_store_set (gtp_engines_list_store, &iterator,
			ENGINES_DATA, engine_data,
			ENGINES_IS_VISIBLE, !engine_data->is_hidden,
			ENGINES_NAME, engine_data->screen_name,
			ENGINES_COMMAND_LINE, engine_data->command_line, -1);
#else
    gtk_list_store_set (gtp_engines_list_store, &iterator,
			ENGINES_DATA, engine_data,
			ENGINES_IS_VISIBLE, !engine_data->is_hidden,
			ENGINES_NAME, engine_data->screen_name, -1);
#endif
  }

  g_signal_connect (GTK_TREE_MODEL (gtp_engines_list_store), "row-changed",
		    G_CALLBACK (handle_drag_and_drop), NULL);

  gui_back_end_register_object_to_finalize (gtp_engines_list_store);

#if GTK_2_4_OR_LATER

  non_hidden_gtp_engines_tree_model
    = gtk_tree_model_filter_new (GTK_TREE_MODEL (gtp_engines_list_store),
				 NULL);

  gtk_tree_model_filter_set_visible_column
    (GTK_TREE_MODEL_FILTER (non_hidden_gtp_engines_tree_model),
     ENGINES_IS_VISIBLE);

  gui_back_end_register_object_to_finalize (non_hidden_gtp_engines_tree_model);

  for (k = 0; k < NUM_SUPPORTED_GAMES; k++) {
    only_one_game_gtp_engines_tree_model[k]
      = gtk_tree_model_filter_new (GTK_TREE_MODEL (gtp_engines_list_store),
				   NULL);

    gtk_tree_model_filter_set_visible_func
      (GTK_TREE_MODEL_FILTER (only_one_game_gtp_engines_tree_model[k]),
       engine_is_visible_and_supports_game, GINT_TO_POINTER (k), NULL);

    gui_back_end_register_object_to_finalize
      (only_one_game_gtp_engines_tree_model[k]);
  }

#endif
}


static void
handle_drag_and_drop (GtkTreeModel *gtp_engines_tree_model,
		      GtkTreePath *tree_path, GtkTreeIter *iterator)
{
  GtpEngineListItem *engine_data;
  GtpEngineListItem *notch = NULL;

  gtk_tree_model_get (gtp_engines_tree_model, iterator,
		      ENGINES_DATA, &engine_data, -1);

  if (gtk_tree_path_prev (tree_path)) {
    GtkTreeIter notch_iterator;

    gtk_tree_model_get_iter (gtp_engines_tree_model,
			     &notch_iterator, tree_path);
    gtk_tree_model_get (gtp_engines_tree_model, &notch_iterator,
			ENGINES_DATA, &notch, -1);

    /* Restore tree path just in case GTK+ needs it. */
    gtk_tree_path_next (tree_path);
  }

  prepare_to_rebuild_menus ();
  string_list_move (&gtp_engines, engine_data, notch);
  rebuild_all_menus ();
}


void
gtk_preferences_dialog_present (gpointer page_to_select)
{
  static const PreferencesDialogCategory preferences_dialog_categories[] = {
    { NULL,
      GTK_STOCK_CUT,		N_("<b>Editing &amp; Viewing</b>"),
      NULL,			NULL },
    { create_game_tree_page,
      NULL,			N_("Game Tree"),
      NULL,			N_("Game Tree") },

    { NULL,
      GTK_STOCK_PREFERENCES,	N_("<b>GTP</b>"),
      NULL,			NULL },
    { create_gtp_engines_page,
      NULL,			N_("GTP Engines"),
      GTK_STOCK_EXECUTE,	N_("GTP Engines") },

    { NULL,
      QUARRY_STOCK_ICON_FILE,	N_("<b>Game Records (SGF)</b>"),
      NULL,			NULL },
    { create_saving_sgf_page,
      NULL,			N_("Saving"),
      GTK_STOCK_SAVE,		N_("Saving Game Records") },

    { NULL,
      GTK_STOCK_SELECT_COLOR,	N_("<b>Board Appearance</b>"),
      NULL,			NULL },
    { create_go_board_appearance_page,
      NULL,			N_("Go"),
      GTK_STOCK_SELECT_COLOR,	N_("Go Board Appearance") },
    { create_amazons_board_appearance_page,
      NULL,			N_("Amazons"),
      GTK_STOCK_SELECT_COLOR,	N_("Amazons Board Appearance") },
    { create_othello_board_appearance_page,
      NULL,			N_("Othello"),
      GTK_STOCK_SELECT_COLOR,	N_("Othello Board Appearance") }
  };

  int k;
  int category_to_select;
  int subcategory_to_select;
  GtkTreePath *tree_path;
  gint page_to_select_gint = (GPOINTER_TO_INT (page_to_select) >= 0
			      ? GPOINTER_TO_INT (page_to_select)
			      : last_selected_page);

  if (!preferences_dialog) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Preferences"), NULL, 0,
						     GTK_STOCK_CLOSE,
						     GTK_RESPONSE_CLOSE,
						     GTK_STOCK_HELP,
						     GTK_RESPONSE_HELP, NULL);
    GtkTreeStore *categories;
    GtkWidget *category_list;
    GtkWidget *label;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *notebook_widget;
    GtkNotebook *notebook;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkTreeIter category_parent;
    int page_index;

    preferences_dialog = GTK_WINDOW (dialog);
    gtk_control_center_window_created (preferences_dialog);
    gtk_utils_null_pointer_on_destroy (&preferences_dialog, TRUE);

    gtk_dialog_set_default_response (GTK_DIALOG (preferences_dialog),
				     GTK_RESPONSE_CLOSE);

    g_signal_connect (preferences_dialog, "response",
		      G_CALLBACK (gtk_preferences_dialog_response), NULL);

    categories = gtk_tree_store_new (CATEGORIES_NUM_COLUMNS,
				     G_TYPE_INT,
#if GTK_2_2_OR_LATER
				     G_TYPE_STRING,
#endif
				     G_TYPE_STRING);

    category_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (categories));
    category_tree_view = GTK_TREE_VIEW (category_list);
    gtk_tree_view_set_headers_visible (category_tree_view, FALSE);
    g_object_unref (categories);

    label = gtk_utils_create_mnemonic_label (_("Categor_y:"), category_list);

    vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				  label, GTK_UTILS_FILL,
				  gtk_utils_sink_widget (category_list),
				  GTK_UTILS_PACK_DEFAULT, NULL);

#if GTK_2_2_OR_LATER
    renderer = gtk_cell_renderer_pixbuf_new ();
    column = gtk_tree_view_column_new_with_attributes (NULL, renderer,
						       "stock-id",
						       CATEGORIES_ICON,
						       NULL);
    gtk_tree_view_append_column (category_tree_view, column);
#endif

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (NULL, renderer,
						       "markup",
						       CATEGORIES_TEXT,
						       NULL);
    gtk_tree_view_append_column (category_tree_view, column);

    notebook_widget = gtk_utils_create_invisible_notebook ();
    notebook = GTK_NOTEBOOK (notebook_widget);

    g_signal_connect (gtk_tree_view_get_selection (category_tree_view),
		      "changed",
		      G_CALLBACK (gtk_preferences_dialog_change_page),
		      notebook);

    for (k = 0, page_index = 0;
	 k < (sizeof preferences_dialog_categories
	      / sizeof (PreferencesDialogCategory));
	 k++) {
      GtkTreeIter iterator;
      const PreferencesDialogCategory *category_data
	= preferences_dialog_categories + k;
      int this_page_index;

      if (category_data->create_page) {
	GtkWidget *page
	  = gtk_utils_create_titled_page (category_data->create_page (),
					  category_data->page_icon_stock_id,
					  _(category_data->page_title));

	gtk_notebook_append_page (notebook, page, NULL);

	gtk_tree_store_append (categories, &iterator, &category_parent);
	this_page_index = page_index++;
      }
      else {
	gtk_tree_store_append (categories, &category_parent, NULL);
	iterator = category_parent;
	this_page_index = -1;
      }

      gtk_tree_store_set (categories, &iterator,
			  CATEGORIES_PAGE_INDEX, this_page_index,
#if GTK_2_2_OR_LATER
			  CATEGORIES_ICON, category_data->tree_icon_stock_id,
#endif
			  CATEGORIES_TEXT, _(category_data->tree_title), -1);
    }

    gtk_tree_view_expand_all (category_tree_view);

    hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				  vbox, GTK_UTILS_FILL,
				  gtk_vseparator_new (), GTK_UTILS_FILL,
				  notebook_widget, GTK_UTILS_PACK_DEFAULT,
				  NULL);
    gtk_utils_standardize_dialog (GTK_DIALOG (dialog), hbox);
    gtk_widget_show_all (hbox);
  }

  assert (page_to_select_gint < NUM_PREFERENCES_DIALOG_PAGES);

  for (category_to_select = -1, subcategory_to_select = -1, k = 0;
       page_to_select_gint >= 0; k++) {
    if (preferences_dialog_categories[k].create_page) {
      page_to_select_gint--;
      subcategory_to_select++;
    }
    else {
      category_to_select++;
      subcategory_to_select = -1;
    }
  }

#if GTK_2_2_OR_LATER
  tree_path = gtk_tree_path_new_from_indices (category_to_select,
					      subcategory_to_select, -1);
#else
  tree_path = gtk_tree_path_new ();

  gtk_tree_path_append_index (tree_path, category_to_select);
  gtk_tree_path_append_index (tree_path, subcategory_to_select);
#endif

  gtk_tree_view_set_cursor (category_tree_view, tree_path, NULL, FALSE);
  gtk_tree_path_free (tree_path);

  gtk_window_present (preferences_dialog);
}


static GtkWidget *
create_gtp_engines_page (void)
{
  GtkWidget *gtp_engines_widget;
  GtkWidget *label;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *scrolled_window;
  GtkWidget *add_gtp_engine;
  GtkWidget *button_box;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *info_label;
  GtkWidget *name_hbox;
  GtkWidget *version_hbox;
  GtkWidget *supported_games_hbox;
  GtkWidget *command_line_hbox;
  GtkWidget *info_vbox;

  gtp_engines_widget
    = gtk_tree_view_new_with_model (GTK_TREE_MODEL (gtp_engines_list_store));
  gtp_engines_tree_view = GTK_TREE_VIEW (gtp_engines_widget);
  gtk_tree_view_set_reorderable (gtp_engines_tree_view, TRUE);

  renderer = gtk_cell_renderer_toggle_new ();

  g_object_set (renderer, "activatable", TRUE, NULL);
  g_signal_connect (renderer, "toggled",
		    G_CALLBACK (show_or_hide_gtp_engine), NULL);

  column = gtk_tree_view_column_new_with_attributes (_("Show"), renderer,
						     "active",
						     ENGINES_IS_VISIBLE,
						     NULL);
  gtk_tree_view_append_column (gtp_engines_tree_view, column);

  renderer = gtk_cell_renderer_text_new ();
  column   = gtk_tree_view_column_new_with_attributes (_("Name"), renderer,
						       "text", ENGINES_NAME,
						       NULL);
  gtk_tree_view_append_column (gtp_engines_tree_view, column);

  scrolled_window = gtk_utils_make_widget_scrollable (gtp_engines_widget,
						      GTK_POLICY_AUTOMATIC,
						      GTK_POLICY_AUTOMATIC);

  label = gtk_utils_create_mnemonic_label (_("_List of GTP engines:"),
					   gtp_engines_widget);

  add_gtp_engine = gtk_button_new_from_stock (GTK_STOCK_ADD);
  g_signal_connect_swapped (add_gtp_engine, "clicked",
			    G_CALLBACK (gtk_gtp_engine_dialog_present),
			    GINT_TO_POINTER (TRUE));

  modify_gtp_engine = gtk_button_new_from_stock (QUARRY_STOCK_MODIFY);
  g_signal_connect_swapped (modify_gtp_engine, "clicked",
			    G_CALLBACK (gtk_gtp_engine_dialog_present),
			    GINT_TO_POINTER (FALSE));

  remove_gtp_engine = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
  g_signal_connect (remove_gtp_engine, "clicked",
		    G_CALLBACK (do_remove_gtp_engine), NULL);

  move_gtp_engine_up = gtk_button_new_from_stock (QUARRY_STOCK_MOVE_UP);
  g_signal_connect_swapped (move_gtp_engine_up, "clicked",
			    G_CALLBACK (do_move_gtp_engine),
			    GINT_TO_POINTER (TRUE));

  move_gtp_engine_down = gtk_button_new_from_stock (QUARRY_STOCK_MOVE_DOWN);
  g_signal_connect_swapped (move_gtp_engine_down, "clicked",
			    G_CALLBACK (do_move_gtp_engine),
			    GINT_TO_POINTER (FALSE));

  button_box = gtk_utils_pack_in_box (GTK_TYPE_VBUTTON_BOX,
				      QUARRY_SPACING_SMALL,
				      add_gtp_engine, 0, modify_gtp_engine, 0,
				      remove_gtp_engine, 0,
				      move_gtp_engine_up, 0,
				      move_gtp_engine_down, 0, NULL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_START);

  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (button_box),
				      move_gtp_engine_up, TRUE);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (button_box),
				      move_gtp_engine_down, TRUE);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				scrolled_window, GTK_UTILS_PACK_DEFAULT,
				button_box, GTK_UTILS_FILL, NULL);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				label, GTK_UTILS_FILL,
				hbox, GTK_UTILS_PACK_DEFAULT, NULL);

  label		  = gtk_utils_create_left_aligned_label (_("Name:"));
  info_label	  = gtk_utils_create_left_aligned_label (NULL);
  gtp_engine_name = GTK_LABEL (info_label);
  gtk_label_set_selectable (gtp_engine_name, TRUE);

  name_hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				     label, GTK_UTILS_FILL,
				     info_label, GTK_UTILS_PACK_DEFAULT, NULL);

  label		     = gtk_utils_create_left_aligned_label (_("Version:"));
  info_label	     = gtk_utils_create_left_aligned_label (NULL);
  gtp_engine_version = GTK_LABEL (info_label);
  gtk_label_set_selectable (gtp_engine_version, TRUE);

  version_hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
					label, GTK_UTILS_FILL,
					info_label, GTK_UTILS_PACK_DEFAULT,
					NULL);

  label
    = gtk_utils_create_left_aligned_label (_("Supported game(s):"));
  info_label		     = gtk_utils_create_left_aligned_label (NULL);
  gtp_engine_supported_games = GTK_LABEL (info_label);
  gtk_label_set_selectable (gtp_engine_supported_games, TRUE);

  supported_games_hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
						label, GTK_UTILS_FILL,
						info_label,
						GTK_UTILS_PACK_DEFAULT,
						NULL);

  label
    = gtk_utils_create_left_aligned_label (_("Command line:"));
  info_label		  = gtk_utils_create_left_aligned_label (NULL);
  gtp_engine_command_line = GTK_LABEL (info_label);
  gtk_label_set_selectable (gtp_engine_command_line, TRUE);

  command_line_hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
					     label, GTK_UTILS_FILL,
					     info_label,
					     GTK_UTILS_PACK_DEFAULT,
					     NULL);

  info_vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				     name_hbox, GTK_UTILS_FILL,
				     version_hbox, GTK_UTILS_FILL,
				     supported_games_hbox, GTK_UTILS_FILL,
				     command_line_hbox, GTK_UTILS_FILL, NULL);

  label = gtk_utils_create_left_aligned_label (NULL);
  gtp_engine_additional_info = GTK_LABEL (label);

  gtp_engine_info = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
					   QUARRY_SPACING_BIG,
					   info_vbox, GTK_UTILS_FILL,
					   label, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (gtp_engine_info),
				 _("GTP Engine Information"));

  gtk_utils_align_left_widgets (GTK_CONTAINER (gtp_engine_info), NULL);

  gtp_engines_tree_selection
    = gtk_tree_view_get_selection (gtp_engines_tree_view);
  g_signal_connect (gtp_engines_tree_selection, "changed",
		    G_CALLBACK (gtk_preferences_dialog_update_gtp_engine_info),
		    NULL);
  gtk_preferences_dialog_update_gtp_engine_info (gtp_engines_tree_selection);

  return gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				vbox, GTK_UTILS_PACK_DEFAULT,
				gtp_engine_info, GTK_UTILS_FILL, NULL);
}


static GtkWidget *
create_game_tree_page (void)
{
  static const gchar *show_game_tree_radio_labels[3]
    = { N_("_Always"), N_("A_utomatically"), N_("_Never") };
  static const gchar *track_current_node_radio_labels[3]
    = { N_("Al_ways"), N_("Au_tomatically"), N_("Ne_ver") };
  static const gchar *scroll_method_radio_labels[2]
    = { N_("Scroll _minimal distance"),
	N_("_Recenter view on the current node") };

  static const gchar *show_game_tree_hint
    = N_("In automatic mode the game tree is shown only if it has at least "
	 "one variation. In any case, you can show/hide game tree in each "
	 "window separately.");
  static const gchar *track_current_node_hint
    = N_("In automatic mode the game tree view scrolls to show the current "
	 "node only if it has been showing current node before.  I.e. unless "
	 "you scrolled it away manually.");

  GtkWidget *radio_buttons[3];
  GtkWidget *label;
  GtkWidget *show_game_tree_named_vbox;
  GtkWidget *scroll_method_radio_buttons[2];
  GtkWidget *track_current_node_named_vbox;
  int k;

  /* Let's be over-secure (a compile-time error would have been
   * better...)
   */
  assert (SHOW_GAME_TREE_ALWAYS		      == 0
	  && SHOW_GAME_TREE_AUTOMATICALLY     == 1
	  && SHOW_GAME_TREE_NEVER	      == 2
	  && TRACK_CURRENT_NODE_ALWAYS	      == 0
	  && TRACK_CURRENT_NODE_AUTOMATICALLY == 1
	  && TRACK_CURRENT_NODE_NEVER	      == 2);

  gtk_utils_create_radio_chain (radio_buttons, show_game_tree_radio_labels, 3);

  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (radio_buttons[game_tree_view.show_game_tree]), TRUE);

  for (k = 0; k < sizeof radio_buttons / sizeof (GtkWidget *); k++) {
    g_signal_connect (radio_buttons[k], "toggled",
		      G_CALLBACK (store_radio_button_setting),
		      &game_tree_view.show_game_tree);
  }

  label = gtk_utils_create_left_aligned_label (_(show_game_tree_hint));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  show_game_tree_named_vbox
    = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
			     QUARRY_SPACING_SMALL,
			     radio_buttons[0], GTK_UTILS_FILL,
			     radio_buttons[1], GTK_UTILS_FILL,
			     radio_buttons[2], GTK_UTILS_FILL,
			     label, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (show_game_tree_named_vbox),
				 _("Show Game Tree"));

  gtk_utils_create_radio_chain (radio_buttons,
				track_current_node_radio_labels, 3);

  gtk_toggle_button_set_active
    (GTK_TOGGLE_BUTTON (radio_buttons[game_tree_view.track_current_node]),
     TRUE);

  for (k = 0; k < sizeof radio_buttons / sizeof (GtkWidget *); k++) {
    g_signal_connect (radio_buttons[k], "toggled",
		      G_CALLBACK (store_radio_button_setting),
		      &game_tree_view.track_current_node);
  }

  gtk_utils_create_radio_chain (scroll_method_radio_buttons,
				scroll_method_radio_labels, 2);

  if (game_tree_view.center_on_current_node) {
    gtk_toggle_button_set_active
      (GTK_TOGGLE_BUTTON (scroll_method_radio_buttons[1]), TRUE);
  }

  g_signal_connect (scroll_method_radio_buttons[1], "toggled",
		    G_CALLBACK (store_toggle_setting),
		    &game_tree_view.center_on_current_node);

  gtk_utils_set_sensitive_on_toggle (GTK_TOGGLE_BUTTON (radio_buttons[2]),
				     scroll_method_radio_buttons[0], TRUE);
  gtk_utils_set_sensitive_on_toggle (GTK_TOGGLE_BUTTON (radio_buttons[2]),
				     scroll_method_radio_buttons[1], TRUE);

  label = gtk_utils_create_left_aligned_label (_(track_current_node_hint));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  track_current_node_named_vbox
    = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
			     QUARRY_SPACING_SMALL,
			     radio_buttons[0], GTK_UTILS_FILL,
			     radio_buttons[1], GTK_UTILS_FILL,
			     radio_buttons[2], GTK_UTILS_FILL,
			     label, GTK_UTILS_FILL | QUARRY_SPACING_SMALL,
			     scroll_method_radio_buttons[0], GTK_UTILS_FILL,
			     scroll_method_radio_buttons[1], GTK_UTILS_FILL,
			     NULL);
  gtk_named_vbox_set_label_text
    (GTK_NAMED_VBOX (track_current_node_named_vbox),
     _("Track Tree's Current Node"));

  return gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				show_game_tree_named_vbox, GTK_UTILS_FILL,
				track_current_node_named_vbox, GTK_UTILS_FILL,
				NULL);
}


static GtkWidget *
create_saving_sgf_page (void)
{
  static const gchar *radio_labels[2] = { N_("Always use UTF-8 (recommended)"),
					  N_("Preserve original encoding") };
  static const gchar *hint
    = N_("Note that many characters cannot be represented in non-UTF-8 "
	 "encodings and thus some information may be lost if you use them. "
	 "Quarry also works fastest with UTF-8; this may be important "
	 "if you have very large SGF files.");

  GtkWidget *radio_buttons[2];
  GtkWidget *label;
  GtkWidget *named_vbox;

  gtk_utils_create_radio_chain (radio_buttons, radio_labels, 2);
  if (!sgf_configuration.force_utf8)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_buttons[1]), TRUE);

  g_signal_connect (radio_buttons[0], "toggled",
		    G_CALLBACK (store_toggle_setting),
		    &sgf_configuration.force_utf8);

  label = gtk_utils_create_left_aligned_label (_(hint));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  named_vbox = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX,
				      QUARRY_SPACING_SMALL,
				      radio_buttons[0], GTK_UTILS_FILL,
				      radio_buttons[1], GTK_UTILS_FILL,
				      label, GTK_UTILS_FILL, NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (named_vbox), _("Encoding"));

  return named_vbox;
}


static GtkWidget *
create_go_board_appearance_page (void)
{
  static const gchar *labels[4] = {
    N_("_Relative to stone size"),
    N_("On _white stones:"),
    N_("On _black stones:"),
    N_("On _empty intersections:")
  };

  GtkWidget *background_table = create_background_table (GTK_GAME_GO, 3);
  GtkWidget *markup_box = create_markup_box (GTK_GAME_GO, labels);

  return create_board_appearance_notebook_page (background_table, markup_box);
}


static GtkWidget *
create_amazons_board_appearance_page (void)
{
  static const gchar *labels[4] = {
    N_("_Relative to amazon size"),
    N_("On _white amazons:"),
    N_("On _black amazons:"),
    N_("On _empty fields:")
  };

  GtkWidget *background_table = create_background_table (GTK_GAME_AMAZONS, 5);
  GtkTable *table = GTK_TABLE (background_table);
  GtkWidget *markup_box = create_markup_box (GTK_GAME_AMAZONS, labels);
  QuarryColor *quarry_color
    = &amazons_board_appearance.checkerboard_pattern_color;
  GdkColor color;
  GtkWidget *color_button;
  GtkWidget *label;
  double opacity = amazons_board_appearance.checkerboard_pattern_opacity;
  GtkWidget *scale;

  gtk_table_set_row_spacing (table, 3, QUARRY_SPACING);

  gtk_utils_set_gdk_color (&color, *quarry_color);
  color_button = gtk_color_button_new_with_color (&color);
  gtk_color_button_set_title (GTK_COLOR_BUTTON (color_button),
			      _("Pick Checkerboard Pattern Color"));

  g_signal_connect (color_button, "color-set",
		    G_CALLBACK (update_board_appearance),
		    &amazons_board_appearance.checkerboard_pattern_color);

  gtk_table_attach (table, color_button, 1, 2, 2, 3,
		    GTK_EXPAND | GTK_FILL, 0, 0, 0);

  label = gtk_utils_create_mnemonic_label (_("Checkerboard _pattern color:"),
					   color_button);
  gtk_table_attach (table, label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

  scale = gtk_hscale_new_with_range (0.0, 1.0, 0.05);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
  gtk_range_set_value (GTK_RANGE (scale), opacity);

  g_signal_connect (scale, "value-changed",
		    G_CALLBACK (update_board_appearance),
		    &amazons_board_appearance.checkerboard_pattern_opacity);

  gtk_table_attach (table, scale, 1, 2, 3, 4,
		    GTK_EXPAND | GTK_FILL, 0, 0, 0);

  label = gtk_utils_create_mnemonic_label (_("Checkerboard pattern _opacity:"),
					   scale);
  gtk_table_attach (table, label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);

  return create_board_appearance_notebook_page (background_table, markup_box);
}


static GtkWidget *
create_othello_board_appearance_page (void)
{
  static const gchar *labels[4] = {
    N_("_Relative to disk size"),
    N_("On _white disks:"),
    N_("On _black disks:"),
    N_("On _empty fields:")
  };

  GtkWidget *background_table = create_background_table (GTK_GAME_OTHELLO, 3);
  GtkWidget *markup_box = create_markup_box (GTK_GAME_OTHELLO, labels);

  return create_board_appearance_notebook_page (background_table, markup_box);
}


static GtkWidget *
create_background_table (GtkGameIndex game_index, gint num_table_rows)
{
  static const gchar *radio_button_labels[2] = { N_("Use _texture:"),
						 N_("Use _solid color:") };
  BoardAppearance *board_appearance
    = game_index_to_board_appearance_structure (game_index);
  GtkWidget *table_widget = gtk_table_new (num_table_rows, 3, FALSE);
  GtkTable *table = GTK_TABLE (table_widget);
  GdkColor color;
  GtkWidget *radio_buttons[2];
  GtkWidget *label;
  GtkWidget *file_selector;
  GtkWidget *button;
  GtkWidget *color_button;

  gtk_table_set_row_spacings (table, QUARRY_SPACING_SMALL);
  gtk_table_set_row_spacing (table, 1, QUARRY_SPACING);

  gtk_table_set_col_spacing (table, 0, QUARRY_SPACING);
  gtk_table_set_col_spacing (table, 1, QUARRY_SPACING_SMALL);

  gtk_utils_create_radio_chain (radio_buttons, radio_button_labels, 2);
  if (!board_appearance->use_background_texture)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_buttons[1]), TRUE);

  g_signal_connect (radio_buttons[0], "toggled",
		    G_CALLBACK (update_board_appearance),
		    &board_appearance->use_background_texture);

  gtk_table_attach (table, radio_buttons[0], 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
  gtk_table_attach (table, radio_buttons[1], 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

#ifdef GTK_TYPE_FILE_SELECTOR

  file_selector = gtk_file_selector_new ();
  gtk_file_selector_set_glob_patterns (GTK_FILE_SELECTOR (file_selector),
				       "*.jpg\0*.png\0");

  if (board_appearance->background_texture) {
    gtk_file_selector_set_text (GTK_FILE_SELECTOR (file_selector),
				board_appearance->background_texture);
  }

#else /* not defined GTK_TYPE_FILE_SELECTOR */

  file_selector = gtk_utils_create_entry (board_appearance->background_texture,
					  RETURN_DEFAULT_MODE);

#endif /* not defined GTK_TYPE_FILE_SELECTOR */

  /* We use the name `file_selector' for the entry only to have fewer
   * forks on whether GTK_TYPE_FILE_SELECTOR is defined.
   */
  gtk_utils_set_sensitive_on_toggle (GTK_TOGGLE_BUTTON (radio_buttons[0]),
				     file_selector, FALSE);

#ifdef GTK_TYPE_FILE_SELECTOR

  g_signal_connect (GTK_BIN (file_selector)->child, "focus-out-event",
		    G_CALLBACK (update_board_background_texture),
		    GINT_TO_POINTER (game_index));

  g_signal_connect (file_selector, "changed",
		    G_CALLBACK (board_background_texture_changed),
		    GINT_TO_POINTER (game_index));

#else /* not defined GTK_TYPE_FILE_SELECTOR */

  g_signal_connect (file_selector, "focus-out-event",
		    G_CALLBACK (update_board_background_texture),
		    GINT_TO_POINTER (game_index));

#endif /* not defined GTK_TYPE_FILE_SELECTOR */

  gtk_table_attach (table, file_selector,
		    1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  button = gtk_utils_create_browse_button (FALSE, file_selector, FALSE,
					   _("Choose a Background Texture..."),
					   update_board_background_texture,
					   GINT_TO_POINTER (game_index));
  gtk_utils_set_sensitive_on_toggle (GTK_TOGGLE_BUTTON (radio_buttons[0]),
				     button, FALSE);

  gtk_table_attach (table, button, 2, 3, 0, 1, GTK_FILL, 0, GTK_FILL, 0);

  gtk_utils_set_gdk_color (&color, board_appearance->background_color);
  color_button = gtk_color_button_new_with_color (&color);
  gtk_color_button_set_title (GTK_COLOR_BUTTON (color_button),
			      _("Pick Background Color"));
  gtk_utils_set_sensitive_on_toggle (GTK_TOGGLE_BUTTON (radio_buttons[1]),
				     color_button, FALSE);

  g_signal_connect (color_button, "color-set",
		    G_CALLBACK (update_board_appearance),
		    &board_appearance->background_color);

  gtk_table_attach (table, color_button, 1, 2, 1, 2,
		    GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gtk_utils_set_gdk_color (&color, board_appearance->grid_and_labels_color);
  color_button = gtk_color_button_new_with_color (&color);
  gtk_color_button_set_title (GTK_COLOR_BUTTON (color_button),
			      _("Pick Color for Grid and Labels"));

  g_signal_connect (color_button, "color-set",
		    G_CALLBACK (update_board_appearance),
		    &board_appearance->grid_and_labels_color);

  gtk_table_attach (table, color_button,
		    1, 2, num_table_rows - 1, num_table_rows,
		    GTK_EXPAND | GTK_FILL, 0, 0, 0);

  label = gtk_utils_create_mnemonic_label (_("_Grid and labels color:"),
					   color_button);
  gtk_table_attach (table, label, 0, 1, num_table_rows - 1, num_table_rows,
		    GTK_FILL, 0, 0, 0);

  return table_widget;
}


static GtkWidget *
create_markup_box (GtkGameIndex game_index, const gchar *labels[4])
{
  UseThemeDefaultsData *use_theme_defaults_data
    = g_malloc (sizeof (UseThemeDefaultsData));
  BoardAppearance *board_appearance
    = game_index_to_board_appearance_structure (game_index);
  GtkWidget *selector;
  GtkWidget *theme_label;
  GtkWidget *theme_hbox;
  GtkWidget *use_theme_defaults_check_button;
  GtkWidget *scale;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *check_button;
  GtkWidget *vbox;
  GtkWidget *size_and_opacity_named_vbox;
  GtkWidget *color_named_vbox;
  GtkSizeGroup *size_group;
  int k;

  use_theme_defaults_data->game = index_to_game[game_index];

  selector = gtk_utils_create_selector_from_string_list (&markup_themes,
							 (board_appearance
							  ->markup_theme));
  g_signal_connect (selector, "changed",
		    G_CALLBACK (update_board_markup_theme),
		    use_theme_defaults_data);

  theme_label = gtk_utils_create_mnemonic_label (_("_Theme:"), selector);

  theme_hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, 0,
				      theme_label, 0,
				      gtk_label_new ("    "), 0,
				      selector,
				      GTK_UTILS_FILL | QUARRY_SPACING,
				      NULL);

  use_theme_defaults_check_button
    = gtk_check_button_new_with_mnemonic (_("Use theme _defaults"));

  g_signal_connect (use_theme_defaults_check_button, "toggled",
		    G_CALLBACK (update_markup_theme_defaults_usage),
		    use_theme_defaults_data);
  g_signal_connect_swapped (use_theme_defaults_check_button, "destroy",
			    G_CALLBACK (g_free), use_theme_defaults_data);

  scale = gtk_hscale_new_with_range (0.2, 1.0, 0.05);
  use_theme_defaults_data->size_range = GTK_RANGE (scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);

  g_signal_connect (scale, "value-changed",
		    G_CALLBACK (update_board_appearance),
		    &board_appearance->markup_size);

  label = gtk_utils_create_mnemonic_label (_("_Size:"), scale);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, scale, GTK_UTILS_PACK_DEFAULT, NULL);

  check_button = gtk_check_button_new_with_mnemonic (_(labels[0]));
  use_theme_defaults_data->size_is_relative_toggle_button
    = GTK_TOGGLE_BUTTON (check_button);

  vbox = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_SMALL,
				hbox, GTK_UTILS_FILL,
				check_button, GTK_UTILS_FILL, NULL);

  g_signal_connect (check_button, "toggled",
		    G_CALLBACK (update_board_appearance),
		    &board_appearance->markup_size_is_relative);

  scale = gtk_hscale_new_with_range (0.2, 1.0, 0.05);
  use_theme_defaults_data->opacity_range = GTK_RANGE (scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_range_set_update_policy (use_theme_defaults_data->opacity_range,
			       GTK_UPDATE_DELAYED);

  g_signal_connect (scale, "value-changed",
		    G_CALLBACK (update_board_appearance),
		    &board_appearance->markup_opacity);

  label = gtk_utils_create_mnemonic_label (_("_Opacity:"), scale);

  hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				label, 0, scale, GTK_UTILS_PACK_DEFAULT, NULL);

  use_theme_defaults_data->vbox
    = gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING,
			     vbox, GTK_UTILS_FILL, hbox, GTK_UTILS_FILL, NULL);

  size_and_opacity_named_vbox
    = gtk_utils_pack_in_box (GTK_TYPE_NAMED_VBOX, QUARRY_SPACING,
			     use_theme_defaults_check_button, GTK_UTILS_FILL,
			     use_theme_defaults_data->vbox, GTK_UTILS_FILL,
			     NULL);
  gtk_named_vbox_set_label_text (GTK_NAMED_VBOX (size_and_opacity_named_vbox),
				 _("Size & Opacity"));

  size_group = (gtk_utils_align_left_widgets
		(GTK_CONTAINER (size_and_opacity_named_vbox), NULL));
  gtk_size_group_add_widget (size_group, theme_label);

  if (board_appearance->use_theme_defaults) {
    gtk_toggle_button_set_active
      (GTK_TOGGLE_BUTTON (use_theme_defaults_check_button), TRUE);
  }
  else {
    update_markup_theme_defaults_usage
      (GTK_TOGGLE_BUTTON (use_theme_defaults_check_button),
       use_theme_defaults_data);
  }

  color_named_vbox = gtk_named_vbox_new (_("Color"), FALSE,
					 QUARRY_SPACING_SMALL);

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  for (k = 0; k < NUM_SGF_MARKUP_BACKGROUNDS; k++) {
    GdkColor color;
    GtkWidget *color_button;
    int color_index = (NUM_SGF_MARKUP_BACKGROUNDS - 1) - k;

    gtk_utils_set_gdk_color (&color,
			     board_appearance->markup_colors[color_index]);
    color_button = gtk_color_button_new_with_color (&color);
    gtk_color_button_set_title (GTK_COLOR_BUTTON (color_button),
				_("Pick Color for Markup"));

    g_signal_connect (color_button, "color-set",
		      G_CALLBACK (update_board_appearance),
		      &board_appearance->markup_colors[color_index]);

    label = gtk_utils_create_mnemonic_label (_(labels[1 + k]), color_button);

    hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING,
				  label, 0,
				  color_button, GTK_UTILS_PACK_DEFAULT, NULL);
    gtk_box_pack_start_defaults (GTK_BOX (color_named_vbox), hbox);

    gtk_size_group_add_widget (size_group, label);
  }

  return gtk_utils_pack_in_box (GTK_TYPE_VBOX, QUARRY_SPACING_BIG,
				theme_hbox, GTK_UTILS_FILL,
				size_and_opacity_named_vbox, GTK_UTILS_FILL,
				color_named_vbox, GTK_UTILS_FILL, NULL);
}


static GtkWidget *
create_board_appearance_notebook_page (GtkWidget *background_widget,
				       GtkWidget *markup_widget)
{
  GtkWidget *notebook_widget = gtk_notebook_new ();
  GtkNotebook *notebook = GTK_NOTEBOOK (notebook_widget);

  gtk_container_set_border_width (GTK_CONTAINER (background_widget),
				  QUARRY_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (markup_widget),
				  QUARRY_SPACING);

  gtk_notebook_append_page (notebook, background_widget,
			    gtk_label_new (_("Background")));
  gtk_notebook_append_page (notebook, markup_widget,
			    gtk_label_new (_("Markup")));

  return notebook_widget;
}


static void
gtk_preferences_dialog_change_page (GtkTreeSelection *selection,
				    GtkNotebook *notebook)
{
  GtkTreeIter iterator;
  GtkTreeModel *categories_tree_model;

  if (gtk_tree_selection_get_selected (selection, &categories_tree_model,
				       &iterator)) {
    gint page_index;

    gtk_tree_model_get (categories_tree_model, &iterator,
			CATEGORIES_PAGE_INDEX, &page_index, -1);
    if (page_index != -1) {
      last_selected_page = page_index;
      gtk_notebook_set_current_page (notebook, last_selected_page);
    }
  }
}


static void
gtk_preferences_dialog_response (GtkWindow *window, gint response_id)
{
  assert (window == preferences_dialog);

  if (response_id == GTK_RESPONSE_HELP) {
    switch (last_selected_page) {
    case PREFERENCES_PAGE_GTP_ENGINES:
      gtk_help_display ("preferences-gtp-engines");
      break;

    case PREFERENCES_PAGE_GAME_TREE:
      gtk_help_display ("preferences-game-tree");
      break;

    case PREFERENCES_PAGE_SGF_SAVING:
      gtk_help_display ("preferences-saving-game-records");
      break;

    case PREFERENCES_PAGE_GO_BOARD_APPEARANCE:
      gtk_help_display ("preferences-go-board-appearance");
      break;

    case PREFERENCES_PAGE_AMAZONS_BOARD_APPEARANCE:
      gtk_help_display ("preferences-amazons-board-appearance");
      break;

    case PREFERENCES_PAGE_OTHELLO_BOARD_APPEARANCE:
      gtk_help_display ("preferences-othello-board-appearance");
      break;

    default:
      assert (0);
    }
  }
  else
    gtk_widget_destroy (GTK_WIDGET (preferences_dialog));
}


static void
gtk_preferences_dialog_update_gtp_engine_info (GtkTreeSelection *selection)
{
  GtkTreeModel *gtp_engines_tree_model;
  GtkTreeIter iterator;
  GtpEngineListItem *engine_data = NULL;
  char *supported_games = NULL;

  if (gtk_tree_selection_get_selected (selection, &gtp_engines_tree_model,
				       &iterator)) {
    gtk_tree_model_get (gtp_engines_tree_model, &iterator,
			ENGINES_DATA, &engine_data, -1);
    supported_games = string_list_implode (&engine_data->supported_games,
					   ", ");
  }

  gtk_widget_set_sensitive (modify_gtp_engine, engine_data != NULL);
  gtk_widget_set_sensitive (remove_gtp_engine,
			    (engine_data != NULL
			     && engine_data->site_configuration_name == NULL));

  gtk_widget_set_sensitive (move_gtp_engine_up,
			    (engine_data != NULL
			     && engine_data != gtp_engines.first));
  gtk_widget_set_sensitive (move_gtp_engine_down,
			    (engine_data != NULL
			     && engine_data != gtp_engines.last));

  gtk_widget_set_sensitive (gtp_engine_info, engine_data != NULL);

  gtk_label_set_text (gtp_engine_name,
		      (engine_data ? engine_data->name : NULL));
  gtk_label_set_text (gtp_engine_version,
		      (engine_data ? engine_data->version : NULL));
  gtk_label_set_text (gtp_engine_supported_games, supported_games);
  gtk_label_set_text (gtp_engine_command_line,
		      (engine_data ? engine_data->command_line : NULL));
  gtk_label_set_text (gtp_engine_additional_info,
		      (engine_data && engine_data->site_configuration_name
		       ? _("This engine comes from site configuration.")
		       : NULL));

  utils_free (supported_games);
}


static void
show_or_hide_gtp_engine (GtkCellRendererToggle *cell_renderer,
			 gchar *path_string, gpointer user_data)
{
  GtkTreeModel *gtp_engines_tree_model
    = GTK_TREE_MODEL (gtp_engines_list_store);
  GtkTreePath *tree_path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iterator;
  GtpEngineListItem *engine_data;
  
#if !GTK_2_4_OR_LATER
  gboolean had_non_hidden_engine
    = gtk_preferences_have_non_hidden_gtp_engine ();
#endif

  UNUSED (cell_renderer);
  UNUSED (user_data);

  gtk_tree_model_get_iter (gtp_engines_tree_model, &iterator, tree_path);
  gtk_tree_path_free (tree_path);

  gtk_tree_model_get (gtp_engines_tree_model, &iterator,
		      ENGINES_DATA, &engine_data, -1);

  prepare_to_rebuild_menus ();
  engine_data->is_hidden = !engine_data->is_hidden;
  rebuild_all_menus ();

  gtk_list_store_set (gtp_engines_list_store, &iterator,
		      ENGINES_IS_VISIBLE, !engine_data->is_hidden, -1);

#if !GTK_2_4_OR_LATER

  if (gtk_preferences_have_non_hidden_gtp_engine () != had_non_hidden_engine) {
    GSList *item;

    for (item = gtp_engine_selectors; item; item = item->next) {
      GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

      data->callback (data->selector, data->user_data);
    }
  }

#endif
}


static void
do_remove_gtp_engine (void)
{
  GtkTreeModel *gtp_engines_tree_model;
  GtkTreeIter iterator;
  GtpEngineListItem *engine_data;

#if !GTK_2_4_OR_LATER
  gboolean deleting_last_item;
#endif

  GSList *item;
  GtkTreePath *tree_path;

  assert (gtk_tree_selection_get_selected (gtp_engines_tree_selection,
					   &gtp_engines_tree_model,
					   &iterator));
  gtk_tree_model_get (gtp_engines_tree_model, &iterator,
		      ENGINES_DATA, &engine_data, -1);

  item = find_gtp_engine_dialog_by_engine_data (engine_data);
#if !GTK_2_4_OR_LATER
  deleting_last_item = (engine_data->next == NULL);
#endif

  if (item) {
    GtkEngineDialogData *data = (GtkEngineDialogData *) (item->data);

    if (data->progress_dialog) {
      data->engine_deleted = TRUE;
      gtk_schedule_gtp_client_deletion (data->client);
    }

    gtk_widget_destroy (GTK_WIDGET (data->window));
  }

  prepare_to_rebuild_menus ();
  string_list_delete_item (&gtp_engines, engine_data);
  rebuild_all_menus ();

#if !GTK_2_4_OR_LATER

  if (!gtk_preferences_have_non_hidden_gtp_engine ()) {
    for (item = gtp_engine_selectors; item; item = item->next) {
      GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

      data->callback (data->selector, data->user_data);
    }
  }

#endif

  tree_path = gtk_tree_model_get_path (gtp_engines_tree_model, &iterator);

#if GTK_2_4_OR_LATER

  if (!gtk_list_store_remove (gtp_engines_list_store, &iterator))
    gtk_tree_path_prev (tree_path);

#else

  gtk_list_store_remove (gtp_engines_list_store, &iterator);
  if (deleting_last_item)
    gtk_tree_path_prev (tree_path);

#endif

  gtk_tree_view_set_cursor (gtp_engines_tree_view, tree_path, NULL, FALSE);
  gtk_tree_path_free (tree_path);
}


static void
do_move_gtp_engine (gpointer move_upwards)
{
  GtkTreeModel *gtp_engines_tree_model;
  GtkTreeIter first_iterator;
  GtkTreeIter second_iterator;
  GtpEngineListItem *first_engine_data;
  GtpEngineListItem *second_engine_data;
  GtkTreePath *tree_path;

  assert (gtk_tree_selection_get_selected (gtp_engines_tree_selection,
					   &gtp_engines_tree_model,
					   &first_iterator));
  gtk_tree_model_get (gtp_engines_tree_model, &first_iterator,
		      ENGINES_DATA, &first_engine_data, -1);

  tree_path = gtk_tree_model_get_path (gtp_engines_tree_model,
				       &first_iterator);

  prepare_to_rebuild_menus ();

  if (GPOINTER_TO_INT (move_upwards)) {
    string_list_swap_with_previous (&gtp_engines, first_engine_data);
    second_engine_data = first_engine_data->next;

    gtk_tree_path_prev (tree_path);
  }
  else {
    second_engine_data = first_engine_data->next;
    string_list_swap_with_next (&gtp_engines, first_engine_data);

    gtk_tree_path_next (tree_path);
  }

  rebuild_all_menus ();

  gtk_tree_model_get_iter (gtp_engines_tree_model, &second_iterator,
			   tree_path);

#if GTK_2_2_OR_LATER

  gtk_tree_path_free (tree_path);

  gtk_list_store_swap (gtp_engines_list_store,
		       &first_iterator, &second_iterator);
  gtk_preferences_dialog_update_gtp_engine_info (gtp_engines_tree_selection);

#else /* not GTK_2_2_OR_LATER */

  gtk_utils_block_signal_handlers (gtp_engines_list_store,
				   handle_drag_and_drop);

  gtk_list_store_set (gtp_engines_list_store, &first_iterator,
		      ENGINES_DATA, second_engine_data,
		      ENGINES_IS_VISIBLE, !second_engine_data->is_hidden,
		      ENGINES_NAME, second_engine_data->screen_name, -1);
  gtk_list_store_set (gtp_engines_list_store, &second_iterator,
		      ENGINES_DATA, first_engine_data,
		      ENGINES_IS_VISIBLE, !first_engine_data->is_hidden, 
		      ENGINES_NAME, first_engine_data->screen_name, -1);

  gtk_utils_unblock_signal_handlers (gtp_engines_list_store,
				     handle_drag_and_drop);

  gtk_tree_view_set_cursor (gtp_engines_tree_view, tree_path, NULL, FALSE);
  gtk_tree_path_free (tree_path);

#endif /* not GTK_2_2_OR_LATER */
}


static GSList *
find_gtp_engine_dialog_by_engine_data (GtpEngineListItem *engine_data)
{
  GSList *item;

  for (item = gtp_engine_dialogs; item; item = item->next) {
    if (((GtkEngineDialogData *) (item->data))->engine_data == engine_data)
      break;
  }

  return item;
}


static void
gtk_gtp_engine_dialog_present (gpointer new_engine)
{
  static const gchar *hint_text
    = N_("You can use `%n' and `%v' strings in <i>Screen name</i> field. "
	 "They will substituted with name and version of the engine "
	 "correspondingly. By default, `%n %v' is used.");

  GtpEngineListItem *engine_data = NULL;
  GtkEngineDialogData *data;
  GtkTreeModel *gtp_engines_tree_model
    = GTK_TREE_MODEL (gtp_engines_list_store);
  GtkTreeIter iterator;
  GSList *item;

  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *table_widget;
  GtkTable *table;
  GtkWidget *entry;
  GtkWidget *label;

  if (!GPOINTER_TO_INT (new_engine)) {
    assert (gtk_tree_selection_get_selected (gtp_engines_tree_selection,
					     NULL, &iterator));
    gtk_tree_model_get (gtp_engines_tree_model, &iterator,
			ENGINES_DATA, &engine_data, -1);
  }

  item = find_gtp_engine_dialog_by_engine_data (engine_data);
  if (item) {
    data = (GtkEngineDialogData *) (item->data);

    gtk_window_present (data->window);
    if (data->progress_dialog)
      gtk_window_present (GTK_WINDOW (data->progress_dialog));

    return;
  }

  data = g_malloc (sizeof (GtkEngineDialogData));
  data->engine_data	= engine_data;
  data->engine_deleted	= FALSE;
  data->progress_dialog = NULL;

  gtp_engine_dialogs = g_slist_prepend (gtp_engine_dialogs, data);

  dialog = gtk_dialog_new_with_buttons ((engine_data
					 ? _("Modify GTP Engine Information")
					 : _("New GTP Engine")),
					NULL, 0,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					NULL);
  data->window = GTK_WINDOW (dialog);
  gtk_control_center_window_created (data->window);

  gtk_utils_make_window_only_horizontally_resizable (data->window);

  button = gtk_dialog_add_button (GTK_DIALOG (dialog),
				  (engine_data ? GTK_STOCK_OK : GTK_STOCK_ADD),
				  GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (gtk_gtp_engine_dialog_destroy), data);
  g_signal_connect (dialog, "response",
		    G_CALLBACK (gtk_gtp_engine_dialog_response), data);

  table_widget = gtk_table_new (3, 3, FALSE);
  table = GTK_TABLE (table_widget);
  gtk_table_set_row_spacing (table, 0, QUARRY_SPACING);
  gtk_table_set_row_spacing (table, 1, QUARRY_SPACING_SMALL);
  gtk_table_set_col_spacing (table, 0, QUARRY_SPACING);
  gtk_table_set_col_spacing (table, 1, QUARRY_SPACING_SMALL);

#if GTK_2_4_OR_LATER

  entry = gtk_combo_box_entry_new_with_model (gtp_engines_tree_model,
					      ENGINES_COMMAND_LINE);
  if (engine_data)
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (entry), &iterator);

  data->command_line_entry = GTK_ENTRY (GTK_BIN (entry)->child);
  gtk_entry_set_activates_default (data->command_line_entry, TRUE);

#else
  entry = gtk_utils_create_entry ((engine_data
				   ? engine_data->command_line : NULL),
				  RETURN_ACTIVATES_DEFAULT);
  data->command_line_entry = GTK_ENTRY (entry);
#endif

  gtk_utils_set_sensitive_on_input (data->command_line_entry, button);
  gtk_table_attach (table, entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  label = gtk_utils_create_mnemonic_label (_("Command _line:"), entry);
  gtk_table_attach (table, label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

  button
    = gtk_utils_create_browse_button (TRUE,
				      GTK_WIDGET (data->command_line_entry),
				      TRUE, _("Choose GTP Engine..."),
				      NULL, NULL);
  gtk_table_attach (table, button, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);

  entry
    = gtk_utils_create_entry ((engine_data && engine_data->screen_name_format
			       ? engine_data->screen_name_format : "%n %v"),
			      RETURN_ACTIVATES_DEFAULT);
  data->name_entry = GTK_ENTRY (entry);
  gtk_table_attach (table, entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  label = gtk_utils_create_mnemonic_label (_("Screen _name:"), entry);
  gtk_table_attach (table, label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

  label = gtk_utils_create_left_aligned_label (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_markup (GTK_LABEL (label), _(hint_text));
  gtk_table_attach (table, label, 0, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  gtk_utils_standardize_dialog (GTK_DIALOG (dialog), table_widget);

  gtk_widget_show_all (dialog);
}


static void
gtk_gtp_engine_dialog_destroy (GtkWindow *window, GtkEngineDialogData *data)
{
  if (gtk_control_center_window_destroyed (window)) {
    GSList *item = find_gtp_engine_dialog_by_engine_data (data->engine_data);

    assert (item);
    gtp_engine_dialogs = g_slist_delete_link (gtp_engine_dialogs, item);
    g_free (data);
  }
}


static void
gtk_gtp_engine_dialog_response (GtkWindow *window, gint response_id,
				GtkEngineDialogData *data)
{
  if (response_id == GTK_RESPONSE_OK) {
    const gchar *command_line = gtk_entry_get_text (data->command_line_entry);

    if (!data->engine_data
	|| !data->engine_data->command_line
	|| strcmp (data->engine_data->command_line, command_line) != 0) {
      GError *error = NULL;

      data->client = gtk_create_gtp_client (command_line,
					    client_initialized, client_deleted,
					    data, &error);
      if (data->client) {
	data->progress_dialog
	  = gtk_progress_dialog_new (window, "Quarry",
				     _("Querying engine's "
				       "name, version and known commands..."),
				     show_progress_dialog,
				     ((GtkProgressDialogCallback)
				      cancel_engine_query),
				     data);
	/* FIXME: I'd like this to display
	 *	  `preferences-gtp-engine-information-dialog-freeze'.
	 *	  Do newer versions of `yelp' still have this bug?
	 */
	gtk_progress_dialog_set_help_link_id
	  (GTK_PROGRESS_DIALOG (data->progress_dialog),
	   "preferences-gtp-engine-information-dialog");
	g_object_ref (data->progress_dialog);

	gtp_client_setup_connection (data->client);
      }
      else {
	gtk_utils_create_message_dialog (window, GTK_STOCK_DIALOG_ERROR,
					 (GTK_UTILS_BUTTONS_OK
					  | GTK_UTILS_DESTROY_ON_RESPONSE),
					 _("Please make sure you typed "
					   "engine's filename correctly and "
					   "that you have permission to "
					   "execute it."),
					 error->message);
	g_error_free (error);

	gtk_widget_grab_focus (GTK_WIDGET (data->command_line_entry));
      }
    }
    else {
      const gchar *name = gtk_entry_get_text (data->name_entry);

      if (! *name)
	name = "%n %v";

      configuration_set_string_value (&data->engine_data->screen_name_format,
				      name);
      update_engine_screen_name (data->engine_data, TRUE);

      gtk_widget_destroy (GTK_WIDGET (window));
    }
  }
  else if (response_id == GTK_RESPONSE_CANCEL)
    gtk_widget_destroy (GTK_WIDGET (window));
}


static void
client_initialized (GtpClient *client, void *user_data)
{
  GtkWidget *progress_dialog
    = ((GtkEngineDialogData *) user_data)->progress_dialog;

  gtp_client_quit (client);
  gtk_label_set_text (GTK_PROGRESS_DIALOG (progress_dialog)->label,
		      _("Waiting for the engine to quit..."));
}


static void
client_deleted (GtpClient *client, GError *shutdown_reason, void *user_data)
{
  GtkEngineDialogData *data = (GtkEngineDialogData *) user_data;
  GtkWidget *progress_dialog = data->progress_dialog;

  if (!data->engine_deleted) {
    if (client->operation_stage == GTP_CLIENT_QUIT) {
      static const ConfigurationSection *gtp_engines_section
	= &gtk_configuration_sections[SECTION_GTP_ENGINES];

      GtpEngineListItem *engine_data = data->engine_data;
      const gchar *command_line;
      const gchar *name;
      char *screen_name;
      GtkTreeIter iterator;
      GtkTreePath *tree_path;
      GSList *item;

      command_line = gtk_entry_get_text (data->command_line_entry);
      name = gtk_entry_get_text (data->name_entry);
      if (! *name)
	name = "%n %v";

      screen_name = utils_special_printf (name,
					  'n', client->engine_name,
					  'v', client->engine_version, 0);

      if (engine_data) {
	utils_free (engine_data->screen_name);
	engine_data->screen_name = screen_name;
      }
      else {
	string_list_add_ready (&gtp_engines, screen_name);
	configuration_init_repeatable_section (gtp_engines_section,
					       gtp_engines.last);

	engine_data = gtp_engines.last;
      }

      configuration_set_string_value (&engine_data->screen_name_format, name);
      configuration_set_string_value (&engine_data->name, client->engine_name);
      configuration_set_string_value (&engine_data->version,
				      client->engine_version);
      configuration_set_string_list_value_steal_strings
	(&engine_data->supported_games, &client->supported_games);
      configuration_set_string_value (&engine_data->command_line,
				      command_line);

      if (data->engine_data)
	find_gtp_tree_model_iterator_by_engines_data (engine_data, &iterator);
      else {
	gtk_utils_block_signal_handlers (gtp_engines_list_store,
					 handle_drag_and_drop);
	gtk_list_store_append (gtp_engines_list_store, &iterator);
      }

      gtk_list_store_set (gtp_engines_list_store, &iterator,
			  ENGINES_DATA, engine_data,
			  ENGINES_IS_VISIBLE, !engine_data->is_hidden,
			  ENGINES_NAME, screen_name,
#if GTK_2_4_OR_LATER
			  ENGINES_COMMAND_LINE, engine_data->command_line,
#endif
			  -1);

      prepare_to_rebuild_menus ();
      rebuild_all_menus ();

      if (!data->engine_data) {
	gtk_utils_unblock_signal_handlers (gtp_engines_list_store,
					   handle_drag_and_drop);

	if (string_list_is_single_string (&gtp_engines)) {
	  for (item = gtp_engine_selectors; item; item = item->next) {
	    GtkEngineSelectorData *data = ((GtkEngineSelectorData *)
					   (item->data));

	    data->callback (data->selector, data->user_data);

#if GTK_2_4_OR_LATER
	    gtk_combo_box_set_active (GTK_COMBO_BOX (data->selector), 0);
#endif
	  }
	}
      }

      tree_path
	= gtk_tree_model_get_path (GTK_TREE_MODEL (gtp_engines_list_store),
				   &iterator);
      gtk_tree_view_set_cursor (gtp_engines_tree_view, tree_path, NULL, FALSE);
      gtk_tree_path_free (tree_path);

      gtk_preferences_dialog_update_gtp_engine_info
	(gtp_engines_tree_selection);

      gtk_widget_destroy (GTK_WIDGET (data->window));
    }
    else {
      static const gchar *hint
	= N_("The engine might have crashed, quit prematurely or "
	     "disconnected. Please verify command line, including options, "
	     "and consult engine's documentation if needed.");

      gtk_progress_dialog_recover_parent
	(GTK_PROGRESS_DIALOG (progress_dialog));
      data->progress_dialog = NULL;

      gtk_widget_grab_focus (GTK_WIDGET (data->command_line_entry));

      if (shutdown_reason) {
	gtk_utils_create_message_dialog (data->window, GTK_STOCK_DIALOG_ERROR,
					 (GTK_UTILS_BUTTONS_OK
					  | GTK_UTILS_DESTROY_ON_RESPONSE),
					 _(hint),
					 _("Lost connection to GTP Engine "
					   "(%s)."),
					 shutdown_reason->message);
      }
    }
  }

  gtk_widget_destroy (progress_dialog);
  g_object_unref (progress_dialog);
}


static gboolean
show_progress_dialog (GtkProgressDialog *progress_dialog, gpointer user_data)
{
  UNUSED (user_data);

  gtk_widget_show (GTK_WIDGET (progress_dialog));
  return FALSE;
}


static gboolean
cancel_engine_query (GtkProgressDialog *progress_dialog,
		     GtkEngineDialogData *data)
{
  UNUSED (progress_dialog);

  gtk_schedule_gtp_client_deletion (data->client);

  return TRUE;
}



static void
store_toggle_setting (GtkToggleButton *toggle_button, int *value_storage)
{
  *value_storage = gtk_toggle_button_get_active (toggle_button);
}


static void
store_radio_button_setting (GtkRadioButton *radio_button, int *value_storage)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_button))) {
    GSList *radio_button_group = gtk_radio_button_get_group (radio_button);

    *value_storage = ((g_slist_length (radio_button_group) - 1)
		      - g_slist_index (radio_button_group, radio_button));
  }
}


static gboolean
update_board_background_texture (GtkEntry *entry, GdkEventFocus *event,
				 gpointer game_index)
{
  BoardAppearance *board_appearance
    = game_index_to_board_appearance_structure (GPOINTER_TO_INT (game_index));
  const gchar *new_texture = gtk_entry_get_text (entry);

  UNUSED (event);

  if (!board_appearance->background_texture
      || strcmp (new_texture, board_appearance->background_texture) != 0) {
    configuration_set_string_value (&board_appearance->background_texture,
				    new_texture);

    gtk_goban_base_update_appearance
      (index_to_game[GPOINTER_TO_INT (game_index)]);
  }

  return FALSE;
}


#ifdef GTK_TYPE_FILE_SELECTOR

static void
board_background_texture_changed (GtkFileSelector *file_selector,
				  gpointer game_index)
{
  /* This signal is also emitted when the entry is changed, in which
   * case there is no active item.  Updating background texture after
   * a symbol is typed is wrong.
   */
  if (gtk_combo_box_get_active (GTK_COMBO_BOX (file_selector)) != -1) {
    GtkEntry *entry = GTK_ENTRY (GTK_BIN (file_selector)->child);

    update_board_background_texture (entry, NULL, game_index);
  }
}

#endif /* defined GTK_TYPE_FILE_SELECTOR */


static void
update_board_appearance (GtkWidget *widget, gpointer value_storage)
{
  Game game;

  if ((gpointer) &go_board_appearance <= value_storage
      && value_storage < (gpointer) (&go_board_appearance + 1))
    game = GAME_GO;
  else if ((gpointer) &amazons_board_appearance <= value_storage
	   && value_storage < ((gpointer) (&amazons_board_appearance + 1)))
    game = GAME_AMAZONS;
  else if ((gpointer) &othello_board_appearance <= value_storage
	   && value_storage < (gpointer) (&othello_board_appearance + 1))
    game = GAME_OTHELLO;
  else
    assert (0);

  if (GTK_IS_RANGE (widget))
    * (double *) value_storage = gtk_range_get_value (GTK_RANGE (widget));
  else if (GTK_IS_TOGGLE_BUTTON (widget)) {
    * (int *) value_storage
      = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  }
  else if (GTK_IS_COLOR_BUTTON (widget)) {
    GdkColor gdk_color;
    QuarryColor quarry_color;

    gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &gdk_color);
    gtk_utils_set_quarry_color (&quarry_color, &gdk_color);

    if (QUARRY_COLORS_ARE_EQUAL (quarry_color,
				 * (QuarryColor *) value_storage))
      return;

    * (QuarryColor *) value_storage = quarry_color;
  }
  else
    assert (0);

  gtk_goban_base_update_appearance (game);
}


static void
update_board_markup_theme (GtkWidget *widget, UseThemeDefaultsData *data)
{
  BoardAppearance *board_appearance
    = game_to_board_appearance_structure (data->game);
  gint selected_theme_index
    = gtk_utils_get_selector_active_item_index (widget);
  MarkupThemeListItem *selected_theme
    = markup_theme_list_get_item (&markup_themes, selected_theme_index);

  if (strcmp (board_appearance->markup_theme, selected_theme->name) != 0) {
    configuration_set_string_value (&board_appearance->markup_theme,
				    selected_theme->name);

    /* Call it, because it blocks signal handlers. */
    update_markup_theme_defaults_usage (NULL, data);
  }
}


static void
update_markup_theme_defaults_usage (GtkToggleButton *toggle_button,
				    UseThemeDefaultsData *data)
{
  BoardAppearance *board_appearance
    = game_to_board_appearance_structure (data->game);
  gboolean use_theme_defaults;
  gdouble markup_size;
  gboolean markup_size_is_relative;
  gdouble markup_opacity;

  if (toggle_button) {
     use_theme_defaults = gtk_toggle_button_get_active (toggle_button);
     board_appearance->use_theme_defaults = use_theme_defaults;

     gtk_widget_set_sensitive (data->vbox, !use_theme_defaults);
  }
  else
    use_theme_defaults = board_appearance->use_theme_defaults;

  if (toggle_button || use_theme_defaults) {
    gtk_utils_block_signal_handlers (data->size_range,
				     update_board_appearance);
    gtk_utils_block_signal_handlers (data->size_is_relative_toggle_button,
				     update_board_appearance);
    gtk_utils_block_signal_handlers (data->opacity_range,
				     update_board_appearance);

    if (use_theme_defaults) {
      MarkupThemeListItem *current_theme
	= markup_theme_list_find (&markup_themes,
				  board_appearance->markup_theme);

      markup_size	      = current_theme->default_size;
      markup_size_is_relative = current_theme->size_is_relative;
      markup_opacity	      = current_theme->default_opacity;
    }
    else {
      markup_size	      = board_appearance->markup_size;
      markup_size_is_relative = board_appearance->markup_size_is_relative;
      markup_opacity	      = board_appearance->markup_opacity;
    }

    gtk_range_set_value (data->size_range, markup_size);
    gtk_toggle_button_set_active (data->size_is_relative_toggle_button,
				  markup_size_is_relative);
    gtk_range_set_value (data->opacity_range, markup_opacity);

    gtk_utils_unblock_signal_handlers (data->size_range,
				       update_board_appearance);
    gtk_utils_unblock_signal_handlers (data->size_is_relative_toggle_button,
				       update_board_appearance);
    gtk_utils_unblock_signal_handlers (data->opacity_range,
				       update_board_appearance);
  }

  gtk_goban_base_update_appearance (data->game);
}


BoardAppearance *
game_to_board_appearance_structure (Game game)
{
  if (game == GAME_GO)
    return &go_board_appearance;

  if (game == GAME_AMAZONS)
    return &amazons_board_appearance.board_appearance;

  if (game == GAME_OTHELLO)
    return &othello_board_appearance;

  assert (0);
}


static inline BoardAppearance *
game_index_to_board_appearance_structure (GtkGameIndex game_index)
{
  if (game_index == GTK_GAME_GO)
    return &go_board_appearance;

  if (game_index == GTK_GAME_AMAZONS)
    return &amazons_board_appearance.board_appearance;

  if (game_index == GTK_GAME_OTHELLO)
    return &othello_board_appearance;

  assert (0);
}



gboolean
gtk_preferences_have_non_hidden_gtp_engine (void)
{
  GtpEngineListItem *engine_data;

  for (engine_data = gtp_engines.first; engine_data;
       engine_data = engine_data->next) {
    if (!engine_data->is_hidden)
      return TRUE;
  }

  return FALSE;
}


GtkWidget *
gtk_preferences_create_engine_selector (GtkGameIndex game_index,
					gboolean only_this_game,
					const gchar *engine_name,
					GtkEngineChanged callback,
					gpointer user_data)
{
  GtkEngineSelectorData *data = g_malloc (sizeof (GtkEngineSelectorData));
  GtpEngineListItem *engine_data = (engine_name
				    ? gtp_engine_list_find (&gtp_engines,
							    engine_name)
				    : NULL);

#if GTK_2_4_OR_LATER

  GtkWidget *widget;
  GtkCellRenderer *cell_renderer;

  if (only_this_game) {
    widget = (gtk_combo_box_new_with_model
	      (only_one_game_gtp_engines_tree_model[game_index]));
  }
  else
    widget = gtk_combo_box_new_with_model (non_hidden_gtp_engines_tree_model);

  cell_renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell_renderer, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (widget), cell_renderer,
				 "text", ENGINES_NAME);

  if (!only_this_game) {
    data->pixbuf_cell = gtk_cell_renderer_pixbuf_new ();
    g_object_set (data->pixbuf_cell, "xpad", QUARRY_SPACING_SMALL, NULL);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), data->pixbuf_cell,
				FALSE);

    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (widget),
					data->pixbuf_cell,
					set_pixbuf_cell_image,
					GINT_TO_POINTER (game_index), NULL);

    data->selector_game_index = GTK_GAME_UNSUPPORTED;
  }
  else
    data->selector_game_index = game_index;

#else /* not GTK_2_4_OR_LATER */

  GtkWidget *widget = gtk_option_menu_new ();

  build_and_attach_menu (widget, game_index, only_this_game);
  data->selector_game_index = game_index;
  data->only_this_game	    = only_this_game;

#endif /* not GTK_2_4_OR_LATER */

  data->callback  = callback;
  data->selector  = widget;
  data->user_data = user_data;
  gtp_engine_selectors = g_slist_prepend (gtp_engine_selectors, data);

  gtk_preferences_set_engine_selector_selection (widget, engine_data);

  g_signal_connect (widget, "changed",
		    G_CALLBACK (engine_selector_changed), data);
  g_signal_connect_swapped (widget, "destroy",
			    G_CALLBACK (engine_selector_destroyed), data);

  return widget;
}


void
gtk_preferences_set_engine_selector_game_index (GtkWidget *selector,
						GtkGameIndex game_index)
{
  GSList *item;

  assert (0 <= game_index && game_index < NUM_SUPPORTED_GAMES);

  for (item = gtp_engine_selectors; ; item = item->next) {
    GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

    assert (item);

    if (data->selector == selector) {
#if GTK_2_4_OR_LATER

      assert (data->selector_game_index == GTK_GAME_UNSUPPORTED);

      gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (selector),
					  data->pixbuf_cell,
					  set_pixbuf_cell_image,
					  GINT_TO_POINTER (game_index), NULL);

#else

      assert (!data->only_this_game);

      if (game_index != data->selector_game_index) {
	gtk_utils_block_signal_handlers (data->selector,
					 engine_selector_changed);

	build_and_attach_menu (data->selector, game_index, FALSE);

	gtk_utils_unblock_signal_handlers (data->selector,
					   engine_selector_changed);
      }

      data->selector_game_index = game_index;

#endif

      break;
    }
  }
}


void
gtk_preferences_set_engine_selector_selection
  (GtkWidget *selector, const GtpEngineListItem *engine_data)
{
  gint engine_to_select;

  if (engine_data) {
    GSList *item;
    GtpEngineListItem *engine_scan;
    GtkEngineSelectorData *data;

    for (item = gtp_engine_selectors; ; item = item->next) {
      assert (item);

      data = (GtkEngineSelectorData *) (item->data);
      if (data->selector == selector)
	break;
    }

    for (engine_scan = gtp_engines.first, engine_to_select = 0;
	 engine_scan && engine_scan != engine_data;
	 engine_scan = engine_scan->next) {
      if (!engine_scan->is_hidden) {
#if GTK_2_4_OR_LATER

	if (data->pixbuf_cell
	    || gtk_games_engine_supports_game (engine_scan,
					       data->selector_game_index))
	  engine_to_select++;

#else

	if (!data->only_this_game
	    || gtk_games_engine_supports_game (engine_scan,
					       data->selector_game_index))
	  engine_to_select++;

#endif
      }
    }

    if (!engine_scan)
      engine_to_select = 0;
  }
  else
    engine_to_select = 0;

#if GTK_2_4_OR_LATER
  gtk_combo_box_set_active (GTK_COMBO_BOX (selector), engine_to_select);
#else
  gtk_option_menu_set_history (GTK_OPTION_MENU (selector), engine_to_select);
#endif
}


GtpEngineListItem *
gtk_preferences_get_engine_selector_selection (GtkWidget *selector)
{
  GSList *item;
  GtkEngineSelectorData *data;
  GtpEngineListItem *engine_data;
  gint selected_engine = gtk_utils_get_selector_active_item_index (selector);

  for (item = gtp_engine_selectors; ; item = item->next) {
    assert (item);

    data = (GtkEngineSelectorData *) (item->data);
    if (data->selector == selector)
      break;
  }

  for (engine_data = gtp_engines.first; engine_data;
       engine_data = engine_data->next) {
    if (!engine_data->is_hidden) {
#if GTK_2_4_OR_LATER

      if (data->pixbuf_cell
	  || gtk_games_engine_supports_game (engine_data,
					     data->selector_game_index)) {
	if (selected_engine-- == 0)
	  break;
      }

#else

      if (!data->only_this_game
	  || gtk_games_engine_supports_game (engine_data,
					     data->selector_game_index)) {
	if (selected_engine-- == 0)
	  break;
      }

#endif
    }
  }

  return engine_data;
}


static void
engine_selector_changed (GtkWidget *selector, GtkEngineSelectorData *data)
{
  data->callback (selector, data->user_data);
}


static void
engine_selector_destroyed (GtkEngineSelectorData *data)
{
  gtp_engine_selectors = g_slist_remove (gtp_engine_selectors, data);
  g_free (data);
}


#if GTK_2_4_OR_LATER


static void
set_pixbuf_cell_image (GtkCellLayout *cell_layout, GtkCellRenderer *cell,
		       GtkTreeModel *gtp_engines_tree_model,
		       GtkTreeIter *iterator,
		       gpointer data)
{
  GtpEngineListItem *engine_data;
  const gchar *stock_id;

  UNUSED (cell_layout);

  gtk_tree_model_get (gtp_engines_tree_model, iterator,
		      ENGINES_DATA, &engine_data, -1);
  stock_id = (gtk_games_engine_supports_game (engine_data,
					      GPOINTER_TO_INT (data))
	      ? GTK_STOCK_YES : GTK_STOCK_NO);

  g_object_set (cell, "stock-id", stock_id, NULL);
}


static gboolean
engine_is_visible_and_supports_game (GtkTreeModel *tree_model,
				     GtkTreeIter *iterator,
				     gpointer game_index)
{
  GtpEngineListItem *engine_data;

  gtk_tree_model_get (tree_model, iterator, ENGINES_DATA, &engine_data, -1);

  return (!engine_data->is_hidden
	  && gtk_games_engine_supports_game (engine_data,
					     GPOINTER_TO_INT (game_index)));
}


#else /* not GTK_2_4_OR_LATER */


static void
prepare_to_rebuild_menus (void)
{
  GSList *item;

  for (item = gtp_engine_selectors; item; item = item->next) {
    GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

    data->last_selection
      = gtk_preferences_get_engine_selector_selection (data->selector);
  }
}


static void
rebuild_all_menus (void)
{
  GSList *item;

  for (item = gtp_engine_selectors; item; item = item->next) {
    GtkEngineSelectorData *data = (GtkEngineSelectorData *) (item->data);

    gtk_utils_block_signal_handlers (data->selector, engine_selector_changed);

    build_and_attach_menu (data->selector, data->selector_game_index,
			   data->only_this_game);
    gtk_preferences_set_engine_selector_selection (data->selector,
						   data->last_selection);

    gtk_utils_unblock_signal_handlers (data->selector,
				       engine_selector_changed);
  }
}


static void
build_and_attach_menu (GtkWidget *option_menu, GtkGameIndex game_index,
		       gboolean only_this_game)
{
  GtkWidget *menu = gtk_menu_new ();
  GtpEngineListItem *engine_data;

  for (engine_data = gtp_engines.first; engine_data;
       engine_data = engine_data->next) {
    gboolean engine_supports_game
      = gtk_games_engine_supports_game (engine_data, game_index);

    if (!engine_data->is_hidden) {
      if (!only_this_game) {
	GtkWidget *menu_item = gtk_menu_item_new ();
	GtkWidget *icon;
	GtkWidget *hbox;

	icon = gtk_image_new_from_stock ((engine_supports_game
					  ? GTK_STOCK_YES : GTK_STOCK_NO),
					 GTK_ICON_SIZE_MENU);

	hbox = gtk_utils_pack_in_box (GTK_TYPE_HBOX, QUARRY_SPACING_SMALL,
				      gtk_label_new (engine_data->screen_name),
				      GTK_UTILS_FILL,
				      icon, GTK_UTILS_FILL, NULL);

	gtk_container_add (GTK_CONTAINER (menu_item), hbox);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      }
      else if (gtk_games_engine_supports_game (engine_data, game_index)) {
	gtk_menu_shell_append
	  (GTK_MENU_SHELL (menu),
	   gtk_menu_item_new_with_label (engine_data->screen_name));
      }
    }
  }

  gtk_widget_show_all (menu);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
}


#endif /* not GTK_2_4_OR_LATER */


/* FIXME: the functions below need to be written properly. */

GtkEngineChain *
gtk_preferences_create_engines_instantiation_chain
  (GtkWindow *parent_window, GtkEnginesInstantiated callback,
   gpointer user_data)
{
  GtkEngineChain *engine_chain = g_malloc (sizeof (GtkEngineChain));

  engine_chain->parent_window = parent_window;

  engine_chain->instantiation_callback = callback;
  engine_chain->user_data = user_data;

  engine_chain->chain_engine_datum = NULL;
  engine_chain->have_error = FALSE;

  return engine_chain;
}


void
gtk_preferences_instantiate_selected_engine (GtkEngineChain *engine_chain,
					     GtkWidget *selector,
					     GtpClient **gtp_client)
{
  assert (engine_chain);
  assert (selector);
  assert (gtp_client);

  if (!engine_chain->have_error) {
    GtpEngineListItem *engine_data
      = gtk_preferences_get_engine_selector_selection (selector);
    GError *error = NULL;
    GtkChainEngineData *chain_engine_data
      = g_malloc (sizeof (GtkChainEngineData));

    assert (engine_data);

    chain_engine_data->engine_data = engine_data;
    chain_engine_data->engine_chain = engine_chain;

    *gtp_client = gtk_create_gtp_client (engine_data->command_line,
					 ((GtpClientInitializedCallback)
					  chain_client_initialized),
					 NULL, chain_engine_data, &error);

    if (*gtp_client) {
      gtp_client_setup_connection (*gtp_client);

      engine_chain->chain_engine_datum
	= g_slist_prepend (engine_chain->chain_engine_datum,
			   chain_engine_data);
    }
    else {
      g_free (chain_engine_data);

      gtk_utils_create_message_dialog (engine_chain->parent_window,
				       GTK_STOCK_DIALOG_ERROR,
				       (GTK_UTILS_BUTTONS_OK
					| GTK_UTILS_DESTROY_ON_RESPONSE),
				       _("Perhaps engine's binary has been "
					 "deleted or changed. You will "
					 "probably need to alter engine's "
					 "command line in preferences dialog"),
				       error->message);
      g_error_free (error);

      engine_chain->have_error = TRUE;
    }
  }
}


void
gtk_preferences_do_instantiate_engines (GtkEngineChain *engine_chain)
{
  if (engine_chain->chain_engine_datum != NULL) {
  }
  else {
    if (engine_chain->instantiation_callback) {
      engine_chain->instantiation_callback (ENGINES_INSTANTIATED,
					    engine_chain->user_data);
    }

    g_free (engine_chain);
  }
}


static void
chain_client_initialized (GtpClient *client, void *user_data)
{
  GtkChainEngineData *chain_engine_data = (GtkChainEngineData *) user_data;
  GtkEngineChain *engine_chain = chain_engine_data->engine_chain;
  GtpEngineListItem *engine_data = chain_engine_data->engine_data;

  configuration_set_string_value (&engine_data->name, client->engine_name);
  configuration_set_string_value (&engine_data->version,
				  client->engine_version);

  update_engine_screen_name (engine_data, TRUE);

  configuration_set_string_list_value (&engine_data->supported_games,
				       &client->supported_games);

  engine_chain->chain_engine_datum
    = g_slist_remove (engine_chain->chain_engine_datum, chain_engine_data);
  g_free (chain_engine_data);

  if (engine_chain->chain_engine_datum == NULL) {
    if (engine_chain->instantiation_callback) {
      engine_chain->instantiation_callback (ENGINES_INSTANTIATED,
					    engine_chain->user_data);
    }

    g_free (engine_chain);
  }
}


static void
update_engine_screen_name (GtpEngineListItem *engine_data,
			   gboolean update_tree_model)
{
  char *new_screen_name
    = utils_special_printf (engine_data->screen_name_format,
			    'n', engine_data->name,
			    'v', engine_data->version, 0);

  if (strcmp (engine_data->screen_name, new_screen_name) != 0) {
    utils_free (engine_data->screen_name);
    engine_data->screen_name = new_screen_name;

    if (update_tree_model) {
      GtkTreeIter iterator;

      find_gtp_tree_model_iterator_by_engines_data (engine_data, &iterator);
      gtk_list_store_set (gtp_engines_list_store, &iterator,
			  ENGINES_NAME, new_screen_name, -1);
    }
  }
  else
    utils_free (new_screen_name);
}


static void
find_gtp_tree_model_iterator_by_engines_data
  (const GtpEngineListItem *engine_data, GtkTreeIter *iterator)
{
  GtkTreeModel *gtp_engines_tree_model
    = GTK_TREE_MODEL (gtp_engines_list_store);
  GtpEngineListItem *this_engine_data;

  gtk_tree_model_get_iter_first (gtp_engines_tree_model, iterator);
  while (gtk_tree_model_get (gtp_engines_tree_model, iterator,
			     ENGINES_DATA, &this_engine_data, -1),
	 this_engine_data != engine_data)
    gtk_tree_model_iter_next (gtp_engines_tree_model, iterator);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
