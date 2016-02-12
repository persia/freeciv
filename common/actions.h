/**********************************************************************
 Freeciv - Copyright (C) 1996-2013 - Freeciv Development Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifndef FC_ACTIONS_H
#define FC_ACTIONS_H

/* common */
#include "fc_types.h"
#include "metaknowledge.h"
#include "requirements.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SPECENUM_NAME action_actor_kind
#define SPECENUM_VALUE0 AAK_UNIT
#define SPECENUM_VALUE0NAME N_("a unit")
#define SPECENUM_COUNT AAK_COUNT
#include "specenum_gen.h"

#define SPECENUM_NAME action_target_kind
#define SPECENUM_VALUE0 ATK_CITY
#define SPECENUM_VALUE0NAME N_("individual cities")
#define SPECENUM_VALUE1 ATK_UNIT
#define SPECENUM_VALUE1NAME N_("individual units")
#define SPECENUM_VALUE2 ATK_UNITS
#define SPECENUM_VALUE2NAME N_("unit stacks")
#define SPECENUM_VALUE3 ATK_TILE
#define SPECENUM_VALUE3NAME N_("tiles")
/* No target except the actor it self. */
#define SPECENUM_VALUE4 ATK_SELF
#define SPECENUM_VALUE4NAME N_("it self")
#define SPECENUM_COUNT ATK_COUNT
#include "specenum_gen.h"

/* Used in the network protocol. */
#define SPECENUM_NAME gen_action
#define SPECENUM_VALUE0 ACTION_ESTABLISH_EMBASSY
#define SPECENUM_VALUE0NAME N_("Establish Embassy")
#define SPECENUM_VALUE1 ACTION_SPY_INVESTIGATE_CITY
#define SPECENUM_VALUE1NAME N_("Investigate City")
#define SPECENUM_VALUE2 ACTION_SPY_POISON
#define SPECENUM_VALUE2NAME N_("Poison City")
#define SPECENUM_VALUE3 ACTION_SPY_STEAL_GOLD
#define SPECENUM_VALUE3NAME N_("Steal Gold")
#define SPECENUM_VALUE4 ACTION_SPY_SABOTAGE_CITY
#define SPECENUM_VALUE4NAME N_("Sabotage City")
#define SPECENUM_VALUE5 ACTION_SPY_TARGETED_SABOTAGE_CITY
#define SPECENUM_VALUE5NAME N_("Targeted Sabotage City")
#define SPECENUM_VALUE6 ACTION_SPY_STEAL_TECH
#define SPECENUM_VALUE6NAME N_("Steal Tech")
#define SPECENUM_VALUE7 ACTION_SPY_TARGETED_STEAL_TECH
#define SPECENUM_VALUE7NAME N_("Targeted Steal Tech")
#define SPECENUM_VALUE8 ACTION_SPY_INCITE_CITY
#define SPECENUM_VALUE8NAME N_("Incite City")
#define SPECENUM_VALUE9 ACTION_TRADE_ROUTE
#define SPECENUM_VALUE9NAME N_("Establish Trade Route")
#define SPECENUM_VALUE10 ACTION_MARKETPLACE
#define SPECENUM_VALUE10NAME N_("Enter Marketplace")
#define SPECENUM_VALUE11 ACTION_HELP_WONDER
#define SPECENUM_VALUE11NAME N_("Help Wonder")
#define SPECENUM_VALUE12 ACTION_SPY_BRIBE_UNIT
#define SPECENUM_VALUE12NAME N_("Bribe Unit")
#define SPECENUM_VALUE13 ACTION_SPY_SABOTAGE_UNIT
#define SPECENUM_VALUE13NAME N_("Sabotage Unit")
#define SPECENUM_VALUE14 ACTION_CAPTURE_UNITS
#define SPECENUM_VALUE14NAME N_("Capture Units")
#define SPECENUM_VALUE15 ACTION_FOUND_CITY
#define SPECENUM_VALUE15NAME N_("Found City")
#define SPECENUM_VALUE16 ACTION_JOIN_CITY
#define SPECENUM_VALUE16NAME N_("Join City")
#define SPECENUM_VALUE17 ACTION_STEAL_MAPS
#define SPECENUM_VALUE17NAME N_("Steal Maps")
#define SPECENUM_VALUE18 ACTION_BOMBARD
#define SPECENUM_VALUE18NAME N_("Bombard")
#define SPECENUM_VALUE19 ACTION_SPY_NUKE
#define SPECENUM_VALUE19NAME N_("Suitcase Nuke")
#define SPECENUM_VALUE20 ACTION_NUKE
#define SPECENUM_VALUE20NAME N_("Explode Nuclear")
#define SPECENUM_VALUE21 ACTION_DESTROY_CITY
#define SPECENUM_VALUE21NAME N_("Destroy City")
#define SPECENUM_VALUE22 ACTION_EXPEL_UNIT
#define SPECENUM_VALUE22NAME N_("Expel Unit")
#define SPECENUM_VALUE23 ACTION_RECYCLE_UNIT
#define SPECENUM_VALUE23NAME N_("Recycle Unit")
#define SPECENUM_VALUE24 ACTION_DISBAND_UNIT
#define SPECENUM_VALUE24NAME N_("Disband Unit")
#define SPECENUM_VALUE25 ACTION_HOME_CITY
#define SPECENUM_VALUE25NAME N_("Home City")
#define SPECENUM_VALUE26 ACTION_UPGRADE_UNIT
#define SPECENUM_VALUE26NAME N_("Upgrade Unit")
#define SPECENUM_VALUE27 ACTION_PARADROP
#define SPECENUM_VALUE27NAME N_("Paradrop Unit")
/* Limited by what values num2char() can store in unit orders in
 * savegames. */
