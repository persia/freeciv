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
#ifndef FC__UNITHAND_H
#define FC__UNITHAND_H

#include "unit.h"

#include "hand_gen.h"

void unit_activity_handling(struct unit *punit,
                            enum unit_activity new_activity);
void unit_activity_handling_targeted(struct unit *punit,
                                     enum unit_activity new_activity,
                                     struct extra_type **new_target);
void unit_change_homecity_handling(struct unit *punit, struct city *new_pcity,
                                   bool rehome);

bool unit_move_handling(struct unit *punit, struct tile *pdesttile,
                        bool igzoc, bool move_diplomat_city,
                        struct unit *embark_to);

bool unit_perform_action(struct player *pplayer,
                         const int actor_id,
                         const int target_id,
                         const int value,
                         const char *name,
                         const enum gen_action action_type,
                         const enum action_requester requester);

void illegal_action_msg(struct player *pplayer,
                        const enum event_type event,
                        struct unit *actor,
                        const int stopped_action,
                        const struct tile *target_tile,
                        const struct city *target_city,
                        const struct unit *target_unit);

#endif  /* FC__UNITHAND_H */
