/***********************************************************************
 Freeciv - Copyright (C) 2006 - The Freeciv Project
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

/* SDL2 */
#ifdef SDL2_PLAIN_INCLUDE
#include <SDL.h>
#else  /* SDL2_PLAIN_INCLUDE */
#include <SDL2/SDL.h>
#endif /* SDL2_PLAIN_INCLUDE */

/* utility */
#include "log.h"

/* gui-sdl2 */
#include "colors.h"
#include "graphics.h"
#include "themespec.h"

#include "widget.h"
#include "widget_p.h"

static int (*baseclass_redraw)(struct widget *pwidget);

/* =================================================== */
/* ===================== ICON ======================== */
/* =================================================== */

/**************************************************************************
  Blit icon gfx to surface its on.
**************************************************************************/
static int redraw_icon(struct widget *pIcon)
{
  int ret;
  SDL_Rect src, area = pIcon->size;

  ret = (*baseclass_redraw)(pIcon);
  if (ret != 0) {
    return ret;
  }

  if (!pIcon->theme) {
    return -3;
  }

  src.x = (pIcon->theme->w / 4) * (Uint8) (get_wstate(pIcon));
  src.y = 0;
  src.w = (pIcon->theme->w / 4);
  src.h = pIcon->theme->h;

  if (pIcon->size.w != src.w) {
    area.x += (pIcon->size.w - src.w) / 2;
  }

  if (pIcon->size.h != src.h) {
    area.y += (pIcon->size.h - src.h) / 2;
  }

  return alphablit(pIcon->theme, &src, pIcon->dst->surface, &area, 255);
}

/**************************************************************************
  Blit icon2 gfx to surface its on.
**************************************************************************/
static int redraw_icon2(struct widget *pIcon)
{
  int ret;
  SDL_Rect dest;

  ret = (*baseclass_redraw)(pIcon);
  if (ret != 0) {
    return ret;
  }

  if (!pIcon) {
    return -3;
  }

  if (!pIcon->theme) {
    return -4;
  }

  dest.x = pIcon->size.x;
  dest.y = pIcon->size.y;
  dest.w = pIcon->theme->w;
  dest.h = pIcon->theme->h;  

  switch (get_wstate(pIcon)) {
  case FC_WS_SELECTED:
    create_frame(pIcon->dst->surface,
                 dest.x + 1, dest.y + 1,
                 dest.w + adj_size(2), dest.h + adj_size(2),
                 get_theme_color(COLOR_THEME_CUSTOM_WIDGET_SELECTED_FRAME));
    break;

  case FC_WS_PRESSED:
    create_frame(pIcon->dst->surface,
                 dest.x + 1, dest.y + 1,
                 dest.w + adj_size(2), dest.h + adj_size(2),
                 get_theme_color(COLOR_THEME_CUSTOM_WIDGET_SELECTED_FRAME));

    create_frame(pIcon->dst->surface,
                 dest.x, dest.y,
                 dest.w + adj_size(3), dest.h + adj_size(3),
                 get_theme_color(COLOR_THEME_CUSTOM_WIDGET_PRESSED_FRAME));
    break;

  case FC_WS_DISABLED:
    create_frame(pIcon->dst->surface,
                 dest.x + 1, dest.y + 1,
                 dest.w + adj_size(2), dest.h + adj_size(2),
                 get_theme_color(COLOR_THEME_WIDGET_DISABLED_TEXT));
    break;
  case FC_WS_NORMAL:
    /* No frame by default */
    break;
  }

  dest.x += adj_size(2);
  dest.y += adj_size(2);
  ret = alphablit(pIcon->theme, NULL, pIcon->dst->surface, &dest, 255);
  if (ret) {
    return ret;
  }

  return 0;
}

/**************************************************************************
  set new theme and callculate new size.
**************************************************************************/
void set_new_icon_theme(struct widget *pIcon_Widget, SDL_Surface *pNew_Theme)
{
  if ((pNew_Theme) && (pIcon_Widget)) {
    FREESURFACE(pIcon_Widget->theme);
    pIcon_Widget->theme = pNew_Theme;
    pIcon_Widget->size.w = pNew_Theme->w / 4;
    pIcon_Widget->size.h = pNew_Theme->h;
  }
}

/**************************************************************************
  Ugly hack to create 4-state icon theme from static icon;
**************************************************************************/
SDL_Surface *create_icon_theme_surf(SDL_Surface *pIcon)
{
  SDL_Color bg_color = { 255, 255, 255, 128 };
  SDL_Rect dest, src = get_smaller_surface_rect(pIcon);
  SDL_Surface *pTheme = create_surf((src.w + adj_size(4)) * 4, src.h + adj_size(4),
                                    SDL_SWSURFACE);

  dest.x = adj_size(2);
  dest.y = (pTheme->h - src.h) / 2;

  /* normal */
  alphablit(pIcon, &src, pTheme, &dest, 255);

  /* selected */
  dest.x += (src.w + adj_size(4));
  alphablit(pIcon, &src, pTheme, &dest, 255);

  /* draw selected frame */
  create_frame(pTheme,
               dest.x - 1, dest.y - 1, src.w + 1, src.h + 1,
               get_theme_color(COLOR_THEME_CUSTOM_WIDGET_SELECTED_FRAME));

  /* pressed */
  dest.x += (src.w + adj_size(4));
  alphablit(pIcon, &src, pTheme, &dest, 255);

  /* draw selected frame */
  create_frame(pTheme,
               dest.x - 1, dest.y - 1, src.w + 1, src.h + 1,
               get_theme_color(COLOR_THEME_CUSTOM_WIDGET_SELECTED_FRAME));
  /* draw press frame */
  create_frame(pTheme,
               dest.x - adj_size(2), dest.y - adj_size(2),
               src.w + adj_size(3), src.h + adj_size(3),
               get_theme_color(COLOR_THEME_CUSTOM_WIDGET_PRESSED_FRAME));

  /* disabled */
  dest.x += (src.w + adj_size(4));
  alphablit(pIcon, &src, pTheme, &dest, 255);
  dest.w = src.w;
  dest.h = src.h;

  fill_rect_alpha(pTheme, &dest, &bg_color);

  return pTheme;
}

