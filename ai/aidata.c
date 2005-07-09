/********************************************************************** 
 Freeciv - Copyright (C) 2002 - The Freeciv Project
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

#include "aisupport.h"
#include "city.h"
#include "effects.h"
#include "game.h"
#include "government.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "movement.h"
#include "unit.h"

#include "citytools.h"
#include "diplhand.h"
#include "maphand.h"
#include "settlers.h"
#include "unittools.h"

#include "path_finding.h"
#include "pf_tools.h"

#include "advdiplomacy.h"
#include "advmilitary.h"
#include "aicity.h"
#include "aiferry.h"
#include "aihand.h"
#include "ailog.h"
#include "aitools.h"
#include "aiunit.h"

#include "aidata.h"

/*
 * Whether the movemap positions are computed by ignoring xones of control.
 * Ignoring zones of control means that positions that are temporarilly blocked
 * by units are marked as accessible; this is sensible for destinations several
 * turns away.
 */
#define MOVEMAP_IGZOC TRUE

static struct ai_data aidata[MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS];

/**************************************************************************
  Fill movemap with data.  This consumes a lot of CPU.
**************************************************************************/
void ai_data_movemap_init(void)
{
  movemap = fc_calloc(sizeof(*movemap), MAP_INDEX_SIZE);
  whole_map_iterate(ptile) {
    unsigned int r;

    for (r = 0; r < MOVEMAP_RANGE; r++) {
      movemap_vector_init(&MOVEMAP(ptile).range[r]);
    }
  } whole_map_iterate_end;
}

/**************************************************************************
  Dealloc.
**************************************************************************/
void ai_data_movemap_done(void)
{
  /* Clean the slate */
  whole_map_iterate(ptile) {
    unsigned int r;

    for (r = 0; r < MOVEMAP_RANGE; r++) {
      movemap_vector_free(&MOVEMAP(ptile).range[r]);
    }
  } whole_map_iterate_end;

  free(movemap);
}

/**************************************************************************
  Insert unit into list unless it is already there. Expensive operation
  but necessary.
**************************************************************************/
static inline void movemap_insert(struct movemap_vector *vector,
				  struct unit *punit)
{
  bool has_already = FALSE;

  movemap_vector_iterate((*vector), check) {
    if (check == punit->id) {
      has_already = TRUE;
      break;
    }
  } movemap_vector_iterate_end;
  if (!has_already) {
    movemap_vector_append(vector, &punit->id);
  }
}

/**************************************************************************
  Check where passengers onboard ferries can go.
  ptile1:
    landing tile
  id:
    unit ID of ferry
  delay:
    the number of turns of  land movement lost because of movement of
    the ferry and disembarking.

  We have to check that we don't accidentially insert a unit into a vector
  where it already exists. This is cumbersome but I see no other way.
**************************************************************************/
static void movemap_check_ferry(struct tile *ptile1, int id,
				const unsigned int delay)
{
  struct unit *ferry = find_unit_by_id(id);

  if (get_transporter_occupancy(ferry) > 0
      && is_sailing_unit(ferry)) {
    struct tile *ptile = ferry->tile;

    unit_list_iterate(ptile->units, passenger) {
      struct pf_map *pfmap;
      struct pf_parameter parameter;
      int moverate = unit_move_rate(passenger);

      if (!is_ground_unit(passenger)
          || passenger->transported_by != ferry->id) {
        continue;
      }
      pft_fill_unit_attack_param(&parameter, passenger);
      if (MOVEMAP_IGZOC) {
	parameter.get_zoc = NULL;
      }
      parameter.start_tile = ptile1; /* Suppose it landed right here... */
      pfmap = pf_create_map(&parameter);
      pf_iterator(pfmap, pos) {
	const unsigned int when = (pos.total_MC - 1) / moverate + delay;

	if (when < MOVEMAP_RANGE) {
	  movemap_insert(&MOVEMAP(pos.tile).range[when], passenger);
	}
	/* else too far */
      } pf_iterator_end;
      pf_destroy_map(pfmap);
    } unit_list_iterate_end;
  }
}

