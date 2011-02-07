/********************************************************************** 
 Freeciv - Copyright (C) 1996-2005 - Freeciv Development Team
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

// Qt
#include "QGraphicsPixmapItem"

// gui-qt
#include "qtg_cxxside.h"
#include "sprite.h"

#include "canvas.h"

/****************************************************************************
  Create a canvas of the given size.
****************************************************************************/
struct canvas *qtg_canvas_create(int width, int height)
{
  struct canvas *store = new canvas;

  return store;
}

/****************************************************************************
  Free any resources associated with this canvas and the canvas struct
  itself.
****************************************************************************/
void qtg_canvas_free(struct canvas *store)
{
  delete store;
}

/****************************************************************************
  Copies an area from the source canvas to the destination canvas.
****************************************************************************/
void qtg_canvas_copy(struct canvas *dest, struct canvas *src,
		     int src_x, int src_y, int dest_x, int dest_y, int width,
		     int height)
{
  /* PORTME */
}

/****************************************************************************
  Draw some or all of a sprite onto the canvas.
****************************************************************************/
void qtg_canvas_put_sprite(struct canvas *pcanvas,
                           int canvas_x, int canvas_y,
                           struct sprite *sprite,
                           int offset_x, int offset_y, int width, int height)
{
  QGraphicsPixmapItem *gpmi;
  QPixmap pm = sprite->pm->copy(offset_x, offset_y, width, height);

  gpmi = pcanvas->scene.addPixmap(pm);
  gpmi->setPos(canvas_x, canvas_y);
}

/****************************************************************************
  Draw a full sprite onto the canvas.
****************************************************************************/
void qtg_canvas_put_sprite_full(struct canvas *pcanvas,
                                int canvas_x, int canvas_y,
                                struct sprite *sprite)
{
  QGraphicsPixmapItem *gpmi;

  gpmi = pcanvas->scene.addPixmap(*sprite->pm);
  gpmi->setPos(canvas_x, canvas_y);
}

/****************************************************************************
  Draw a full sprite onto the canvas.  If "fog" is specified draw it with
  fog.
****************************************************************************/
void qtg_canvas_put_sprite_fogged(struct canvas *pcanvas,
                                  int canvas_x, int canvas_y,
                                  struct sprite *psprite,
                                  bool fog, int fog_x, int fog_y)
{
  /* PORTME */
}

/****************************************************************************
  Draw a filled-in colored rectangle onto canvas.
****************************************************************************/
void qtg_canvas_put_rectangle(struct canvas *pcanvas,
                              struct color *pcolor,
                              int canvas_x, int canvas_y,
                              int width, int height)
{
  /* PORTME */
}

/****************************************************************************
  Fill the area covered by the sprite with the given color.
****************************************************************************/
void qtg_canvas_fill_sprite_area(struct canvas *pcanvas,
                                 struct sprite *psprite, struct color *pcolor,
                                 int canvas_x, int canvas_y)
{
  /* PORTME */
}

/****************************************************************************
  Fill the area covered by the sprite with the given color.
****************************************************************************/
void qtg_canvas_fog_sprite_area(struct canvas *pcanvas, struct sprite *psprite,
                                int canvas_x, int canvas_y)
{
  /* PORTME */
}

/****************************************************************************
  Draw a 1-pixel-width colored line onto the canvas.
****************************************************************************/
void qtg_canvas_put_line(struct canvas *pcanvas, struct color *pcolor,
                         enum line_type ltype, int start_x, int start_y,
                         int dx, int dy)
{
  /* PORTME */
}

/****************************************************************************
  Draw a 1-pixel-width colored curved line onto the canvas.
****************************************************************************/
void qtg_canvas_put_curved_line(struct canvas *pcanvas, struct color *pcolor,
                                enum line_type ltype, int start_x, int start_y,
                                int dx, int dy)
{
  /* PORTME */
}

/****************************************************************************
  Return the size of the given text in the given font.  This size should
  include the ascent and descent of the text.  Either of width or height
  may be NULL in which case those values simply shouldn't be filled out.
****************************************************************************/
void qtg_get_text_size(int *width, int *height,
                       enum client_font font, const char *text)
{
  /* PORTME */
  if (width) {
    *width = 0;
  }
  if (height) {
    *height = 0;
  }
}

/****************************************************************************
  Draw the text onto the canvas in the given color and font.  The canvas
  position does not account for the ascent of the text; this function must
  take care of this manually.  The text will not be NULL but may be empty.
****************************************************************************/
void qtg_canvas_put_text(struct canvas *pcanvas, int canvas_x, int canvas_y,
                         enum client_font font, struct color *pcolor,
                         const char *text)
{
  /* PORTME */
}
