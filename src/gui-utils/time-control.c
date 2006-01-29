/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Quarry.                                    *
 *                                                                 *
 * Copyright (C) 2004, 2005, 2006 Paul Pogonyshev.                 *
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


#include "time-control.h"

#include "sgf.h"
#include "utils.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>


TimeControl *
time_control_new (int main_time, int overtime_length, int moves_per_overtime)
{
  TimeControl *time_control = utils_malloc (sizeof (TimeControl));

  time_control_init (time_control,
		     main_time, overtime_length, moves_per_overtime);

  return time_control;
}


TimeControl *
time_control_new_from_sgf_node (const SgfNode *sgf_node)
{
  double main_time = 0.0;
  const char *overtime_description;
  int overtime_length	 = 0;
  int moves_per_overtime = 0;

  sgf_node_get_time_limit (sgf_node, &main_time);
  overtime_description = sgf_node_get_text_property_value (sgf_node,
							   SGF_OVERTIME);

  if (overtime_description) {
    /* FIXME: Nasty.  We certainly need utils_sscanf() for
     *	      locale-independent string parsing.
     */

    const char *scan;

    for (scan = overtime_description; '0' <= *scan && *scan <= '9'; scan++)
      overtime_length = overtime_length * 10 + (*scan - '0');

    if (overtime_length > 0) {
      if (strcmp (scan, " per move") == 0)
	moves_per_overtime = 1;
      else if (*scan++ == '/') {
	moves_per_overtime = overtime_length;
	overtime_length	   = 0;

	for (; '0' <= *scan && *scan <= '9'; scan++)
	  overtime_length = overtime_length * 10 + (*scan - '0');

	if (overtime_length == 0 || strcmp (scan, " Canadian") != 0) {
	  overtime_length    = 0;
	  moves_per_overtime = 0;
	}
      }
      else
	overtime_length = 0;
    }
  }

  if (main_time > 0 || overtime_length > 0)
    return time_control_new (main_time, overtime_length, moves_per_overtime);

  return NULL;
}


/* Note that this function only duplicates the settings, not the
 * state of its argument.
 */
inline TimeControl *
time_control_duplicate (const TimeControl *time_control)
{
  return time_control_new (time_control->main_time,
			   time_control->overtime_length,
			   time_control->moves_per_overtime);
}


void
time_control_init (TimeControl *time_control,
		   int main_time, int overtime_length, int moves_per_overtime)
{
  assert (time_control);
  assert (main_time >= 0 && overtime_length >= 0 && moves_per_overtime >= 0
	  && (overtime_length == 0 || moves_per_overtime > 0));

  time_control->main_time	   = main_time;
  time_control->overtime_length	   = overtime_length;
  time_control->moves_per_overtime = moves_per_overtime;

  time_control->seconds_elapsed	= 0.0;
  time_control->moves_to_play	= (main_time > 0 || overtime_length == 0
				   ? 0 : moves_per_overtime);

  time_control->timer_object = NULL;
  time_control->is_active    = 0;
}


void
time_control_delete (TimeControl *time_control)
{
  time_control_dispose (time_control);
  utils_free (time_control);
}


void
time_control_dispose (TimeControl *time_control)
{
  assert (time_control);

  if (time_control->timer_object)
    gui_back_end_timer_delete (time_control->timer_object);
}


void
time_control_save_settings_in_sgf_node (const TimeControl *time_control,
					SgfNode *sgf_node,
					SgfGameTree *sgf_tree)
{
  assert (time_control);
  assert (sgf_node);
  assert (sgf_tree);

  if (time_control->main_time > 0.0 || time_control->overtime_length > 0.0) {
    char *overtime_description;

    if (time_control->overtime_length == 0)
      overtime_description = utils_duplicate_string ("none");
    else if (time_control->moves_per_overtime == 1) {
      overtime_description = utils_cprintf ("%d per move",
					    time_control->overtime_length);
    }
    else {
      overtime_description = utils_cprintf ("%d/%d Canadian",
					    time_control->moves_per_overtime,
					    time_control->overtime_length);
    }

    sgf_node_add_text_property (sgf_node, sgf_tree, SGF_TIME_LIMIT,
				utils_cprintf ("%d.0",
					       time_control->main_time),
				1);
    sgf_node_add_text_property (sgf_node, sgf_tree, SGF_OVERTIME,
				overtime_description, 1);
  }
}


