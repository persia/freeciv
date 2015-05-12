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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* utility */
#include "log.h"
#include "shared.h"
#include "string_vector.h"
#include "support.h"

/* common */
#include "events.h"
#include "fcintl.h"
#include "government.h"
#include "multipliers.h"
#include "packets.h"
#include "player.h"

/* client */
#include "client_main.h"
#include "options.h"

/* client/gui-gtk-2.0 */
#include "chatline.h"
#include "cityrep.h"
#include "dialogs.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "ratesdlg.h"

#include "gamedlgs.h"

/******************************************************************/
static GtkWidget *rates_dialog_shell;
static GtkWidget *rates_gov_label;
static GtkWidget *rates_tax_toggle, *rates_lux_toggle, *rates_sci_toggle;
static GtkWidget *rates_tax_label, *rates_lux_label, *rates_sci_label;
static GtkObject *rates_tax_adj, *rates_lux_adj, *rates_sci_adj;

static GtkWidget *multiplier_dialog_shell;
static GtkWidget *multipliers_scale[MAX_NUM_MULTIPLIERS];

static gulong     rates_tax_sig, rates_lux_sig, rates_sci_sig;
/******************************************************************/

static int rates_tax_value, rates_lux_value, rates_sci_value;

static void rates_changed_callback(GtkAdjustment *adj);

/**************************************************************************
  Set tax values to display
**************************************************************************/
static void rates_set_values(int tax, int no_tax_scroll, 
			     int lux, int no_lux_scroll,
			     int sci, int no_sci_scroll)
{
  char buf[64];
  int tax_lock, lux_lock, sci_lock;
  int maxrate;
  
  tax_lock	= GTK_TOGGLE_BUTTON(rates_tax_toggle)->active;
  lux_lock	= GTK_TOGGLE_BUTTON(rates_lux_toggle)->active;
  sci_lock	= GTK_TOGGLE_BUTTON(rates_sci_toggle)->active;

  if (NULL != client.conn.playing) {
    maxrate = get_player_bonus(client.conn.playing, EFT_MAX_RATES);
  } else {
    maxrate = 100;
  }
  /* This's quite a simple-minded "double check".. */
  tax=MIN(tax, maxrate);
  lux=MIN(lux, maxrate);
  sci=MIN(sci, maxrate);
  
  if(tax+sci+lux!=100)
  {
    if((tax!=rates_tax_value))
    {
      if(!lux_lock)
	lux=MIN(MAX(100-tax-sci, 0), maxrate);
      if(!sci_lock)
	sci=MIN(MAX(100-tax-lux, 0), maxrate);
    }
    else if((lux!=rates_lux_value))
    {
      if(!tax_lock)
	tax=MIN(MAX(100-lux-sci, 0), maxrate);
      if(!sci_lock)
	sci=MIN(MAX(100-lux-tax, 0), maxrate);
    }
    else if((sci!=rates_sci_value))
    {
      if(!lux_lock)
	lux=MIN(MAX(100-tax-sci, 0), maxrate);
      if(!tax_lock)
	tax=MIN(MAX(100-lux-sci, 0), maxrate);
    }
    
    if(tax+sci+lux!=100) {
      tax=rates_tax_value;
      lux=rates_lux_value;
      sci=rates_sci_value;

      rates_tax_value=-1;
      rates_lux_value=-1;
      rates_sci_value=-1;

      no_tax_scroll=0;
      no_lux_scroll=0;
      no_sci_scroll=0;
    }

  }

  if (tax!=rates_tax_value) {
    fc_snprintf(buf, sizeof(buf), "%3d%%", tax);
    if (strcmp(buf, GTK_LABEL(rates_tax_label)->label) != 0)
	gtk_label_set_text(GTK_LABEL(rates_tax_label), buf);
    if(!no_tax_scroll)
    {
	g_signal_handler_block(rates_tax_adj, rates_tax_sig);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(rates_tax_adj), tax/10 );
	g_signal_handler_unblock(rates_tax_adj, rates_tax_sig);
    }
    rates_tax_value=tax;
  }

  if(lux!=rates_lux_value) {
    fc_snprintf(buf, sizeof(buf), "%3d%%", lux);
    if (strcmp(buf, GTK_LABEL(rates_lux_label)->label) != 0)
	gtk_label_set_text(GTK_LABEL(rates_lux_label), buf);
    if(!no_lux_scroll)
    {
	g_signal_handler_block(rates_lux_adj, rates_lux_sig);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(rates_lux_adj), lux/10 );
	g_signal_handler_unblock(rates_lux_adj, rates_lux_sig);
    }
    rates_lux_value=lux;
  }

  if(sci!=rates_sci_value) {
    fc_snprintf(buf, sizeof(buf), "%3d%%", sci);
    if (strcmp(buf, GTK_LABEL(rates_sci_label)->label) != 0)
	gtk_label_set_text(GTK_LABEL(rates_sci_label),buf);
    if(!no_sci_scroll)
    {
	g_signal_handler_block(rates_sci_adj, rates_sci_sig);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(rates_sci_adj), sci/10 );
	g_signal_handler_unblock(rates_sci_adj, rates_sci_sig);
    }
    rates_sci_value=sci;
  }
}


