/********************************************************************** 
 Freeciv - Copyright (C) 2003 - The Freeciv Project
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
#include <string.h>

#include "log.h"
#include "mem.h"

#include "base.h"
#include "game.h"
#include "movement.h"

#include "pf_tools.h"


static void pft_fill_unit_default_parameter(struct pf_parameter *parameter,
					    struct unit *punit);

/*************************************************************
  Cost of moving one normal step.
*************************************************************/
static inline int single_move_cost(const struct pf_parameter *param,
                                   const struct tile *src_tile,
                                   const struct tile *dest_tile)
{
  if (!dest_tile->city
      && BV_ISSET(param->unit_flags, F_TRIREME)
      && !is_safe_ocean(dest_tile)) {
    return PF_IMPOSSIBLE_MC;
  } else if (unit_class_flag(param->class, UCF_TERRAIN_SPEED)) {
    return map_move_cost(src_tile, dest_tile);
  } else {
    return SINGLE_MOVE;
  }
}

/* ===================== Move Cost Callbacks ========================= */

/*************************************************************
  A cost function for SEA_MOVING.  Allows shore bombardment.
  Should be used in conjunction with a TB callback which 
  prohibits going through an enemy city/tile.
*************************************************************/
static int seamove(const struct tile *ptile, enum direction8 dir,
                   const struct tile *ptile1, struct pf_parameter *param)
{
  if (is_native_tile_to_class(param->class, ptile1)) {
    return single_move_cost(param, ptile, ptile1);
  } else if (ptile1->city
             || is_non_allied_unit_tile(ptile1, param->owner)) {
    /* Entering port or shore bombardment */
    return SINGLE_MOVE;
  } else {
    return PF_IMPOSSIBLE_MC;
  }
}

/*************************************************************
  SINGLE_MOVE cost function for AIR_MOVING
*************************************************************/
static int single_airmove(const struct tile *ptile, enum direction8 dir,
			  const struct tile *ptile1,
			  struct pf_parameter *param)
{
  if (!ptile->city && !is_native_tile_to_class(param->class, ptile1)) {
    return PF_IMPOSSIBLE_MC;
  } else if (!is_native_tile_to_class(param->class, ptile1)) {
    return SINGLE_MOVE;
  }
  return single_move_cost(param, ptile, ptile1);
}

/*************************************************************
  A cost function for SEA_MOVING.  Does not allow shore 
  bombardment.
*************************************************************/
static int seamove_no_bombard(const struct tile *ptile, enum direction8 dir,
			      const struct tile *ptile1,
			      struct pf_parameter *param)
{
  if (is_native_tile_to_class(param->class, ptile1)) {
    return single_move_cost(param, ptile, ptile1);
  } else if (is_allied_city_tile(ptile1, param->owner)) {
    /* Entering port */
    return SINGLE_MOVE;
  } else {
    return PF_IMPOSSIBLE_MC;
  }
}

/************************************************************
  A cost function for a sea unit which allows going one step 
  into the land (for shore bombardment and ferries).
  Things to remember: we should prevent going from land to
  anywhere, unless we are leaving a friendly city, in which
  case we can move into the ocean but not into the land.
************************************************************/
static int sea_overlap_move(const struct tile *ptile, enum direction8 dir,
			    const struct tile *ptile1,
			    struct pf_parameter *param)
{
  if (is_allied_city_tile(ptile, param->owner)
      && is_native_tile_to_class(param->class, ptile1)) {
    return single_move_cost(param, ptile, ptile1);
  } else if (!is_native_tile_to_class(param->class, ptile)) {
    return PF_IMPOSSIBLE_MC;
  } else if (is_native_tile_to_class(param->class, ptile1)) {
    return single_move_cost(param, ptile, ptile1);
  } else {
    /* Entering port or bombardment */
    return SINGLE_MOVE;
  }
}

/**********************************************************************
  Sea attack is the same as overlap (consider bombardment) but we don't
  want to pass through enemy tiles.
**********************************************************************/
static int sea_attack_move(const struct tile *src_tile, enum direction8 dir,
			   const struct tile *dest_tile,
			   struct pf_parameter *param)
{
  if (is_native_tile_to_class(param->class, src_tile)) {
    if (is_non_allied_unit_tile(src_tile, param->owner)) {
      return PF_IMPOSSIBLE_MC;
    }
    if (is_native_tile_to_class(param->class, dest_tile)) {
      return single_move_cost(param, src_tile, dest_tile);
    }
    return SINGLE_MOVE;
  } else if (is_allied_city_tile(src_tile, param->owner)
	     && is_native_tile_to_class(param->class, dest_tile)) {
    return single_move_cost(param, src_tile, dest_tile);
  }

