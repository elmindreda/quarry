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


#ifndef QUARRY_GTP_CLIENT_H
#define QUARRY_GTP_CLIENT_H


#include "sgf.h"
#include "board.h"
#include "utils.h"
#include "quarry.h"

#include <stdio.h>


#define GTP_MIN_BOARD_SIZE	 2
#define GTP_MAX_BOARD_SIZE	25


typedef enum {
  GTP_CLIENT_UNINITIALIZED,
  GTP_CLIENT_WORKING,
  GTP_CLIENT_QUIT_SCHEDULED,
  GTP_CLIENT_QUIT
} GtpClientOperationStage;

typedef enum {
  GTP_ERROR_WRONG_RESPONSE_FORMAT,
  GTP_WARNING_UNEXPECTED_OUTPUT,
  GTP_ERROR_UNRECOGNIZED_RESPONSE,
  GTP_ERROR_MISMATCHED_ID,
  GTP_ERROR_UNEXPECTED_ID,
  GTP_WARNING_FUTURE_GTP_VERSION,

  /* Used internally */
  GTP_SUCCESS
} GtpError;


typedef struct _GtpClient	GtpClient;


typedef void (* GtpClientSendToEngine) (const char *command, void *user_data);
typedef void (* GtpClientLineCallback) (const char *line,
					int is_command, int internal_index,
					void *user_data);
typedef void (* GtpClientErrorCallback) (GtpError error, int command_id,
					 void *user_data);

typedef void (* GtpClientInitializedCallback) (GtpClient *client,
					       void *user_data);
typedef void (* GtpClientDeletedCallback) (GtpClient *client, void *user_data);

typedef int (* GtpClientResponseCallback) (GtpClient *client, int successful,
					   void *user_data);


typedef struct _GtpCommandListItem	GtpCommandListItem;
typedef struct _GtpCommandList		GtpCommandList;

struct _GtpCommandListItem {
  GtpCommandListItem	     *next;
  char			     *command;

  int			      command_id;
  GtpClientResponseCallback   response_callback;
  void			     *user_data;
};

struct _GtpCommandList {
  GtpCommandListItem	     *first;
  GtpCommandListItem	     *last;

  int			      item_size;
  StringListItemDispose	      item_dispose;
};


#define gtp_command_list_new()						\
  ((GtpCommandList *)							\
   string_list_new_derived(sizeof(GtpCommandListItem), NULL)	

#define gtp_command_list_init(list)					\
  string_list_init_derived((list), sizeof(GtpCommandListItem),	NULL)

#define STATIC_GTP_COMMAND_LIST						\
  STATIC_STRING_LIST_DERIVED(GtpCommandListItem, NULL)


#define gtp_command_list_get_item(list, item_index)			\
  ((GtpCommandListItem *) string_list_get_item((list), (item_index)))

#define gtp_command_list_find(list, command)				\
  ((GtpCommandListItem *) string_list_find((list), (command)))

#define gtp_command_list_find_after_notch(list, command, notch)		\
  ((GtpCommandListItem *)						\
   string_list_find_after_notch((list), (command), (notch)))


struct _GtpClient {
  GtpClientSendToEngine		 send_to_engine;
  GtpClientLineCallback		 line_callback;
  GtpClientErrorCallback	 error_callback;
  GtpClientInitializedCallback	 initialized_callback;
  GtpClientDeletedCallback	 deleted_callback;
  void				*user_data;

  int				 protocol_version;
  char				*engine_name;
  char				*engine_version;

  StringList			 known_commands;
  StringList			 supported_games;

  Game				 game;
  int				 board_size;

  GtpCommandList		 pending_commands;

  StringList			 response;
  int				 incomplete_line;
  int				 successful;

  int				 echo_mode;

  GtpClientOperationStage	 operation_stage;
  int				 internal_command_index;
  int				 internal_response_index;
};


typedef void (* GtpClientFreeHandicapCallback)
  (GtpClient *client, int successful, void *user_data,
   BoardPositionList *handicap_stones);

typedef void (* GtpClientMoveCallback) (GtpClient *client, int successful,
					void *user_data,
					int color, int x, int y,
					BoardAbstractMoveData *move_data);


GtpClient *	gtp_client_new
		  (GtpClientSendToEngine send_to_engine,
		   GtpClientLineCallback line_callback,
		   GtpClientErrorCallback error_callback,
		   GtpClientInitializedCallback initialized_callback,
		   GtpClientDeletedCallback deletied_callback,
		   void *user_data);
void		gtp_client_setup_connection(GtpClient *client);

void		gtp_client_delete(GtpClient *client);
void		gtp_client_quit(GtpClient *client);


void		gtp_client_grab_response(GtpClient *client,
					 char *response, int length);

int		gtp_client_set_echo_mode(GtpClient *client, int echo_mode);

#define gtp_client_echo_on(client)	gtp_client_set_echo_mode((client), 1)
#define gtp_client_echo_off(client)	gtp_client_set_echo_mode((client), 0)


int		gtp_client_is_known_command(const GtpClient *client,
					    const char *command);

void		gtp_client_set_game
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data, Game game);
void		gtp_client_set_board_size
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data, int board_size);
void		gtp_client_set_fixed_handicap
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data, int handicap);
void		gtp_client_place_free_handicap
		  (GtpClient *client,
		   GtpClientFreeHandicapCallback response_callback,
		   void *user_data, int handicap);
void		gtp_client_set_free_handicap
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data, const BoardPositionList *handicap_stones);
void		gtp_client_set_komi
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data, double komi);

void		gtp_client_send_time_settings
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data,
		   int main_time, int byo_yomi_time,
		   int moves_per_byo_yomi_period);
void		gtp_client_send_time_left
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data,
		   int color, int seconds_left, int moves_left);

void		gtp_client_play_move
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data, int color, ...);
void		gtp_client_play_move_from_sgf_node
		  (GtpClient *client,
		   GtpClientResponseCallback response_callback,
		   void *user_data,
		   const SgfGameTree *sgf_game_tree, const SgfNode *sgf_node);
void		gtp_client_generate_move
		  (GtpClient *client,
		   GtpClientMoveCallback response_callback,
		   void *user_data, int color);


#endif /* QUARRY_GTP_CLIENT_H */


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
