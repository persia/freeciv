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

#include "city.h"
#include "fcintl.h"
#include "log.h"
#include "support.h"

#include "citydlg_g.h"
#include "mapview_g.h"

#include "citydlg_common.h"
#include "civclient.h"		/* for can_client_issue_orders() */
#include "climap.h"
#include "clinet.h"
#include "control.h"
#include "mapview_common.h"
#include "options.h"		/* for concise_city_production */
#include "tilespec.h"		/* for tileset_is_isometric(tileset) */

static int citydlg_width, citydlg_height;

/**************************************************************************
  Return the width of the city dialog canvas.
**************************************************************************/
int get_citydlg_canvas_width(void)
{
  return citydlg_width;
}

/**************************************************************************
  Return the height of the city dialog canvas.
**************************************************************************/
int get_citydlg_canvas_height(void)
{
  return citydlg_height;
}

/**************************************************************************
  Calculate the citydlg width and height.
**************************************************************************/
void generate_citydlg_dimensions(void)
{
  int min_x = 0, max_x = 0, min_y = 0, max_y = 0;

  city_map_iterate(city_x, city_y) {
    int canvas_x, canvas_y;

    map_to_gui_vector(tileset, &canvas_x, &canvas_y,
		      city_x - CITY_MAP_RADIUS, city_y - CITY_MAP_RADIUS);

    min_x = MIN(canvas_x, min_x);
    max_x = MAX(canvas_x, max_x);
    min_y = MIN(canvas_y, min_y);
    max_y = MAX(canvas_y, max_y);
  } city_map_iterate_end;

  citydlg_width = max_x - min_x + tileset_tile_width(tileset);
  citydlg_height = max_y - min_y + tileset_tile_height(tileset);
}

/**************************************************************************
  Converts a (cartesian) city position to citymap canvas coordinates.
  Returns TRUE if the city position is valid.
**************************************************************************/
bool city_to_canvas_pos(int *canvas_x, int *canvas_y, int city_x, int city_y)
{
  const int x0 = CITY_MAP_RADIUS, y0 = CITY_MAP_RADIUS;
  const int width = get_citydlg_canvas_width();
  const int height = get_citydlg_canvas_height();

  /* The citymap is centered over the center of the citydlg canvas. */
  map_to_gui_vector(tileset, canvas_x, canvas_y, city_x - x0, city_y - y0);
  *canvas_x += (width - tileset_tile_width(tileset)) / 2;
  *canvas_y += (height - tileset_tile_height(tileset)) / 2;

  if (!is_valid_city_coords(city_x, city_y)) {
    assert(FALSE);
    return FALSE;
  }
  return TRUE;
}

/**************************************************************************
  Converts a citymap canvas position to a (cartesian) city coordinate
  position.  Returns TRUE iff the city position is valid.
**************************************************************************/
bool canvas_to_city_pos(int *city_x, int *city_y, int canvas_x, int canvas_y)
{
  int orig_canvas_x = canvas_x, orig_canvas_y = canvas_y;
  const int width = get_citydlg_canvas_width();
  const int height = get_citydlg_canvas_height();

  /* The citymap is centered over the center of the citydlg canvas. */
  canvas_x -= (width - tileset_tile_width(tileset)) / 2;
  canvas_y -= (height - tileset_tile_height(tileset)) / 2;

  if (tileset_is_isometric(tileset)) {
    const int W = tileset_tile_width(tileset), H = tileset_tile_height(tileset);

    /* Shift the tile left so the top corner of the origin tile is at
       canvas position (0,0). */
    canvas_x -= W / 2;

    /* Perform a pi/4 rotation, with scaling.  See canvas_pos_to_map_pos
       for a full explanation. */
    *city_x = DIVIDE(canvas_x * H + canvas_y * W, W * H);
    *city_y = DIVIDE(canvas_y * W - canvas_x * H, W * H);
  } else {
    *city_x = DIVIDE(canvas_x, tileset_tile_width(tileset));
    *city_y = DIVIDE(canvas_y, tileset_tile_height(tileset));
  }

  /* Add on the offset of the top-left corner to get the final
   * coordinates (like in canvas_to_map_pos). */
  *city_x += CITY_MAP_RADIUS;
  *city_y += CITY_MAP_RADIUS;

  freelog(LOG_DEBUG, "canvas_to_city_pos(pos=(%d,%d))=(%d,%d)",
	  orig_canvas_x, orig_canvas_y, *city_x, *city_y);

  return is_valid_city_coords(*city_x, *city_y);
}

