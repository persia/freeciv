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
#ifndef FC__DIALOGS_H
#define FC__DIALOGS_H

#include <gtk/gtk.h>

#include "dialogs_g.h"

void popup_revolution_dialog(struct government *government);
void message_dialog_button_set_sensitive(GtkWidget *shl, int button,
                                         gboolean state);
gboolean taxrates_callback(GtkWidget *w, GdkEventButton *ev, gpointer data);
void nationset_sync_to_server(const char *nationset);

#endif  /* FC__DIALOGS_H */
