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
#ifndef FC__INPUTDLG_H
#define FC__INPUTDLG_H

#include <gtk/gtk.h>

typedef void (*input_dialog_callback_t)(gpointer response_cli_data,
                                        gint response, const char *input);

GtkWidget *input_dialog_create(GtkWindow *parent, const char *dialogname, 
                               const char *text, const char *postinputtest,
                               input_dialog_callback_t response_callback,
                               gpointer response_cli_data);

#endif  /* FC__INPUTDLG_H */