/* Iterate over all known tiles in the city.  This iteration follows the
 * painter's algorithm and can be used for drawing. */
#define citydlg_iterate(pcity, ptile, pedge, pcorner, canvas_x, canvas_y)   \
{									    \
  int _my_gui_x0, _my_gui_y0;						    \
  struct city *_pcity = (pcity);					    \
  const int _my_width = get_citydlg_canvas_width();			    \
  const int _my_height = get_citydlg_canvas_height();			    \
  									    \
  map_to_gui_vector(tileset, &_my_gui_x0, &_my_gui_y0,			    \
		    _pcity->tile->x, _pcity->tile->y);			    \
  _my_gui_x0 -= (_my_width - tileset_tile_width(tileset)) / 2;		    \
  _my_gui_y0 -= (_my_height - tileset_tile_height(tileset)) / 2;	    \
  freelog(LOG_DEBUG, "citydlg: %d,%d + %dx%d",				    \
	  _my_gui_x0, _my_gui_y0, _my_width, _my_height);		    \
									    \
  gui_rect_iterate(_my_gui_x0, _my_gui_y0, _my_width, _my_height,	    \
		   ptile, pedge, pcorner, _gui_x, _gui_y) {		    \
    const int canvas_x = _gui_x - _my_gui_x0;				    \
    const int canvas_y = _gui_y - _my_gui_y0;				    \
    {

#define citydlg_iterate_end						    \
    }                                                                       \
  } gui_rect_iterate_end;						    \
}

/****************************************************************************
  Draw the full city map onto the canvas store.  Works for both isometric
  and orthogonal views.
****************************************************************************/
void city_dialog_redraw_map(struct city *pcity,
			    struct canvas *pcanvas)
{
  /* First make it all black. */
  canvas_put_rectangle(pcanvas, get_color(tileset, COLOR_MAPVIEW_UNKNOWN),
		       0, 0,
		       get_citydlg_canvas_width(),
		       get_citydlg_canvas_height());

  mapview_layer_iterate(layer) {
    citydlg_iterate(pcity, ptile, pedge, pcorner, canvas_x, canvas_y) {
      struct unit *punit
	= ptile ? get_drawable_unit(tileset, ptile, pcity) : NULL;
      struct city *pcity_draw = ptile ? ptile->city : NULL;

      put_one_element(pcanvas, layer, ptile, pedge, pcorner,
		      punit, pcity_draw, canvas_x, canvas_y, pcity);
    } citydlg_iterate_end;
  } mapview_layer_iterate_end;
}

/**************************************************************************
  Find the city dialog city production text for the given city, and
  place it into the buffer.  This will check the
  concise_city_production option.  pcity may be NULL; in this case a
  filler string is returned.
**************************************************************************/
void get_city_dialog_production(struct city *pcity,
				char *buffer, size_t buffer_len)
{
  int turns, cost, stock;

  if (pcity == NULL) {
    /* 
     * Some GUIs use this to build a "filler string" so that they can
     * properly size the widget to hold the string.  This has some
     * obvious problems; the big one is that we have two forms of time
     * information: "XXX turns" and "never".  Later this may need to
     * be extended to return the longer of the two; in the meantime
     * translators can fudge it by changing this "filler" string. 
     */
    my_snprintf(buffer, buffer_len, Q_("?filler:XXX/XXX XXX turns"));
    return;
  }

  turns = city_turns_to_build(pcity, pcity->production, TRUE);
  stock = pcity->shield_stock;

  if (pcity->production.is_unit) {
    cost = unit_build_shield_cost(get_unit_type(pcity->production.value));
  } else {
    cost = impr_build_shield_cost(pcity->production.value);
  }

  if (get_current_construction_bonus(pcity, EFT_PROD_TO_GOLD) > 0) {
    my_snprintf(buffer, buffer_len, _("%3d gold per turn"),
		MAX(0, pcity->surplus[O_SHIELD]));
  } else {
    char time[50];

    if (turns < FC_INFINITY) {
      if (concise_city_production) {
	my_snprintf(time, sizeof(time), "%3d", turns);
      } else {
	my_snprintf(time, sizeof(time),
		    PL_("%3d turn", "%3d turns", turns), turns);
      }
    } else {
      my_snprintf(time, sizeof(time), "%s",
		  concise_city_production ? "-" : _("never"));
    }

    if (concise_city_production) {
      my_snprintf(buffer, buffer_len, _("%3d/%3d:%s"), stock, cost, time);
    } else {
      my_snprintf(buffer, buffer_len, _("%3d/%3d %s"), stock, cost, time);
    }
  }
}