void
time_control_save_state_in_sgf_node (const TimeControl *time_control,
				     SgfNode *sgf_node, SgfGameTree *sgf_tree,
				     int color)
{
  double seconds_left;
  int moves_to_play;

  assert (time_control);
  assert (!time_control->is_active);

  if (!TIME_CONTROL_CLOCK_RUNS_DOWN (time_control))
    return;

  seconds_left = time_control_get_time_left (time_control, &moves_to_play);
  sgf_utils_set_time_left (sgf_node, sgf_tree, color,
			   floor (seconds_left * 1000.0 + 0.5) / 1000.0,
			   moves_to_play);
}


void
time_control_apply_defaults_if_needed (const TimeControl *time_control,
				       double seconds_left, int moves_to_play,
				       double *new_seconds_left,
				       int *new_moves_to_play)
{
  assert (time_control);
  assert (new_seconds_left);
  assert (new_moves_to_play);

  if (seconds_left >= 0.0)
    *new_seconds_left = seconds_left;
  else {
    *new_seconds_left = (time_control->main_time > 0
			 ? time_control->main_time
			 : time_control->overtime_length);
  }

  if (moves_to_play >= 0)
    *new_moves_to_play = moves_to_play;
  else {
    *new_moves_to_play = (time_control->main_time > 0
			  ? 0 : time_control->moves_per_overtime);
  }
}


/* FIXME: Validate new state. */
void
time_control_set_state (TimeControl *time_control,
			double seconds_left, int moves_to_play)
{
  double new_seconds_left;

  assert (time_control);
  assert (!time_control->is_active);

  time_control_apply_defaults_if_needed (time_control,
					 seconds_left, moves_to_play,
					 &new_seconds_left,
					 &time_control->moves_to_play);

  if (time_control->moves_to_play) {
    time_control->seconds_elapsed = (time_control->overtime_length
				     - new_seconds_left);
  }
  else {
    time_control->seconds_elapsed = (time_control->main_time
				     - new_seconds_left);
  }
}


/* Start timer (time control object's owner is about to play a move).
 * Return (positive) number of seconds within which a move absolutely
 * has to be played, or the player loses on time.  If the player is
 * already out of time, return `OUT_OF_TIME'.  When there are no time
 * limits, return `NO_TIME_LIMITS'.
 */
double
time_control_start (TimeControl *time_control)
{
  double time_remaining;

  assert (time_control);
  assert (!time_control->is_active);

  time_control->timer_object
    = gui_back_end_timer_restart (time_control->timer_object);
  time_control->is_active = 1;

  if (time_control->moves_to_play == 0) {
    if (time_control->main_time == 0)
      return NO_TIME_LIMITS;

    time_remaining = ((double) time_control->main_time
		      + (double) time_control->overtime_length
		      - time_control->seconds_elapsed);
  }
  else {
    time_remaining =  (((double) time_control->overtime_length
			- time_control->seconds_elapsed));
  }

  return (time_remaining > 0.0 ? time_remaining : OUT_OF_TIME);
}


/* Get the number of seconds the clock should show and, optionally,
 * number of moves to be played in the current overtime pediod.  If
 * the number of seconds is negative or zero, then the player has lost
 * on time.  If the number of moves to be played is zero, then the
 * player is currently in main time, not in his overtime period.
 */