#define SPECENUM_COUNT ACTION_COUNT
#include "specenum_gen.h"

/* Used in searches to signal that any action at all is OK. */
#define ACTION_ANY ACTION_COUNT

/*
 * Action probability
 *
 * An action probability is the probability that an action will be
 * successfull under the given cicumstances. It goes from 0% to 100%.
 * Alternatively it can signal a special case.
 *
 * Values from 0 up to and including 200 are to be understood as the chance
 * of success measured in half percentag points. The value 3 indicates that
 * the chance is 1.5%. The value 10 indicates that the chance is 5%. The
 * probability may have been rounded to the nearest half percentage point.
 *
 * Values above 200 (100%) up to and including 255 are special cases. All
 * special cases should be declared and documented below. A value in this
 * range should be considered a bug if it isn't. If a special value for
 * internal use is needed please avoid the range from and including 0 up
 * to and including 255.
 *
 */
typedef int action_probability;

/*
 * ACTPROB_IMPOSSIBLE is another way of saying that the probability is 0%.
 */
#define ACTPROB_IMPOSSIBLE 0

/*
 * The special value ACTPROB_NA indicates that no probability should exist.
 */
#define ACTPROB_NA 253

/*
 * The special value ACTPROB_NOT_IMPLEMENTED indicates that support
 * for finding this probability currently is missing.
 */
#define ACTPROB_NOT_IMPLEMENTED 254

/*
 * The special value ACTPROB_NOT_KNOWN indicates that the player don't know
 * enough to find out. It is caused by the probability depending on a rule
 * that depends on game state the player don't have access to. It may be
 * possible for the player to later gain access to this game state.
 */
#define ACTPROB_NOT_KNOWN 255

/* Who ordered the action to be performed? */
#define SPECENUM_NAME action_requester
/* The player ordered it directly. */
#define SPECENUM_VALUE0 ACT_REQ_PLAYER
#define SPECENUM_VALUE0NAME N_("the player")
/* The game it self because the rules requires it. */
#define SPECENUM_VALUE1 ACT_REQ_RULES
#define SPECENUM_VALUE1NAME N_("the game rules")
/* A server side autonomous agent working for the player. */
#define SPECENUM_VALUE2 ACT_REQ_SS_AGENT
#define SPECENUM_VALUE2NAME N_("a server agent")
/* Number of action requesters. */
#define SPECENUM_COUNT ACT_REQ_COUNT
#include "specenum_gen.h"