/**************************************************************************
 Pretty sprints the info about a production (name, info, cost, turns
 to build) into a single text string.

 This is very similar to get_city_dialog_production_row; the
 difference is that instead of placing the data into an array of
 strings it all goes into one long string.  This means it can be used
 by frontends that do not use a tabled structure, but it also gives
 less flexibility.
**************************************************************************/
void get_city_dialog_production_full(char *buffer, size_t buffer_len,
				     struct city_production target,
				     struct city *pcity)
{
  if (!target.is_unit
      && building_has_effect(target.value, EFT_PROD_TO_GOLD)) {
    my_snprintf(buffer, buffer_len, _("%s (XX) %d/turn"),
		get_impr_name_ex(pcity, target.value),
		MAX(0, pcity->surplus[O_SHIELD]));
  } else {
    int turns = city_turns_to_build(pcity, target, TRUE);
    const char *name;
    int cost;

    if (target.is_unit) {
      name = get_unit_name(get_unit_type(target.value));
      cost = unit_build_shield_cost(get_unit_type(target.value));
    } else {
      name = get_impr_name_ex(pcity, target.value);
      cost = impr_build_shield_cost(target.value);
    }

    if (turns < FC_INFINITY) {
      my_snprintf(buffer, buffer_len,
		  PL_("%s (%d) %d turn", "%s (%d) %d turns", turns),
		  name, cost, turns);
    } else {
      my_snprintf(buffer, buffer_len, "%s (%d) never", name, cost);
    }
  }
}

/**************************************************************************
 Pretty sprints the info about a production in 4 columns (name, info,
 cost, turns to build). The columns must each have a size of
 column_size bytes.
**************************************************************************/
void get_city_dialog_production_row(char *buf[], size_t column_size,
				    struct city_production target,
				    struct city *pcity)
{
  if (target.is_unit) {
    struct unit_type *ptype = get_unit_type(target.value);

    my_snprintf(buf[0], column_size, unit_name(ptype));

    /* from unit.h get_unit_name() */
    if (ptype->fuel > 0) {
      my_snprintf(buf[1], column_size, "%d/%d/%d(%d)",
		  ptype->attack_strength, ptype->defense_strength,
		  ptype->move_rate / 3,
		  (ptype->move_rate / 3) * ptype->fuel);
    } else {
      my_snprintf(buf[1], column_size, "%d/%d/%d", ptype->attack_strength,
		  ptype->defense_strength, ptype->move_rate / 3);
    }
    my_snprintf(buf[2], column_size, "%d", unit_build_shield_cost(ptype));
  } else {
    struct player *pplayer = pcity->owner;

    /* Total & turns left meaningless on capitalization */
    if (building_has_effect(target.value, EFT_PROD_TO_GOLD)) {
      my_snprintf(buf[0], column_size, get_improvement_name(target.value));
      buf[1][0] = '\0';
      my_snprintf(buf[2], column_size, "---");
    } else {
      my_snprintf(buf[0], column_size, get_improvement_name(target.value));

      /* from city.c get_impr_name_ex() */
      if (pcity && is_building_replaced(pcity, target.value)) {
	my_snprintf(buf[1], column_size, "*");
      } else {
	const char *state = "";

	if (is_great_wonder(target.value)) {
          if (improvement_obsolete(pplayer, target.value)) {
            state = _("Obsolete");
          } else if (great_wonder_was_built(target.value)) {
            state = _("Built");
          } else {
            state = _("Great Wonder");
          }
	}
	if (is_small_wonder(target.value)) {
	  state = _("Small Wonder");
	  if (find_city_from_small_wonder(pplayer, target.value)) {
	    state = _("Built");
	  }
	  if (improvement_obsolete(pplayer, target.value)) {
	    state = _("Obsolete");
	  }
	}
	my_snprintf(buf[1], column_size, "%s", state);
      }

      my_snprintf(buf[2], column_size, "%d",
		  impr_build_shield_cost(target.value));
    }
  }