/**************************************************************************
  The movemap structure is a quick way to find threats on the map.  Use
  the iterators defined in the header file to iterate over units that
  can reach a tile in one or two turns (as specified).  We correctly 
  calculate in the possibility of travel by ferry.

  This function fills the movemap with data.  It is rather CPU intensive.
**************************************************************************/
void ai_data_movemap_recalculate(void)
{
  TIMING_LOG(AIT_MOVEMAP, TIMER_START);

  /* Clean the slate */
  whole_map_iterate(ptile) {
    unsigned int r;
    for (r = 0; r < MOVEMAP_RANGE; r++) {
      movemap_vector_free(&MOVEMAP(ptile).range[r]);
    }
  } whole_map_iterate_end;

  players_iterate(pplayer) {
    unit_list_iterate(pplayer->units, punit) {
      struct pf_map *pfmap;
      struct pf_parameter parameter;
      int moverate = unit_move_rate(punit);

      if (get_transporter_occupancy(punit) > 0
          && is_sailing_unit(punit)) {
        pft_fill_unit_overlap_param(&parameter, punit);
      } else {
        pft_fill_unit_attack_param(&parameter, punit);
      }
      if (MOVEMAP_IGZOC) {
	parameter.get_zoc = NULL;
      }
      pfmap = pf_create_map(&parameter);
      pf_iterator(pfmap, pos) {
	const unsigned int when = (pos.total_MC - 1) / moverate;

	if (when < MOVEMAP_RANGE) {
	  movemap_vector_append(&MOVEMAP(pos.tile).range[when], &punit->id);
	}
	/* else too far */
      } pf_iterator_end;
      pf_destroy_map(pfmap);
    } unit_list_iterate_end;
  } players_iterate_end;

  /* Now do ferries. This is ugly, but correct. I gave up on beating
   * pf into submission. It is also probably very very slow. */
  whole_map_iterate(ptile) {
    unsigned int r;

    if (is_ocean(tile_get_terrain(ptile))) {
      continue;
    }
    /* Check all ferries that can land on this spot. */
    for (r = 0; r < MOVEMAP_RANGE - game.info.slow_invasions; r++) {
      const unsigned int delay = r + game.info.slow_invasions;

      movemap_vector_iterate(MOVEMAP(ptile).range[r], id) {
	movemap_check_ferry(ptile, id, delay);
      } movemap_vector_iterate_end;
    }
  } whole_map_iterate_end;

  TIMING_LOG(AIT_MOVEMAP, TIMER_STOP);
}

/**************************************************************************
  According to the movemap, how many turns will it take for a unit
  to reach a destination tile? Returns a value <= MOVEMAP_RANGE + 1;
  a value of MOVEMAP_RANGE + 1 indicates the unit is outside movemap range.
**************************************************************************/
unsigned int movemap_turns(struct unit *punit, struct tile *dest)
{
  unsigned int r;

  unit_list_iterate(dest->units, aunit) {
    if (punit == aunit) {
      return 0;
    }
  } unit_list_iterate_end;

  for (r = 0; r < MOVEMAP_RANGE; r++) {
    movemap_iterate(dest, r, aunit) {
      if (punit == aunit) {
	return r + 1;
      }
    } movemap_iterate_end;
  }

  /* else out of movemap range */
  return MOVEMAP_RANGE + 1;
}