  return PF_IMPOSSIBLE_MC;
}

/************************************************************ 
  LAND_MOVE cost function for a unit 
************************************************************/
static int normal_move_unit(const struct tile *ptile, enum direction8 dir,
			    const struct tile *ptile1,
			    struct pf_parameter *param)
{
  int move_cost;

  if (!is_native_tile_to_class(param->class, ptile1)) {
    if (unit_class_transporter_capacity(ptile1, param->owner, param->class) > 0) {
      move_cost = SINGLE_MOVE;
    } else {
      move_cost = PF_IMPOSSIBLE_MC;
    }
  } else if (!is_native_tile_to_class(param->class, ptile)) {
    if (!BV_ISSET(param->unit_flags, F_MARINES)
        && (is_non_allied_unit_tile(ptile1, param->owner) 
            || is_non_allied_city_tile(ptile1, param->owner))) {
      move_cost = PF_IMPOSSIBLE_MC;
    } else {
      move_cost = single_move_cost(param, ptile, ptile1);
    }
  } else {
    move_cost = single_move_cost(param, ptile, ptile1);
  }

  return move_cost;
}

/******************************************************************* 
  LAND_MOVE cost function for a unit, but taking into account
  possibilities of attacking.
*******************************************************************/
static int land_attack_move(const struct tile *src_tile, enum direction8 dir,
			    const struct tile *tgt_tile,
			    struct pf_parameter *param)
{
  int move_cost;

  if (!is_native_tile_to_class(param->class, tgt_tile)) {

    /* Any-to-Sea */
    if (unit_class_transporter_capacity(tgt_tile, param->owner, param->class) > 0) {
      move_cost = SINGLE_MOVE;
    } else {
      move_cost = PF_IMPOSSIBLE_MC;
    }
  } else if (!is_native_tile_to_class(param->class, src_tile)) {

    /* Sea-to-Land. */
    if (!is_non_allied_unit_tile(tgt_tile, param->owner)
        && !is_non_allied_city_tile(tgt_tile, param->owner)) {
      move_cost = tgt_tile->terrain->movement_cost * SINGLE_MOVE;
    } else if (BV_ISSET(param->unit_flags, F_MARINES)) {
      /* Can attack!! */
      move_cost = single_move_cost(param, src_tile, tgt_tile);
    } else {
      move_cost = PF_IMPOSSIBLE_MC;
    }
  } else {

    /* Land-to-Land */
    if (is_non_allied_unit_tile(src_tile, param->owner)) {
      /* Cannot pass through defended tiles */
      move_cost = PF_IMPOSSIBLE_MC;
    } else if (is_non_allied_unit_tile(tgt_tile, param->owner)) {

      /* Attack! */
      move_cost = SINGLE_MOVE;
    } else {
      move_cost = single_move_cost(param, src_tile, tgt_tile);
    }
  }

  return move_cost;
}


/************************************************************ 
  A cost function for a land unit, which allows going into
  the ocean (with moves costing SINGLE_MOVE).  It is 
  recommended to use dont_cross_ocean TB callback with this 
  one, so we don't venture too far into the ocean ;)

  Alternatively, we can change the flow to
  if (!is_native_tile_to_class(param->class, ptile)) {
    move_cost = PF_IMPOSSIBLE_MC;
  } else if (!is_native_tile_to_class(param->class, ptile1)) {
    move_cost = SINGLE_MOVE;
  } else {
    move_cost = single_move_cost(param, ptile, ptile1);
  }
  which will achieve the same without call-back.
************************************************************/
static int land_overlap_move(const struct tile *ptile, enum direction8 dir,
			     const struct tile *ptile1,
			     struct pf_parameter *param)
{
  int move_cost;

  if (!is_native_tile_to_class(param->class, ptile1)) {
    move_cost = SINGLE_MOVE;
  } else {
    move_cost = single_move_cost(param, ptile, ptile1);
  }