struct action
{
  enum gen_action id;
  enum action_actor_kind actor_kind;
  enum action_target_kind target_kind;

  bool hostile; /* TODO: Should this be a scale in stead? */

  /* Is the player required to specify details about this action? Only true
   * IFF the action needs details AND the server won't fill them in when
   * unspecified. */
  bool requires_details;

  /* A unit's ability to perform this action will pop up the action
   * selection dialog before the player asks for it only in exceptional
   * cases.
   *
   * The motivation for setting rare_pop_up is to minimize player
   * annoyance and mistakes. Getting a pop up every time a unit moves is
   * annoying. An unexpected offer to do something that in many cases is
   * destructive can lead the player's muscle memory to perform the wrong
   * action. */
  bool rare_pop_up;

  /* Limits on the distance on the map between the actor and the target.
   * The action is legal iff the distance is min_distance, max_distance or
   * a value in between. */
  int min_distance, max_distance;

  /* The name of the action shown in the UI */
  char ui_name[MAX_LEN_NAME];
};

struct action_enabler
{
  enum gen_action action;
  struct requirement_vector actor_reqs;
  struct requirement_vector target_reqs;
};

#define SPECLIST_TAG action_enabler
#define SPECLIST_TYPE struct action_enabler
#include "speclist.h"
#define action_enabler_list_iterate(action_enabler_list, aenabler) \
  TYPED_LIST_ITERATE(struct action_enabler, action_enabler_list, aenabler)
#define action_enabler_list_iterate_end LIST_ITERATE_END

#define action_iterate(_act_)                          \
{                                                      \
  int _act_;                                           \
  for (_act_ = 0; _act_ < ACTION_COUNT; _act_++) {

#define action_iterate_end                             \
  }                                                    \
}

#define action_enablers_iterate(_enabler_)               \
{                                                        \
  action_iterate(_act_) {                                \
    action_enabler_list_iterate(                         \
      action_enablers_for_action(_act_), _enabler_) {

#define action_enablers_iterate_end                      \
    } action_enabler_list_iterate_end;                   \
  } action_iterate_end;                                  \
}

/* The reason why an action should be auto performed. */
#define SPECENUM_NAME action_auto_perf_cause
/* Can't pay the unit's upkeep. */
/* (Can be triggered by food, shield or gold upkeep) */
#define SPECENUM_VALUE0 AAPC_UNIT_UPKEEP
#define SPECENUM_VALUE0NAME N_("Unit Upkeep")
/* Number of forced action auto performer causes. */
#define SPECENUM_COUNT AAPC_COUNT
#include "specenum_gen.h"

/* An Action Auto Performer rule makes an actor try to perform an action
 * without being ordered to do so by the player controlling it.
 * - the first auto performer that matches the cause and fulfills the reqs
 *   is selected.
 * - the actions listed by the selected auto performer is tried in order
 *   until an action is successful, all actions have been tried or the
 *   actor disappears.
 * - if no action inside the selected auto performer is legal no action is
 *   performed. The system won't try to select another auto performer.
 */
struct action_auto_perf
{
  /* The reason for trying to auto perform an action. */
  enum action_auto_perf_cause cause;

  /* Must be fulfilled if the game should try to force an action from this
   * action auto performer. */
  struct requirement_vector reqs;

  /* Auto perform the first legal action in this list.
   * The list is terminated by ACTION_COUNT. */
  enum gen_action alternatives[ACTION_COUNT];
};

#define action_auto_perf_iterate(_act_perf_)                              \
{                                                                         \
  int _ap_num_;                                                           \
                                                                          \
  for (_ap_num_ = 0;                                                      \
       _ap_num_ < MAX_NUM_ACTION_AUTO_PERFORMERS                          \
       && (action_auto_perf_by_number(_ap_num_)->cause                    \
           != AAPC_COUNT);                                                \
       _ap_num_++) {                                                      \
    const struct action_auto_perf *_act_perf_                             \
              = action_auto_perf_by_number(_ap_num_);

#define action_auto_perf_iterate_end                                      \
  }                                                                       \
}