  /* Add the turns-to-build entry in the 4th position */
  if (pcity) {
    if (!target.is_unit
	&& building_has_effect(target.value, EFT_PROD_TO_GOLD)) {
      my_snprintf(buf[3], column_size, _("%d/turn"),
		  MAX(0, pcity->surplus[O_SHIELD]));
    } else {
      int turns = city_turns_to_build(pcity, target, FALSE);

      if (turns < FC_INFINITY) {
	my_snprintf(buf[3], column_size, "%d", turns);
      } else {
	my_snprintf(buf[3], column_size, "%s", _("never"));
      }
    }
  } else {
    my_snprintf(buf[3], column_size, "---");
  }
}

/**************************************************************************
  Return text describing the production output.
**************************************************************************/
void get_city_dialog_output_text(const struct city *pcity,
				 Output_type_id otype,
				 char *buf, size_t bufsz)
{
  int total = 0;
  int priority;
  int tax[O_COUNT];
  struct output_type *output = &output_types[otype];

  buf[0] = '\0';

  cat_snprintf(buf, bufsz,
	       _("%+4d : Citizens\n"), pcity->citizen_base[otype]);
  total += pcity->citizen_base[otype];

  /* Hack to get around the ugliness of add_tax_income. */
  memset(tax, 0, O_COUNT * sizeof(*tax));
  add_tax_income(city_owner(pcity), pcity->prod[O_TRADE], tax);
  if (tax[otype] != 0) {
    cat_snprintf(buf, bufsz, _("%+4d : Taxed from trade\n"), tax[otype]);
    total += tax[otype];
  }

  /* Special cases for "bonus" production.  See set_city_production in
   * city.c. */
  if (otype == O_TRADE) {
    int i;

    for (i = 0; i < NUM_TRADEROUTES; i++) {
      if (pcity->trade[i] != 0 && pcity->trade_value[i] != 0) {
	/* There have been bugs causing the trade city to not be sent
	 * properly to the client.  If this happens we trust the
	 * trade_value[] array and simply don't give the name of the
	 * city. */
	struct city *trade_city = find_city_by_id(pcity->trade[i]);
	char *name = trade_city ? trade_city->name : _("(unknown)");

	cat_snprintf(buf, bufsz, _("%+4d : Trade route with %s\n"),
		     pcity->trade_value[i], name);
	total += pcity->trade_value[i];
      }
    }
  } else if (otype == O_GOLD) {
    int tithes = get_city_tithes_bonus(pcity);

    if (tithes != 0) {
      cat_snprintf(buf, bufsz, _("%+4d : Building tithes\n"), tithes);
      total += tithes;
    }
  }

  for (priority = 0; priority < 2; priority++) {
    enum effect_type eft[] = {EFT_OUTPUT_BONUS, EFT_OUTPUT_BONUS_2};

    {
      int base = total, bonus = 100;
      struct effect_list *plist = effect_list_new();

      (void) get_city_bonus_effects(plist, pcity, output, eft[priority]);

      effect_list_iterate(plist, peffect) {
	char buf2[512];
	int new_total;

	get_effect_req_text(peffect, buf2, sizeof(buf2));

	bonus += peffect->value;
	new_total = bonus * base / 100;
	cat_snprintf(buf, bufsz,
		     _("%+4d : Bonus from %s (%+d%%)\n"),
		     (new_total - total), buf2,
		     peffect->value);
	total = new_total;
      } effect_list_iterate_end;
      effect_list_unlink_all(plist);
      effect_list_free(plist);
    }
  }

  if (pcity->waste[otype] != 0) {
    cat_snprintf(buf, bufsz,
		 _("%+4d : Waste\n"), -pcity->waste[otype]);
    total -= pcity->waste[otype];
  }

  if (pcity->unhappy_penalty[otype] != 0) {
    cat_snprintf(buf, bufsz,
		 _("%+4d : Disorder\n"), -pcity->unhappy_penalty[otype]);
    total -= pcity->unhappy_penalty[otype];
  }

  if (pcity->usage[otype] > 0) {
    cat_snprintf(buf, bufsz,
		 _("%+4d : Used\n"), -pcity->usage[otype]);
    total -= pcity->usage[otype];
  }

  cat_snprintf(buf, bufsz,
	       _("==== : Adds up to\n"));
  cat_snprintf(buf, bufsz,
	       _("%4d : Total surplus"), pcity->surplus[otype]);
}

