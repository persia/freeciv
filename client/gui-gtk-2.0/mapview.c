/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#include "fcintl.h"
#include "game.h"
#include "government.h"		/* government_graphic() */
#include "log.h"
#include "map.h"
#include "player.h"
#include "rand.h"
#include "support.h"
#include "timing.h"

#include "civclient.h"
#include "climisc.h"
#include "colors.h"
#include "control.h" /* set_unit_focus_no_center and get_unit_in_focus */
#include "goto.h"
#include "graphics.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "mapctrl.h"
#include "options.h"
#include "tilespec.h"

#include "mapview.h"

/* contains the x0, y0 coordinates of the upper left corner block */
int map_view_x0, map_view_y0;

/* T: area above the actual tile.
   M: the top of the actual tile.
   B: the bottom of the actual tile.
   L: left.
   R: right.
*/
enum draw_part {
  D_T_L = 1, D_T_R = 2, D_M_L = 4, D_M_R = 8, D_B_L = 16, D_B_R = 32
};

/* Format: D_[TMB]+_[LR]+.
   The drawing algorithm don't take all possible combinations into account,
   but only those that are rectangles.
*/
/* Some usefull definitions: */
enum draw_type {
  D_FULL = D_T_L | D_T_R | D_M_L | D_M_R | D_B_L | D_B_R,
  D_B_LR = D_B_L | D_B_R,
  D_MB_L = D_M_L | D_B_L,
  D_MB_R = D_M_R | D_B_R,
  D_TM_L = D_T_L | D_M_L,
  D_TM_R = D_T_R | D_M_R,
  D_T_LR = D_T_L | D_T_R,
  D_TMB_L = D_T_L | D_M_L | D_B_L,
  D_TMB_R = D_T_R | D_M_R | D_B_R,
  D_M_LR = D_M_L | D_M_R,
  D_MB_LR = D_M_L | D_M_R | D_B_L | D_B_R
};

static void pixmap_put_overlay_tile(GdkDrawable *pixmap,
				    int canvas_x, int canvas_y,
				    struct Sprite *ssprite);
static void put_overlay_tile_gpixmap(GtkPixcomm *p,
				     int canvas_x, int canvas_y,
				     struct Sprite *ssprite);
static void put_unit_pixmap(struct unit *punit, GdkPixmap *pm,
			    int canvas_x, int canvas_y);
static void put_line(GdkDrawable *pm, int x, int y, int dir);

static void put_unit_pixmap_draw(struct unit *punit, GdkPixmap *pm,
				 int canvas_x, int canvas_y,
				 int offset_x, int offset_y_unit,
				 int width, int height_unit);
static void pixmap_put_overlay_tile_draw(GdkDrawable *pixmap,
					 int canvas_x, int canvas_y,
					 struct Sprite *ssprite,
					 int offset_x, int offset_y,
					 int width, int height,
					 int fog);
static void really_draw_segment(int src_x, int src_y, int dir,
				bool write_to_screen, bool force);
static void pixmap_put_tile_iso(GdkDrawable *pm, int x, int y,
				int canvas_x, int canvas_y,
				int citymode,
				int offset_x, int offset_y, int offset_y_unit,
				int width, int height, int height_unit,
				enum draw_type draw);
static void pixmap_put_black_tile_iso(GdkDrawable *pm,
				      int canvas_x, int canvas_y,
				      int offset_x, int offset_y,
				      int width, int height);

/* the intro picture is held in this pixmap, which is scaled to
   the screen size */
static SPRITE *scaled_intro_sprite = NULL;

static GtkObject *map_hadj, *map_vadj;


/***********************************************************************
  This function can be used by mapview_common code to determine the
  location and dimensions of the mapview canvas.
***********************************************************************/
void get_mapview_dimensions(int *map_view_topleft_map_x,
			    int *map_view_topleft_map_y,
			    int *map_view_pixel_width,
			    int *map_view_pixel_height)
{
  *map_view_topleft_map_x = map_view_x0;
  *map_view_topleft_map_y = map_view_y0;
  gdk_window_get_size(map_canvas->window,
		      map_view_pixel_width, map_view_pixel_height);
}