double
time_control_get_clock_seconds (const TimeControl *time_control,
				int *moves_to_play)
{
  double seconds_elapsed;

  assert (time_control);

  seconds_elapsed = time_control->seconds_elapsed;
  if (time_control->is_active) {
    seconds_elapsed
      += gui_back_end_timer_get_seconds_elapsed (time_control->timer_object);
  }

  if (moves_to_play)
    *moves_to_play = time_control->moves_to_play;

  if (time_control->moves_to_play == 0) {
    if (time_control->main_time > 0) {
      double result = (double) time_control->main_time - seconds_elapsed;

      if (result <= 0.0) {
	if (moves_to_play)
	  *moves_to_play = time_control->moves_per_overtime;

	result += (double) time_control->overtime_length;
      }

      return result;
    }

    return seconds_elapsed;
  }
  else
    return (double) time_control->overtime_length - seconds_elapsed;
}


/* Stop timer (a move has been played) and update internals
 * accordingly.  Return value and parameters are as for
 * time_control_get_clock_seconds().
 */
double
time_control_stop (TimeControl *time_control, int *moves_to_play)
{
  assert (time_control);
  assert (time_control->is_active);

  time_control->seconds_elapsed
    += gui_back_end_timer_get_seconds_elapsed (time_control->timer_object);

  time_control->is_active = 0;

  if (time_control->moves_to_play == 0) {
    if (time_control->seconds_elapsed < (double) time_control->main_time
	|| time_control->overtime_length == 0) {
      if (moves_to_play)
	*moves_to_play = 0;

      if (time_control->main_time > 0) {
	return ((double) time_control->main_time
		- time_control->seconds_elapsed);
      }
      else
	return time_control->seconds_elapsed;
    }

    time_control->seconds_elapsed -= (double) time_control->main_time;
    time_control->moves_to_play = time_control->moves_per_overtime;
  }

  if (--time_control->moves_to_play == 0) {
    if (time_control->seconds_elapsed
	< (double) time_control->overtime_length)
      time_control->seconds_elapsed = 0.0;

    time_control->moves_to_play = time_control->moves_per_overtime;
  }

  if (moves_to_play)
    *moves_to_play = time_control->moves_to_play;

  return ((double) time_control->overtime_length
	  - time_control->seconds_elapsed);
}


double
time_control_get_time_left (const TimeControl *time_control,
			    int *moves_to_play)
{
  double time_remaining;

  assert (time_control);
  assert (!time_control->is_active);

  if (moves_to_play)
    *moves_to_play = time_control->moves_to_play;

  if (time_control->moves_to_play == 0) {
    if (time_control->main_time == 0)
      return NO_TIME_LIMITS;

    time_remaining = ((double) time_control->main_time
		      - time_control->seconds_elapsed);
  }
  else {
    time_remaining = ((double) time_control->overtime_length
		      - time_control->seconds_elapsed);
  }

  return (time_remaining > 0.0 ? time_remaining : OUT_OF_TIME);
}


/* NOTE: this function assumes that seconds displayed are
 *
 *	ceil (time_control_get_clock_seconds (...))
 */
double
time_control_get_time_till_seconds_update (const TimeControl *time_control)
{
  double seconds_elapsed;

  assert (time_control);

  seconds_elapsed = time_control->seconds_elapsed;
  if (time_control->is_active) {
    seconds_elapsed
      += gui_back_end_timer_get_seconds_elapsed (time_control->timer_object);
  }

  return (ceil (seconds_elapsed
		+ (TIME_CONTROL_CLOCK_RUNS_DOWN (time_control)
		   ? DBL_EPSILON : 0.0))
	  - seconds_elapsed);
}


int
time_control_is_short_on_time (const TimeControl *time_control)
{
  double seconds_elapsed;
  double seconds_left;

  assert (time_control);

  if (!TIME_CONTROL_CLOCK_RUNS_DOWN (time_control))
    return 0;

  seconds_elapsed = time_control->seconds_elapsed;
  if (time_control->is_active) {
    seconds_elapsed
      += gui_back_end_timer_get_seconds_elapsed (time_control->timer_object);
  }

  if (time_control->moves_to_play == 0) {
    seconds_left = (((double) time_control->main_time - seconds_elapsed)
		    + (double) time_control->overtime_length);
  }
  else
    seconds_left = (double) time_control->overtime_length - seconds_elapsed;

  return 0.0 < seconds_left && seconds_left <= 30.0;
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