/**************************************************************************
  User changes rates
**************************************************************************/
static void rates_changed_callback(GtkAdjustment *adj)
{
  int percent=adj->value;

  if(adj==GTK_ADJUSTMENT(rates_tax_adj)) {
    int tax_value;

    tax_value=10*percent;
    tax_value=MIN(tax_value, 100);
    rates_set_values(tax_value,1, rates_lux_value,0, rates_sci_value,0);
  }
  else if(adj==GTK_ADJUSTMENT(rates_lux_adj)) {
    int lux_value;

    lux_value=10*percent;
    lux_value=MIN(lux_value, 100);
    rates_set_values(rates_tax_value,0, lux_value,1, rates_sci_value,0);
  }
  else {
    int sci_value;

    sci_value=10*percent;
    sci_value=MIN(sci_value, 100);
    rates_set_values(rates_tax_value,0, rates_lux_value,0, sci_value,1);
  }
}


/**************************************************************************
  User has responded to rates dialog
**************************************************************************/
static void rates_command_callback(GtkWidget *w, gint response_id)
{
  if (response_id == GTK_RESPONSE_OK) {
    dsend_packet_player_rates(&client.conn, rates_tax_value, rates_lux_value,
			      rates_sci_value);
  }
  gtk_widget_destroy(rates_dialog_shell);
}

/**************************************************************************
  Convert real multiplier value to scale value
**************************************************************************/
static int mult_to_scale(const struct multiplier *pmul, int val)
{
  return (val - pmul->start) / pmul->step;
}

/**************************************************************************
  Convert scale units to real multiplier value
**************************************************************************/
static int scale_to_mult(const struct multiplier *pmul, int scale)
{
  return scale * pmul->step + pmul->start;
}

/**************************************************************************
  Format value for multiplier scales
**************************************************************************/
static gchar *multiplier_value_callback(GtkScale *scale, gdouble value,
                                        void *udata)
{
  const struct multiplier *pmul = udata;

  return g_strdup_printf("%d", scale_to_mult(pmul, value));
}

/**************************************************************************
  User has responded to multipliers dialog
**************************************************************************/
static void multipliers_command_callback(GtkWidget *w, gint response_id)
{
  struct packet_player_multiplier mul;

  if (response_id == GTK_RESPONSE_OK && can_client_issue_orders()) {
    multipliers_iterate(m) {
      Multiplier_type_id i = multiplier_index(m);
      int value = gtk_range_get_value(GTK_RANGE(multipliers_scale[i]));

      mul.multipliers[i] = scale_to_mult(m, value);
    } multipliers_iterate_end;
    mul.count = multiplier_count();
    send_packet_player_multiplier(&client.conn, &mul);
  }
  gtk_widget_destroy(multiplier_dialog_shell);
}

/**************************************************************************
  Update multipliers dialog (for player observers)
**************************************************************************/
void real_multipliers_dialog_update(void)
{
  if (!multiplier_dialog_shell) {
    return;
  }

  if (can_client_issue_orders()) {
    /* We don't want to lose the player's local changes. This dialog
     * should be the only way the values can change, anyway. */
    return;
  }

  multipliers_iterate(pmul) {
    Multiplier_type_id multiplier = multiplier_index(pmul);
    int val = player_multiplier_target_value(client_player(), pmul);

    gtk_range_set_value(GTK_RANGE(multipliers_scale[multiplier]),
                        mult_to_scale(pmul, val));
  } multipliers_iterate_end;
}