  return move_cost;
}

/************************************************************ 
  Reversed LAND_MOVE cost function for a unit.
  Will be used. DO NOT REMOVE.
************************************************************/
#ifdef UNUSED
static int reverse_move_unit(const struct tile *tile0, enum direction8 dir,
			     const struct tile *ptile,
			     struct pf_parameter *param)
{
  int move_cost = PF_IMPOSSIBLE_MC;

  if (!is_native_to_class(param->class, ptile)) {
    if (unit_class_transporter_capacity(ptile, param->owner, param->class) > 0) {
      /* Landing */
      move_cost = terrain0->movement_cost * SINGLE_MOVE;
    } else {
      /* Nothing to land from */
      move_cost = PF_IMPOSSIBLE_MC;
    }
  } else if (!is_native_tile_to_class(param->class, tile0)) {
    /* Boarding */
    move_cost = SINGLE_MOVE;
  } else {
    move_cost = ptile->move_cost[DIR_REVERSE(dir)];
  }

  return move_cost;
}
#endif

/************************************************************ 
  IGTER_MOVE cost function for a unit 
************************************************************/
static int igter_move_unit(const struct tile *ptile, enum direction8 dir,
			   const struct tile *ptile1,
			   struct pf_parameter *param)
{
  int move_cost;

  if (!is_native_tile_to_class(param->class, ptile1)) {
    if (unit_class_transporter_capacity(ptile1, param->owner, param->class) > 0) {
      move_cost = MOVE_COST_ROAD;
    } else {
      move_cost = PF_IMPOSSIBLE_MC;
    }
  } else if (!is_native_tile_to_class(param->class, ptile)) {
    if (!BV_ISSET(param->unit_flags, F_MARINES)
        && (is_non_allied_unit_tile(ptile1, param->owner) 
            || is_non_allied_city_tile(ptile1, param->owner))) {
      move_cost = PF_IMPOSSIBLE_MC;
    } else {
      move_cost = MOVE_COST_ROAD;
    }
  } else if (unit_class_flag(param->class, UCF_TERRAIN_SPEED)) {
    move_cost = (map_move_cost(ptile, ptile1) != 0
                 ? MOVE_COST_ROAD : 0);
  } else {
    move_cost = SINGLE_MOVE;
  }
  return move_cost;
}

/************************************************************ 
  Reversed IGTER_MOVE cost function for a unit.
  Will be used. DO NOT REMOVE.
************************************************************/
#ifdef UNUSED
static int reverse_igter_move_unit(const struct tile *tile0,
				   enum direction8 dir,
				   const struct tile *ptile,
				   struct pf_parameter *param)
{
  int move_cost;

  if (!is_native_to_class(param->class, ptile)) {
    if (unit_class_transporter_capacity(ptile, param->owner, param->class) > 0) {
      /* Landing */
      move_cost = MOVE_COST_ROAD;
    } else {
      move_cost = PF_IMPOSSIBLE_MC;
    }
  } else if (!is_native_to_class(param->class, tile0)) {
    /* Boarding */
    move_cost = MOVE_COST_ROAD;
  } else {
    move_cost =
	(ptile->move_cost[DIR_REVERSE(dir)] != 0 ? MOVE_COST_ROAD : 0);
  }
  return move_cost;
}
#endif


/****************************************************************************
  A cost function for amphibious movement.
****************************************************************************/
static int amphibious_move(const struct tile *ptile, enum direction8 dir,
			   const struct tile *ptile1,
			   struct pf_parameter *param)
{
  struct pft_amphibious *amphibious = param->data;
  const bool src_ferry = is_native_tile_to_class(amphibious->sea.class, ptile);
  const bool dst_ferry = is_native_tile_to_class(amphibious->sea.class, ptile1);
  const bool dst_psng = is_native_tile_to_class(amphibious->land.class, ptile1);
  int cost, scale;

