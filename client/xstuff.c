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
#include <stdlib.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>

extern Display	*display;
extern int screen_number;
extern GC civ_gc, font_gc;
extern int display_depth;

void xaw_expose_now(Widget w);

void put_line_8(char *psrc, char *pdst,  int dst_w, int xoffset_table[]);
void put_line_16(char *psrc, char *pdst,  int dst_w, int xoffset_table[]);
void put_line_24(char *psrc, char *pdst,  int dst_w, int xoffset_table[]);
void put_line_32(char *psrc, char *pdst,  int dst_w, int xoffset_table[]);

void xaw_expose_now(Widget w);

/**************************************************************************
...
**************************************************************************/
void xaw_set_relative_position(Widget ref, Widget w, int px, int py)
{
  Position x, y;
  Dimension width, height;
  
  XtVaGetValues(ref, XtNwidth, &width, XtNheight, &height, NULL);
  XtTranslateCoords(ref, (Position) px*width/100, (Position) py*height/100, &x, &y);
  XtVaSetValues(w, XtNx, x, XtNy, y, NULL);
}


/**************************************************************************
...
**************************************************************************/
void xaw_horiz_center(Widget w)
{
  Dimension width, width2;

  XtVaGetValues(XtParent(w), XtNwidth, &width, NULL);
  XtVaGetValues(w, XtNwidth, &width2, NULL);
  XtVaSetValues(w, XtNhorizDistance, (width-width2)/2, NULL);
}

/**************************************************************************
...
**************************************************************************/
void xaw_set_bitmap(Widget w, Pixmap pm)
{
  XtVaSetValues(w, XtNbitmap, (XtArgVal)pm, NULL);
  xaw_expose_now(w);
}


/**************************************************************************
...
**************************************************************************/
void xaw_set_label(Widget w, char *text)
{
  String str;

  XtVaGetValues(w, XtNlabel, &str, NULL);
  if(strcmp(str, text)) {
    XtVaSetValues(w, XtNlabel, (XtArgVal)text, NULL);
    xaw_expose_now(w);
  }
  
}

/**************************************************************************
...
**************************************************************************/
void xaw_expose_now(Widget w)
{
  Dimension width, height;
  XExposeEvent xeev;
  
  xeev.type = Expose;
  xeev.display = XtDisplay (w);
  xeev.window = XtWindow(w);
  xeev.x = 0;
  xeev.y = 0;
  XtVaGetValues(w, XtNwidth, &width, XtNheight, &height, NULL);
  (XtClass(w))->core_class.expose(w, (XEvent *)&xeev, NULL);
}

/**************************************************************************
...
**************************************************************************/
void x_simulate_button_click(Widget w)
{
  XButtonEvent ev;
  
  ev.display = XtDisplay(w);
  ev.window = XtWindow(w);
  ev.type=ButtonPress;
  ev.button=Button1;
  ev.x=10;
  ev.y=10;

  XtDispatchEvent((XEvent *)&ev);
  ev.type=ButtonRelease;
  XtDispatchEvent((XEvent *)&ev);
}

/**************************************************************************
...
**************************************************************************/
Pixmap x_scale_pixmap(Pixmap src, int src_w, int src_h, int dst_w, int dst_h, 
		      Window root)
{
  Pixmap dst;
  XImage *xi_src, *xi_dst;
  int xoffset_table[4096];
  int x, xoffset, xadd, xremsum, xremadd;
  int y, yoffset, yadd, yremsum, yremadd;
  char *pdst_data;
  char *psrc_data;

  xi_src=XGetImage(display, src, 0, 0, src_w, src_h, AllPlanes, ZPixmap);
  xi_dst=XCreateImage(display, DefaultVisual(display, screen_number),
		       xi_src->depth, ZPixmap,
		       0, NULL,
		       dst_w, dst_h,
		       xi_src->bitmap_pad, 0);

  xi_dst->data=(char*)malloc(xi_dst->bytes_per_line * xi_dst->height);
  memset(xi_dst->data, 0, xi_dst->bytes_per_line * xi_dst->height);

  /* for each pixel in dst, calculate pixel offset in src */
  xadd=src_w/dst_w;
  xremadd=src_w%dst_w;
  xoffset=0;
  xremsum=dst_w/2;

  for(x=0; x<dst_w; ++x) {
    xoffset_table[x]=xoffset;
    xoffset+=xadd;
    xremsum+=xremadd;
    if(xremsum>=dst_w) {
      xremsum-=dst_w;
      ++xoffset;
    }
  }

  yadd=src_h/dst_h;
  yremadd=src_h%dst_h;
  yoffset=0;
  yremsum=dst_h/2; 

  for(y=0; y<dst_h; ++y) {
    psrc_data=xi_src->data + (yoffset * xi_src->bytes_per_line);
    pdst_data=xi_dst->data + (y * xi_dst->bytes_per_line);

    switch(xi_src->bits_per_pixel) {
     case 8:
      put_line_8(psrc_data, pdst_data, dst_w, xoffset_table);
      break;
     case 16:
      put_line_16(psrc_data, pdst_data, dst_w, xoffset_table);
      break;
     case 24:
      put_line_24(psrc_data, pdst_data, dst_w, xoffset_table);
      break;
     case 32:
      put_line_32(psrc_data, pdst_data, dst_w, xoffset_table);
      break;
     default:
      memcpy(pdst_data, psrc_data, (src_w<dst_w) ? src_w : dst_w);
      break;
    }

    yoffset+=yadd;
    yremsum+=yremadd;
    if(yremsum>=dst_h) {
      yremsum-=dst_h;
      ++yoffset;
    }
  }

  dst=XCreatePixmap(display, root, dst_w, dst_h, display_depth);
  XPutImage(display, dst, civ_gc, xi_dst, 0, 0, 0, 0, dst_w, dst_h);

  return dst;
}

void put_line_8(char *psrc, char *pdst,  int dst_w, int xoffset_table[])
{
  int x;
  for(x=0; x<dst_w; ++x)
    *pdst++=*(psrc+xoffset_table[x]+0);
}

void put_line_16(char *psrc, char *pdst,  int dst_w, int xoffset_table[])
{
  int x;
  for(x=0; x<dst_w; ++x) {
    *pdst++=*(psrc+2*xoffset_table[x]+0);
    *pdst++=*(psrc+2*xoffset_table[x]+1);
  }
}

void put_line_24(char *psrc, char *pdst,  int dst_w, int xoffset_table[])
{
  int x;
  for(x=0; x<dst_w; ++x) {
    *pdst++=*(psrc+3*xoffset_table[x]+0);
    *pdst++=*(psrc+3*xoffset_table[x]+1);
    *pdst++=*(psrc+3*xoffset_table[x]+1);
  }
}

void put_line_32(char *psrc, char *pdst,  int dst_w, int xoffset_table[])
{
  int x;
  for(x=0; x<dst_w; ++x) {
    *pdst++=*(psrc+4*xoffset_table[x]+0);
    *pdst++=*(psrc+4*xoffset_table[x]+1);
    *pdst++=*(psrc+4*xoffset_table[x]+2);
    *pdst++=*(psrc+4*xoffset_table[x]+3);
  }
}