/**************************************************************************
  Precalculates some important data about the improvements in the game
  that we use later in ai/aicity.c.  We mark improvements as 'calculate'
  if we want to run a full test on them, as 'estimate' if we just want
  to do some guesses on them, or as 'unused' is they are useless to us.
  Then we find the largest range of calculatable effects in the
  improvement and record it for later use.
**************************************************************************/
static void ai_data_city_impr_calc(struct player *pplayer, struct ai_data *ai)
{
  int count[AI_IMPR_LAST];

  memset(count, 0, sizeof(count));

  impr_type_iterate(id) {
    struct req_source source = {
      .type = REQ_BUILDING,
      .value = {.building = id}
    };

    ai->impr_calc[id] = AI_IMPR_ESTIMATE;

    /* Find largest extension */
    effect_list_iterate(get_req_source_effects(&source), peffect) {
      switch (peffect->type) {
#if 0
      /* TODO */
      case EFT_FORCE_CONTENT:
      case EFT_FORCE_CONTENT_PCT:
      case EFT_MAKE_CONTENT:
      case EFT_MAKE_CONTENT_MIL:
      case EFT_MAKE_CONTENT_MIL_PER:
      case EFT_MAKE_CONTENT_PCT:
      case EFT_MAKE_HAPPY:
#endif
      case EFT_CAPITAL_CITY:
      case EFT_POLLU_POP_PCT:
      case EFT_POLLU_PROD_PCT:
      case EFT_OUTPUT_BONUS:
      case EFT_OUTPUT_BONUS_2:
      case EFT_OUTPUT_ADD_TILE:
      case EFT_OUTPUT_PER_TILE:
      case EFT_OUTPUT_INC_TILE:
      case EFT_OUTPUT_WASTE_PCT:
      case EFT_UPKEEP_FREE:
	requirement_list_iterate(peffect->reqs, preq) {
	  if (preq->source.type == REQ_BUILDING
	      && preq->source.value.building == id) {
	    ai->impr_calc[id] = AI_IMPR_CALCULATE;
	    if (preq->range > ai->impr_range[id]) {
	      ai->impr_range[id] = preq->range;
	    }
	  }
	} requirement_list_iterate_end;
      break;
      default:
      /* Nothing! */
      break;
      }
    } effect_list_iterate_end;
  } impr_type_iterate_end;
}

/**************************************************************************
  Check if the player still takes advantage of EFT_TECH_PARASITE.
  Research is useless if there are still techs which may be given to the
  player for free.
**************************************************************************/
static bool player_has_really_useful_tech_parasite(struct player* pplayer)
{
  int players_needed = get_player_bonus(pplayer, EFT_TECH_PARASITE);

  if (players_needed == 0) {
    return FALSE;
  }
  
  tech_type_iterate(tech) {
    int players_having;

    if (get_invention(pplayer, tech) == TECH_KNOWN
        || !tech_is_available(pplayer, tech)) {
      continue;
    }

    players_having = 0;

    players_iterate(aplayer) {
      if (aplayer == pplayer || !aplayer->is_alive) {
        continue;
      }

      if (get_invention(aplayer, tech) == TECH_KNOWN
          || get_player_research(aplayer)->researching == tech) {
	players_having++;
	if (players_having >= players_needed) {
	  return TRUE;
	}
      }
    } players_iterate_end;
  } tech_type_iterate_end;
  return FALSE;
}

/**************************************************************************
  Analyze rulesets. Must be run after rulesets after loaded, unlike
  _init, which must be run before savegames are loaded, which is usually
  before rulesets.
**************************************************************************/
void ai_data_analyze_rulesets(struct player *pplayer)
{
  struct ai_data *ai = &aidata[pplayer->player_no];

  ai_data_city_impr_calc(pplayer, ai);
}

/**************************************************************************
  This function is called each turn to initialize pplayer->ai.stats.units.
**************************************************************************/
static void count_my_units(struct player *pplayer)
{
  struct ai_data *ai = ai_data_get(pplayer);

  memset(&ai->stats.units, 0, sizeof(ai->stats.units));

  unit_list_iterate(pplayer->units, punit) {
    switch (unit_type(punit)->move_type) {
    case LAND_MOVING:
      ai->stats.units.land++;
      break;
    case SEA_MOVING:
      ai->stats.units.sea++;
      break;
    case HELI_MOVING:
    case AIR_MOVING:
      ai->stats.units.air++;
      break;
    }

    if (unit_flag(punit, F_TRIREME)) {
      ai->stats.units.triremes++;
    }
    if (unit_flag(punit, F_MISSILE)) {
      ai->stats.units.missiles++;
    }
    if (can_upgrade_unittype(pplayer, punit->type) >= 0) {
      ai->stats.units.upgradeable++;
    }
  } unit_list_iterate_end;
}

