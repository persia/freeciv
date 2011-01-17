/**********************************************************************
 Freeciv - Copyright (C) 2005 - The Freeciv Project
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

#include <math.h>

/* utilities */
#include "log.h"
#include "rand.h"

#include "api_utilities.h"

/************************************************************************
  Generate random number.
************************************************************************/
int api_utilities_random(int min, int max)
{
  double roll = ((double) (fc_rand(MAX_UINT32) % MAX_UINT32)
                 / (double) MAX_UINT32);

  return (min + floor(roll * (max - min + 1)));
}

/**************************************************************************
  One log message. This module is used by script_game and script_auth.
**************************************************************************/
void api_utilities_log_base(int level, const char *message)
{
  log_base(level, "%s", message);
}
