/**********************************************************************
 Freeciv - Copyright (C) 1996-2015 - Freeciv Development Team
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
#include <fc_config.h>
#endif

/* common */
#include "actions.h"

/* server */
#include "aiiface.h"
#include "actiontools.h"
#include "notify.h"
#include "plrhand.h"
#include "unithand.h"

typedef void (*action_notify)(struct player *,
                              const int,
                              struct player *,
                              struct player *,
                              const struct tile *,
                              const char *);

/**************************************************************************
  Give the victim a casus belli against the offender.
**************************************************************************/
static void action_give_casus_belli(struct player *offender,
                                    struct player *victim_player,
                                    const bool global)
{
  if (global) {
    /* This action is seen as a reason for any other player, no matter who
     * the victim was, to declare war on the actor. It could be used to
     * label certain actions atrocities in rule sets where international
     * outrage over an action fits the setting. */

    players_iterate(oplayer) {
      if (oplayer != offender) {
        player_diplstate_get(oplayer, offender)->has_reason_to_cancel = 2;
      }
    } players_iterate_end;
  } else if (victim_player && offender != victim_player) {
    /* If an unclaimed tile is nuked there is no victim to give casus
     * belli. If an actor nukes his own tile he is more than willing to
     * forgive him self. */

    /* Give the victim player a casus belli. */
    player_diplstate_get(victim_player, offender)->has_reason_to_cancel =
        2;
  }
}

/**************************************************************************
  Returns the kind of diplomatic incident an action may cause.
**************************************************************************/
static enum incident_type action_to_incident(const int action_id)
{
  if (action_id == ACTION_NUKE
      || action_id == ACTION_SPY_NUKE) {
    return INCIDENT_NUCLEAR;
  } else {
    /* FIXME: Some actions are neither nuclear nor diplomat. */
    return INCIDENT_DIPLOMAT;
  }
}

/**************************************************************************
  Notify the players controlled by the built in AI.
**************************************************************************/
static void action_notify_ai(const int action_id,
                             struct player *offender,
                             struct player *victim_player)
{
  const int incident = action_to_incident(action_id);

  /* Notify the victim player. */
  call_incident(incident, offender, victim_player);

  if (incident == INCIDENT_NUCLEAR) {
    /* Tell the world. */
    if (offender == victim_player) {
      players_iterate(oplayer) {
        if (victim_player != oplayer) {
          call_incident(INCIDENT_NUCLEAR_SELF, offender, oplayer);
        }
      } players_iterate_end;
    } else {
      players_iterate(oplayer) {
        if (victim_player != oplayer) {
          call_incident(INCIDENT_NUCLEAR_NOT_TARGET, offender, oplayer);
        }
      } players_iterate_end;
    }
  }

  /* TODO: Should incident be called when the ai gets a casus belli because
   * of something done to a third party? If yes: should a new incident kind
   * be used? */
}

/**************************************************************************
  Take care of any consequences (like casus belli) of the given action
  when the situation was as specified.

  victim_player can be NULL
**************************************************************************/
static void action_consequence_common(const int action_id,
                                      struct player *offender,
                                      struct player *victim_player,
                                      const struct tile *victim_tile,
                                      const char *victim_link,
                                      const action_notify notify_actor,
                                      const action_notify notify_victim,
                                      const action_notify notify_global,
                                      const enum effect_type eft)
{
  int casus_belli_amount;

  /* The victim gets a casus belli if 1 or above. Everyone gets a casus
   * belli if 1000 or above. */
  casus_belli_amount =
      get_target_bonus_effects(NULL,
                               offender, victim_player,
                               tile_city(victim_tile),
                               NULL,
                               victim_tile,
                               NULL, NULL,
                               NULL, NULL,
                               action_by_number(action_id),
                               eft);

  if (casus_belli_amount >= 1) {
    /* In this situation the specified action provides a casus belli
     * against the actor. */

    /* This isn't just between the offender and the victim. */
    const bool global = casus_belli_amount >= 1000;

    /* Notify the involved players by sending them a message. */
    notify_actor(offender, action_id, offender, victim_player,
                victim_tile, victim_link);
    notify_victim(victim_player, action_id, offender, victim_player,
                  victim_tile, victim_link);

    if (global) {
      /* Every other player gets a casus belli against the actor. Tell each
       * players about it. */
      players_iterate(oplayer) {
        notify_global(oplayer, action_id, offender,
                      victim_player, victim_tile, victim_link);
      } players_iterate_end;
    }

    /* Give casus belli. */
    action_give_casus_belli(offender, victim_player, global);

    /* Notify players controlled by the built in AI. */
    action_notify_ai(action_id, offender, victim_player);

    /* Update the clients. */
    send_player_all_c(offender, NULL);

    if (victim_player != NULL && victim_player != offender) {
      /* The actor player was just sent. */
      /* An action against an ownerless tile is victimless. */
      send_player_all_c(victim_player, NULL);
    }
  }
}

