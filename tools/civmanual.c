/**********************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Project
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

/* utility */
#include "capability.h"
#include "fciconv.h"
#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "registry.h"
#include "shared.h"
#include "support.h"

/* common */
#include "connection.h"
#include "events.h"
#include "fc_cmdhelp.h"
#include "fc_interface.h"
#include "fc_types.h" /* LINE_BREAK */
#include "game.h"
#include "government.h"
#include "improvement.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "version.h"

/* client */
#include "client_main.h"
#include "climisc.h"
#include "helpdata.h"
#include "helpdlg_g.h"
#include "tilespec.h"

/* server */
#include "citytools.h"
#include "commands.h"
#include "connecthand.h"
#include "console.h"
#include "diplhand.h"
#include "gamehand.h"
#include "plrhand.h"
#include "report.h"
#include "ruleset.h"
#include "settings.h"
#include "sernet.h"
#include "srv_main.h"
#include "stdinhand.h"

enum manuals {
  MANUAL_SETTINGS,
  MANUAL_COMMANDS,
  MANUAL_TERRAIN,
  MANUAL_BUILDINGS,
  MANUAL_WONDERS,
  MANUAL_GOVS,
  MANUAL_UNITS,
  MANUAL_COUNT
};

/* This formats the manual for an HTML wiki. */
#ifdef MANUAL_USE_HTML
#define FILE_EXT "html"
#define HEADER "<html><head><link rel=\"stylesheet\" type=\"text/css\" "\
               "href=\"manual.css\"/><meta http-equiv=\"Content-Type\" "\
               "content=\"text/html; charset=UTF-8\"/></head><body>\n\n"
#define TITLE_BEGIN "<h1>"
#define TITLE_END "</h1>"
#define SECTION_BEGIN "<h3>"
#define SECTION_END "</h3>"
#define IMAGE_BEGIN "<img src=\""
#define IMAGE_END ".png\">"
#define SEPARATOR " "
#define TAIL "</body></html>"
#else  /* MANUAL_USE_HTML */
#define FILE_EXT "mediawiki"
#define HEADER " "
#define TITLE_BEGIN "="
#define TITLE_END "="
#define SECTION_BEGIN "==="
#define SECTION_END "==="
#define IMAGE_BEGIN "[[Image:"
#define IMAGE_END ".png]]"
#define SEPARATOR "----\n\n"
#define TAIL " "
#endif /* MANUAL_USE_HTML */

void insert_client_build_info(char *outbuf, size_t outlen);

/* Needed for "About Freeciv" help */
const char *client_string = "freeciv-manual";

static char *ruleset = NULL;

/**************************************************************************
  Replace html special characters ('&', '<' and '>').
**************************************************************************/
static char *html_special_chars(char *str, size_t *len)
{
  char *buf;

  buf = fc_strrep_resize(str, len, "&", "&amp;");
  buf = fc_strrep_resize(buf, len, "<", "&lt;");
  buf = fc_strrep_resize(buf, len, ">", "&gt;");

  return buf;
}


/*******************************************
  Useless stubs for compiling client code.
*/

/**************************************************************************
  Client stub
**************************************************************************/
void popup_help_dialog_string(const char *item)
{
  /* Empty stub. */
}

/**************************************************************************
  Client stub
**************************************************************************/
void popdown_help_dialog(void)
{
  /* Empty stub. */
}

struct tileset *tileset;

/**************************************************************************
  Client stub
**************************************************************************/
const char *tileset_name(struct tileset *t)
{
  return NULL;
}

/**************************************************************************
  Client stub
**************************************************************************/
const char *tileset_version(struct tileset *t)
{
  return NULL;
}

/**************************************************************************
  Client stub
**************************************************************************/
const char *tileset_summary(struct tileset *t)
{
  return NULL;
}

/**************************************************************************
  Client stub
**************************************************************************/
const char *tileset_description(struct tileset *t)
{
  return NULL;
}

enum client_states client_state(void)
{
  return C_S_INITIAL;
}

bool client_nation_is_in_current_set(const struct nation_type *pnation)
{
  /* Currently, there is no way to select a nation set for freeciv-manual.
   * Then, let's assume we want to print help for all nations. */
  return TRUE;
}

