/***********************************************************************
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
#include <fc_config.h>
#endif

/* utility */
#include "support.h"

/* common */
#include "disaster.h"
#include "game.h"
#include "government.h"
#include "improvement.h"
#include "requirements.h"
#include "specialist.h"
#include "style.h"
#include "tech.h"

#include "validity.h"

/**************************************************************************
  Check if universal is mentioned in the requirement vector.
**************************************************************************/
static bool universal_in_req_vec(const struct universal *uni,
                                 const struct requirement_vector *preqs)
{
  requirement_vector_iterate(preqs, preq) {
    if (are_universals_equal(uni, &preq->source)) {
      return TRUE;
    }
  } requirement_vector_iterate_end;

  return FALSE;
}

struct effect_list_cb_data
{
  bool needed;
  struct universal *uni;
  requirers_cb cb;
  void *requirers_data;
};

/**************************************************************************
  Callback to check if effect needs universal.
**************************************************************************/
static bool effect_list_universal_needed_cb(struct effect *peffect,
                                            void *data)
{
  struct effect_list_cb_data *cbdata = (struct effect_list_cb_data *)data;

  if (universal_in_req_vec(cbdata->uni, &peffect->reqs)) {
    cbdata->cb(R__("Effect"), cbdata->requirers_data);
    cbdata->needed = TRUE;
  }

  /* Always continue to next until all effects checked */
  return TRUE;
}

/**************************************************************************
  Check if anything in ruleset needs universal
**************************************************************************/
static bool is_universal_needed(struct universal *uni, requirers_cb cb,
                                void *data)
{
  bool needed = FALSE;
  bool needed_by_music_style = FALSE;
  int i;
  struct effect_list_cb_data cb_data;

  disaster_type_iterate(pdis) {
    if (universal_in_req_vec(uni, &pdis->reqs)) {
      cb(disaster_rule_name(pdis), data);
      needed = TRUE;
    }
  } disaster_type_iterate_end;

  improvement_iterate(pimprove) {
    if (universal_in_req_vec(uni, &pimprove->reqs)
        || universal_in_req_vec(uni, &pimprove->obsolete_by)) {
      cb(improvement_rule_name(pimprove), data);
      needed = TRUE;
    }
  } improvement_iterate_end;

  governments_iterate(pgov) {
    if (universal_in_req_vec(uni, &pgov->reqs)) {
      cb(government_rule_name(pgov), data);
      needed = TRUE;
    }
  } governments_iterate_end;

  specialist_type_iterate(sp) {
    struct specialist *psp = specialist_by_number(sp);

    if (universal_in_req_vec(uni, &psp->reqs)) {
      cb(specialist_rule_name(psp), data);
      needed = TRUE;
    }
  } specialist_type_iterate_end;

  extra_type_iterate(pextra) {
    if (universal_in_req_vec(uni, &pextra->reqs)
        || universal_in_req_vec(uni, &pextra->rmreqs)) {
      cb(extra_rule_name(pextra), data);
      needed = TRUE;
    }
  } extra_type_iterate_end;

  goods_type_iterate(pgood) {
    if (universal_in_req_vec(uni, &pgood->reqs)) {
      cb(goods_rule_name(pgood), data);
      needed = TRUE;
    }
  } goods_type_iterate_end;

  action_iterate(act) {
    action_enabler_list_iterate(action_enablers_for_action(act), enabler) {
      if (universal_in_req_vec(uni, &(enabler->actor_reqs))
          || universal_in_req_vec(uni, &(enabler->target_reqs))) {
        cb(R__("Action Enabler"), data);
        needed = TRUE;
      }
    } action_enabler_list_iterate_end;
  } action_iterate_end;

  for (i = 0; i < game.control.styles_count; i++) {
    if (universal_in_req_vec(uni, &city_styles[i].reqs)) {
      cb(city_style_rule_name(i), data);
      needed = TRUE;
    }
  }

  music_styles_iterate(pmus) {
    if (universal_in_req_vec(uni, &pmus->reqs)) {
      needed_by_music_style = TRUE;
    }
  } music_styles_iterate_end;

  if (needed_by_music_style) {
    cb(R__("Music Style"), data);
    needed = TRUE;
  }

  cb_data.needed = FALSE;
  cb_data.uni = uni;
  cb_data.cb = cb;
  cb_data.requirers_data = data;

  iterate_effect_cache(effect_list_universal_needed_cb, &cb_data);
  needed |= cb_data.needed;

  return needed;
}

/**************************************************************************
  Check if anything in ruleset needs tech
**************************************************************************/
bool is_tech_needed(struct advance *padv, requirers_cb cb, void *data)
{
  struct universal uni = { .value.advance = padv, .kind = VUT_ADVANCE };
  bool needed = FALSE;

  advance_iterate(A_FIRST, pdependant) {
    if (pdependant->require[AR_ONE] == padv
        || pdependant->require[AR_TWO] == padv
        || pdependant->require[AR_ROOT] == padv) {
      cb(advance_rule_name(pdependant), data);
      needed = TRUE;
    }
  } advance_iterate_end;

  unit_type_iterate(ptype) {
    if (ptype->require_advance == padv) {
      cb(utype_rule_name(ptype), data);
      needed = TRUE;
    }
  } unit_type_iterate_end;

  needed |= is_universal_needed(&uni, cb, data);

  return needed;
}

/**************************************************************************
  Check if anything in ruleset needs building
**************************************************************************/
bool is_building_needed(struct impr_type *pimpr, requirers_cb cb,
                        void *data)
{
  struct universal uni = { .value.building = pimpr, .kind = VUT_IMPROVEMENT };
  bool needed = FALSE;

  needed |= is_universal_needed(&uni, cb, data);

  return needed;
}

/**************************************************************************
  Check if anything in ruleset needs unit type
**************************************************************************/
bool is_utype_needed(struct unit_type *ptype, requirers_cb cb,
                     void *data)
{
  struct universal uni = { .value.utype = ptype, .kind = VUT_UTYPE };
  bool needed = FALSE;

  needed |= is_universal_needed(&uni, cb, data);

  return needed;
}

/**************************************************************************
  Check if anything in ruleset needs goods type
**************************************************************************/
bool is_good_needed(struct goods_type *pgood, requirers_cb cb,
                    void *data)
{
  struct universal uni = { .value.good = pgood, .kind = VUT_GOOD };
  bool needed = FALSE;

  needed |= is_universal_needed(&uni, cb, data);

  return needed;
}

/**************************************************************************
  Check if anything in ruleset needs government
**************************************************************************/
bool is_government_needed(struct government *pgov, requirers_cb cb, void *data)
{
  struct universal uni = { .value.govern = pgov, .kind = VUT_GOVERNMENT };
  bool needed = FALSE;

  needed |= is_universal_needed(&uni, cb, data);

  return needed;
}