/**************************************************************************
  Notify the actor that the failed action gave the victim a casus belli
  against the actor.
**************************************************************************/
static void notify_actor_caught(struct player *receiver,
                                const int action_id,
                                struct player *offender,
                                struct player *victim_player,
                                const struct tile *victim_tile,
                                const char *victim_link)
{
  if (!victim_player || offender == victim_player) {
    /* There is no victim or the actor did this to him self. */
    return;
  }

  /* Custom message based on action type. */
  switch (action_get_target_kind(action_id)) {
  case ATK_CITY:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Suitcase Nuke ... San Francisco */
                  _("You have caused an incident getting caught"
                    " trying to do %s to %s."),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_UNIT:
  case ATK_UNITS:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* Bribe Enemy Unit ... American ... Partisan */
                  _("You have caused an incident getting caught"
                    " trying to do %s to %s %s."),
                  action_get_ui_name(action_id),
                  nation_adjective_for_player(victim_player),
                  victim_link);
    break;
  case ATK_TILE:
    /* Explode Nuclear ... (54, 26) */
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  _("You have caused an incident getting caught"
                    " trying to do %s at %s."),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_SELF:
    /* Special actor notice not needed. Actor is victim. */
    break;
  case ATK_COUNT:
    fc_assert(ATK_COUNT != ATK_COUNT);
    break;
  }
}

