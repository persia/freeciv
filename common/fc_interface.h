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
#ifndef FC__FC_INTERFACE_H
#define FC__FC_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* utility */
#include "support.h"

/* common */
#include "vision.h"

struct player;
struct tile;
struct color;
struct extra_type;

/* The existence of each function should be checked in interface_init()! */
struct functions {
  void (*create_extra)(struct tile *ptile, struct extra_type *pextra,
                       struct player *pplayer);
  void (*destroy_extra)(struct tile *ptile, struct extra_type *pextra);
  /* Returns iff the player 'pplayer' has the vision in the layer
     'vision' at tile given by 'ptile'. */
  bool (*player_tile_vision_get)(const struct tile *ptile,
                                 const struct player *pplayer,
                                 enum vision_layer vision);
  void (*gui_color_free)(struct color *pcolor);
};

const extern struct functions *fc_funcs;

struct functions *fc_interface_funcs(void);
void fc_interface_init(void);
void free_libfreeciv(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FC__FC_INTERFACE_H */