  if (src_ferry && dst_ferry) {
    /* Sea move */
    cost = amphibious->sea.get_MC(ptile, dir, ptile1, &amphibious->sea);
    scale = amphibious->sea_scale;
  } else if (src_ferry && is_allied_city_tile(ptile1, param->owner)) {
    /* Moving from native terrain to a city. */
    cost = amphibious->sea.get_MC(ptile, dir, ptile1, &amphibious->sea);
    scale = amphibious->sea_scale;
  } else if (src_ferry && dst_psng) {
    /* Disembark; use land movement function to handle F_MARINES */
    cost = amphibious->land.get_MC(ptile, dir, ptile1, &amphibious->land);
    scale = amphibious->land_scale;
  } else if (src_ferry) {
    /* Neither ferry nor passenger can enter tile. */
    cost = PF_IMPOSSIBLE_MC;
    scale = amphibious->sea_scale;
  } else if (is_allied_city_tile(ptile, param->owner) && dst_ferry) {
    /* Leaving port (same as sea move) */
    cost = amphibious->sea.get_MC(ptile, dir, ptile1, &amphibious->sea);
    scale = amphibious->sea_scale;
  } else if (!dst_psng) {
    /* Now we have disembarked, our ferry can not help us - we have to
     * stay on the land. */
    cost = PF_IMPOSSIBLE_MC;
    scale = amphibious->land_scale;
  } else {
    /* land move */
    cost = amphibious->land.get_MC(ptile, dir, ptile1, &amphibious->land);
    scale = amphibious->land_scale;
  }
  if (cost != PF_IMPOSSIBLE_MC) {
    cost *= scale;
  }
  return cost;
}

/* ===================== Extra Cost Callbacks ======================== */

/*********************************************************************
  An example of EC callback.  DO NOT REMOVE you pricks!
*********************************************************************/
#ifdef UNUSED
static int afraid_of_dark_forest(const struct tile *ptile,
				 enum known_type known,
				 struct pf_parameter *param)
{
  if (ptile->terrain->index == T_FOREST) {
    /* Willing to spend extra 2 turns to go around a forest tile */
    return PF_TURN_FACTOR * 2;
  }

  return 0;
}
#endif

/****************************************************************************
  Extra cost call back for amphibious movement
****************************************************************************/
static int amphibious_extra_cost(const struct tile *ptile,
				 enum known_type known,
				 struct pf_parameter *param)
{
  struct pft_amphibious *amphibious = param->data;
  const bool ferry_move = is_native_tile_to_class(amphibious->sea.class, ptile);
  int cost, scale;

  if (known == TILE_UNKNOWN) {
    /* We can travel almost anywhere */
    cost = SINGLE_MOVE;
    scale = MAX(amphibious->sea_scale, amphibious->land_scale);
  } else if (ferry_move && amphibious->sea.get_EC) {
    /* Do the EC callback for sea moves. */
    cost = amphibious->sea.get_EC(ptile, known, &amphibious->sea);
    scale = amphibious->sea_scale;
  } else if (!ferry_move && amphibious->land.get_EC) {
    /* Do the EC callback for land moves. */
    cost = amphibious->land.get_EC(ptile, known, &amphibious->land);
    scale = amphibious->land_scale;
  } else {
    cost = 0;
    scale = 1;
  }

  if (cost != PF_IMPOSSIBLE_MC) {
    cost *= scale;
  }
  return cost;
}


/* ===================== Tile Behaviour Callbacks ==================== */

/*********************************************************************
  A callback for maps overlapping one square into the non-native
  terrain.  Insures that we don't continue walking over ocean.
*********************************************************************/
static enum tile_behavior dont_cross_ocean(const struct tile *ptile,
					   enum known_type known,
					   struct pf_parameter *param)
{
  if (!is_native_tile_to_class(param->class, ptile)) {
    return TB_DONT_LEAVE;
  }
  return TB_NORMAL;
}

/********************************************************************** 
  PF callback to prohibit going into the unknown.  Also makes sure we 
  don't plan to attack anyone.
***********************************************************************/
enum tile_behavior no_fights_or_unknown(const struct tile *ptile,
                                        enum known_type known,
                                        struct pf_parameter *param)
{
  if (known == TILE_UNKNOWN
      || is_non_allied_unit_tile(ptile, param->owner)
      || is_non_allied_city_tile(ptile, param->owner)) {
    /* Can't attack */
    return TB_IGNORE;
  }
  return TB_NORMAL;
}