/**************************************************************************
  Return text describing the pollution output.
**************************************************************************/
void get_city_dialog_pollution_text(const struct city *pcity,
				    char *buf, size_t bufsz)
{
  int pollu, prod, pop, mod;

  pollu = city_pollution_types(pcity, pcity->prod[O_SHIELD],
			       &prod, &pop, &mod);
  buf[0] = '\0';

  cat_snprintf(buf, bufsz,
	       "%+4d : Pollution from shields\n", prod);
  cat_snprintf(buf, bufsz,
	       "%+4d : Pollution from citizens\n", pop);
  cat_snprintf(buf, bufsz,
	       "%+4d : Pollution modifier\n", mod);
  cat_snprintf(buf, bufsz,
	       "==== : Adds up to\n");
  cat_snprintf(buf, bufsz,
	       "%4d : Total surplus", pollu);
}

/**************************************************************************
  Provide a list of all citizens in the city, in order.  "index"
  should be the happiness index (currently [0..4]; 4 = final
  happiness).  "citizens" should be an array large enough to hold all
  citizens (use MAX_CITY_SIZE to be on the safe side).
**************************************************************************/
void get_city_citizen_types(struct city *pcity, int index,
			    struct citizen_type *citizens)
{
  int i = 0, n;
  assert(index >= 0 && index < 5);

  for (n = 0; n < pcity->ppl_happy[index]; n++, i++) {
    citizens[i].type = CITIZEN_HAPPY;
  }
  for (n = 0; n < pcity->ppl_content[index]; n++, i++) {
    citizens[i].type = CITIZEN_CONTENT;
  }
  for (n = 0; n < pcity->ppl_unhappy[index]; n++, i++) {
    citizens[i].type = CITIZEN_UNHAPPY;
  }
  for (n = 0; n < pcity->ppl_angry[index]; n++, i++) {
    citizens[i].type = CITIZEN_ANGRY;
  }

  specialist_type_iterate(sp) {
    for (n = 0; n < pcity->specialists[sp]; n++, i++) {
      citizens[i].type = CITIZEN_SPECIALIST;
      citizens[i].spec_type = sp;
    }
  } specialist_type_iterate_end;

  assert(i == pcity->size);
}

/**************************************************************************
  Rotate the given specialist citizen to the next type of citizen.
**************************************************************************/
void city_rotate_specialist(struct city *pcity, int citizen_index)
{
  struct citizen_type citizens[MAX_CITY_SIZE];
  Specialist_type_id from, to;

  if (citizen_index < 0 || citizen_index >= pcity->size) {
    return;
  }

  get_city_citizen_types(pcity, 4, citizens);

  if (citizens[citizen_index].type != CITIZEN_SPECIALIST) {
    return;
  }
  from = citizens[citizen_index].spec_type;

  /* Loop through all specialists in order until we find a usable one
   * (or run out of choices). */
  to = from;
  assert(to >= 0 && to < SP_COUNT);
  do {
    to = (to + 1) % SP_COUNT;
  } while (to != from && !city_can_use_specialist(pcity, to));

  if (from != to) {
    city_change_specialist(pcity, from, to);
  }
}
    