/****************************************************************
  Create multipliers dialog
****************************************************************/
static GtkWidget *create_multiplier_dialog(void)
{
  GtkWidget     *shell, *content;
  GtkWidget     *label, *scale;
  struct player *pplayer = client_player();

  if (can_client_issue_orders()) {
    shell = gtk_dialog_new_with_buttons(_("Change policies"),
                                        NULL,
                                        0,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK,
                                        GTK_RESPONSE_OK,
                                        NULL);
  } else {
    shell = gtk_dialog_new_with_buttons(_("Policies"),
                                        NULL,
                                        0,
                                        GTK_STOCK_CLOSE,
                                        GTK_RESPONSE_CLOSE,
                                        NULL);
  }

  gtk_window_set_position(GTK_WINDOW(shell), GTK_WIN_POS_MOUSE);
  content = gtk_dialog_get_content_area(GTK_DIALOG(shell));

  multipliers_iterate(pmul) {
    Multiplier_type_id multiplier = multiplier_index(pmul);

    fc_assert(multiplier < ARRAY_SIZE(multipliers_scale));
    label = gtk_label_new(multiplier_name_translation(pmul));
    /* Map each multiplier step to a single step on the UI, to enforce
     * the step size. */
    scale = gtk_hscale_new_with_range(mult_to_scale(pmul, pmul->start),
                                      mult_to_scale(pmul, pmul->stop), 1);
    multipliers_scale[multiplier] = scale;
    gtk_range_set_increments(GTK_RANGE(multipliers_scale[multiplier]),
                             1, MAX(2, mult_to_scale(pmul, pmul->stop) / 10));
    {
      int val = player_multiplier_target_value(pplayer, pmul);

      fc_assert(scale_to_mult(pmul, mult_to_scale(pmul, val)) == val);
      gtk_range_set_value(GTK_RANGE(multipliers_scale[multiplier]),
                          mult_to_scale(pmul, val));
    }
    g_signal_connect(multipliers_scale[multiplier], "format-value",
                     G_CALLBACK(multiplier_value_callback), pmul);
    g_signal_connect(multipliers_scale[multiplier], "destroy",
                     G_CALLBACK(gtk_widget_destroyed),
                     &multipliers_scale[multiplier]);
    gtk_box_pack_start( GTK_BOX( content ), label, TRUE, TRUE, 5 );
    gtk_box_pack_start( GTK_BOX( content ), scale, TRUE, TRUE, 5 );
    gtk_widget_set_sensitive(multipliers_scale[multiplier],
                             can_client_issue_orders());
  } multipliers_iterate_end;

  g_signal_connect(shell, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &multiplier_dialog_shell);

  g_signal_connect(shell, "response",
                   G_CALLBACK(multipliers_command_callback), NULL);

  gtk_widget_show_all(content);

  return shell;
}

/****************************************************************
  Popup multipliers dialog
*****************************************************************/
void popup_multiplier_dialog(void)
{
  if (!multiplier_dialog_shell) {
    multiplier_dialog_shell = create_multiplier_dialog();
  }

  if (!multiplier_dialog_shell) {
    return;
  }

  gtk_window_present(GTK_WINDOW(multiplier_dialog_shell));
}