/**************************************************************************
  Write a server manual in the format chosen at build time, then quit.
**************************************************************************/
static bool manual_command(void)
{
  FILE *doc;
  char filename[40];
  enum manuals manuals;
  struct connection my_conn;

  /* Default client access. */
  connection_common_init(&my_conn);
  my_conn.access_level = ALLOW_CTRL;

  /* Reset aifill to zero */
  game.info.aifill = 0;

  if (!load_rulesets(NULL, FALSE, FALSE, FALSE)) {
    /* Failed to load correct ruleset */
    return FALSE;
  }

  for (manuals = 0; manuals < MANUAL_COUNT; manuals++) {
    int i;
    int ri;

    fc_snprintf(filename, sizeof(filename), "%s%d.%s",
                game.server.rulesetdir, manuals + 1, FILE_EXT);

    if (!is_reg_file_for_access(filename, TRUE)
        || !(doc = fc_fopen(filename, "w"))) {
      log_error(_("Could not write manual file %s."), filename);
      return FALSE;
    }

    fprintf(doc, HEADER);
    fprintf(doc, "<!-- Generated by freeciv-manual version %s -->\n\n",
            freeciv_datafile_version());

    switch (manuals) {
    case MANUAL_SETTINGS:
      fprintf(doc, _("%sFreeciv %s server options%s\n\n"), TITLE_BEGIN,
              VERSION_STRING, TITLE_END);
      settings_iterate(SSET_ALL, pset) {
        char buf[256];
        const char *sethelp;

        fprintf(doc, SEPARATOR);
        fprintf(doc, "%s%s - %s%s\n\n", SECTION_BEGIN, setting_name(pset),
                _(setting_short_help(pset)), SECTION_END);
        sethelp = _(setting_extra_help(pset, TRUE));
        if (strlen(sethelp) > 0) {
          char *help = fc_strdup(sethelp);
          size_t help_len = strlen(help) + 1;

          fc_break_lines(help, LINE_BREAK);
          help = html_special_chars(help, &help_len);
          fprintf(doc, "<pre>%s</pre>\n\n", help);
          FC_FREE(help);
        }
        fprintf(doc, "<p class=\"misc\">");
        fprintf(doc, _("Level: %s.<br>"),
                _(sset_level_name(setting_level(pset))));
        fprintf(doc, _("Category: %s.<br>"),
                _(sset_category_name(setting_category(pset))));

        /* first check if the setting is locked because this is included in
         * the function setting_is_changeable() */
        if (setting_locked(pset)) {
          fprintf(doc, _("Is locked by the ruleset."));
        } else if (!setting_is_changeable(pset, &my_conn, NULL, 0)) {
          fprintf(doc, _("Can only be used in server console."));
        }

        fprintf(doc, "</p>\n\n");
        setting_default_name(pset, TRUE, buf, sizeof(buf));
        switch (setting_type(pset)) {
        case SSET_INT:
          fprintf(doc, "<p class=\"bounds\">%s %d, %s %s, %s %d</p>\n\n",
                  _("Minimum:"), setting_int_min(pset),
                  _("Default:"), buf,
                  _("Maximum:"), setting_int_max(pset));
          break;
        case SSET_ENUM:
          {
            const char *value;

            fprintf(doc, "<p class=\"bounds\">%s</p>\n",
                    _("Possible values:"));
            for (i = 0; (value = setting_enum_val(pset, i, FALSE)); i++) {
              fprintf(doc, "<p class=\"bounds\"><li/> %s: \"%s\"</p>\n",
                      value, setting_enum_val(pset, i, TRUE));
            }
          }
          break;
        case SSET_BITWISE:
          {
            const char *value;

            fprintf(doc, "<p class=\"bounds\">%s</p>\n",
                    _("Possible values (option can take any number of these):"));
            for (i = 0; (value = setting_bitwise_bit(pset, i, FALSE)); i++) {
              fprintf(doc, "<p class=\"bounds\"><li/> %s: \"%s\"</p>\n",
                      value, setting_bitwise_bit(pset, i, TRUE));
            }
          }
          break;
        case SSET_BOOL:
        case SSET_STRING:
          break;
        }
        if (SSET_INT != setting_type(pset)) {
          fprintf(doc, "<p class=\"bounds\">%s %s</p>\n\n",
                  _("Default:"), buf);
        }
        if (setting_non_default(pset)) {
          fprintf(doc, _("<p class=\"changed\">Value set to %s</p>\n\n"),
                  setting_value_name(pset, TRUE, buf, sizeof(buf)));
        }
      } settings_iterate_end;
      break;

    case MANUAL_COMMANDS:
      fprintf(doc, _("%sFreeciv %s server commands%s\n\n"), TITLE_BEGIN,
              VERSION_STRING, TITLE_END);
      for (i = 0; i < CMD_NUM; i++) {
        const struct command *cmd = command_by_number(i);

        fprintf(doc, SEPARATOR);
        fprintf(doc, "%s%s  -  %s%s\n\n", SECTION_BEGIN, command_name(cmd),
                command_short_help(cmd), SECTION_END);
        if (command_synopsis(cmd)) {
          char *cmdstr = fc_strdup(command_synopsis(cmd));
          size_t cmdstr_len = strlen(cmdstr) + 1;

          cmdstr = html_special_chars(cmdstr, &cmdstr_len);
          fprintf(doc, _("<table>\n<tr>\n<td valign=\"top\">"
                         "<pre>Synopsis:</pre></td>\n<td>"));
          fprintf(doc, "<pre>%s</pre></td></tr></table>", cmdstr);
          FC_FREE(cmdstr);
        }
        fprintf(doc, _("<p class=\"level\">Level: %s</p>\n\n"),
                cmdlevel_name(command_level(cmd)));
        {
          char *help = command_extra_help(cmd);
          if (help) {
            size_t help_len = strlen(help) + 1;

            fc_break_lines(help, LINE_BREAK);
            help = html_special_chars(help, &help_len);
            fprintf(doc, _("<p>Description:</p>\n\n"));
            fprintf(doc, "<pre>%s</pre>\n\n", help);
            FC_FREE(help);
          }
        }
      }
      break;

    case MANUAL_TERRAIN:
      fprintf(doc, _("%sFreeciv %s terrain help%s\n\n"), TITLE_BEGIN,
              VERSION_STRING, TITLE_END);
      fprintf(doc, "<table><tr bgcolor=#9bc3d1><th colspan=2>%s</th>", _("Terrain"));
      fprintf(doc, "<th>F/P/T</th><th>%s</th>", _("Resources"));
      fprintf(doc, "<th>%s<br/>%s</th>", _("Move cost"), _("Defense bonus"));
      fprintf(doc, "<th>%s<br/>%s<br/>%s<br/>%s<br/>(%s)</th>",
              _("Irrigation"), _("Mining"), _("Transform"),
              /* xgettext:no-c-format */
              _("% of Road bonus"), _("turns"));
      fprintf(doc, "<th>%s<br/>%s</th>",
              _("Clean pollution"), _("Clean fallout"));
      ri = 0;
      if (game.control.num_road_types > 0) {
        fprintf(doc, "<th>");
      }
      road_type_iterate(proad) {
        if (++ri < game.control.num_road_types) {
          fprintf(doc, "%s<br/>", road_name_translation(proad));
        } else {
          /* Last one */
          fprintf(doc, "%s</th>", road_name_translation(proad));
        }
      } road_type_iterate_end;
      fprintf(doc, "</tr>\n\n");
      terrain_type_iterate(pterrain) {
        struct resource_type **r;

        if (0 == strlen(terrain_rule_name(pterrain))) {
          /* Must be a disabled piece of terrain */
          continue;
        }

        fprintf(doc, "<tr><td>" IMAGE_BEGIN "%s" IMAGE_END "</td><td>%s</td>",
                pterrain->graphic_str, terrain_name_translation(pterrain));
        fprintf(doc, "<td>%d/%d/%d</td>\n",
                pterrain->output[O_FOOD], pterrain->output[O_SHIELD],
                pterrain->output[O_TRADE]);

        fprintf(doc, "<td><table width=\"100%%\">\n");
        for (r = pterrain->resources; *r; r++) {
          fprintf(doc, "<tr><td>" IMAGE_BEGIN "%s" IMAGE_END "</td><td>%s</td>"
                  "<td align=\"right\">%d/%d/%d</td></tr>\n",
                  (*r)->self->graphic_str,
                  resource_name_translation(*r),
                  (*r)->output[O_FOOD],
                  (*r)->output[O_SHIELD],
                  (*r)->output[O_TRADE]);
        }
        fprintf(doc, "</table></td>\n");

        fprintf(doc, "<td align=\"center\">%d<br/>+%d%%</td>\n",
                pterrain->movement_cost, pterrain->defense_bonus);

        fprintf(doc, "<td><table width=\"100%%\">\n");
        if (pterrain->irrigation_result == pterrain) {
          fprintf(doc, "<tr><td>+%d F</td><td align=\"right\">(%d)</td></tr>\n",
                  pterrain->irrigation_food_incr, pterrain->irrigation_time);
        } else if (pterrain->irrigation_result == T_NONE) {
          fprintf(doc, "<tr><td>%s</td></tr>\n", _("impossible"));
        } else {
          fprintf(doc, "<tr><td>%s</td><td align=\"right\">(%d)</td></tr>\n",
                  terrain_name_translation(pterrain->irrigation_result),
                  pterrain->irrigation_time);
        }
        if (pterrain->mining_result == pterrain) {
          fprintf(doc, "<tr><td>+%d P</td><td align=\"right\">(%d)</td></tr>\n",
                  pterrain->mining_shield_incr, pterrain->mining_time);
        } else if (pterrain->mining_result == T_NONE) {
          fprintf(doc, "<tr><td>%s</td></tr>\n", _("impossible"));
        } else {
          fprintf(doc, "<tr><td>%s</td><td align=\"right\">(%d)</td></tr>\n",
                  terrain_name_translation(pterrain->mining_result),
                  pterrain->mining_time);
        }

        if (pterrain->transform_result) {
          fprintf(doc, "<tr><td>%s</td><td align=\"right\">(%d)</td></tr>\n",
                  terrain_name_translation(pterrain->transform_result),
                  pterrain->transform_time);
        } else {
          fprintf(doc, "<tr><td>-</td><td align=\"right\">(-)</td></tr>\n");
        }
        fprintf(doc, "<tr><td>%d / %d / %d</td></tr>\n</table></td>\n",
                pterrain->road_output_incr_pct[O_FOOD],
                pterrain->road_output_incr_pct[O_SHIELD],
                pterrain->road_output_incr_pct[O_TRADE]);

        fprintf(doc, "<td align=\"center\">%d / %d</td>",
                pterrain->clean_pollution_time, pterrain->clean_fallout_time);

        ri = 0;
        if (game.control.num_road_types > 0) {
          fprintf(doc, "<td>");
        }
        road_type_iterate(proad) {
          if (++ri < game.control.num_road_types) {
            fprintf(doc, "%d / ", terrain_extra_build_time(pterrain, ACTIVITY_GEN_ROAD,
                                                           road_extra_get(proad)));
          } else {
            fprintf(doc, "%d</td>", terrain_extra_build_time(pterrain, ACTIVITY_GEN_ROAD,
                                                             road_extra_get(proad)));
          }
        } road_type_iterate_end;
        fprintf(doc, "</tr>\n\n");
      } terrain_type_iterate_end;

      fprintf(doc, "</table>\n");

      break;

    case MANUAL_BUILDINGS:
    case MANUAL_WONDERS:
      if (manuals == MANUAL_BUILDINGS) {
        fprintf(doc, _("%sFreeciv %s buildings help%s\n\n"), TITLE_BEGIN,
                VERSION_STRING, TITLE_END);
      } else {
        fprintf(doc, _("%sFreeciv %s wonders help%s\n\n"), TITLE_BEGIN,
                VERSION_STRING, TITLE_END);
      }

      fprintf(doc, "<table>\n<tr bgcolor=#9bc3d1><th colspan=2>%s</th>"
                   "<th>%s<br/>%s</th><th>%s<br/>%s</th><th>%s</th></tr>\n\n",
              _("Name"), _("Cost"), _("Upkeep"),
              _("Requirement"), _("Obsolete by"), _("More info"));

      improvement_iterate(pimprove) {
        char buf[64000];
        struct advance *obs_tech = NULL;

        if (!valid_improvement(pimprove)
         || is_great_wonder(pimprove) == (manuals == MANUAL_BUILDINGS)) {
          continue;
        }

        helptext_building(buf, sizeof(buf), NULL, NULL, pimprove);

        fprintf(doc, "<tr><td>" IMAGE_BEGIN "%s" IMAGE_END "</td><td>%s</td>\n"
                     "<td align=\"center\"><b>%d</b><br/>%d</td>\n<td>",
                pimprove->graphic_str,
                improvement_name_translation(pimprove),
                pimprove->build_cost,
                pimprove->upkeep);

        requirement_vector_iterate(&pimprove->reqs, req) {
          char text[512], text2[512];
          fc_snprintf(text2, sizeof(text2),
                      /* TRANS: improvement requires a feature to be absent. */
                      req->present ? "%s" : _("no %s"),
                      VUT_NONE != req->source.kind
                      ? universal_name_translation(&req->source,
                                                   text, sizeof(text))
                      : Q_("?req:None"));
          fprintf(doc, "%s<br/>", text2);
        } requirement_vector_iterate_end;

        requirement_vector_iterate(&pimprove->obsolete_by, pobs) {
          if (pobs->source.kind == VUT_ADVANCE) {
            obs_tech = pobs->source.value.advance;
            break;
          }
        } requirement_vector_iterate_end;

        fprintf(doc, "<em>%s</em></td>\n",
                obs_tech != NULL
                ? advance_name_translation(obs_tech)
                : Q_("?tech:None"));
        fprintf(doc, "<td>%s</td>\n</tr>\n\n", buf);
      } improvement_iterate_end;
      break;

    case MANUAL_GOVS:
      /* FIXME: this doesn't resemble the wiki manual at all. */
      fprintf(doc, _("%sFreeciv %s governments help%s\n\n"), TITLE_BEGIN,
              VERSION_STRING, TITLE_END);
      governments_iterate(pgov) {
        char buf[64000];
        fprintf(doc, "%s%s%s\n\n", SECTION_BEGIN,
                government_name_translation(pgov), SECTION_END);
        helptext_government(buf, sizeof(buf), NULL, NULL, pgov);
        fprintf(doc, "%s\n\n", buf);
      } governments_iterate_end;
      break;

    case MANUAL_UNITS:
      /* FIXME: this doesn't resemble the wiki manual at all. */
      fprintf(doc, _("%sFreeciv %s unit types help%s\n\n"),
              TITLE_BEGIN, VERSION_STRING, TITLE_END);
      unit_type_iterate(putype) {
        char buf[64000];
        fprintf(doc, "%s%s%s\n\n", SECTION_BEGIN,
                utype_name_translation(putype), SECTION_END);
        fprintf(doc,
                PL_("Cost: %d shield\n",
                    "Cost: %d shields\n",
                    utype_build_shield_cost(putype)),
                utype_build_shield_cost(putype));
        fprintf(doc, _("Upkeep: %s\n"),
                helptext_unit_upkeep_str(putype));
        fprintf(doc, _("Moves: %s\n"),
                move_points_text(putype->move_rate, TRUE));
        fprintf(doc, _("Vision: %d\n"),
                (int)sqrt((double)putype->vision_radius_sq));
        fprintf(doc, _("Attack: %d\n"),
                putype->attack_strength);
        fprintf(doc, _("Defense: %d\n"),
                putype->defense_strength);
        fprintf(doc, _("Firepower: %d\n"),
                putype->firepower);
        fprintf(doc, _("Hitpoints: %d\n"),
                putype->hp);
        helptext_unit(buf, sizeof(buf), NULL, "", putype);
        fprintf(doc, "%s", buf);
        fprintf(doc, SEPARATOR);
      } unit_type_iterate_end;
      break;

    case MANUAL_COUNT:
      break;

    } /* switch */

    fprintf(doc, TAIL);
    fclose(doc);
    log_normal(_("Manual file %s successfully written."), filename);
  } /* manuals */

  return TRUE;
}