/**************************************************************************
  Activate all units on the given map tile.
**************************************************************************/
void activate_all_units(struct tile *ptile)
{
  struct unit_list *punit_list = ptile->units;
  struct unit *pmyunit = NULL;

  unit_list_iterate(punit_list, punit) {
    if (game.player_ptr == punit->owner) {
      /* Activate this unit. */
      pmyunit = punit;
      request_new_unit_activity(punit, ACTIVITY_IDLE);
    }
  } unit_list_iterate_end;
  if (pmyunit) {
    /* Put the focus on one of the activated units. */
    set_unit_focus(pmyunit);
  }
}

/**************************************************************************
  Change the production of a given city.  Return the request ID.
**************************************************************************/
int city_change_production(struct city *pcity, struct city_production target)
{
  return dsend_packet_city_change(&aconnection, pcity->id,
				  target.value, target.is_unit);
}

/**************************************************************************
  Set the worklist for a given city.  Return the request ID.

  Note that the worklist does NOT include the current production.
**************************************************************************/
int city_set_worklist(struct city *pcity, struct worklist *pworklist)
{
  struct worklist copy;

  copy_worklist(&copy, pworklist);

  /* Don't send the worklist name to the server. */
  copy.name[0] = '\0';

  return dsend_packet_city_worklist(&aconnection, pcity->id, &copy);
}


/**************************************************************************
  Insert an item into the city's queue.  This function will send new
  production requests to the server but will NOT send the new worklist
  to the server - the caller should call city_set_worklist() if the
  function returns TRUE.

  Note that the queue DOES include the current production.
**************************************************************************/
static bool base_city_queue_insert(struct city *pcity, int position,
				   struct city_production item)
{
  if (position == 0) {
    struct city_production old = pcity->production;

    /* Insert as current production. */
    if (item.is_unit
	&& !can_build_unit_direct(pcity, get_unit_type(item.value))) {
      return FALSE;
    }
    if (!item.is_unit && !can_build_improvement_direct(pcity, item.value)) {
      return FALSE;
    }

    if (!worklist_insert(&pcity->worklist, old, 0)) {
      return FALSE;
    }

    city_change_production(pcity, item);
  } else if (position >= 1
	     && position <= worklist_length(&pcity->worklist)) {
    /* Insert into middle. */
    if (item.is_unit
	&& !can_eventually_build_unit(pcity, get_unit_type(item.value))) {
      return FALSE;
    }
    if (!item.is_unit
	&& !can_eventually_build_improvement(pcity, item.value)) {
      return FALSE;
    }
    if (!worklist_insert(&pcity->worklist, item, position - 1)) {
      return FALSE;
    }
  } else {
    /* Insert at end. */
    if (item.is_unit
	&& !can_eventually_build_unit(pcity, get_unit_type(item.value))) {
      return FALSE;
    }
    if (!item.is_unit
	&& !can_eventually_build_improvement(pcity, item.value)) {
      return FALSE;
    }
    if (!worklist_append(&pcity->worklist, item)) {
      return FALSE;
    }
  }
  return TRUE;
}

/**************************************************************************
  Insert an item into the city's queue.

  Note that the queue DOES include the current production.
**************************************************************************/
bool city_queue_insert(struct city *pcity, int position,
		       struct city_production item)
{
  if (base_city_queue_insert(pcity, position, item)) {
    city_set_worklist(pcity, &pcity->worklist);
    return TRUE;
  }
  return FALSE;
}

/**************************************************************************
  Clear the queue (all entries except the first one since that can't be
  cleared).

  Note that the queue DOES include the current production.
**************************************************************************/
bool city_queue_clear(struct city *pcity)
{
  init_worklist(&pcity->worklist);

  return TRUE;
}

/**************************************************************************
  Insert the worklist into the city's queue at the given position.

  Note that the queue DOES include the current production.
**************************************************************************/
bool city_queue_insert_worklist(struct city *pcity, int position,
				struct worklist *worklist)
{
  bool success = FALSE;

  if (worklist_length(worklist) == 0) {
    return TRUE;
  }