/********************************************************************** 
  PF callback to prohibit attacking anyone.
***********************************************************************/
enum tile_behavior no_fights(const struct tile *ptile, enum known_type known,
			     struct pf_parameter *param)
{
  if (is_non_allied_unit_tile(ptile, param->owner)
      || is_non_allied_city_tile(ptile, param->owner)) {
    /* Can't attack */
    return TB_IGNORE;
  }
  return TB_NORMAL;
}

/****************************************************************************
  PF callback to prohibit attacking anyone, except at the destination.
****************************************************************************/
enum tile_behavior no_intermediate_fights(const struct tile *ptile,
					  enum known_type known,
					  struct pf_parameter *param)
{
  if (is_non_allied_unit_tile(ptile, param->owner)
      || is_non_allied_city_tile(ptile, param->owner)) {
    return TB_DONT_LEAVE;
  }
  return TB_NORMAL;
}

/*********************************************************************
  A callback for amphibious movement
*********************************************************************/
static enum tile_behavior amphibious_behaviour(const struct tile *ptile,
					       enum known_type known,
					       struct pf_parameter *param)
{
  struct pft_amphibious *amphibious = param->data;
  const bool ferry_move = is_native_tile_to_class(amphibious->sea.class, ptile);

  /* Simply a wrapper for the sea or land tile_behavior callbacks. */
  if (ferry_move && amphibious->sea.get_TB) {
    return amphibious->sea.get_TB(ptile, known, &amphibious->sea);
  } else if (!ferry_move && amphibious->land.get_TB) {
    return amphibious->land.get_TB(ptile, known, &amphibious->land);
  }
  return TB_NORMAL;
}

/* =====================  Postion Dangerous Callbacks ================ */

/****************************************************************************
  Position-dangerous callback for air units.
****************************************************************************/
static bool air_is_pos_dangerous(const struct tile *ptile,
				 enum known_type known,
				 struct pf_parameter *param)
{
  /* FIXME: bombers with fuel remaining should not worry about danger. */

  if (is_allied_city_tile(ptile, param->owner)) {
    return FALSE;
  }

  if (tile_has_base_flag(ptile, BF_REFUEL)) {
    /* All airbases are considered non-dangerous, although non-allied ones
     * are inaccessible. */
    return FALSE;
  }

  /* Carriers are ignored since they are likely to move. */
  return TRUE;
}

/****************************************************************************
  Position-dangerous callback for amphibious movement.
****************************************************************************/
static bool amphibious_is_pos_dangerous(const struct tile *ptile,
					enum known_type known,
					struct pf_parameter *param)
{
  struct pft_amphibious *amphibious = param->data;
  const bool ferry_move = is_native_tile_to_class(amphibious->sea.class, ptile);

  /* Simply a wrapper for the sea or land danger callbacks. */
  if (ferry_move && amphibious->sea.is_pos_dangerous) {
    return amphibious->sea.is_pos_dangerous(ptile, known, param);
  } else if (!ferry_move && amphibious->land.is_pos_dangerous) {
    return amphibious->land.is_pos_dangerous(ptile, known, param);
  }
  return FALSE;
}

/* =====================  Tools for filling parameters =============== */

/**********************************************************************
  Fill unit-dependent parameters
***********************************************************************/
void pft_fill_unit_parameter(struct pf_parameter *parameter,
			     struct unit *punit)
{
  pft_fill_unit_default_parameter(parameter, punit);

  switch (get_unit_move_type(unit_type(punit))) {
  case LAND_MOVING:
    if (unit_flag(punit, F_IGTER)) {
      parameter->get_MC = igter_move_unit;
    } else {
      parameter->get_MC = normal_move_unit;
    }
    break;
  case SEA_MOVING:
    if (can_attack_non_native(unit_type(punit))) {
      parameter->get_MC = seamove;
    } else {
      parameter->get_MC = seamove_no_bombard;
    }
    break;
  case AIR_MOVING:
    parameter->get_MC = single_airmove;
    if (unit_type(punit)->fuel > 0) {
      parameter->is_pos_dangerous = air_is_pos_dangerous;
    } else {
      parameter->is_pos_dangerous = NULL;
    }
    parameter->turn_mode = TM_WORST_TIME;
    break;
  case HELI_MOVING:
    /* Helicoptors are treated similarly to airplanes. */
    parameter->get_MC = single_airmove;
    break;
  default:
    freelog(LOG_ERROR, "Impossible move type to pft_fill_unit_parameter()!");
    break;
  }

