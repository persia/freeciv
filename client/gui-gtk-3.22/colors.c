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

#include <stdio.h>

#include <gtk/gtk.h>

/* utility */
#include "log.h"
#include "mem.h"

/* common */
#include "rgbcolor.h"

/* client/gui-gtk-3.22 */
#include "gui_main.h"

#include "colors.h"

/*************************************************************
  Get display color type of default visual
*************************************************************/
enum Display_color_type get_visual(void)
{
  GdkVisual *visual;
  GdkVisualType type;

  visual = gdk_screen_get_system_visual(gdk_screen_get_default());
  type = gdk_visual_get_visual_type(visual);

  if (type == GDK_VISUAL_STATIC_GRAY) { 
    /* StaticGray, use black and white */
    log_verbose("found B/W display.");
    return BW_DISPLAY;
  }

  if (type < GDK_VISUAL_STATIC_COLOR) {
    /* No color visual available at default depth */
    log_verbose("found grayscale(?) display.");
    return GRAYSCALE_DISPLAY;
  }

  log_verbose("color system booted ok.");

  return COLOR_DISPLAY;
}

/****************************************************************************
  Allocate a color (well, sort of)
  and return a pointer to it.
****************************************************************************/
struct color *color_alloc(int r, int g, int b)
{
  struct color *color = fc_malloc(sizeof(*color));

  color->color.red = (double)r/255;
  color->color.green = (double)g/255;
  color->color.blue = (double)b/255;
  color->color.alpha = 1.0;

  return color;
}

/****************************************************************************
  Free a previously allocated color.  See color_alloc.
****************************************************************************/
void color_free(struct color *color)
{
  free(color);
}

/****************************************************************************
  Return a number indicating the perceptual brightness of this color
  relative to others (larger is brighter).
****************************************************************************/
int color_brightness_score(struct color *pcolor)
{
  struct rgbcolor *prgb = rgbcolor_new(pcolor->color.red * 255,
                                       pcolor->color.green * 255,
                                       pcolor->color.blue * 255);
  int score = rgbcolor_brightness_score(prgb);

  rgbcolor_destroy(prgb);
  return score;
}