/**************************************************************************
  Make and cache lots of calculations needed for other functions.

  Note: We use map.num_continents here rather than pplayer->num_continents
  because we are omniscient and don't care about such trivialities as who
  can see what.

  FIXME: We should try to find the lowest common defence strength of our
  defending units, and ignore enemy units that are incapable of harming 
  us, instead of just checking attack strength > 1.
**************************************************************************/
void ai_data_phase_init(struct player *pplayer, bool is_new_phase)
{
  struct ai_data *ai = &aidata[pplayer->player_no];
  int i, nuke_units = num_role_units(F_NUCLEAR);
  bool danger_of_nukes = FALSE;
  int ally_strength = -1;
  struct player *ally_strongest = NULL;

  /*** Threats ***/

  TIMING_LOG(AIT_AIDATA, TIMER_START);

  ai->num_continents    = map.num_continents;
  ai->num_oceans        = map.num_oceans;
  ai->threats.continent = fc_calloc(ai->num_continents + 1, sizeof(bool));
  ai->threats.invasions = FALSE;
  ai->threats.air       = FALSE;
  ai->threats.nuclear   = 0; /* none */
  ai->threats.ocean     = fc_calloc(ai->num_oceans + 1, sizeof(bool));
  ai->threats.igwall    = FALSE;

  players_iterate(aplayer) {
    if (!is_player_dangerous(pplayer, aplayer)) {
      continue;
    }

    /* The idea is that if there aren't any hostile cities on
     * our continent, the danger of land attacks is not big
     * enough to warrant city walls. Concentrate instead on 
     * coastal fortresses and hunting down enemy transports. */
    city_list_iterate(aplayer->cities, acity) {
      Continent_id continent = tile_get_continent(acity->tile);
      ai->threats.continent[continent] = TRUE;
    } city_list_iterate_end;

    unit_list_iterate(aplayer->units, punit) {
      if (unit_flag(punit, F_IGWALL)) {
        ai->threats.igwall = TRUE;
      }

      if (is_sailing_unit(punit)) {
        /* If the enemy has not started sailing yet, or we have total
         * control over the seas, don't worry, keep attacking. */
        if (is_ground_units_transport(punit)) {
          ai->threats.invasions = TRUE;
        }

        /* The idea is that while our enemies don't have any offensive
         * seaborne units, we don't have to worry. Go on the offensive! */
        if (unit_type(punit)->attack_strength > 1) {
	  if (is_ocean(tile_get_terrain(punit->tile))) {
	    Continent_id continent = tile_get_continent(punit->tile);
	    ai->threats.ocean[-continent] = TRUE;
	  } else {
	    adjc_iterate(punit->tile, tile2) {
	      if (is_ocean(tile_get_terrain(tile2))) {
	        Continent_id continent = tile_get_continent(tile2);
	        ai->threats.ocean[-continent] = TRUE;
	      }
	    } adjc_iterate_end;
	  }
        } 
        continue;
      }

      /* The next idea is that if our enemies don't have any offensive
       * airborne units, we don't have to worry. Go on the offensive! */
      if ((is_air_unit(punit) || is_heli_unit(punit))
           && unit_type(punit)->attack_strength > 1) {
        ai->threats.air = TRUE;
      }

      /* If our enemy builds missiles, worry about missile defence. */
      if (unit_flag(punit, F_MISSILE)
          && unit_type(punit)->attack_strength > 1) {
        ai->threats.missile = TRUE;
      }

      /* If he builds nukes, worry a lot. */
      if (unit_flag(punit, F_NUCLEAR)) {
        danger_of_nukes = TRUE;
      }
    } unit_list_iterate_end;

    /* Check for nuke capability */
    for (i = 0; i < nuke_units; i++) {
      Unit_type_id nuke = get_role_unit(F_NUCLEAR, i);
      if (can_player_build_unit_direct(aplayer, nuke)) { 
        ai->threats.nuclear = 1;
      }
    }
  } players_iterate_end;

  /* Increase from fear to terror if opponent actually has nukes */
  if (danger_of_nukes) ai->threats.nuclear++; /* sum of both fears */

  /*** Exploration ***/

  ai->explore.land_done = TRUE;
  ai->explore.sea_done = TRUE;
  ai->explore.continent = fc_calloc(ai->num_continents + 1, sizeof(bool));
  ai->explore.ocean = fc_calloc(ai->num_oceans + 1, sizeof(bool));
  whole_map_iterate(ptile) {
    Continent_id continent = tile_get_continent(ptile);

    if (is_ocean(ptile->terrain)) {
      if (ai->explore.sea_done && ai_handicap(pplayer, H_TARGETS) 
          && !map_is_known(ptile, pplayer)) {
	/* We're not done there. */
        ai->explore.sea_done = FALSE;
        ai->explore.ocean[-continent] = TRUE;
      }
      /* skip rest, which is land only */
      continue;
    }
    if (ai->explore.continent[ptile->continent]) {
      /* we don't need more explaining, we got the point */
      continue;
    }
    if (tile_has_special(ptile, S_HUT) 
        && (!ai_handicap(pplayer, H_HUTS)
             || map_is_known(ptile, pplayer))) {
      ai->explore.land_done = FALSE;
      ai->explore.continent[continent] = TRUE;
      continue;
    }
    if (ai_handicap(pplayer, H_TARGETS) && !map_is_known(ptile, pplayer)) {
      /* this AI must explore */
      ai->explore.land_done = FALSE;
      ai->explore.continent[continent] = TRUE;
    }
  } whole_map_iterate_end;

  /*** Statistics ***/

  ai->stats.workers = fc_calloc(ai->num_continents + 1, sizeof(int));
  ai->stats.cities = fc_calloc(ai->num_continents + 1, sizeof(int));
  ai->stats.average_production = 0;
  city_list_iterate(pplayer->cities, pcity) {
    ai->stats.cities[(int)tile_get_continent(pcity->tile)]++;
    ai->stats.average_production += pcity->surplus[O_SHIELD];
  } city_list_iterate_end;
  ai->stats.average_production /= MAX(1, city_list_size(pplayer->cities));
  BV_CLR_ALL(ai->stats.diplomat_reservations);
  unit_list_iterate(pplayer->units, punit) {
    struct tile *ptile = punit->tile;

    if (!is_ocean(ptile->terrain) && unit_flag(punit, F_SETTLERS)) {
      ai->stats.workers[(int)tile_get_continent(punit->tile)]++;
    }
    if (unit_flag(punit, F_DIPLOMAT) && punit->ai.ai_role == AIUNIT_ATTACK) {
      /* Heading somewhere on a mission, reserve target. */
      struct city *pcity = tile_get_city(punit->goto_tile);

      if (pcity) {
        BV_SET(ai->stats.diplomat_reservations, pcity->id);
      }
    }
  } unit_list_iterate_end;
  aiferry_init_stats(pplayer);

  /*** Diplomacy ***/

  if (pplayer->ai.control && !is_barbarian(pplayer) && is_new_phase) {
    ai_diplomacy_begin_new_phase(pplayer, ai);
  }

  /* Set per-player variables. We must set all players, since players 
   * can be created during a turn, and we don't want those to have 
   * invalid values. */
  for (i = 0; i < MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS; i++) {
    struct player *aplayer = get_player(i);

    ai->diplomacy.player_intel[i].is_allied_with_enemy = NULL;
    ai->diplomacy.player_intel[i].at_war_with_ally = NULL;
    ai->diplomacy.player_intel[i].is_allied_with_ally = NULL;

    /* Determine who is the leader of our alliance. That is,
     * whoever has the more cities. */
    if (pplayers_allied(pplayer, aplayer)
        && city_list_size(aplayer->cities) > ally_strength) {
      ally_strength = city_list_size(aplayer->cities);
      ally_strongest = aplayer;
    }

    players_iterate(check_pl) {
      if (check_pl == pplayer
          || check_pl == aplayer
          || !check_pl->is_alive) {
        continue;
      }
      if (pplayers_allied(aplayer, check_pl)
          && pplayer_get_diplstate(pplayer, check_pl)->type == DS_WAR) {
       ai->diplomacy.player_intel[i].is_allied_with_enemy = check_pl;
      }
      if (pplayers_allied(pplayer, check_pl)
          && pplayer_get_diplstate(aplayer, check_pl)->type == DS_WAR) {
        ai->diplomacy.player_intel[i].at_war_with_ally = check_pl;
      }
      if (pplayers_allied(aplayer, check_pl)
          && pplayers_allied(pplayer, check_pl)) {
        ai->diplomacy.player_intel[i].is_allied_with_ally = check_pl;
      }
    } players_iterate_end;
  }
  if (ally_strongest != ai->diplomacy.alliance_leader) {
    ai->diplomacy.alliance_leader = ally_strongest;
  }
  ai->diplomacy.spacerace_leader = player_leading_spacerace();
  
  ai->diplomacy.production_leader = NULL;
  players_iterate(aplayer) {
    if (ai->diplomacy.production_leader == NULL
        || ai->diplomacy.production_leader->score.mfg < aplayer->score.mfg) {
      ai->diplomacy.production_leader = aplayer;
    }
  } players_iterate_end;

  /*** Priorities ***/

  /* NEVER set these to zero! Weight values are usually multiplied by 
   * these values, so be careful with them. They are used in city 
   * and government calculations, and food and shields should be 
   * slightly bigger because we only look at surpluses there. They
   * are all WAGs. */
  ai->food_priority = FOOD_WEIGHTING;
  ai->shield_priority = SHIELD_WEIGHTING;
  if (ai_wants_no_science(pplayer)) {
    ai->luxury_priority = TRADE_WEIGHTING;
    ai->science_priority = 1;
  } else {
    ai->luxury_priority = 1;
    ai->science_priority = TRADE_WEIGHTING;
  }
  ai->gold_priority = TRADE_WEIGHTING;
  ai->happy_priority = 1;
  ai->unhappy_priority = TRADE_WEIGHTING; /* danger */
  ai->angry_priority = TRADE_WEIGHTING * 3; /* grave danger */
  ai->pollution_priority = POLLUTION_WEIGHTING;

  /*** Interception engine ***/

  /* We are tracking a unit if punit->ai.cur_pos is not NULL. If we
   * are not tracking, start tracking by setting cur_pos. If we are, 
   * fill prev_pos with previous cur_pos. This way we get the 
   * necessary coordinates to calculate a probably trajectory. */
  players_iterate(aplayer) {
    if (!aplayer->is_alive || aplayer == pplayer) {
      continue;
    }
    unit_list_iterate(aplayer->units, punit) {
      if (!punit->ai.cur_pos) {
        /* Start tracking */
        punit->ai.cur_pos = &punit->ai.cur_struct;
        punit->ai.prev_pos = NULL;
      } else {
        punit->ai.prev_struct = punit->ai.cur_struct;
        punit->ai.prev_pos = &punit->ai.prev_struct;
      }
      *punit->ai.cur_pos = punit->tile;
    } unit_list_iterate_end;
  } players_iterate_end;
  
  /* Research want */
  if (is_future_tech(get_player_research(pplayer)->researching)
      || player_has_really_useful_tech_parasite(pplayer)) {
    ai->wants_no_science = TRUE;
  } else {
    ai->wants_no_science = FALSE;
  }
  
  /* max num cities
   * The idea behind this code is that novice players don't understand that
   * expansion is critical and find it very annoying.
   * With the following code AI players will try to be only a bit better 
   * than the best human players. This should lead to more exciting games
   * for the beginners.
   */
  if (ai_handicap(pplayer, H_EXPANSION)) {
    bool found_human = FALSE;
    ai->max_num_cities = 3;
    players_iterate(aplayer) {
      if (aplayer == pplayer || aplayer->ai.control || !aplayer->is_alive) {
        continue;
      }
      ai->max_num_cities = MAX(ai->max_num_cities,
                               city_list_size(aplayer->cities) + 3);
      found_human = TRUE;
    } players_iterate_end;
    if (!found_human) {
      ai->max_num_cities = MAP_INDEX_SIZE;
    }
  } else {
    ai->max_num_cities = MAP_INDEX_SIZE;
  }

  count_my_units(pplayer);

  TIMING_LOG(AIT_AIDATA, TIMER_STOP);

  /* Government */
  TIMING_LOG(AIT_GOVERNMENT, TIMER_START);
  ai_best_government(pplayer);
  TIMING_LOG(AIT_GOVERNMENT, TIMER_STOP);
}

