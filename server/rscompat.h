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
#ifndef FC__RSCOMPAT_H
#define FC__RSCOMPAT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* utility */
#include "support.h"

#define RULESET_COMPAT_CAP "+Freeciv-2.6-ruleset"

struct rscompat_info
{
  bool compat_mode;
  int ver_buildings;
  int ver_cities;
  int ver_effects;
  int ver_game;
  int ver_governments;
  int ver_nations;
  int ver_styles;
  int ver_techs;
  int ver_terrain;
  int ver_units;
};

void rscompat_init_info(struct rscompat_info *info);

int rscompat_check_capabilities(struct section_file *file, const char *filename,
                                struct rscompat_info *info);

bool rscompat_names(struct rscompat_info *info);

void rscompat_postprocess(struct rscompat_info *info);

/* Functions specific to 2.6 -> 3.0 transition */
void rscompat_goods_3_0(void);
struct extra_type *rscompat_extra_from_resource_3_0(struct section_file *sfile,
                                                    const char *sec_name);
const char *rscompat_req_type_name_3_0(const char *type, const char *range,
                                       bool survives, bool present,
                                       bool quiet, const char *value);
const char *rscompat_utype_flag_name_3_0(struct rscompat_info *info,
                                         const char *old_type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FC__RSCOMPAT_H */