#define action_auto_perf_by_cause_iterate(_cause_, _act_perf_)            \
action_auto_perf_iterate(_act_perf_) {                                    \
  if (_act_perf_->cause != _cause_) {                                     \
    continue;                                                             \
  }

#define action_auto_perf_by_cause_iterate_end                             \
} action_auto_perf_iterate_end

/* Hard coded location of action auto performers. Used for conversion while
 * action auto performers aren't directly exposed to the ruleset. */
#define ACTION_AUTO_UPKEEP_FOOD   0
#define ACTION_AUTO_UPKEEP_GOLD   1
#define ACTION_AUTO_UPKEEP_SHIELD 2

/* Initialization */
void actions_init(void);
void actions_free(void);

bool actions_are_ready(void);

bool action_id_is_valid(const int action_id);

struct action *action_by_number(int action_id);
struct action *action_by_rule_name(const char *name);

enum action_actor_kind action_get_actor_kind(int action_id);
enum action_target_kind action_get_target_kind(int action_id);

int action_number(const struct action *action);
const char *action_rule_name(const struct action *action);

const char *action_name_translation(const struct action *action);

bool action_is_hostile(int action_id);

bool action_requires_details(int action_id);

bool action_id_is_rare_pop_up(int action_id);

bool action_distance_accepted(const struct action *action,
                              const int distance);
#define action_id_distance_accepted(action_id, distance)                  \
  action_distance_accepted(action_by_number(action_id), distance)

int action_get_role(int action_id);

const char *action_get_rule_name(int action_id);
const char *action_get_ui_name(int action_id);
const char *action_prepare_ui_name(int action_id, const char* mnemonic,
                                   const action_probability prob,
                                   const char *custom);
const char *action_get_tool_tip(const int action_id,
                                const action_probability act_prob);

struct action_enabler_list *
action_enablers_for_action(enum gen_action action);

struct action_enabler *action_enabler_new(void);
struct action_enabler *
action_enabler_copy(const struct action_enabler *original);
void action_enabler_add(struct action_enabler *enabler);

struct action *action_blocks_attack(const struct unit *actor_unit,
                                    const struct tile *target_tile);
struct action *action_is_blocked_by(const int action_id,
                                    const struct unit *actor_unit,
                                    const struct tile *target_tile,
                                    const struct city *target_city,
                                    const struct unit *target_unit);

bool is_action_enabled_unit_on_city(const enum gen_action wanted_action,
                                    const struct unit *actor_unit,
                                    const struct city *target_city);

bool is_action_enabled_unit_on_unit(const enum gen_action wanted_action,
                                    const struct unit *actor_unit,
                                    const struct unit *target_unit);

bool is_action_enabled_unit_on_units(const enum gen_action wanted_action,
                                     const struct unit *actor_unit,
                                     const struct tile *target_tile);

bool is_action_enabled_unit_on_tile(const enum gen_action wanted_action,
                                    const struct unit *actor_unit,
                                    const struct tile *target_tile);

bool is_action_enabled_unit_on_self(const enum gen_action wanted_action,
                                    const struct unit *actor_unit);

action_probability action_prob_vs_city(const struct unit* actor,
                                       const int action_id,
                                       const struct city* victim);

action_probability action_prob_vs_unit(const struct unit* actor,
                                       const int action_id,
                                       const struct unit* victim);

action_probability action_prob_vs_units(const struct unit* actor,
                                        const int action_id,
                                        const struct tile* victims);

action_probability action_prob_vs_tile(const struct unit *actor,
                                       const int action_id,
                                       const struct tile *victims);

action_probability action_prob_self(const struct unit *actor,
                                    const int action_id);

bool action_prob_possible(action_probability probability);

bool
action_actor_utype_hard_reqs_ok(const enum gen_action wanted_action,
                                const struct unit_type *actor_unittype);

/* Reasoning about actions */
bool action_immune_government(struct government *gov, int act);

bool is_action_possible_on_city(const enum gen_action action_id,
                                const struct player *actor_player,
                                const struct city* target_city);

/* Action auto performers */
const struct action_auto_perf *action_auto_perf_by_number(const int num);
struct action_auto_perf *action_auto_perf_slot_number(const int num);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FC_ACTIONS_H */
