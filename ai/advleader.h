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
#ifndef __ADVLEADER_H
#define __ADVLEADER_H

void leader_adjust_tech_choice(struct player *pplayer, 
			       struct ai_choice *choice, int advisor);

void leader_adjust_build_choice(struct player *pplayer, 
				struct ai_choice *choice, int advisor);

#endif