/**************************************************************************
  Notify the victim that the failed action gave the victim a
  casus belli against the actor.
**************************************************************************/
static void notify_victim_caught(struct player *receiver,
                                 const int action_id,
                                 struct player *offender,
                                 struct player *victim_player,
                                 const struct tile *victim_tile,
                                 const char *victim_link)
{
  if (!victim_player || offender == victim_player) {
    /* There is no victim or the actor did this to him self. */
    return;
  }

  /* Custom message based on action type. */
  switch (action_get_target_kind(action_id)) {
  case ATK_CITY:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Europeans ... Suitcase Nuke ... San Francisco */
                  _("The %s have caused an incident getting caught"
                    " trying to do %s to %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_UNIT:
  case ATK_UNITS:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* Europeans ... Bribe Enemy Unit ... Partisan */
                  _("The %s have caused an incident getting caught"
                    " trying to do %s to your %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_TILE:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* Europeans ... Explode Nuclear ... (54, 26) */
                  _("The %s have caused an incident getting caught"
                    " trying to do %s at %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_SELF:
    /* Special victim notice not needed. Actor is victim. */
    break;
  case ATK_COUNT:
    fc_assert(ATK_COUNT != ATK_COUNT);
    break;
  }
}

/**************************************************************************
  Notify the world that the failed action gave the everyone a casus belli
  against the actor.
**************************************************************************/
static void notify_global_caught(struct player *receiver,
                                 const int action_id,
                                 struct player *offender,
                                 struct player *victim_player,
                                 const struct tile *victim_tile,
                                 const char *victim_link)
{
  if (receiver == offender) {
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Suitcase Nuke */
                  _("Getting caught while trying to do %s gives "
                    "everyone a casus belli against you."),
                  action_get_ui_name(action_id));
  } else if (receiver == victim_player) {
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Suitcase Nuke ... Europeans */
                  _("Getting caught while trying to do %s to you gives "
                    "everyone a casus belli against the %s."),
                  action_get_ui_name(action_id),
                  nation_plural_for_player(offender));
  } else if (victim_player == NULL) {
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Europeans ... Suitcase Nuke */
                  _("You now have a casus belli against the %s. "
                    "They got caught trying to do %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id));
  } else {
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Europeans ... Suitcase Nuke ... Americans */
                  _("You now have a casus belli against the %s. "
                    "They got caught trying to do %s to the %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id),
                  nation_plural_for_player(victim_player));
  }
}

/**************************************************************************
  Take care of any consequences (like casus belli) of getting caught while
  trying to perform the given action.

  victim_player can be NULL
**************************************************************************/
void action_consequence_caught(const int action_id,
                               struct player *offender,
                               struct player *victim_player,
                               const struct tile *victim_tile,
                               const char *victim_link)
{

  action_consequence_common(action_id, offender,
                            victim_player, victim_tile, victim_link,
                            notify_actor_caught,
                            notify_victim_caught,
                            notify_global_caught,
                            EFT_CASUS_BELLI_CAUGHT);
}

/**************************************************************************
  Notify the actor that the performed action gave the victim a casus belli
  against the actor.
**************************************************************************/
static void notify_actor_success(struct player *receiver,
                                 const int action_id,
                                 struct player *offender,
                                 struct player *victim_player,
                                 const struct tile *victim_tile,
                                 const char *victim_link)
{
  if (!victim_player || offender == victim_player) {
    /* There is no victim or the actor did this to him self. */
    return;
  }

  /* Custom message based on action type. */
  switch (action_get_target_kind(action_id)) {
  case ATK_CITY:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Suitcase Nuke ... San Francisco */
                  _("You have caused an incident doing %s to %s."),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_UNIT:
  case ATK_UNITS:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* Bribe Enemy Unit ... American ... Partisan */
                  _("You have caused an incident doing %s to %s %s."),
                  action_get_ui_name(action_id),
                  nation_adjective_for_player(victim_player),
                  victim_link);
    break;
  case ATK_TILE:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* Explode Nuclear ... (54, 26) */
                  _("You have caused an incident doing %s at %s."),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_SELF:
    /* Special actor notice not needed. Actor is victim. */
    break;
  case ATK_COUNT:
    fc_assert(ATK_COUNT != ATK_COUNT);
    break;
  }
}

/**************************************************************************
  Notify the victim that the performed action gave the victim a casus
  belli against the actor.
**************************************************************************/
static void notify_victim_success(struct player *receiver,
                                  const int action_id,
                                  struct player *offender,
                                  struct player *victim_player,
                                  const struct tile *victim_tile,
                                  const char *victim_link)
{
  if (!victim_player || offender == victim_player) {
    /* There is no victim or the actor did this to him self. */
    return;
  }

  /* Custom message based on action type. */
  switch (action_get_target_kind(action_id)) {
  case ATK_CITY:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Europeans ... Suitcase Nuke ... San Francisco */
                  _("The %s have caused an incident doing %s to %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_UNIT:
  case ATK_UNITS:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* Europeans ... Bribe Enemy Unit ... Partisan */
                  _("The %s have caused an incident doing "
                    "%s to your %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_TILE:
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* Europeans ... Explode Nuclear ... (54, 26) */
                  _("The %s have caused an incident doing %s at %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id),
                  victim_link);
    break;
  case ATK_SELF:
    /* Special victim notice not needed. Actor is victim. */
    break;
  case ATK_COUNT:
    fc_assert(ATK_COUNT != ATK_COUNT);
    break;
  }
}

/**************************************************************************
  Notify the world that the performed action gave the everyone a casus
  belli against the actor.
**************************************************************************/
static void notify_global_success(struct player *receiver,
                                  const int action_id,
                                  struct player *offender,
                                  struct player *victim_player,
                                  const struct tile *victim_tile,
                                  const char *victim_link)
{
  if (receiver == offender) {
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Suitcase Nuke */
                  _("Doing %s gives everyone a casus belli against you."),
                  action_get_ui_name(action_id));
  } else if (receiver == victim_player) {
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Suitcase Nuke ... Europeans */
                  _("Doing %s to you gives everyone a casus belli against "
                    "the %s."),
                  action_get_ui_name(action_id),
                  nation_plural_for_player(offender));
  } else if (victim_player == NULL) {
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Europeans ... Suitcase Nuke */
                  _("You now have a casus belli against the %s. "
                    "They did %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id));
  } else {
    notify_player(receiver, victim_tile,
                  E_DIPLOMATIC_INCIDENT, ftc_server,
                  /* TRANS: Europeans ... Suitcase Nuke ... Americans */
                  _("You now have a casus belli against the %s. "
                    "They did %s to the %s."),
                  nation_plural_for_player(offender),
                  action_get_ui_name(action_id),
                  nation_plural_for_player(victim_player));
  }
}

/**************************************************************************
  Take care of any consequences (like casus belli) of successfully
  performing the given action.

  victim_player can be NULL
**************************************************************************/
void action_consequence_success(const int action_id,
                                struct player *offender,
                                struct player *victim_player,
                                const struct tile *victim_tile,
                                const char *victim_link)
{
  action_consequence_common(action_id, offender,
                            victim_player, victim_tile, victim_link,
                            notify_actor_success,
                            notify_victim_success,
                            notify_global_success,
                            EFT_CASUS_BELLI_SUCCESS);
}

/**************************************************************************
  Returns TRUE iff, from the point of view of the owner of the actor unit,
  it looks like the actor unit may be able to do any action to the target
  city.

  If the owner of the actor unit don't have the knowledge needed to know
  for sure if the unit can act TRUE will be returned.

  If the only action(s) that can be performed against a target has the
  rare_pop_up property the target will only be considered valid if the
  accept_all_actions argument is TRUE.
**************************************************************************/
static bool may_unit_act_vs_city(struct unit *actor, struct city *target,
                                 bool accept_all_actions)
{
  if (actor == NULL || target == NULL) {
    /* Can't do any actions if actor or target are missing. */
    return FALSE;
  }

  action_iterate(act) {
    if (!(action_get_actor_kind(act) == AAK_UNIT
        && action_get_target_kind(act) == ATK_CITY)) {
      /* Not a relevant action. */
      continue;
    }

    if (action_id_is_rare_pop_up(act) && !accept_all_actions) {
      /* Not relevant since not accepted here. */
      continue;
    }

    if (action_prob_possible(action_prob_vs_city(actor, act, target))) {
      /* The actor unit may be able to do this action to the target
       * city. */
      return TRUE;
    }
  } action_iterate_end;

  return FALSE;
}

/**************************************************************************
  Find a city to target for an action on the specified tile.

  Returns NULL if no proper target is found.

  If the only action(s) that can be performed against a target has the
  rare_pop_up property the target will only be considered valid if the
  accept_all_actions argument is TRUE.
**************************************************************************/
struct city *action_tgt_city(struct unit *actor, struct tile *target_tile,
                             bool accept_all_actions)
{
  struct city *target = tile_city(target_tile);

  if (target && may_unit_act_vs_city(actor, target, accept_all_actions)) {
    /* It may be possible to act against this city. */
    return target;
  }

  return NULL;
}

/**************************************************************************
  Returns TRUE iff, from the point of view of the owner of the actor unit,
  it looks like the actor unit may be able to do any action to the target
  unit.

  If the owner of the actor unit don't have the knowledge needed to know
  for sure if the unit can act TRUE will be returned.

  If the only action(s) that can be performed against a target has the
  rare_pop_up property the target will only be considered valid if the
  accept_all_actions argument is TRUE.
**************************************************************************/
static bool may_unit_act_vs_unit(struct unit *actor, struct unit *target,
                                 bool accept_all_actions)
{
  if (actor == NULL || target == NULL) {
    /* Can't do any actions if actor or target are missing. */
    return FALSE;
  }

  action_iterate(act) {
    if (!(action_get_actor_kind(act) == AAK_UNIT
        && action_get_target_kind(act) == ATK_UNIT)) {
      /* Not a relevant action. */
      continue;
    }

    if (action_id_is_rare_pop_up(act) && !accept_all_actions) {
      /* Not relevant since not accepted here. */
      continue;
    }

    if (action_prob_possible(action_prob_vs_unit(actor, act, target))) {
      /* The actor unit may be able to do this action to the target
       * unit. */
      return TRUE;
    }
  } action_iterate_end;

  return FALSE;
}

/**************************************************************************
  Find a unit to target for an action at the specified tile.

  Returns the first unit found at the tile that the actor may act against
  or NULL if no proper target is found.

  If the only action(s) that can be performed against a target has the
  rare_pop_up property the target will only be considered valid if the
  accept_all_actions argument is TRUE.
**************************************************************************/
struct unit *action_tgt_unit(struct unit *actor, struct tile *target_tile,
                             bool accept_all_actions)
{
  unit_list_iterate(target_tile->units, target) {
    if (may_unit_act_vs_unit(actor, target, accept_all_actions)) {
      return target;
    }
  } unit_list_iterate_end;

  return NULL;
}

/**************************************************************************
  Returns the tile iff it, from the point of view of the owner of the
  actor unit, looks like each unit on it is an ATK_UNITS target for the
  actor unit.

  Returns NULL if the player knows that the actor unit can't do any
  ATK_UNITS action to all units at the target tile.

  If the owner of the actor unit don't have the knowledge needed to know
  for sure if the unit can act the tile will be returned.

  If the only action(s) that can be performed against a target has the
  rare_pop_up property the target will only be considered valid if the
  accept_all_actions argument is TRUE.
**************************************************************************/
struct tile *action_tgt_tile_units(struct unit *actor,
                                   struct tile *target,
                                   bool accept_all_actions)
{
  if (actor == NULL || target == NULL) {
    /* Can't do any actions if actor or target are missing. */
    return NULL;
  }

  action_iterate(act) {
    if (!(action_get_actor_kind(act) == AAK_UNIT
        && action_get_target_kind(act) == ATK_UNITS)) {
      /* Not a relevant action. */
      continue;
    }

    if (action_id_is_rare_pop_up(act) && !accept_all_actions) {
      /* Not relevant since not accepted here. */
      continue;
    }

    if (action_prob_possible(action_prob_vs_units(actor, act, target))) {
      /* One action is enough. */
      return target;
    }
  } action_iterate_end;

  return NULL;
}

/**************************************************************************
  Returns the tile iff it, from the point of view of the owner of the
  actor unit, looks like a target tile.

  Returns NULL if the player knows that the actor unit can't do any
  ATK_TILE action to the tile.

  If the owner of the actor unit doesn't have the knowledge needed to know
  for sure if the unit can act the tile will be returned.

  If the only action(s) that can be performed against a target has the
  rare_pop_up property the target will only be considered valid if the
  accept_all_actions argument is TRUE.
**************************************************************************/
struct tile *action_tgt_tile(struct unit *actor,
                             struct tile *target,
                             bool accept_all_actions)
{
  if (actor == NULL || target == NULL) {
    /* Can't do any actions if actor or target are missing. */
    return NULL;
  }

  action_iterate(act) {
    if (!(action_get_actor_kind(act) == AAK_UNIT
        && action_get_target_kind(act) == ATK_TILE)) {
      /* Not a relevant action. */
      continue;
    }

    if (action_id_is_rare_pop_up(act) && !accept_all_actions) {
      /* Not relevant since not accepted here. */
      continue;
    }

    if (action_prob_possible(action_prob_vs_tile(actor, act, target))) {
      /* The actor unit may be able to do this action to the target
       * tile. */
      return target;
    }
  } action_iterate_end;

  return NULL;
}

/**************************************************************************
  Returns the action auto performer that the specified cause can force the
  specified actor to perform. Returns NULL if no such action auto performer
  exists.
**************************************************************************/
const struct action_auto_perf *
action_auto_perf_unit_sel(const enum action_auto_perf_cause cause,
                          const struct unit *actor,
                          const struct output_type *output)
{
  action_auto_perf_by_cause_iterate(cause, autoperformer) {
    if (are_reqs_active(unit_owner(actor), NULL,
                        NULL, NULL, unit_tile(actor),
                        actor, unit_type_get(actor),
                        output, NULL, NULL,
                        &autoperformer->reqs, RPT_CERTAIN)) {
      /* Select this action auto performer. */
      return autoperformer;
    }
  } action_auto_perf_by_cause_iterate_end;

  /* Can't even try to force an action. */
  return NULL;
}

/**************************************************************************
  Make the specified actor unit perform an action because of cause.

  Returns the action the actor unit was forced to perform.
  Returns NULL if that didn't happen.

  Note that the return value doesn't say anything about survival.
**************************************************************************/
const struct action *
action_auto_perf_unit_do(const enum action_auto_perf_cause cause,
                         struct unit *actor,
                         const struct output_type *output)
{
  int i;

  int actor_id;

  struct city *tgt_city;
  struct tile *tgt_tile;
  struct unit *tgt_unit;
  struct tile *tgt_units;

  const struct action_auto_perf *autoperf
      = action_auto_perf_unit_sel(cause, actor, output);

  if (!autoperf) {
    /* No matching action forcer. */
    return NULL;
  }

  actor_id = actor->id;

  /* Acquire the targets. */
  tgt_city = action_tgt_city(actor, unit_tile(actor), TRUE);
  tgt_tile = action_tgt_tile(actor, unit_tile(actor), TRUE);
  tgt_unit = action_tgt_unit(actor, unit_tile(actor), TRUE);
  tgt_units = action_tgt_tile_units(actor, unit_tile(actor), TRUE);

  for (i = 0; i < ACTION_COUNT; i++) {
    const enum gen_action act = autoperf->alternatives[i];

    if (act == ACTION_COUNT) {
      /* No more alternative actions. */
      break;
    } else if (action_get_actor_kind(act) == AAK_UNIT) {
      /* This action can be done by units. */

#define perform_action_to(act, actor, tgtid)                              \
  if (unit_perform_action(unit_owner(actor),                              \
                          actor->id, tgtid,                               \
                          0, NULL, act, ACT_REQ_RULES)) {                 \
    return action_by_number(act);                                         \
  }

      switch (action_get_target_kind(act)) {
      case ATK_UNITS:
        if (tgt_units
            && is_action_enabled_unit_on_units(act, actor, tgt_units)) {
          perform_action_to(act, actor, tgt_units->index);
        }
        break;
      case ATK_TILE:
        if (tgt_tile
            && is_action_enabled_unit_on_tile(act, actor, tgt_tile)) {
          perform_action_to(act, actor, tgt_tile->index);
        }
        break;
      case ATK_CITY:
        if (tgt_city
            && is_action_enabled_unit_on_city(act, actor, tgt_city)) {
          perform_action_to(act, actor, tgt_city->id)
        }
        break;
      case ATK_UNIT:
        if (tgt_unit
            && is_action_enabled_unit_on_unit(act, actor, tgt_unit)) {
          perform_action_to(act, actor, tgt_unit->id);
        }
        break;
      case ATK_SELF:
        if (actor
            && is_action_enabled_unit_on_self(act, actor)) {
          perform_action_to(act, actor, actor->id);
        }
        break;
      case ATK_COUNT:
        fc_assert(action_get_target_kind(act) != ATK_COUNT);
      }

      if (!unit_alive(actor_id)) {
        /* The unit is gone. Maybe it was killed in Lua? */
        return NULL;
      }
    }
  }

  return NULL;
}