/**************************************************************************
...
**************************************************************************/
void pixmap_put_tile(GdkDrawable *pm, int x, int y,
		     int canvas_x, int canvas_y, int citymode)
{
  struct Sprite *tile_sprs[80];
  int fill_bg;
  struct player *pplayer;

  if (normalize_map_pos(&x, &y) && tile_get_known(x, y)) {
    int count = fill_tile_sprite_array(tile_sprs, x, y, citymode, &fill_bg, &pplayer);
    int i = 0;

    if (fill_bg) {
      if (pplayer) {
	gdk_gc_set_foreground(fill_bg_gc,
			      colors_standard[player_color(pplayer)]);
      } else {
	gdk_gc_set_foreground(fill_bg_gc,
			      colors_standard[COLOR_STD_BACKGROUND]);	
      }
      gdk_draw_rectangle(pm, fill_bg_gc, TRUE,
			 canvas_x, canvas_y,
			 NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
    } else {
      /* first tile without mask */
      gdk_draw_pixmap(pm, civ_gc, tile_sprs[0]->pixmap,
                      0, 0, canvas_x, canvas_y,
                      tile_sprs[0]->width, tile_sprs[0]->height);
      i++;
    }

    for (;i<count; i++) {
      if (tile_sprs[i]) {
        pixmap_put_overlay_tile(pm, canvas_x, canvas_y, tile_sprs[i]);
      }
    }

    if (draw_map_grid && !citymode) {
      /* left side... */
      gdk_gc_set_foreground(civ_gc,
			    colors_standard[get_grid_color
					    (x, y, x - 1, y)]);
      gdk_draw_line(pm, civ_gc, canvas_x, canvas_y, canvas_x,
		    canvas_y + NORMAL_TILE_HEIGHT);

      /* top side... */
      gdk_gc_set_foreground(civ_gc,
			    colors_standard[get_grid_color
					    (x, y, x, y - 1)]);
      gdk_draw_line(pm, civ_gc, canvas_x, canvas_y,
		    canvas_x + NORMAL_TILE_WIDTH, canvas_y);
    }

    if (draw_coastline && !draw_terrain) {
      enum tile_terrain_type t1 = map_get_terrain(x, y), t2;
      int x1 = x-1, y1 = y;
      gdk_gc_set_foreground(civ_gc, colors_standard[COLOR_STD_OCEAN]);
      if (normalize_map_pos(&x1, &y1)) {
	t2 = map_get_terrain(x1, y1);
	/* left side */
	if ((t1 == T_OCEAN) ^ (t2 == T_OCEAN))
	  gdk_draw_line(pm, civ_gc,
			canvas_x, canvas_y,
			canvas_x, canvas_y + NORMAL_TILE_HEIGHT);
      }
      /* top side */
      x1 = x; y1 = y-1;
      if (normalize_map_pos(&x1, &y1)) {
	t2 = map_get_terrain(x1, y1);
	if ((t1 == T_OCEAN) ^ (t2 == T_OCEAN))
	  gdk_draw_line(pm, civ_gc,
			canvas_x, canvas_y,
			canvas_x + NORMAL_TILE_WIDTH, canvas_y);
      }
    }
  } else {
    /* tile is unknown */
    pixmap_put_black_tile(pm, canvas_x, canvas_y);
  }

  if (!citymode) {
    /* put any goto lines on the tile. */
    if (is_real_tile(x, y)) {
      int dir;
      for (dir = 0; dir < 8; dir++) {
	if (get_drawn(x, y, dir)) {
	  put_line(map_canvas_store, x, y, dir);
	}
      }
    }

    /* Some goto lines overlap onto the tile... */
    if (NORMAL_TILE_WIDTH%2 == 0 || NORMAL_TILE_HEIGHT%2 == 0) {
      int line_x = x - 1;
      int line_y = y;
      if (normalize_map_pos(&line_x, &line_y)
	  && get_drawn(line_x, line_y, 2)) {
	/* it is really only one pixel in the top right corner */
	put_line(map_canvas_store, line_x, line_y, 2);
      }
    }
  }
}

/**************************************************************************
 This function is called to decrease a unit's HP smoothly in battle
 when combat_animation is turned on.
**************************************************************************/
void decrease_unit_hp_smooth(struct unit *punit0, int hp0, 
			     struct unit *punit1, int hp1)
{
  static struct timer *anim_timer = NULL; 
  struct unit *losing_unit = (hp0 == 0 ? punit0 : punit1);
  int i;

  set_units_in_combat(punit0, punit1);

  do {
    anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

    if (punit0->hp > hp0
	&& myrand((punit0->hp - hp0) + (punit1->hp - hp1)) < punit0->hp - hp0)
      punit0->hp--;
    else if (punit1->hp > hp1)
      punit1->hp--;
    else
      punit0->hp--;

    refresh_tile_mapcanvas(punit0->x, punit0->y, TRUE);
    refresh_tile_mapcanvas(punit1->x, punit1->y, TRUE);

    gdk_flush();
    usleep_since_timer_start(anim_timer, 10000);

  } while (punit0->hp > hp0 || punit1->hp > hp1);

  for (i = 0; i < num_tiles_explode_unit; i++) {
    int canvas_x, canvas_y;
    get_canvas_xy(losing_unit->x, losing_unit->y, &canvas_x, &canvas_y);
    anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);
    if (is_isometric) {
      /* We first draw the explosion onto the unit and draw draw the
	 complete thing onto the map canvas window. This avoids flickering. */
      gdk_draw_pixmap(single_tile_pixmap, civ_gc, map_canvas_store,
		      canvas_x, canvas_y,
		      0, 0,
		      NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
      pixmap_put_overlay_tile(single_tile_pixmap,
			      NORMAL_TILE_WIDTH/4, 0,
			      sprites.explode.unit[i]);
      gdk_draw_pixmap(map_canvas->window, civ_gc, single_tile_pixmap,
		      0, 0,
		      canvas_x, canvas_y,
		      NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
    } else { /* is_isometric */
      /* FIXME: maybe do as described in the above comment. */
      pixmap_put_tile(single_tile_pixmap, losing_unit->x, losing_unit->y,
		      0, 0, 0);
      put_unit_pixmap(losing_unit, single_tile_pixmap, 0, 0);
      pixmap_put_overlay_tile(single_tile_pixmap, 0, 0,
			      sprites.explode.unit[i]);

      gdk_draw_pixmap(map_canvas->window, civ_gc, single_tile_pixmap,
		      0, 0,
		      canvas_x, canvas_y,
		      UNIT_TILE_WIDTH,
		      UNIT_TILE_HEIGHT);
    }
    gdk_flush();
    usleep_since_timer_start(anim_timer, 20000);
  }

  set_units_in_combat(NULL, NULL);
  refresh_tile_mapcanvas(punit0->x, punit0->y, TRUE);
  refresh_tile_mapcanvas(punit1->x, punit1->y, TRUE);
}

/**************************************************************************
  If do_restore is FALSE it will invert the turn done button style. If
  called regularly from a timer this will give a blinking turn done
  button. If do_restore is TRUE this will reset the turn done button
  to the default style.
**************************************************************************/
void update_turn_done_button(bool do_restore)
{
  static bool flip = FALSE;

  if (!get_turn_done_button_state()) {
    return;
  }

  if ((do_restore && flip) || !do_restore) {
    GdkGC *fore = turn_done_button->style->bg_gc[GTK_STATE_NORMAL];
    GdkGC *back = turn_done_button->style->light_gc[GTK_STATE_NORMAL];

    turn_done_button->style->bg_gc[GTK_STATE_NORMAL] = back;
    turn_done_button->style->light_gc[GTK_STATE_NORMAL] = fore;

    gtk_expose_now(turn_done_button);

    flip = !flip;
  }
}

/**************************************************************************
...
**************************************************************************/
void update_timeout_label(void)
{
  char buffer[512];

  if (game.timeout <= 0)
    sz_strlcpy(buffer, Q_("?timeout:off"));
  else
    format_duration(buffer, sizeof(buffer), seconds_to_turndone);
  gtk_set_label(timeout_label, buffer);
}

/**************************************************************************
...
**************************************************************************/
void update_info_label( void )
{
  char buffer	[512];
  int  d;

  gtk_frame_set_label( GTK_FRAME( main_frame_civ_name ), get_nation_name(game.player_ptr->nation) );

  my_snprintf(buffer, sizeof(buffer),
	      _("Population: %s\nYear: %s\n"
		"Gold %d\nTax: %d Lux: %d Sci: %d"),
	      population_to_text(civ_population(game.player_ptr)),
	      textyear(game.year), game.player_ptr->economic.gold,
	      game.player_ptr->economic.tax,
	      game.player_ptr->economic.luxury,
	      game.player_ptr->economic.science);

  gtk_set_label(main_label_info, buffer);

  set_indicator_icons(client_research_sprite(),
		      client_warming_sprite(),
		      client_cooling_sprite(),
		      game.player_ptr->government);

  d=0;
  for(;d<(game.player_ptr->economic.luxury)/10;d++)
    gtk_image_set_from_pixmap(GTK_IMAGE(econ_label[d]),
	get_citizen_pixmap(0), NULL); /* elvis tile */
 
  for(;d<(game.player_ptr->economic.science+game.player_ptr->economic.luxury)/10;d++)
    gtk_image_set_from_pixmap(GTK_IMAGE(econ_label[d]),
	get_citizen_pixmap(1), NULL); /* scientist tile */
 
   for(;d<10;d++)
    gtk_image_set_from_pixmap(GTK_IMAGE(econ_label[d]),
	get_citizen_pixmap(2), NULL); /* taxman tile */
 
  update_timeout_label();
}

/**************************************************************************
  Update the information label which gives info on the current unit and the
  square under the current unit, for specified unit.  Note that in practice
  punit is always the focus unit.
  Clears label if punit is NULL.
  Also updates the cursor for the map_canvas (this is related because the
  info label includes a "select destination" prompt etc).
  Also calls update_unit_pix_label() to update the icons for units on this
  square.
**************************************************************************/
void update_unit_info_label(struct unit *punit)
{
  if(punit) {
    char buffer[512];
    struct city *pcity =
	player_find_city_by_id(game.player_ptr, punit->homecity);
    int infrastructure =
	get_tile_infrastructure_set(map_get_tile(punit->x, punit->y));

    my_snprintf(buffer, sizeof(buffer), "%s %s", 
            unit_type(punit)->name,
            (punit->veteran) ? _("(veteran)") : "" );
    gtk_frame_set_label( GTK_FRAME(unit_info_frame), buffer);


    my_snprintf(buffer, sizeof(buffer), "%s\n%s\n%s%s%s",
		(hover_unit == punit->id) ?
		_("Select destination") : unit_activity_text(punit),
		map_get_tile_info_text(punit->x, punit->y),
		infrastructure ?
		map_get_infrastructure_text(infrastructure) : "",
		infrastructure ? "\n" : "", pcity ? pcity->name : "");
    gtk_set_label( unit_info_label, buffer);

    if (hover_unit != punit->id)
      set_hover_state(NULL, HOVER_NONE);

    switch (hover_state) {
    case HOVER_NONE:
      gdk_window_set_cursor (root_window, NULL);
      break;
    case HOVER_PATROL:
      gdk_window_set_cursor (root_window, patrol_cursor);
      break;
    case HOVER_GOTO:
    case HOVER_CONNECT:
      gdk_window_set_cursor (root_window, goto_cursor);
      break;
    case HOVER_NUKE:
      gdk_window_set_cursor (root_window, nuke_cursor);
      break;
    case HOVER_PARADROP:
      gdk_window_set_cursor (root_window, drop_cursor);
      break;
    }
  } else {
    gtk_frame_set_label( GTK_FRAME(unit_info_frame),"");
    gtk_set_label(unit_info_label,"\n\n");
    gdk_window_set_cursor(root_window, NULL);
  }
  update_unit_pix_label(punit);
}


/**************************************************************************
...
**************************************************************************/
GdkPixmap *get_thumb_pixmap(int onoff)
{
  return sprites.treaty_thumb[BOOL_VAL(onoff)]->pixmap;
}

/**************************************************************************
...
**************************************************************************/
GdkPixmap *get_citizen_pixmap(int frame)
{
  frame = CLIP(0, frame, NUM_TILES_CITIZEN-1);
  return sprites.citizen[frame]->pixmap;
}


/**************************************************************************
...
**************************************************************************/
SPRITE *get_citizen_sprite(int frame)
{
  frame = CLIP(0, frame, NUM_TILES_CITIZEN-1);
  return sprites.citizen[frame];
}


/**************************************************************************
...
**************************************************************************/
void set_indicator_icons(int bulb, int sol, int flake, int gov)
{
  struct Sprite *gov_sprite;

  bulb = CLIP(0, bulb, NUM_TILES_PROGRESS-1);
  sol = CLIP(0, sol, NUM_TILES_PROGRESS-1);
  flake = CLIP(0, flake, NUM_TILES_PROGRESS-1);

  gtk_image_set_from_pixmap(GTK_IMAGE(bulb_label),
			    sprites.bulb[bulb]->pixmap, NULL);
  gtk_image_set_from_pixmap(GTK_IMAGE(sun_label),
			    sprites.warming[sol]->pixmap, NULL);
  gtk_image_set_from_pixmap(GTK_IMAGE(flake_label),
			    sprites.cooling[flake]->pixmap, NULL);

  if (game.government_count==0) {
    /* not sure what to do here */
    gov_sprite = sprites.citizen[7]; 
  } else {
    gov_sprite = get_government(gov)->sprite;
  }
  gtk_image_set_from_pixmap(GTK_IMAGE(government_label),
			    gov_sprite->pixmap, NULL);
}


/**************************************************************************
...
**************************************************************************/
bool tile_visible_mapcanvas(int x, int y)
{
  if (is_isometric) {
    int dummy_x, dummy_y; /* well, it needs two pointers... */
    return get_canvas_xy(x, y, &dummy_x, &dummy_y);
  } else {
    return (y>=map_view_y0 && y<map_view_y0+map_canvas_store_theight &&
	    ((x>=map_view_x0 && x<map_view_x0+map_canvas_store_twidth) ||
	     (x+map.xsize>=map_view_x0 && 
	      x+map.xsize<map_view_x0+map_canvas_store_twidth)));
  }
}

/**************************************************************************
...
**************************************************************************/
bool tile_visible_and_not_on_border_mapcanvas(int x, int y)
{
  if (is_isometric) {
    int canvas_x, canvas_y;
    int width, height;
    gdk_window_get_size(map_canvas->window, &width, &height);
    get_canvas_xy(x, y, &canvas_x, &canvas_y);

    return canvas_x > NORMAL_TILE_WIDTH/2
      && canvas_x < (width - 3*NORMAL_TILE_WIDTH/2)
      && canvas_y >= NORMAL_TILE_HEIGHT
      && canvas_y < height - 3 * NORMAL_TILE_HEIGHT/2;
  } else {
    return ((y>=map_view_y0+2 || (y >= map_view_y0 && map_view_y0 == 0))
	    && (y<map_view_y0+map_canvas_store_theight-2 ||
		(y<map_view_y0+map_canvas_store_theight &&
		 map_view_y0 + map_canvas_store_theight-EXTRA_BOTTOM_ROW == map.ysize))
	    && ((x>=map_view_x0+2 && x<map_view_x0+map_canvas_store_twidth-2) ||
		(x+map.xsize>=map_view_x0+2
		 && x+map.xsize<map_view_x0+map_canvas_store_twidth-2)));
  }
}


/**************************************************************************
Animates punit's "smooth" move from (x0,y0) to (x0+dx,y0+dy).
Note: Works only for adjacent-square moves.
(Tiles need not be square.)
FIXME: The unit flickers while it is moved because we first undraw the unit
and then draw it. The correct way to do this is to have an internal pixmap
where we draw the complete scene onto, and then draw it onto the screen in
one go.
**************************************************************************/
void move_unit_map_canvas(struct unit *punit, int x0, int y0, int dx, int dy)
{
  static struct timer *anim_timer = NULL; 
  int dest_x, dest_y, is_real;

  /* only works for adjacent-square moves */
  if ((dx < -1) || (dx > 1) || (dy < -1) || (dy > 1) ||
      ((dx == 0) && (dy == 0))) {
    return;
  }

  if (punit == get_unit_in_focus() && hover_state != HOVER_NONE) {
    set_hover_state(NULL, HOVER_NONE);
    update_unit_info_label(punit);
  }

  dest_x = x0 + dx;
  dest_y = y0 + dy;
  is_real = normalize_map_pos(&dest_x, &dest_y);
  assert(is_real);

  if (player_can_see_unit(game.player_ptr, punit) &&
      (tile_visible_mapcanvas(x0, y0) ||
       tile_visible_mapcanvas(dest_x, dest_y))) {
    int i, steps;
    int start_x, start_y;
    int this_x, this_y;
    int canvas_dx, canvas_dy;

    if (is_isometric) {
      if (dx == 0) {
	canvas_dx = -NORMAL_TILE_WIDTH/2 * dy;
	canvas_dy = NORMAL_TILE_HEIGHT/2 * dy;
      } else if (dy == 0) {
	canvas_dx = NORMAL_TILE_WIDTH/2 * dx;
	canvas_dy = NORMAL_TILE_HEIGHT/2 * dx;
      } else {
	if (dx > 0) {
	  if (dy > 0) {
	    canvas_dx = 0;
	    canvas_dy = NORMAL_TILE_HEIGHT;
	  } else { /* dy < 0 */
	    canvas_dx = NORMAL_TILE_WIDTH;
	    canvas_dy = 0;
	  }
	} else { /* dx < 0 */
	  if (dy > 0) {
	    canvas_dx = -NORMAL_TILE_WIDTH;
	    canvas_dy = 0;
	  } else { /* dy < 0 */
	    canvas_dx = 0;
	    canvas_dy = -NORMAL_TILE_HEIGHT;
	  }
	}
      }
    } else {
      canvas_dx = NORMAL_TILE_WIDTH * dx;
      canvas_dy = NORMAL_TILE_HEIGHT * dy;
    }

    if (smooth_move_unit_steps < 2) {
      steps = 2;
    } else if (smooth_move_unit_steps > MAX(ABS(canvas_dx), ABS(canvas_dy))) {
      steps = MAX(ABS(canvas_dx), ABS(canvas_dy));
    } else {
      steps = smooth_move_unit_steps;
    }

    get_canvas_xy(x0, y0, &start_x, &start_y);
    if (is_isometric) {
      start_y -= NORMAL_TILE_HEIGHT/2;
    }

    this_x = start_x;
    this_y = start_y;

    for (i = 1; i <= steps; i++) {
      anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

      if (is_isometric) {
	/* FIXME: We need to draw units on tiles below the moving unit on top. */
	gdk_draw_pixmap(map_canvas->window, civ_gc, map_canvas_store,
			this_x, this_y, this_x, this_y,
			single_tile_pixmap_width, single_tile_pixmap_height);

	this_x = start_x + ((i * canvas_dx)/steps);
	this_y = start_y + ((i * canvas_dy)/steps);

	gdk_draw_pixmap(single_tile_pixmap, civ_gc, map_canvas_store,
			this_x, this_y, 0, 0,
			single_tile_pixmap_width, single_tile_pixmap_height);
	put_unit_pixmap(punit, single_tile_pixmap, 0, 0);

	gdk_draw_pixmap(map_canvas->window, civ_gc, single_tile_pixmap,
			0, 0, this_x, this_y,
			single_tile_pixmap_width, single_tile_pixmap_height);
      } else {
	gdk_draw_pixmap(map_canvas->window, civ_gc, map_canvas_store,
			this_x, this_y, this_x, this_y,
			NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);

	this_x = start_x + ((i * canvas_dx)/steps);
	this_y = start_y + ((i * canvas_dy)/steps);

	gdk_draw_pixmap(single_tile_pixmap, civ_gc, map_canvas_store,
			this_x, this_y, 0, 0,
			NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
	put_unit_pixmap(punit, single_tile_pixmap, 0, 0);

	gdk_draw_pixmap(map_canvas->window, civ_gc, single_tile_pixmap,
			0, 0, this_x, this_y,
			NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
      }

      gdk_flush();
      if (i < steps) {
	usleep_since_timer_start(anim_timer, 10000);
      }
    }
  }
}

/**************************************************************************
Centers the mapview around (x, y).

This function is almost identical between all GUI's.
**************************************************************************/
void center_tile_mapcanvas(int x, int y)
{
  base_center_tile_mapcanvas(x, y, &map_view_x0, &map_view_y0,
			     map_canvas_store_twidth,
			     map_canvas_store_theight);

  update_map_canvas_visible();
  update_map_canvas_scrollbars();
  refresh_overview_viewrect();
  if (hover_state == HOVER_GOTO || hover_state == HOVER_PATROL)
    create_line_at_mouse_pos();
}

/**************************************************************************
...
**************************************************************************/
void set_overview_dimensions(int x, int y)
{
  overview_canvas_store_width=2*x;
  overview_canvas_store_height=2*y;

  if (overview_canvas_store)
    gdk_pixmap_unref(overview_canvas_store);
  
  overview_canvas_store	= gdk_pixmap_new(root_window,
			  overview_canvas_store_width,
			  overview_canvas_store_height, -1);

  gdk_gc_set_foreground(fill_bg_gc, colors_standard[COLOR_STD_BLACK]);
  gdk_draw_rectangle(overview_canvas_store, fill_bg_gc, TRUE,
		     0, 0,
		     overview_canvas_store_width, overview_canvas_store_height);

  gtk_widget_set_usize(overview_canvas, 2*x, 2*y);
  update_map_canvas_scrollbars_size();
}

/**************************************************************************
...
**************************************************************************/
gboolean overview_canvas_expose(GtkWidget *w, GdkEventExpose *ev, gpointer data)
{
  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE) {
    if(radar_gfx_sprite)
      gdk_draw_pixmap(overview_canvas->window, civ_gc,
		      radar_gfx_sprite->pixmap, ev->area.x, ev->area.y,
		      ev->area.x, ev->area.y, ev->area.width, ev->area.height);
    return TRUE;
  }
  
  refresh_overview_viewrect();
  return TRUE;
}

/**************************************************************************
...
**************************************************************************/
static void set_overview_tile_foreground_color(int x, int y)
{
  gdk_gc_set_foreground(fill_bg_gc,
			colors_standard[overview_tile_color(x, y)]);
}

/**************************************************************************
...
**************************************************************************/
void refresh_overview_canvas(void)
{
  whole_map_iterate(x, y) {
    set_overview_tile_foreground_color(x, y);
    gdk_draw_rectangle(overview_canvas_store, fill_bg_gc, TRUE, x * 2,
		       y * 2, 2, 2);
  } whole_map_iterate_end;

  gdk_gc_set_foreground(fill_bg_gc, colors_standard[COLOR_STD_BLACK]);
}


/**************************************************************************
...
**************************************************************************/
void overview_update_tile(int x, int y)
{
  int screen_width, pos;

  if (is_isometric) {
    screen_width = map_canvas_store_twidth + map_canvas_store_theight;
  } else {
    screen_width = map_canvas_store_twidth;
  }
  pos = x + map.xsize/2 - (map_view_x0 + screen_width/2);
  
  pos %= map.xsize;
  if (pos < 0)
    pos += map.xsize;
  
  set_overview_tile_foreground_color(x, y);
  gdk_draw_rectangle(overview_canvas_store, fill_bg_gc, TRUE, x*2, y*2,
		     2, 2);
  
  gdk_draw_rectangle(overview_canvas->window, fill_bg_gc, TRUE, pos*2, y*2,
		     2, 2);
}

/**************************************************************************
...
**************************************************************************/
void refresh_overview_viewrect(void)
{
  int screen_width, delta;
  if (is_isometric) {
    screen_width = map_canvas_store_twidth + map_canvas_store_theight;
  } else {
    screen_width = map_canvas_store_twidth;
  }
  delta = map.xsize/2 - (map_view_x0 + screen_width/2);

  if (delta>=0) {
    gdk_draw_pixmap( overview_canvas->window, civ_gc, overview_canvas_store,
		0, 0, 2*delta, 0,
		overview_canvas_store_width-2*delta,
		overview_canvas_store_height );
    gdk_draw_pixmap( overview_canvas->window, civ_gc, overview_canvas_store,
		overview_canvas_store_width-2*delta, 0,
		0, 0,
		2*delta, overview_canvas_store_height );
  } else {
    gdk_draw_pixmap( overview_canvas->window, civ_gc, overview_canvas_store,
		-2*delta, 0,
		0, 0,
		overview_canvas_store_width+2*delta,
		overview_canvas_store_height );

    gdk_draw_pixmap( overview_canvas->window, civ_gc, overview_canvas_store,
		0, 0,
		overview_canvas_store_width+2*delta, 0,
		-2*delta, overview_canvas_store_height );
  }

  gdk_gc_set_foreground( civ_gc, colors_standard[COLOR_STD_WHITE] );
  
  if (is_isometric) {
    /* The x's and y's are in overview coordinates.
       All the extra factor 2's are because one tile in the overview
       is 2x2 pixels. */
    int Wx = overview_canvas_store_width/2 - screen_width /* *2/2 */;
    int Wy = map_view_y0 * 2;
    int Nx = Wx + 2 * map_canvas_store_twidth;
    int Ny = Wy - 2 * map_canvas_store_twidth;
    int Sx = Wx + 2 * map_canvas_store_theight;
    int Sy = Wy + 2 * map_canvas_store_theight;
    int Ex = Nx + 2 * map_canvas_store_theight;
    int Ey = Ny + 2 * map_canvas_store_theight;
    
    freelog(LOG_DEBUG, "wx,wy: %d,%d nx,ny:%d,%x ex,ey:%d,%d, sx,sy:%d,%d",
	    Wx, Wy, Nx, Ny, Ex, Ey, Sx, Sy);

    /* W to N */
    gdk_draw_line(overview_canvas->window, civ_gc,
		  Wx, Wy, Nx, Ny);

    /* N to E */
    gdk_draw_line(overview_canvas->window, civ_gc,
		  Nx, Ny, Ex, Ey);

    /* E to S */
    gdk_draw_line(overview_canvas->window, civ_gc,
		  Ex, Ey, Sx, Sy);

    /* S to W */
    gdk_draw_line(overview_canvas->window, civ_gc,
		  Sx, Sy, Wx, Wy);
  } else {
    gdk_draw_rectangle(overview_canvas->window, civ_gc, FALSE,
		       (overview_canvas_store_width-2*map_canvas_store_twidth)/2,
		       2*map_view_y0,
		       2*map_canvas_store_twidth, 2*map_canvas_store_theight-1);
  }
}

/**************************************************************************
...
**************************************************************************/
static bool map_center = TRUE;
static bool map_configure = FALSE;

gboolean map_canvas_configure(GtkWidget *w, GdkEventConfigure *ev,
			      gpointer data)
{
  int tile_width, tile_height;

  tile_width = (ev->width + NORMAL_TILE_WIDTH - 1) / NORMAL_TILE_WIDTH;
  tile_height = (ev->height + NORMAL_TILE_HEIGHT - 1) / NORMAL_TILE_HEIGHT;
  
  if (map_canvas_store_twidth !=tile_width ||
      map_canvas_store_theight!=tile_height) { /* resized? */

    if (map_canvas_store) {
      g_object_unref(map_canvas_store);
    }

    map_canvas_store_twidth  = tile_width;
    map_canvas_store_theight = tile_height;

    map_canvas_store = gdk_pixmap_new(ev->window,
				      tile_width  * NORMAL_TILE_WIDTH,
				      tile_height * NORMAL_TILE_HEIGHT,
				      -1);

    gdk_gc_set_foreground(fill_bg_gc, colors_standard[COLOR_STD_BLACK]);
    gdk_draw_rectangle(map_canvas_store, fill_bg_gc, TRUE, 0, 0, -1, -1);
    update_map_canvas_scrollbars_size();

    if (get_client_state() == CLIENT_GAME_RUNNING_STATE) {
      if (map.xsize) { /* do we have a map at all */
        update_map_canvas_visible();
        update_map_canvas_scrollbars();
        refresh_overview_viewrect();
      }
    }
    
    map_configure = TRUE;
  }

  return TRUE;
}

/**************************************************************************
...
**************************************************************************/
gboolean map_canvas_expose(GtkWidget *w, GdkEventExpose *ev, gpointer data)
{
  static bool cleared = FALSE;

  if (get_client_state() != CLIENT_GAME_RUNNING_STATE) {
    if (map_configure || !scaled_intro_sprite) {

      if (!intro_gfx_sprite) {
        load_intro_gfx();
      }

      if (scaled_intro_sprite) {
        free_sprite(scaled_intro_sprite);
      }

      scaled_intro_sprite = sprite_scale(intro_gfx_sprite,
      	      	      	      	      	 w->allocation.width, w->allocation.height);
    }

    if (scaled_intro_sprite) {
      gdk_draw_pixmap(map_canvas->window, civ_gc,
		      scaled_intro_sprite->pixmap,
		      ev->area.x, ev->area.y, ev->area.x, ev->area.y,
		      ev->area.width, ev->area.height);
      cleared = FALSE;
    } else {
      if (!cleared) {
        gtk_widget_queue_draw(w);
	cleared = TRUE;
      }
    }
    map_center = TRUE;
  }
  else
  {
    if (scaled_intro_sprite) {
      free_sprite(scaled_intro_sprite);
      scaled_intro_sprite = NULL;
    }

    if (map.xsize) { /* do we have a map at all */
      gdk_draw_pixmap(map_canvas->window, civ_gc, map_canvas_store,
		ev->area.x, ev->area.y, ev->area.x, ev->area.y,
		ev->area.width, ev->area.height);
      show_city_descriptions();
      cleared = FALSE;
    } else {
      if (!cleared) {
        gtk_widget_queue_draw(w);
	cleared = TRUE;
      }
    }
    refresh_overview_canvas();

    if (!map_center) {
      center_on_something();
      map_center = FALSE;
    }
  }

  map_configure = FALSE;

  return TRUE;
}

/**************************************************************************
...
**************************************************************************/
void pixmap_put_black_tile(GdkDrawable *pm,
			   int canvas_x, int canvas_y)
{
  gdk_gc_set_foreground( fill_bg_gc, colors_standard[COLOR_STD_BLACK] );
  gdk_draw_rectangle(pm, fill_bg_gc, TRUE,
		     canvas_x, canvas_y,
		     NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
}

/**************************************************************************
FIXME: Find a better way to put flags and such on top.
**************************************************************************/
static void put_unit_pixmap(struct unit *punit, GdkPixmap *pm,
			    int canvas_x, int canvas_y)
{
  int solid_bg;

  if (is_isometric) {
    struct Sprite *sprites[40];
    int count = fill_unit_sprite_array(sprites, punit, &solid_bg);
    int i;

    assert(!solid_bg);
    for (i=0; i<count; i++) {
      if (sprites[i]) {
	pixmap_put_overlay_tile(pm, canvas_x, canvas_y, sprites[i]);
      }
    }
  } else { /* is_isometric */
    struct Sprite *sprites[40];
    int count = fill_unit_sprite_array(sprites, punit, &solid_bg);

    if (count) {
      int i = 0;

      if (solid_bg) {
	gdk_gc_set_foreground(fill_bg_gc,
			      colors_standard[player_color(unit_owner(punit))]);
	gdk_draw_rectangle(pm, fill_bg_gc, TRUE,
			   canvas_x, canvas_y,
			   UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);
      } else {
	if (flags_are_transparent) {
	  pixmap_put_overlay_tile(pm, canvas_x, canvas_y, sprites[0]);
	} else {
	  gdk_draw_pixmap(pm, civ_gc, sprites[0]->pixmap,
			  0, 0, canvas_x, canvas_y,
			  sprites[0]->width, sprites[0]->height);
	}
	i++;
      }

      for (; i<count; i++) {
	if (sprites[i])
	  pixmap_put_overlay_tile(pm, canvas_x, canvas_y, sprites[i]);
      }
    }
  }
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void put_unit_pixmap_draw(struct unit *punit, GdkPixmap *pm,
				 int canvas_x, int canvas_y,
				 int offset_x, int offset_y_unit,
				 int width, int height_unit)
{
  struct Sprite *sprites[40];
  int dummy;
  int count = fill_unit_sprite_array(sprites, punit, &dummy);
  int i;

  for (i=0; i<count; i++) {
    if (sprites[i]) {
      pixmap_put_overlay_tile_draw(pm, canvas_x, canvas_y, sprites[i],
				   offset_x, offset_y_unit,
				   width, height_unit, 0);
    }
  }
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
void put_one_tile_full(GdkDrawable *pm, int x, int y,
		       int canvas_x, int canvas_y, int citymode)
{
  pixmap_put_tile_iso(pm, x, y, canvas_x, canvas_y, citymode,
		      0, 0, 0,
		      NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, UNIT_TILE_HEIGHT,
		      D_FULL);
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void put_one_tile(int x, int y, enum draw_type draw)
{
  int canvas_x, canvas_y;
  int height, width, height_unit;
  int offset_x, offset_y, offset_y_unit;

  if (!tile_visible_mapcanvas(x, y)) {
    freelog(LOG_DEBUG, "dropping %d,%d", x, y);
    return;
  }
  freelog(LOG_DEBUG, "putting %d,%d draw %x", x, y, draw);

  width = (draw & D_TMB_L) && (draw & D_TMB_R) ? NORMAL_TILE_WIDTH : NORMAL_TILE_WIDTH/2;
  if (!(draw & D_TMB_L))
    offset_x = NORMAL_TILE_WIDTH/2;
  else
    offset_x = 0;

  height = 0;
  if (draw & D_M_LR) height += NORMAL_TILE_HEIGHT/2;
  if (draw & D_B_LR) height += NORMAL_TILE_HEIGHT/2;
  if (draw & D_T_LR)
    height_unit = height + NORMAL_TILE_HEIGHT/2;
  else
    height_unit = height;

  offset_y = (draw & D_M_LR) ? 0 : NORMAL_TILE_HEIGHT/2;
  if (!(draw & D_T_LR))
    offset_y_unit = (draw & D_M_LR) ? NORMAL_TILE_HEIGHT/2 : NORMAL_TILE_HEIGHT;
  else
    offset_y_unit = 0;

  /* returns whether the tile is visible. */
  if (get_canvas_xy(x, y, &canvas_x, &canvas_y)) {
    if (normalize_map_pos(&x, &y)) {
      pixmap_put_tile_iso(map_canvas_store, x, y, canvas_x, canvas_y, 0,
			  offset_x, offset_y, offset_y_unit,
			  width, height, height_unit,
			  draw);
    } else {
      pixmap_put_black_tile_iso(map_canvas_store, canvas_x, canvas_y,
				offset_x, offset_y,
				width, height);
    }
  }
}

/**************************************************************************
Refresh and draw to sceen all the tiles in a rectangde width,height (as
seen in overhead ciew) with the top corner at x,y.
All references to "left","right", "top" and "bottom" refer to the sides of
the rectangle width, height as it would be seen in top-down view, unless
said otherwise.
The trick is to draw tiles furthest up on the map first, since we will be
drawing on top of them when we draw tiles further down.

Works by first refreshing map_canvas_store and then drawing the result to
the screen.
**************************************************************************/
void update_map_canvas(int x, int y, int width, int height, 
		       bool write_to_screen)
{
  freelog(LOG_DEBUG,
	  "update_map_canvas(pos=(%d,%d), size=(%d,%d), write_to_screen=%d)",
	  x, y, width, height, write_to_screen);

  if (is_isometric) {
    int i;
    int x_itr, y_itr;

    /*** First refresh the tiles above the area to remove the old tiles'
	 overlapping gfx ***/
    put_one_tile(x-1, y-1, D_B_LR); /* top_left corner */

    for (i=0; i<height-1; i++) { /* left side - last tile. */
      int x1 = x - 1;
      int y1 = y + i;
      put_one_tile(x1, y1, D_MB_LR);
    }
    put_one_tile(x-1, y+height-1, D_TMB_R); /* last tile left side. */

    for (i=0; i<width-1; i++) { /* top side */
      int x1 = x + i;
      int y1 = y - 1;
      put_one_tile(x1, y1, D_MB_LR);
    }
    if (width > 1) /* last tile top side. */
      put_one_tile(x+width-1, y-1, D_TMB_L);
    else
      put_one_tile(x+width-1, y-1, D_MB_L);

    /*** Now draw the tiles to be refreshed, from the top down to get the
	 overlapping areas correct ***/
    for (x_itr = x; x_itr < x+width; x_itr++) {
      for (y_itr = y; y_itr < y+height; y_itr++) {
	put_one_tile(x_itr, y_itr, D_FULL);
      }
    }

    /*** Then draw the tiles underneath to refresh the parts of them that
	 overlap onto the area just drawn ***/
    put_one_tile(x, y+height, D_TM_R);  /* bottom side */
    for (i=1; i<width; i++) {
      int x1 = x + i;
      int y1 = y + height;
      put_one_tile(x1, y1, D_TM_R);
      put_one_tile(x1, y1, D_T_L);
    }

    put_one_tile(x+width, y, D_TM_L); /* right side */
    for (i=1; i < height; i++) {
      int x1 = x + width;
      int y1 = y + i;
      put_one_tile(x1, y1, D_TM_L);
      put_one_tile(x1, y1, D_T_R);
    }

    put_one_tile(x+width, y+height, D_T_LR); /* right-bottom corner */


    /*** Draw the goto line on top of the whole thing. Done last as
	 we want it completely on top. ***/
    /* Drawing is cheap; we just draw all the lines.
       Perhaps this should be optimized, though... */
    for (x_itr = x-1; x_itr <= x+width; x_itr++) {
      for (y_itr = y-1; y_itr <= y+height; y_itr++) {
	int x1 = x_itr;
	int y1 = y_itr;
	if (normalize_map_pos(&x1, &y1)) {
	  adjc_dir_iterate(x1, y1, x2, y2, dir) {
	    if (get_drawn(x1, y1, dir)) {
	      really_draw_segment(x1, y1, dir, FALSE, TRUE);
	    }
	  } adjc_dir_iterate_end;
	}
      }
    }


    /*** Lastly draw our changes to the screen. ***/
    if (write_to_screen) {
      int canvas_start_x, canvas_start_y;
      get_canvas_xy(x, y, &canvas_start_x, &canvas_start_y); /* top left corner */
      /* top left corner in isometric view */
      canvas_start_x -= height * NORMAL_TILE_WIDTH/2;

      /* because of where get_canvas_xy() sets canvas_x */
      canvas_start_x += NORMAL_TILE_WIDTH/2;

      /* And because units fill a little extra */
      canvas_start_y -= NORMAL_TILE_HEIGHT/2;

      /* here we draw a rectangle that includes the updated tiles. */
      gdk_draw_pixmap(map_canvas->window, civ_gc, map_canvas_store,
		      canvas_start_x, canvas_start_y,
		      canvas_start_x, canvas_start_y,
		      (height + width) * NORMAL_TILE_WIDTH/2,
		      (height + width) * NORMAL_TILE_HEIGHT/2 + NORMAL_TILE_HEIGHT/2);
    }

  } else { /* is_isometric */
    int map_x, map_y;

    for (map_y = y; map_y < y + height; map_y++) {
      for (map_x = x; map_x < x + width; map_x++) {
	int canvas_x, canvas_y;

	/*
	 * We don't normalize until later because we want to draw
	 * black tiles for unreal positions.
	 */
	if (get_canvas_xy(map_x, map_y, &canvas_x, &canvas_y)) {
	  pixmap_put_tile(map_canvas_store,
			  map_x, map_y, canvas_x, canvas_y, 0);
	}
      }
    }

    if (write_to_screen) {
      int canvas_x, canvas_y;

      get_canvas_xy(x, y, &canvas_x, &canvas_y);
      gdk_draw_pixmap(map_canvas->window, civ_gc, map_canvas_store,
		      canvas_x, canvas_y,
		      canvas_x, canvas_y,
		      width*NORMAL_TILE_WIDTH,
		      height*NORMAL_TILE_HEIGHT);
    }
  }
}

/**************************************************************************
 Update display of descriptions associated with cities on the main map.
**************************************************************************/
void update_city_descriptions(void)
{
  update_map_canvas_visible();
}

/**************************************************************************
...
**************************************************************************/
static void show_desc_at_tile(PangoLayout *layout, int x, int y)
{
  static char buffer[512];
  struct city *pcity;
  if ((pcity = map_get_city(x, y))) {
    int canvas_x, canvas_y;
    PangoRectangle rect;

    get_canvas_xy(x, y, &canvas_x, &canvas_y);
    if (draw_city_names) {
      my_snprintf(buffer, sizeof(buffer), "%s", pcity->name);
      
      pango_layout_set_font_description(layout, main_font);
      pango_layout_set_text(layout, buffer, -1);

      pango_layout_get_pixel_extents(layout, &rect, NULL);
      gtk_draw_shadowed_string(map_canvas->window,
			   toplevel->style->black_gc,
			   toplevel->style->white_gc,
			   canvas_x + NORMAL_TILE_WIDTH / 2 - rect.width / 2,
			   canvas_y + NORMAL_TILE_HEIGHT +
			   PANGO_ASCENT(rect), layout);
    }

    if (draw_city_productions && (pcity->owner==game.player_idx)) {
      int y_offset;

      get_city_mapview_production(pcity, buffer, sizeof(buffer));

      if (draw_city_names)
	y_offset = rect.height + 3;
      else
	y_offset = 0;

	pango_layout_set_font_description(layout, city_productions_font);
	pango_layout_set_text(layout, buffer, -1);

	pango_layout_get_pixel_extents(layout, &rect, NULL);
	gtk_draw_shadowed_string(map_canvas->window,
			   toplevel->style->black_gc,
			   toplevel->style->white_gc,
			   canvas_x + NORMAL_TILE_WIDTH / 2 - rect.width / 2,
			   canvas_y + NORMAL_TILE_HEIGHT +
			   PANGO_ASCENT(rect) + y_offset, layout);
    }
  }
}

/**************************************************************************
...
**************************************************************************/
void show_city_descriptions(void)
{
  PangoLayout *layout;

  if (!draw_city_names && !draw_city_productions)
    return;

  layout = pango_layout_new(gdk_pango_context_get());

  if (is_isometric ) {
    int x, y;
    int w, h;

    for (h=-1; h<map_canvas_store_theight*2; h++) {
      int x_base = map_view_x0 + h/2 + (h != -1 ? h%2 : 0);
      int y_base = map_view_y0 + h/2 + (h == -1 ? -1 : 0);
      for (w=0; w<=map_canvas_store_twidth; w++) {
	x = (x_base + w);
	y = y_base - w;
	if (normalize_map_pos(&x, &y)) {
	  show_desc_at_tile(layout, x, y);
	}
      }
    }
  } else { /* is_isometric */
    int x1, y1;
    for (x1 = 0; x1 < map_canvas_store_twidth; x1++) {
      int x = map_view_x0 + x1;
      for (y1 = 0; y1 < map_canvas_store_theight; y1++) {
	int y = map_view_y0 + y1;

	if (normalize_map_pos(&x, &y)) {
	  show_desc_at_tile(layout, x, y);
	}
      }
    }
  }

  gdk_gc_set_clip_rectangle(toplevel->style->black_gc, NULL);
  gdk_gc_set_clip_rectangle(toplevel->style->white_gc, NULL);
  g_object_unref(layout);
}

/**************************************************************************
...
**************************************************************************/
void put_city_tile_output(GdkDrawable *pm, int canvas_x, int canvas_y, 
			  int food, int shield, int trade)
{
  food = CLIP(0, food, NUM_TILES_DIGITS-1);
  trade = CLIP(0, trade, NUM_TILES_DIGITS-1);
  shield = CLIP(0, shield, NUM_TILES_DIGITS-1);
  
  if (is_isometric) {
    canvas_x += NORMAL_TILE_WIDTH/3;
    canvas_y -= NORMAL_TILE_HEIGHT/3;
  }

  pixmap_put_overlay_tile(pm, canvas_x, canvas_y,
			  sprites.city.tile_foodnum[food]);
  pixmap_put_overlay_tile(pm, canvas_x, canvas_y,
			  sprites.city.tile_shieldnum[shield]);
  pixmap_put_overlay_tile(pm, canvas_x, canvas_y,
			  sprites.city.tile_tradenum[trade]);
}

/**************************************************************************
...
**************************************************************************/
void put_unit_gpixmap(struct unit *punit, GtkPixcomm *p)
{
  struct Sprite *sprites[40];
  int solid_bg;
  int count = fill_unit_sprite_array(sprites, punit, &solid_bg);

  gtk_pixcomm_freeze(p);
  gtk_pixcomm_clear(p);

  if (count) {
    int i;

    if (solid_bg) {
      gtk_pixcomm_fill(p, colors_standard[player_color(unit_owner(punit))]);
    }

    for (i=0;i<count;i++) {
      if (sprites[i])
        put_overlay_tile_gpixmap(p, 0, 0, sprites[i]);
    }
  }

  gtk_pixcomm_thaw(p);
}


/**************************************************************************
  FIXME:
  For now only two food, one shield and two masks can be drawn per unit,
  the proper way to do this is probably something like what Civ II does.
  (One food/shield/mask drawn N times, possibly one top of itself. -- SKi 
**************************************************************************/
void put_unit_gpixmap_city_overlays(struct unit *punit, GtkPixcomm *p)
{
  int upkeep_food = CLIP(0, punit->upkeep_food, 2);
  int unhappy = CLIP(0, punit->unhappiness, 2);
 
  gtk_pixcomm_freeze(p);

  /* draw overlay pixmaps */
  if (punit->upkeep > 0)
    put_overlay_tile_gpixmap(p, 0, NORMAL_TILE_HEIGHT, sprites.upkeep.shield);
  if (upkeep_food > 0)
    put_overlay_tile_gpixmap(p, 0, NORMAL_TILE_HEIGHT, sprites.upkeep.food[upkeep_food-1]);
  if (unhappy > 0)
    put_overlay_tile_gpixmap(p, 0, NORMAL_TILE_HEIGHT, sprites.upkeep.unhappy[unhappy-1]);

  gtk_pixcomm_thaw(p);
}

/**************************************************************************
...
**************************************************************************/
void put_nuke_mushroom_pixmaps(int x, int y)
{
  if (is_isometric) {
    int canvas_x, canvas_y;
    struct Sprite *mysprite = sprites.explode.iso_nuke;

    get_canvas_xy(x, y, &canvas_x, &canvas_y);
    canvas_x += NORMAL_TILE_WIDTH/2 - mysprite->width/2;
    canvas_y += NORMAL_TILE_HEIGHT/2 - mysprite->height/2;

    pixmap_put_overlay_tile(map_canvas->window, canvas_x, canvas_y,
			    mysprite);

    gdk_flush();
    sleep(1);

    update_map_canvas_visible();
  } else {
    int x_itr, y_itr;
    int canvas_x, canvas_y;

    for (y_itr=0; y_itr<3; y_itr++) {
      for (x_itr=0; x_itr<3; x_itr++) {
	struct Sprite *mysprite = sprites.explode.nuke[y_itr][x_itr];
	get_canvas_xy(x + x_itr - 1, y + y_itr - 1, &canvas_x, &canvas_y);

	gdk_draw_pixmap(single_tile_pixmap, civ_gc, map_canvas_store,
			canvas_x, canvas_y, 0, 0,
			NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
	pixmap_put_overlay_tile(single_tile_pixmap, 0, 0, mysprite);
	gdk_draw_pixmap(map_canvas->window, civ_gc, single_tile_pixmap,
			0, 0, canvas_x, canvas_y,
			NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
      }
    }

    gdk_flush();
    sleep(1);

    update_map_canvas(x-1, y-1, 3, 3, TRUE);
  }
}

/**************************************************************************
canvas_x, canvas_y is the top left corner of the pixmap.
**************************************************************************/
void pixmap_frame_tile_red(GdkDrawable *pm,
			   int canvas_x, int canvas_y)
{
  if (is_isometric) {
    gdk_gc_set_foreground(thick_line_gc, colors_standard[COLOR_STD_RED]);

    gdk_draw_line(pm, thick_line_gc,
		  canvas_x+NORMAL_TILE_WIDTH/2-1, canvas_y,
		  canvas_x+NORMAL_TILE_WIDTH-1, canvas_y+NORMAL_TILE_HEIGHT/2-1);
    gdk_draw_line(pm, thick_line_gc,
		  canvas_x+NORMAL_TILE_WIDTH-1, canvas_y+NORMAL_TILE_HEIGHT/2-1,
		  canvas_x+NORMAL_TILE_WIDTH/2-1, canvas_y+NORMAL_TILE_HEIGHT-1);
    gdk_draw_line(pm, thick_line_gc,
		  canvas_x+NORMAL_TILE_WIDTH/2-1, canvas_y+NORMAL_TILE_HEIGHT-1,
		  canvas_x, canvas_y + NORMAL_TILE_HEIGHT/2-1);
    gdk_draw_line(pm, thick_line_gc,
		  canvas_x, canvas_y + NORMAL_TILE_HEIGHT/2-1,
		  canvas_x+NORMAL_TILE_WIDTH/2-1, canvas_y);
  } else {
    gdk_gc_set_foreground(fill_bg_gc, colors_standard[COLOR_STD_RED]);

    gdk_draw_rectangle(pm, fill_bg_gc, FALSE,
		       canvas_x, canvas_y,
		       NORMAL_TILE_WIDTH-1, NORMAL_TILE_HEIGHT-1);
  }
}

/**************************************************************************
...
**************************************************************************/
static void put_overlay_tile_gpixmap(GtkPixcomm *p, int canvas_x, int canvas_y,
				     struct Sprite *ssprite)
{
  if (!ssprite)
    return;

  gtk_pixcomm_copyto(p, ssprite, canvas_x, canvas_y);
}

/**************************************************************************
...
**************************************************************************/
static void pixmap_put_overlay_tile(GdkDrawable *pixmap,
				    int canvas_x, int canvas_y,
				    struct Sprite *ssprite)
{
  if (!ssprite)
    return;
      
  gdk_gc_set_clip_origin(civ_gc, canvas_x, canvas_y);
  gdk_gc_set_clip_mask(civ_gc, ssprite->mask);

  gdk_draw_pixmap(pixmap, civ_gc, ssprite->pixmap,
		  0, 0,
		  canvas_x, canvas_y,
		  ssprite->width, ssprite->height);
  gdk_gc_set_clip_mask(civ_gc, NULL);
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_overlay_tile_draw(GdkDrawable *pixmap,
					 int canvas_x, int canvas_y,
					 struct Sprite *ssprite,
					 int offset_x, int offset_y,
					 int width, int height,
					 int fog)
{
  if (!ssprite || !width || !height)
    return;

  gdk_gc_set_clip_origin(civ_gc, canvas_x, canvas_y);
  gdk_gc_set_clip_mask(civ_gc, ssprite->mask);

  gdk_draw_pixmap(pixmap, civ_gc, ssprite->pixmap,
		  offset_x, offset_y,
		  canvas_x+offset_x, canvas_y+offset_y,
		  MIN(width, MAX(0, ssprite->width-offset_x)),
		  MIN(height, MAX(0, ssprite->height-offset_y)));
  gdk_gc_set_clip_mask(civ_gc, NULL);

  /* I imagine this could be done more efficiently. Some pixels We first
     draw from the sprite, and then draw black afterwards. It would be much
     faster to just draw every second pixel black in the first place. */
  if (fog) {
    gdk_gc_set_clip_origin(fill_tile_gc, canvas_x, canvas_y);
    gdk_gc_set_clip_mask(fill_tile_gc, ssprite->mask);
    gdk_gc_set_foreground(fill_tile_gc, colors_standard[COLOR_STD_BLACK]);
    gdk_gc_set_stipple(fill_tile_gc, black50);

    gdk_draw_rectangle(pixmap, fill_tile_gc, TRUE,
		       canvas_x+offset_x, canvas_y+offset_y,
		       MIN(width, MAX(0, ssprite->width-offset_x)),
		       MIN(height, MAX(0, ssprite->height-offset_y)));
    gdk_gc_set_clip_mask(fill_tile_gc, NULL);
  }
}

/**************************************************************************
 Draws a cross-hair overlay on a tile
**************************************************************************/
void put_cross_overlay_tile(int x, int y)
{
  int canvas_x, canvas_y;
  get_canvas_xy(x, y, &canvas_x, &canvas_y);

  if (tile_visible_mapcanvas(x, y)) {
    pixmap_put_overlay_tile(map_canvas->window,
			    canvas_x, canvas_y,
			    sprites.user.attention);
  }
}

/**************************************************************************
...
**************************************************************************/
void put_city_workers(struct city *pcity, int color)
{
  int canvas_x, canvas_y;
  static struct city *last_pcity=NULL;

  if (color==-1) {
    if (pcity!=last_pcity)
      city_workers_color = city_workers_color%3 + 1;
    color=city_workers_color;
  }
  gdk_gc_set_foreground(fill_tile_gc, colors_standard[color]);

  city_map_checked_iterate(pcity->x, pcity->y, i, j, x, y) {
    enum city_tile_type worked = get_worker_city(pcity, i, j);

    get_canvas_xy(x, y, &canvas_x, &canvas_y);

    /* stipple the area */
    if (!is_city_center(i, j)) {
      if (worked == C_TILE_EMPTY) {
	gdk_gc_set_stipple(fill_tile_gc, gray25);
      } else if (worked == C_TILE_WORKER) {
	gdk_gc_set_stipple(fill_tile_gc, gray50);
      } else
	continue;

      if (is_isometric) {
	gdk_gc_set_clip_origin(fill_tile_gc, canvas_x, canvas_y);
	gdk_gc_set_clip_mask(fill_tile_gc, sprites.black_tile->mask);
	gdk_draw_pixmap(map_canvas->window, fill_tile_gc, map_canvas_store,
			canvas_x, canvas_y,
			canvas_x, canvas_y,
			NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
	gdk_draw_rectangle(map_canvas->window, fill_tile_gc, TRUE,
			   canvas_x, canvas_y,
			   NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
	gdk_gc_set_clip_mask(fill_tile_gc, NULL);
      } else {
	gdk_draw_pixmap(map_canvas->window, civ_gc, map_canvas_store,
			canvas_x, canvas_y,
			canvas_x, canvas_y,
			NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
	gdk_draw_rectangle(map_canvas->window, fill_tile_gc, TRUE,
			   canvas_x, canvas_y,
			   NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
      }
    }

    /* draw tile output */
    if (worked == C_TILE_WORKER) {
      put_city_tile_output(map_canvas->window,
			   canvas_x, canvas_y,
			   city_get_food_tile(i, j, pcity),
			   city_get_shields_tile(i, j, pcity),
			   city_get_trade_tile(i, j, pcity));
    }
  } city_map_checked_iterate_end;

  last_pcity=pcity;
}

/**************************************************************************
...
**************************************************************************/
void update_map_canvas_scrollbars(void)
{
  gtk_adjustment_set_value(GTK_ADJUSTMENT(map_hadj), map_view_x0);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(map_vadj), map_view_y0);
}

/**************************************************************************
...
**************************************************************************/
void update_map_canvas_scrollbars_size(void)
{
  map_hadj=gtk_adjustment_new(-1, 0, map.xsize, 1,
	   map_canvas_store_twidth, map_canvas_store_twidth);
  map_vadj=gtk_adjustment_new(-1, 0, map.ysize+EXTRA_BOTTOM_ROW, 1,
	   map_canvas_store_theight, map_canvas_store_theight);
  gtk_range_set_adjustment(GTK_RANGE(map_horizontal_scrollbar),
	GTK_ADJUSTMENT(map_hadj));
  gtk_range_set_adjustment(GTK_RANGE(map_vertical_scrollbar),
	GTK_ADJUSTMENT(map_vadj));

  gtk_signal_connect(GTK_OBJECT(map_hadj), "value_changed",
	GTK_SIGNAL_FUNC(scrollbar_jump_callback),
	GINT_TO_POINTER(TRUE));
  gtk_signal_connect(GTK_OBJECT(map_vadj), "value_changed",
	GTK_SIGNAL_FUNC(scrollbar_jump_callback),
	GINT_TO_POINTER(FALSE));
}

/**************************************************************************
...
**************************************************************************/
void scrollbar_jump_callback(GtkAdjustment *adj, gpointer hscrollbar)
{
  int last_map_view_x0;
  int last_map_view_y0;

  gfloat percent=adj->value;

  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE)
     return;

  last_map_view_x0=map_view_x0;
  last_map_view_y0=map_view_y0;

  if(hscrollbar)
    map_view_x0=percent;
  else {
    map_view_y0=percent;
    map_view_y0=(map_view_y0<0) ? 0 : map_view_y0;
    map_view_y0=
      (map_view_y0>map.ysize+EXTRA_BOTTOM_ROW-map_canvas_store_theight) ? 
      map.ysize+EXTRA_BOTTOM_ROW-map_canvas_store_theight :
      map_view_y0;
  }

  if (last_map_view_x0!=map_view_x0 || last_map_view_y0!=map_view_y0) {
    update_map_canvas_visible();
    refresh_overview_viewrect();
  }
}

  
/**************************************************************************
draw a line from src_x,src_y -> dest_x,dest_y on both map_canvas and
map_canvas_store
FIXME: We currently always draw the line.
Only used for isometric view.
**************************************************************************/
static void really_draw_segment(int src_x, int src_y, int dir,
				bool write_to_screen, bool force)
{
  int dest_x, dest_y, is_real;
  int canvas_start_x, canvas_start_y;
  int canvas_end_x, canvas_end_y;

  gdk_gc_set_foreground(thick_line_gc, colors_standard[COLOR_STD_CYAN]);

  is_real = MAPSTEP(dest_x, dest_y, src_x, src_y, dir);
  assert(is_real);

  /* Find middle of tiles. y-1 to not undraw the the middle pixel of a
     horizontal line when we refresh the tile below-between. */
  get_canvas_xy(src_x, src_y, &canvas_start_x, &canvas_start_y);
  get_canvas_xy(dest_x, dest_y, &canvas_end_x, &canvas_end_y);
  canvas_start_x += NORMAL_TILE_WIDTH/2;
  canvas_start_y += NORMAL_TILE_HEIGHT/2-1;
  canvas_end_x += NORMAL_TILE_WIDTH/2;
  canvas_end_y += NORMAL_TILE_HEIGHT/2-1;

  /* somewhat hackish way of solving the problem where draw from a tile on
     one side of the screen out of the screen, and the tile we draw to is
     found to be on the other side of the screen. */
  if (abs(canvas_end_x - canvas_start_x) > NORMAL_TILE_WIDTH
      || abs(canvas_end_y - canvas_start_y) > NORMAL_TILE_HEIGHT)
    return;

  /* draw it! */
  gdk_draw_line(map_canvas_store, thick_line_gc,
		canvas_start_x, canvas_start_y, canvas_end_x, canvas_end_y);
  if (write_to_screen)
    gdk_draw_line(map_canvas->window, thick_line_gc,
		  canvas_start_x, canvas_start_y, canvas_end_x, canvas_end_y);
  return;
}

/**************************************************************************
...
**************************************************************************/
void draw_segment(int src_x, int src_y, int dir)
{
  if (is_isometric) {
    increment_drawn(src_x, src_y, dir);
    if (get_drawn(src_x, src_y, dir) > 1) {
      return;
    } else {
      really_draw_segment(src_x, src_y, dir, TRUE, FALSE);
    }
  } else {
    int dest_x, dest_y, is_real;

    is_real = MAPSTEP(dest_x, dest_y, src_x, src_y, dir);
    assert(is_real);

    /* A previous line already marks the place */
    if (get_drawn(src_x, src_y, dir)) {
      increment_drawn(src_x, src_y, dir);
      return;
    }

    if (tile_visible_mapcanvas(src_x, src_y)) {
      put_line(map_canvas_store, src_x, src_y, dir);
      put_line(map_canvas->window, src_x, src_y, dir);
    }
    if (tile_visible_mapcanvas(dest_x, dest_y)) {
      put_line(map_canvas_store, dest_x, dest_y, DIR_REVERSE(dir));
      put_line(map_canvas->window, dest_x, dest_y, DIR_REVERSE(dir));
    }

    increment_drawn(src_x, src_y, dir);
  }
}

/**************************************************************************
remove the line from src_x,src_y -> dest_x,dest_y on both map_canvas and
map_canvas_store.
**************************************************************************/
void undraw_segment(int src_x, int src_y, int dir)
{
  int dest_x, dest_y, is_real;

  is_real = MAPSTEP(dest_x, dest_y, src_x, src_y, dir);
  assert(is_real);

  if (is_isometric) {
    assert(get_drawn(src_x, src_y, dir));
    decrement_drawn(src_x, src_y, dir);

    /* somewhat inefficient */
    if (!get_drawn(src_x, src_y, dir)) {
      update_map_canvas(MIN(src_x, dest_x), MIN(src_y, dest_y),
			src_x == dest_x ? 1 : 2,
			src_y == dest_y ? 1 : 2,
			TRUE);
    }
  } else {
    int drawn = get_drawn(src_x, src_y, dir);

    assert(drawn > 0);
    /* If we walk on a path twice it looks just like walking on it once. */
    if (drawn > 1) {
      decrement_drawn(src_x, src_y, dir);
      return;
    }

    decrement_drawn(src_x, src_y, dir);
    refresh_tile_mapcanvas(src_x, src_y, TRUE);
    refresh_tile_mapcanvas(dest_x, dest_y, TRUE);
    if (NORMAL_TILE_WIDTH%2 == 0 || NORMAL_TILE_HEIGHT%2 == 0) {
      int is_real;

      if (dir == DIR8_NORTHEAST) {
	/* Since the tile doesn't have a middle we draw an extra pixel
	   on the adjacent tile when drawing in this direction. */
	dest_x = src_x + 1;
	dest_y = src_y;
	is_real = normalize_map_pos(&dest_x, &dest_y);
	assert(is_real);
	refresh_tile_mapcanvas(dest_x, dest_y, TRUE);
      } else if (dir == DIR8_SOUTHWEST) {	/* the same */
	dest_x = src_x;
	dest_y = src_y + 1;
	is_real = normalize_map_pos(&dest_x, &dest_y);
	assert(is_real);
	refresh_tile_mapcanvas(dest_x, dest_y, TRUE);
      }
    }
  }
}

/**************************************************************************
Not used in isometric view.
**************************************************************************/
static void put_line(GdkDrawable *pm, int x, int y, int dir)
{
  int canvas_src_x, canvas_src_y, canvas_dest_x, canvas_dest_y;
  get_canvas_xy(x, y, &canvas_src_x, &canvas_src_y);
  canvas_src_x += NORMAL_TILE_WIDTH/2;
  canvas_src_y += NORMAL_TILE_HEIGHT/2;
  DIRSTEP(canvas_dest_x, canvas_dest_y, dir);
  canvas_dest_x = canvas_src_x + (NORMAL_TILE_WIDTH * canvas_dest_x) / 2;
  canvas_dest_y = canvas_src_y + (NORMAL_TILE_WIDTH * canvas_dest_y) / 2;

  gdk_gc_set_foreground(civ_gc, colors_standard[COLOR_STD_CYAN]);

  gdk_draw_line(pm, civ_gc,
		canvas_src_x, canvas_src_y,
		canvas_dest_x, canvas_dest_y);
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void put_city_pixmap_draw(struct city *pcity, GdkPixmap *pm,
				 int canvas_x, int canvas_y,
				 int offset_x, int offset_y_unit,
				 int width, int height_unit,
				 int fog)
{
  struct Sprite *sprites[80];
  int count = fill_city_sprite_array_iso(sprites, pcity);
  int i;

  for (i=0; i<count; i++) {
    if (sprites[i]) {
      pixmap_put_overlay_tile_draw(pm, canvas_x, canvas_y, sprites[i],
				   offset_x, offset_y_unit,
				   width, height_unit,
				   fog);
    }
  }
}
/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_black_tile_iso(GdkDrawable *pm,
				      int canvas_x, int canvas_y,
				      int offset_x, int offset_y,
				      int width, int height)
{
  gdk_gc_set_clip_origin(civ_gc, canvas_x, canvas_y);
  gdk_gc_set_clip_mask(civ_gc, sprites.black_tile->mask);

  assert(width <= NORMAL_TILE_WIDTH);
  assert(height <= NORMAL_TILE_HEIGHT);
  gdk_draw_pixmap(pm, civ_gc, sprites.black_tile->pixmap,
		  offset_x, offset_y,
		  canvas_x+offset_x, canvas_y+offset_y,
		  width, height);

  gdk_gc_set_clip_mask(civ_gc, NULL);
}

/**************************************************************************
Blend the tile with neighboring tiles.
Only used for isometric view.
**************************************************************************/
static void dither_tile(GdkDrawable *pixmap, struct Sprite **dither,
			int canvas_x, int canvas_y,
			int offset_x, int offset_y,
			int width, int height, int fog)
{
  if (!width || !height)
    return;

  gdk_gc_set_clip_mask(civ_gc, sprites.dither_tile->mask);
  gdk_gc_set_clip_origin(civ_gc, canvas_x, canvas_y);
  assert(offset_x == 0 || offset_x == NORMAL_TILE_WIDTH/2);
  assert(offset_y == 0 || offset_y == NORMAL_TILE_HEIGHT/2);
  assert(width == NORMAL_TILE_WIDTH || width == NORMAL_TILE_WIDTH/2);
  assert(height == NORMAL_TILE_HEIGHT || height == NORMAL_TILE_HEIGHT/2);

  /* north */
  if (dither[0]
      && (offset_x != 0 || width == NORMAL_TILE_WIDTH)
      && (offset_y == 0)) {
    gdk_draw_pixmap(pixmap, civ_gc, dither[0]->pixmap,
		    NORMAL_TILE_WIDTH/2, 0,
		    canvas_x + NORMAL_TILE_WIDTH/2, canvas_y,
		    NORMAL_TILE_WIDTH/2, NORMAL_TILE_HEIGHT/2);
  }

  /* south */
  if (dither[1] && offset_x == 0
      && (offset_y == NORMAL_TILE_HEIGHT/2 || height == NORMAL_TILE_HEIGHT)) {
    gdk_draw_pixmap(pixmap, civ_gc, dither[1]->pixmap,
		    0, NORMAL_TILE_HEIGHT/2,
		    canvas_x,
		    canvas_y + NORMAL_TILE_HEIGHT/2,
		    NORMAL_TILE_WIDTH/2, NORMAL_TILE_HEIGHT/2);
  }

  /* east */
  if (dither[2]
      && (offset_x != 0 || width == NORMAL_TILE_WIDTH)
      && (offset_y != 0 || height == NORMAL_TILE_HEIGHT)) {
    gdk_draw_pixmap(pixmap, civ_gc, dither[2]->pixmap,
		    NORMAL_TILE_WIDTH/2, NORMAL_TILE_HEIGHT/2,
		    canvas_x + NORMAL_TILE_WIDTH/2,
		    canvas_y + NORMAL_TILE_HEIGHT/2,
		    NORMAL_TILE_WIDTH/2, NORMAL_TILE_HEIGHT/2);
  }

  /* west */
  if (dither[3] && offset_x == 0 && offset_y == 0) {
    gdk_draw_pixmap(pixmap, civ_gc, dither[3]->pixmap,
		    0, 0,
		    canvas_x,
		    canvas_y,
		    NORMAL_TILE_WIDTH/2, NORMAL_TILE_HEIGHT/2);
  }

  gdk_gc_set_clip_mask(civ_gc, NULL);

  if (fog) {
    gdk_gc_set_clip_origin(fill_tile_gc, canvas_x, canvas_y);
    gdk_gc_set_clip_mask(fill_tile_gc, sprites.dither_tile->mask);
    gdk_gc_set_foreground(fill_tile_gc, colors_standard[COLOR_STD_BLACK]);
    gdk_gc_set_stipple(fill_tile_gc, black50);

    gdk_draw_rectangle(pixmap, fill_tile_gc, TRUE,
		       canvas_x+offset_x, canvas_y+offset_y,
		       MIN(width, MAX(0, NORMAL_TILE_WIDTH-offset_x)),
		       MIN(height, MAX(0, NORMAL_TILE_HEIGHT-offset_y)));
    gdk_gc_set_clip_mask(fill_tile_gc, NULL);
  }
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_tile_iso(GdkDrawable *pm, int x, int y,
				int canvas_x, int canvas_y,
				int citymode,
				int offset_x, int offset_y, int offset_y_unit,
				int width, int height, int height_unit,
				enum draw_type draw)
{
  struct Sprite *tile_sprs[80];
  struct Sprite *coasts[4];
  struct Sprite *dither[4];
  struct city *pcity;
  struct unit *punit, *pfocus;
  enum tile_special_type special;
  int count, i = 0;
  int fog;
  int solid_bg;

  if (!width || !(height || height_unit))
    return;

  count = fill_tile_sprite_array_iso(tile_sprs, coasts, dither,
				     x, y, citymode, &solid_bg);

  if (count == -1) { /* tile is unknown */
    pixmap_put_black_tile_iso(pm, canvas_x, canvas_y,
			      offset_x, offset_y, width, height);
    return;
  }

  /* Replace with check for is_normal_tile later */
  assert(is_real_tile(x, y));
  normalize_map_pos(&x, &y);

  fog = tile_get_known(x, y) == TILE_KNOWN_FOGGED && draw_fog_of_war;
  pcity = map_get_city(x, y);
  punit = get_drawable_unit(x, y, citymode);
  pfocus = get_unit_in_focus();
  special = map_get_special(x, y);

  if (solid_bg) {
    gdk_gc_set_clip_origin(fill_bg_gc, canvas_x, canvas_y);
    gdk_gc_set_clip_mask(fill_bg_gc, sprites.black_tile->mask);
    gdk_gc_set_foreground(fill_bg_gc, colors_standard[COLOR_STD_BACKGROUND]);

    gdk_draw_rectangle(pm, fill_bg_gc, TRUE,
		       canvas_x+offset_x, canvas_y+offset_y,
		       MIN(width, MAX(0, sprites.black_tile->width-offset_x)),
		       MIN(height, MAX(0, sprites.black_tile->height-offset_y)));
    gdk_gc_set_clip_mask(fill_bg_gc, NULL);
    if (fog) {
      gdk_gc_set_clip_origin(fill_tile_gc, canvas_x, canvas_y);
      gdk_gc_set_clip_mask(fill_tile_gc, sprites.black_tile->mask);
      gdk_gc_set_foreground(fill_tile_gc, colors_standard[COLOR_STD_BLACK]);
      gdk_gc_set_stipple(fill_tile_gc, black50);

      gdk_draw_rectangle(pm, fill_tile_gc, TRUE,
			 canvas_x+offset_x, canvas_y+offset_y,
			 MIN(width, MAX(0, sprites.black_tile->width-offset_x)),
			 MIN(height, MAX(0, sprites.black_tile->height-offset_y)));
      gdk_gc_set_clip_mask(fill_tile_gc, NULL);
    }
  }

  if (draw_terrain) {
    if (map_get_terrain(x, y) == T_OCEAN) { /* coasts */
      int dx, dy;
      /* top */
      dx = offset_x-NORMAL_TILE_WIDTH/4;
      pixmap_put_overlay_tile_draw(pm, canvas_x + NORMAL_TILE_WIDTH/4,
				   canvas_y, coasts[0],
				   MAX(0, dx),
				   offset_y,
				   MAX(0, width-MAX(0, -dx)),
				   height,
				   fog);
      /* bottom */
      dx = offset_x-NORMAL_TILE_WIDTH/4;
      dy = offset_y-NORMAL_TILE_HEIGHT/2;
      pixmap_put_overlay_tile_draw(pm, canvas_x + NORMAL_TILE_WIDTH/4,
				   canvas_y + NORMAL_TILE_HEIGHT/2, coasts[1],
				   MAX(0, dx),
				   MAX(0, dy),
				   MAX(0, width-MAX(0, -dx)),
				   MAX(0, height-MAX(0, -dy)),
				   fog);
      /* left */
      dy = offset_y-NORMAL_TILE_HEIGHT/4;
      pixmap_put_overlay_tile_draw(pm, canvas_x,
				   canvas_y + NORMAL_TILE_HEIGHT/4, coasts[2],
				   offset_x,
				   MAX(0, dy),
				   width,
				   MAX(0, height-MAX(0, -dy)),
				   fog);
      /* right */
      dx = offset_x-NORMAL_TILE_WIDTH/2;
      dy = offset_y-NORMAL_TILE_HEIGHT/4;
      pixmap_put_overlay_tile_draw(pm, canvas_x + NORMAL_TILE_WIDTH/2,
				   canvas_y + NORMAL_TILE_HEIGHT/4, coasts[3],
				   MAX(0, dx),
				   MAX(0, dy),
				   MAX(0, width-MAX(0, -dx)),
				   MAX(0, height-MAX(0, -dy)),
				   fog);
    } else {
      pixmap_put_overlay_tile_draw(pm, canvas_x, canvas_y, tile_sprs[0],
				   offset_x, offset_y, width, height, fog);
      i++;
    }

    /*** Dither base terrain ***/
    if (draw_terrain)
      dither_tile(pm, dither, canvas_x, canvas_y,
		  offset_x, offset_y, width, height, fog);
  }

  /*** Rest of terrain and specials ***/
  for (; i<count; i++) {
    if (tile_sprs[i])
      pixmap_put_overlay_tile_draw(pm, canvas_x, canvas_y, tile_sprs[i],
				   offset_x, offset_y, width, height, fog);
    else
      freelog(LOG_ERROR, "sprite is NULL");
  }

  /*** Map grid ***/
  if (draw_map_grid) {
    /* we draw the 2 lines on top of the tile; the buttom lines will be
       drawn by the tiles underneath. */
    if (draw & D_M_R) {
      gdk_gc_set_foreground(thin_line_gc,
			    colors_standard[get_grid_color
					    (x, y, x, y - 1)]);
      gdk_draw_line(pm, thin_line_gc,
		    canvas_x + NORMAL_TILE_WIDTH / 2, canvas_y,
		    canvas_x + NORMAL_TILE_WIDTH,
		    canvas_y + NORMAL_TILE_HEIGHT / 2);
    }

    if (draw & D_M_L) {
      gdk_gc_set_foreground(thin_line_gc,
			    colors_standard[get_grid_color
					    (x, y, x - 1, y)]);
      gdk_draw_line(pm, thin_line_gc,
		    canvas_x, canvas_y + NORMAL_TILE_HEIGHT / 2,
		    canvas_x + NORMAL_TILE_WIDTH / 2, canvas_y);
    }
  }

  if (draw_coastline && !draw_terrain) {
    enum tile_terrain_type t1 = map_get_terrain(x, y), t2;
    int x1, y1;
    gdk_gc_set_foreground(thin_line_gc, colors_standard[COLOR_STD_OCEAN]);
    x1 = x; y1 = y-1;
    if (normalize_map_pos(&x1, &y1)) {
      t2 = map_get_terrain(x1, y1);
      if (draw & D_M_R && ((t1 == T_OCEAN) ^ (t2 == T_OCEAN)))
	gdk_draw_line(pm, thin_line_gc,
		      canvas_x+NORMAL_TILE_WIDTH/2, canvas_y,
		      canvas_x+NORMAL_TILE_WIDTH, canvas_y+NORMAL_TILE_HEIGHT/2);
    }
    x1 = x-1; y1 = y;
    if (normalize_map_pos(&x1, &y1)) {
      t2 = map_get_terrain(x1, y1);
      if (draw & D_M_L && ((t1 == T_OCEAN) ^ (t2 == T_OCEAN)))
	gdk_draw_line(pm, thin_line_gc,
		      canvas_x, canvas_y + NORMAL_TILE_HEIGHT/2,
		      canvas_x+NORMAL_TILE_WIDTH/2, canvas_y);
    }
  }

  /*** City and various terrain improvements ***/
  if (pcity && draw_cities) {
    put_city_pixmap_draw(pcity, pm,
			 canvas_x, canvas_y - NORMAL_TILE_HEIGHT/2,
			 offset_x, offset_y_unit,
			 width, height_unit, fog);
  }
  if (contains_special(special, S_AIRBASE) && draw_fortress_airbase)
    pixmap_put_overlay_tile_draw(pm,
				 canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				 sprites.tx.airbase,
				 offset_x, offset_y_unit,
				 width, height_unit, fog);
  if (contains_special(special, S_FALLOUT) && draw_pollution)
    pixmap_put_overlay_tile_draw(pm,
				 canvas_x, canvas_y,
				 sprites.tx.fallout,
				 offset_x, offset_y,
				 width, height, fog);
  if (contains_special(special, S_POLLUTION) && draw_pollution)
    pixmap_put_overlay_tile_draw(pm,
				 canvas_x, canvas_y,
				 sprites.tx.pollution,
				 offset_x, offset_y,
				 width, height, fog);

  /*** city size ***/
  /* Not fogged as it would be unreadable */
  if (pcity && draw_cities) {
    if (pcity->size>=10)
      pixmap_put_overlay_tile_draw(pm, canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				   sprites.city.size_tens[pcity->size/10],
				   offset_x, offset_y_unit,
				   width, height_unit, 0);

    pixmap_put_overlay_tile_draw(pm, canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				 sprites.city.size[pcity->size%10],
				 offset_x, offset_y_unit,
				 width, height_unit, 0);
  }

  /*** Unit ***/
  if (punit && (draw_units || (punit == pfocus && draw_focus_unit))) {
    put_unit_pixmap_draw(punit, pm,
			 canvas_x, canvas_y - NORMAL_TILE_HEIGHT/2,
			 offset_x, offset_y_unit,
			 width, height_unit);
    if (!pcity && unit_list_size(&map_get_tile(x, y)->units) > 1)
      pixmap_put_overlay_tile_draw(pm,
				   canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				   sprites.unit.stack,
				   offset_x, offset_y_unit,
				   width, height_unit, fog);
  }

  if (contains_special(special, S_FORTRESS) && draw_fortress_airbase)
    pixmap_put_overlay_tile_draw(pm,
				 canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				 sprites.tx.fortress,
				 offset_x, offset_y_unit,
				 width, height_unit, fog);
}

/**************************************************************************
  This function is called when the tileset is changed.
**************************************************************************/
void tileset_changed(void)
{
  /* PORTME */
  /* Here you should do any necessary redraws (for instance, the city
   * dialogs usually need to be resized).
   */
}