  if (!parameter->is_pos_dangerous
      && is_losing_hp(punit)) {
    /* Unit loses hitpoints each turn when not in city/safe base/carrier */
    parameter->is_pos_dangerous = air_is_pos_dangerous;
    parameter->turn_mode = TM_WORST_TIME;
  }

  if (get_unit_move_type(unit_type(punit)) == LAND_MOVING 
      && !unit_flag(punit, F_IGZOC)) {
    parameter->get_zoc = is_my_zoc;
  } else {
    parameter->get_zoc = NULL;
  }
}

/**********************************************************************
  Switch on one tile overlapping into the non-native terrain.
  For sea/land bombardment and for ferries.
**********************************************************************/
void pft_fill_unit_overlap_param(struct pf_parameter *parameter,
				 struct unit *punit)
{
  pft_fill_unit_default_parameter(parameter, punit);

  switch (get_unit_move_type(unit_type(punit))) {
  case LAND_MOVING:
    parameter->get_MC = land_overlap_move;
    parameter->get_TB = dont_cross_ocean;
    break;
  case SEA_MOVING:
    parameter->get_MC = sea_overlap_move;
    break;
  case AIR_MOVING:
  case HELI_MOVING:
    parameter->get_MC = single_airmove; /* very crude */
    break;
  default:
    freelog(LOG_ERROR, "Impossible move type to pft_fill_unit_overlap_param()!");
    break;
  }

  parameter->get_zoc = NULL;
}

/**********************************************************************
  Consider attacking and non-attacking possibilities properly
**********************************************************************/
void pft_fill_unit_attack_param(struct pf_parameter *parameter,
                                struct unit *punit)
{
  pft_fill_unit_default_parameter(parameter, punit);

  switch (get_unit_move_type(unit_type(punit))) {
  case LAND_MOVING:
    parameter->get_MC = land_attack_move;
    break;
  case SEA_MOVING:
    parameter->get_MC = sea_attack_move;
    break;
  case AIR_MOVING:
  case HELI_MOVING:
    parameter->get_MC = single_airmove; /* very crude */
    break;
  default:
    freelog(LOG_ERROR, "Impossible move type to pft_fill_unit_attack_param()!");
    break;
  }

  if (get_unit_move_type(unit_type(punit)) == LAND_MOVING 
      && !unit_flag(punit, F_IGZOC)) {
    parameter->get_zoc = is_my_zoc;
  } else {
    parameter->get_zoc = NULL;
  }

  /* It is too complicated to work with danger here */
  parameter->is_pos_dangerous = NULL;
}

/****************************************************************************
  Fill parameters for combined sea-land movement.
  This is suitable for the case of a land unit riding a ferry.
  The starting position of the ferry is taken to be the starting position for
  the PF. The passenger is assumed to initailly be on the given ferry.
  The destination may be inland, in which case the passenger will ride
  the ferry to a beach head, disembark, then continue on land.
  One complexity of amphibious movement is that the movement rate on land
  might be different from that at sea. We therefore scale up the movement
  rates (and the corresponding movement consts) to the product of the two
  rates.
****************************************************************************/
void pft_fill_amphibious_parameter(struct pft_amphibious *parameter)
{
  const int move_rate = parameter->land.move_rate * parameter->sea.move_rate;

  parameter->combined = parameter->sea;
  parameter->land_scale = move_rate / parameter->land.move_rate;
  parameter->sea_scale = move_rate / parameter->sea.move_rate;
  parameter->combined.moves_left_initially *= parameter->sea_scale;
  parameter->combined.move_rate = move_rate;
  /* To ensure triremes behave correctly: FIXME: Probably incorrect now */
  parameter->combined.turn_mode = TM_WORST_TIME;
  parameter->combined.get_MC = amphibious_move;
  parameter->combined.get_TB = amphibious_behaviour;
  parameter->combined.get_EC = amphibious_extra_cost;
  parameter->combined.is_pos_dangerous = amphibious_is_pos_dangerous;
  BV_CLR_ALL(parameter->combined.unit_flags);

  parameter->combined.data = parameter;
}

/**********************************************************************
  Fill general use parameters to defaults
***********************************************************************/
static void pft_fill_unit_default_parameter(struct pf_parameter *parameter,
					    struct unit *punit)
{
  parameter->turn_mode = TM_CAPPED;