/**************************************************************************
  Clean up our mess.
**************************************************************************/
void ai_data_phase_done(struct player *pplayer)
{
  struct ai_data *ai = &aidata[pplayer->player_no];

  free(ai->explore.ocean);
  ai->explore.ocean = NULL;

  free(ai->explore.continent);
  ai->explore.continent = NULL;

  free(ai->threats.continent);
  ai->threats.continent = NULL;

  free(ai->threats.ocean);
  ai->threats.ocean = NULL;

  free(ai->stats.workers);
  ai->stats.workers = NULL;

  free(ai->stats.cities);
  ai->stats.cities = NULL;
}

/**************************************************************************
  Return a pointer to our data
**************************************************************************/
struct ai_data *ai_data_get(struct player *pplayer)
{
  struct ai_data *ai = &aidata[pplayer->player_no];

  if (ai->num_continents != map.num_continents
      || ai->num_oceans != map.num_oceans) {
    /* we discovered more continents, recalculate! */
    ai_data_phase_done(pplayer);
    ai_data_phase_init(pplayer, FALSE);
  }
  return ai;
}

/**************************************************************************
  Initialize with sane values.
**************************************************************************/
void ai_data_init(struct player *pplayer)
{
  struct ai_data *ai = &aidata[pplayer->player_no];
  int i;

  ai->govt_reeval = 0;
  ai->government_want = fc_realloc(ai->government_want,
				   ((game.control.government_count + 1)
				    * sizeof(*ai->government_want)));
  memset(ai->government_want, 0,
	 (game.control.government_count + 1) * sizeof(*ai->government_want));

  ai->wonder_city = 0;
  ai->diplomacy.target = NULL;
  ai->diplomacy.strategy = WIN_OPEN;
  ai->diplomacy.timer = 0;
  ai->diplomacy.countdown = 0;
  ai->diplomacy.love_coeff = 4; /* 4% */
  ai->diplomacy.love_incr = MAX_AI_LOVE * 4 / 100;
  ai->diplomacy.req_love_for_peace = MAX_AI_LOVE * 8 / 100;
  ai->diplomacy.req_love_for_alliance = MAX_AI_LOVE * 16 / 100;
  ai->diplomacy.req_love_for_ceasefire = 0;
  ai->diplomacy.alliance_leader = pplayer;

  for (i = 0; i < MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS; i++) {
    ai->diplomacy.player_intel[i].spam = i % 5; /* pseudorandom */
    ai->diplomacy.player_intel[i].distance = 1;
    ai->diplomacy.player_intel[i].ally_patience = 0;
    pplayer->ai.love[i] = 1;
    ai->diplomacy.player_intel[i].asked_about_peace = 0;
    ai->diplomacy.player_intel[i].asked_about_alliance = 0;
    ai->diplomacy.player_intel[i].asked_about_ceasefire = 0;
    ai->diplomacy.player_intel[i].warned_about_space = 0;
  }
  ai->wants_no_science = FALSE;
  ai->max_num_cities = 10000;
}