/**************************************************************************
  Entry point of whole freeciv-manual program
**************************************************************************/
int main(int argc, char **argv)
{
  int inx;
  bool showhelp = FALSE;
  bool showvers = FALSE;
  char *option = NULL;
  int retval = EXIT_SUCCESS;

  init_nls();
  registry_module_init();
  init_character_encodings(FC_DEFAULT_DATA_ENCODING, FALSE);

  /* Set the default log level. */
  srvarg.loglevel = LOG_NORMAL;

  /* parse command-line arguments... */
  inx = 1;
  while (inx < argc) {
    if ((option = get_option_malloc("--ruleset", argv, &inx, argc))) {
      if (ruleset != NULL) {
        log_error(_("Multiple rulesets requested. Only one ruleset at time supported"));
        free(option);
      } else {
        ruleset = option;
      }
    } else if (is_option("--help", argv[inx])) {
      showhelp = TRUE;
      break;
    } else if (is_option("--version", argv[inx])) {
      showvers = TRUE;
    } else if ((option = get_option_malloc("--log", argv, &inx, argc))) {
      srvarg.log_filename = option; /* Never freed. */
#ifndef FREECIV_NDEBUG
    } else if (is_option("--Fatal", argv[inx])) {
      if (inx + 1 >= argc || '-' == argv[inx + 1][0]) {
        srvarg.fatal_assertions = SIGABRT;
      } else if (str_to_int(argv[inx + 1], &srvarg.fatal_assertions)) {
        inx++;
      } else {
        fc_fprintf(stderr, _("Invalid signal number \"%s\".\n"),
                   argv[inx + 1]);
        inx++;
        showhelp = TRUE;
      }
#endif /* FREECIV_NDEBUG */
    } else if ((option = get_option_malloc("--debug", argv, &inx, argc))) {
      if (!log_parse_level_str(option, &srvarg.loglevel)) {
        showhelp = TRUE;
        break;
      }
      free(option);
    } else {
      fc_fprintf(stderr, _("Unrecognized option: \"%s\"\n"), argv[inx]);
      exit(EXIT_FAILURE);
    }
    inx++;
  }

  /* must be before con_log_init() */
  init_connections();
  con_log_init(srvarg.log_filename, srvarg.loglevel,
               srvarg.fatal_assertions);
  /* logging available after this point */

  /* Imitate a server - this is needed for as some function only work if this
   * is set. */
  i_am_server();

  /* Initialize game with default values */
  game_init();

  /* Set ruleset user requested in to use */
  if (ruleset != NULL) {
    sz_strlcpy(game.server.rulesetdir, ruleset);
  }

  settings_init(FALSE);

  if (showvers && !showhelp) {
    fc_fprintf(stderr, "%s \n", freeciv_name_version());
    exit(EXIT_SUCCESS);
  } else if (showhelp) {
    struct cmdhelp *help = cmdhelp_new(argv[0]);

#ifdef DEBUG
    cmdhelp_add(help, "d",
                  /* TRANS: "debug" is exactly what user must type, do not translate. */
                _("debug NUM"),
                _("Set debug log level (%d to %d, or %d:file1,min,max:...)"),
                LOG_FATAL, LOG_DEBUG, LOG_DEBUG);
#else
    cmdhelp_add(help, "d",
                  /* TRANS: "debug" is exactly what user must type, do not translate. */
                _("debug NUM"),
                _("Set debug log level (%d to %d)"),
                LOG_FATAL, LOG_VERBOSE);
#endif /* DEBUG */
#ifndef FREECIV_NDEBUG
    cmdhelp_add(help, "F",
                  /* TRANS: "Fatal" is exactly what user must type, do not translate. */
                _("Fatal [SIGNAL]"),
                _("Raise a signal on failed assertion"));
#endif /* FREECIV_NDEBUG */
    cmdhelp_add(help, "h", "help",
                _("Print a summary of the options"));
    cmdhelp_add(help, "l",
                  /* TRANS: "log" is exactly what user must type, do not translate. */
                _("log FILE"),
                _("Use FILE as logfile"));
    cmdhelp_add(help, "r",
                  /* TRANS: "ruleset" is exactly what user must type, do not translate. */
                _("ruleset RULESET"),
                _("Make manual for RULESET"));
    cmdhelp_add(help, "v", "version",
                _("Print the version number"));

    /* The function below prints a header and footer for the options.
     * Furthermore, the options are sorted. */
    cmdhelp_display(help, TRUE, FALSE, TRUE);
    cmdhelp_destroy(help);

    exit(EXIT_SUCCESS);
  }

  if (!manual_command()) {
    retval = EXIT_FAILURE;
  }

  FC_FREE(ruleset);

  con_log_close();
  registry_module_close();
  free_libfreeciv();
  free_nls();

  return retval;
}

/**************************************************************************
  Empty function required by helpdata
**************************************************************************/
void insert_client_build_info(char *outbuf, size_t outlen)
{
  /* Nothing here */
}