  parameter->unknown_MC = SINGLE_MOVE;
  if (unit_class_flag(get_unit_class(unit_type(punit)), UCF_TERRAIN_SPEED)) {
    /* Unit is subject to terrain movement costs */
    struct unit_type *punittype = unit_type(punit);
    bv_special specials;

    BV_CLR_ALL(specials); /* This works at the moment, since road is
                           * only special that affects is_native_terrain()
                           * Even if tile contains road, we can safely
                           * ignore it since movement cost for it is
                           * certainly less than SINGLE_MOVE. */

    terrain_type_iterate(pterrain) {
      if (is_native_terrain(punittype, pterrain, specials)) {
        /* Exact movement cost matters only if we can enter
         * the tile. */
        int mr = 2 * pterrain->movement_cost;

        parameter->unknown_MC = MAX(mr, parameter->unknown_MC);
      } else {
        /* FIXME: We might be unable to enter tile at all.
                  This should have some cost too? */
      }
    } terrain_type_iterate_end;
  }
  if (is_sailing_unit(punit)) {
    /* Sailing units explore less */
    parameter->unknown_MC *= 2;
  }

  parameter->get_TB = NULL;
  parameter->get_EC = NULL;
  parameter->is_pos_dangerous = NULL;
  parameter->get_costs = NULL;
  parameter->get_zoc = NULL;
  BV_CLR_ALL(parameter->unit_flags);

  parameter->start_tile = punit->tile;
  parameter->moves_left_initially = punit->moves_left;
  parameter->move_rate = unit_move_rate(punit);
  if (is_air_unit(punit)) {
    parameter->fuel_left_initially = punit->fuel;
    parameter->fuel = unit_type(punit)->fuel;
  } else {
    parameter->fuel = 1;
    parameter->fuel_left_initially = 1;
  }
  parameter->owner = unit_owner(punit);
  parameter->class = get_unit_class(unit_type(punit));
  parameter->unit_flags = unit_type(punit)->flags;

  parameter->omniscience = !ai_handicap(unit_owner(punit), H_MAP);
}

/**********************************************************************
  Concatenate two paths together.  The additional segment (src_path)
  should start where the initial segment (dest_path) stops.  The
  overlapping position is removed.

  If dest_path == NULL, we just copy the src_path and nothing else.
***********************************************************************/
struct pf_path *pft_concat(struct pf_path *dest_path,
			   const struct pf_path *src_path)
{
  if (!dest_path) {
    dest_path = fc_malloc(sizeof(*dest_path));
    dest_path->length = src_path->length;
    dest_path->positions =
	fc_malloc(sizeof(*dest_path->positions) * dest_path->length);
    memcpy(dest_path->positions, src_path->positions,
	   sizeof(*dest_path->positions) * dest_path->length);
  } else {
    int old_length = dest_path->length;

    assert(pf_last_position(dest_path)->tile == src_path->positions[0].tile);
    assert(pf_last_position(dest_path)->moves_left ==
	   src_path->positions[0].moves_left);
    dest_path->length += src_path->length - 1;
    dest_path->positions =
	fc_realloc(dest_path->positions,
		   sizeof(*dest_path->positions) * dest_path->length);
    /* Be careful to include the first position of src_path, it contains
     * the direction (it is undefined in the last position of dest_path) */
    memcpy(dest_path->positions + old_length - 1, src_path->positions,
	   src_path->length * sizeof(*dest_path->positions));
  }
  return dest_path;
}

/****************************************************************************
  Remove the part of a path leading up to a given tile.
  The given tile must be on the path.  If it is on the path more than once
  then the first occurrance will be the one used.
****************************************************************************/
void pft_advance_path(struct pf_path *path, struct tile *ptile)
{
  int i;
  struct pf_position *new_positions;

  for (i = 0; i < path->length; i++) {
    if (path->positions[i].tile == ptile) {
      break;
    }
  }
  assert(i < path->length);
  path->length -= i;
  new_positions = fc_malloc(sizeof(*path->positions) * path->length);
  memcpy(new_positions, path->positions + i,
	 path->length * sizeof(*path->positions));
  free(path->positions);
  path->positions = new_positions;
}