  worklist_iterate(worklist, target) {
    if (base_city_queue_insert(pcity, position, target)) {
      if (position > 0) {
	/* Move to the next position (unless position == -1 in which case
	 * we're appending. */
	position++;
      }
      success = TRUE;
    }
  } worklist_iterate_end;

  if (success) {
    city_set_worklist(pcity, &pcity->worklist);
  }

  return success;
}

/**************************************************************************
  Get the city current production and the worklist, like it should be.
**************************************************************************/
void city_get_queue(struct city *pcity, struct worklist *pqueue)
{
  copy_worklist(pqueue, &pcity->worklist);

  /* The GUI wants current production to be in the task list, but the
     worklist API wants it out for reasons unknown. Perhaps someone enjoyed
     making things more complicated than necessary? So I dance around it. */

  /* We want the current production to be in the queue. Always. */
  worklist_remove(pqueue, MAX_LEN_WORKLIST - 1);

  worklist_insert(pqueue, pcity->production, 0);
}

/**************************************************************************
  Set the city current production and the worklist, like it should be.
**************************************************************************/
void city_set_queue(struct city *pcity, struct worklist *pqueue)
{
  struct worklist copy;
  struct city_production target;

  copy_worklist(&copy, pqueue);

  /* The GUI wants current production to be in the task list, but the
     worklist API wants it out for reasons unknown. Perhaps someone enjoyed
     making things more complicated than necessary? So I dance around it. */
  if (worklist_peek(&copy, &target)) {
    worklist_advance(&copy);

    city_set_worklist(pcity, &copy);
    city_change_production(pcity, target);
  } else {
    /* You naughty boy, you can't erase the current production. Nyah! */
    if (worklist_is_empty(&pcity->worklist)) {
      refresh_city_dialog(pcity);
    } else {
      city_set_worklist(pcity, &copy);
    }
  }
}

/**************************************************************************
  Return TRUE iff the city can buy.
**************************************************************************/
bool city_can_buy(const struct city *pcity)
{
  /* See really_handle_city_buy() in the server.  However this function
   * doesn't allow for error messages.  It doesn't check the cost of
   * buying; that's handled separately (and with an error message). */
  return (can_client_issue_orders()
	  && pcity
	  && pcity->owner == game.player_ptr
	  && pcity->turn_founded != game.info.turn
	  && !pcity->did_buy
	  && get_current_construction_bonus(pcity, EFT_PROD_TO_GOLD) <= 0
	  && !(pcity->production.is_unit && pcity->anarchy != 0)
	  && city_buy_cost(pcity) > 0);
}

/**************************************************************************
  Change the production of a given city.  Return the request ID.
**************************************************************************/
int city_sell_improvement(struct city *pcity, Impr_type_id sell_id)
{
  return dsend_packet_city_sell(&aconnection, pcity->id, sell_id);
}

/**************************************************************************
  Buy the current production item in a given city.  Return the request ID.
**************************************************************************/
int city_buy_production(struct city *pcity)
{
  return dsend_packet_city_buy(&aconnection, pcity->id);
}

/**************************************************************************
  Change a specialist in the given city.  Return the request ID.
**************************************************************************/
int city_change_specialist(struct city *pcity, Specialist_type_id from,
			   Specialist_type_id to)
{
  return dsend_packet_city_change_specialist(&aconnection, pcity->id, from,
					     to);
}

/**************************************************************************
  Toggle a worker<->specialist at the given city tile.  Return the
  request ID.
**************************************************************************/
int city_toggle_worker(struct city *pcity, int city_x, int city_y)
{
  assert(is_valid_city_coords(city_x, city_y));

  if (pcity->city_map[city_x][city_y] == C_TILE_WORKER) {
    return dsend_packet_city_make_specialist(&aconnection, pcity->id, city_x,
					     city_y);
  } else if (pcity->city_map[city_x][city_y] == C_TILE_EMPTY) {
    return dsend_packet_city_make_worker(&aconnection, pcity->id, city_x,
					 city_y);
  } else {
    return 0;
  }
}

/**************************************************************************
  Tell the server to rename the city.  Return the request ID.
**************************************************************************/
int city_rename(struct city *pcity, const char *name)
{
  return dsend_packet_city_rename(&aconnection, pcity->id, name);
}