/****************************************************************
  Create rates dialog
*****************************************************************/
static GtkWidget *create_rates_dialog(void)
{
  GtkWidget     *shell;
  GtkWidget	*frame, *hbox;

  GtkWidget	*scale;

  if (!can_client_issue_orders()) {
    return NULL;
  }
  
  shell = gtk_dialog_new_with_buttons(_("Select tax, luxury and science rates"),
  	NULL,
	0,
	GTK_STOCK_CANCEL,
	GTK_RESPONSE_CANCEL,
	GTK_STOCK_OK,
	GTK_RESPONSE_OK,
	NULL);
  setup_dialog(shell, toplevel);
  gtk_dialog_set_default_response(GTK_DIALOG(shell), GTK_RESPONSE_OK);
  gtk_window_set_position(GTK_WINDOW(shell), GTK_WIN_POS_MOUSE);

  rates_gov_label = gtk_label_new("");
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( shell )->vbox ), rates_gov_label, TRUE, TRUE, 5 );

  frame = gtk_frame_new( _("Tax") );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( shell )->vbox ), frame, TRUE, TRUE, 5 );

  hbox = gtk_hbox_new( FALSE, 10 );
  gtk_container_add( GTK_CONTAINER( frame ), hbox );

  rates_tax_adj = gtk_adjustment_new( 0.0, 0.0, 11.0, 1.0, 1.0, 1.0 );
  scale = gtk_hscale_new( GTK_ADJUSTMENT( rates_tax_adj ) );
  gtk_widget_set_size_request(scale, 300, 40);
  gtk_scale_set_digits( GTK_SCALE( scale ), 0 );
  gtk_scale_set_draw_value( GTK_SCALE( scale ), FALSE );
  gtk_box_pack_start( GTK_BOX( hbox ), scale, TRUE, TRUE, 0 );

  rates_tax_label = gtk_label_new("  0%");
  gtk_box_pack_start( GTK_BOX( hbox ), rates_tax_label, TRUE, TRUE, 0 );
  gtk_widget_set_size_request(rates_tax_label, 40, -1);

  rates_tax_toggle = gtk_check_button_new_with_label( _("Lock") );
  gtk_box_pack_start( GTK_BOX( hbox ), rates_tax_toggle, TRUE, TRUE, 0 );

  frame = gtk_frame_new( _("Luxury") );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( shell )->vbox ), frame, TRUE, TRUE, 5 );

  hbox = gtk_hbox_new( FALSE, 10 );
  gtk_container_add( GTK_CONTAINER( frame ), hbox );

  rates_lux_adj = gtk_adjustment_new( 0.0, 0.0, 11.0, 1.0, 1.0, 1.0 );
  scale = gtk_hscale_new( GTK_ADJUSTMENT( rates_lux_adj ) );
  gtk_widget_set_size_request(scale, 300, 40);
  gtk_scale_set_digits( GTK_SCALE( scale ), 0 );
  gtk_scale_set_draw_value( GTK_SCALE( scale ), FALSE );
  gtk_box_pack_start( GTK_BOX( hbox ), scale, TRUE, TRUE, 0 );

  rates_lux_label = gtk_label_new("  0%");
  gtk_box_pack_start( GTK_BOX( hbox ), rates_lux_label, TRUE, TRUE, 0 );
  gtk_widget_set_size_request(rates_lux_label, 40, -1);

  rates_lux_toggle = gtk_check_button_new_with_label( _("Lock") );
  gtk_box_pack_start( GTK_BOX( hbox ), rates_lux_toggle, TRUE, TRUE, 0 );

  frame = gtk_frame_new( _("Science") );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( shell )->vbox ), frame, TRUE, TRUE, 5 );

  hbox = gtk_hbox_new( FALSE, 10 );
  gtk_container_add( GTK_CONTAINER( frame ), hbox );

  rates_sci_adj = gtk_adjustment_new( 0.0, 0.0, 11.0, 1.0, 1.0, 1.0 );
  scale = gtk_hscale_new( GTK_ADJUSTMENT( rates_sci_adj ) );
  gtk_widget_set_size_request(scale, 300, 40);
  gtk_scale_set_digits( GTK_SCALE( scale ), 0 );
  gtk_scale_set_draw_value( GTK_SCALE( scale ), FALSE );
  gtk_box_pack_start( GTK_BOX( hbox ), scale, TRUE, TRUE, 0 );

  rates_sci_label = gtk_label_new("  0%");
  gtk_box_pack_start( GTK_BOX( hbox ), rates_sci_label, TRUE, TRUE, 0 );
  gtk_widget_set_size_request(rates_sci_label, 40, -1);

  rates_sci_toggle = gtk_check_button_new_with_label( _("Lock") );
  gtk_box_pack_start( GTK_BOX( hbox ), rates_sci_toggle, TRUE, TRUE, 0 );


  g_signal_connect(shell, "response",
		   G_CALLBACK(rates_command_callback), NULL);
  g_signal_connect(shell, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &rates_dialog_shell);

  gtk_widget_show_all( GTK_DIALOG( shell )->vbox );
  gtk_widget_show_all( GTK_DIALOG( shell )->action_area );

  rates_tax_value=-1;
  rates_lux_value=-1;
  rates_sci_value=-1;

  rates_tax_sig =
    g_signal_connect_after(rates_tax_adj, "value_changed",
			   G_CALLBACK(rates_changed_callback), NULL);

  rates_lux_sig =
    g_signal_connect_after(rates_lux_adj, "value_changed",
			   G_CALLBACK(rates_changed_callback), NULL);

  rates_sci_sig =
    g_signal_connect_after(rates_sci_adj, "value_changed",
			   G_CALLBACK(rates_changed_callback), NULL);

  rates_set_values(client.conn.playing->economic.tax, 0,
		   client.conn.playing->economic.luxury, 0,
		   client.conn.playing->economic.science, 0);
  return shell;
}


/****************************************************************
  Popup rates dialog
*****************************************************************/
void popup_rates_dialog(void)
{
  if (!can_client_issue_orders()) {
    return;
  }

  if (!rates_dialog_shell) {
    rates_dialog_shell = create_rates_dialog();
  }
  if (!rates_dialog_shell) {
    return;
  }

  gchar *buf = g_strdup_printf(_("%s max rate: %d%%"),
      government_name_for_player(client.conn.playing),
      get_player_bonus(client.conn.playing, EFT_MAX_RATES));
  gtk_label_set_text(GTK_LABEL(rates_gov_label), buf);
  g_free(buf);

  gtk_window_present(GTK_WINDOW(rates_dialog_shell));
}