/**************************************************************************
  Create ( malloc ) Icon Widget ( flat Button )
**************************************************************************/
struct widget *create_themeicon(SDL_Surface *pIcon_theme,
                                struct gui_layer *pDest,
                                Uint32 flags)
{
  struct widget *pIcon_Widget = widget_new();

  pIcon_Widget->theme = pIcon_theme;

  set_wflag(pIcon_Widget, (WF_FREE_STRING | WF_FREE_GFX | flags));
  set_wstate(pIcon_Widget, FC_WS_DISABLED);
  set_wtype(pIcon_Widget, WT_ICON);
  pIcon_Widget->mod = KMOD_NONE;
  pIcon_Widget->dst = pDest;

  baseclass_redraw = pIcon_Widget->redraw;
  pIcon_Widget->redraw = redraw_icon;

  if (pIcon_theme) {
    pIcon_Widget->size.w = pIcon_theme->w / 4;
    pIcon_Widget->size.h = pIcon_theme->h;
  }

  return pIcon_Widget;
}

/**************************************************************************
  Draw the icon.
**************************************************************************/
int draw_icon(struct widget *pIcon, Sint16 start_x, Sint16 start_y)
{
  pIcon->size.x = start_x;
  pIcon->size.y = start_y;

  if (get_wflags(pIcon) & WF_RESTORE_BACKGROUND) {
    refresh_widget_background(pIcon);
  }

  return draw_icon_from_theme(pIcon->theme, get_wstate(pIcon), pIcon->dst,
                              pIcon->size.x, pIcon->size.y);
}

/**************************************************************************
  Blit Icon image to pDest(ination) on positon
  start_x , start_y. WARRING: pDest must exist.

  Graphic is taken from pIcon_theme surface.

  Type of Icon depend of "state" parameter.
    state = 0 - normal
    state = 1 - selected
    state = 2 - pressed
    state = 3 - disabled

  Function return: -3 if pIcon_theme is NULL. 
  std return of alphablit(...) function.
**************************************************************************/
int draw_icon_from_theme(SDL_Surface *pIcon_theme, Uint8 state,
                         struct gui_layer *pDest, Sint16 start_x, Sint16 start_y)
{
  SDL_Rect src, des = {start_x, start_y, 0, 0};

  if (!pIcon_theme) {
    return -3;
  }
  src.x = 0 + (pIcon_theme->w / 4) * state;
  src.y = 0;
  src.w = pIcon_theme->w / 4;
  src.h = pIcon_theme->h;

  return alphablit(pIcon_theme, &src, pDest->surface, &des, 255);
}

/**************************************************************************
  Create Icon image then return pointer to this image.

  Graphic is take from pIcon_theme surface and blit to new created image.

  Type of Icon depend of "state" parameter.
    state = 0 - normal
    state = 1 - selected
    state = 2 - pressed
    state = 3 - disabled

  Function return NULL if pIcon_theme is NULL or blit fail. 
**************************************************************************/
SDL_Surface *create_icon_from_theme(SDL_Surface *pIcon_theme, Uint8 state)
{
  SDL_Rect src;

  if (!pIcon_theme) {
    return NULL;
  }

  src.w = pIcon_theme->w / 4;
  src.x = src.w * state;
  src.y = 0;
  src.h = pIcon_theme->h;

  return crop_rect_from_surface(pIcon_theme, &src);
}

/* =================================================== */
/* ===================== ICON2 ======================= */
/* =================================================== */

/**************************************************************************
  set new theme and calculate new size.
**************************************************************************/
void set_new_icon2_theme(struct widget *pIcon_Widget, SDL_Surface *pNew_Theme,
                         bool free_old_theme)
{
  if ((pNew_Theme) && (pIcon_Widget)) {
    if (free_old_theme) {
      FREESURFACE(pIcon_Widget->theme);
    }
    pIcon_Widget->theme = pNew_Theme;
    pIcon_Widget->size.w = pNew_Theme->w + adj_size(4);
    pIcon_Widget->size.h = pNew_Theme->h + adj_size(4);
  }
}

/**************************************************************************
  Create ( malloc ) Icon2 Widget ( flat Button )
**************************************************************************/
struct widget *create_icon2(SDL_Surface *pIcon, struct gui_layer *pDest,
                            Uint32 flags)
{
  struct widget *pIcon_Widget = widget_new();

  pIcon_Widget->theme = pIcon;

  set_wflag(pIcon_Widget, (WF_FREE_STRING | WF_FREE_GFX | flags));
  set_wstate(pIcon_Widget, FC_WS_DISABLED);
  set_wtype(pIcon_Widget, WT_ICON2);
  pIcon_Widget->mod = KMOD_NONE;
  pIcon_Widget->dst = pDest;

  baseclass_redraw = pIcon_Widget->redraw;
  pIcon_Widget->redraw = redraw_icon2;

  if (pIcon) {
    pIcon_Widget->size.w = pIcon->w + adj_size(4);
    pIcon_Widget->size.h = pIcon->h + adj_size(4);
  }

  return pIcon_Widget;
}
