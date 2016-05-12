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

#ifdef FREECIV_MSWINDOWS
#include <windows.h>
#endif

/* utility */
#include "fciconv.h"
#include "registry.h"
#include "string_vector.h"

/* common */
#include "fc_cmdhelp.h"
#include "fc_interface.h"

/* server */
#include "ruleset.h"
#include "sernet.h"
#include "settings.h"

/* tools/ruledit */
#include "comments.h"
#include "rulesave.h"

static char *rs_selected = NULL;

/**************************************************************************
  Parse freeciv-ruleup commandline parameters.
**************************************************************************/
static void rup_parse_cmdline(int argc, char *argv[])
{
  int i = 1;

  while (i < argc) {
    char *option = NULL;

    if (is_option("--help", argv[i])) {
      struct cmdhelp *help = cmdhelp_new(argv[0]);

      cmdhelp_add(help, "h", "help",
                  _("Print a summary of the options"));
      cmdhelp_add(help, "r",
                  /* TRANS: "ruleset" is exactly what user must type, do not translate. */
                  _("ruleset RULESET"),
                  _("Update RULESET"));

      /* The function below prints a header and footer for the options.
       * Furthermore, the options are sorted. */
      cmdhelp_display(help, TRUE, FALSE, TRUE);
      cmdhelp_destroy(help);

      exit(EXIT_SUCCESS);
    } else if ((option = get_option_malloc("--ruleset", argv, &i, argc))) {
      if (rs_selected != NULL) {
        fc_fprintf(stderr,
                   _("Multiple rulesets requested. Only one ruleset at time supported.\n"));
        free(option);
      } else {
        rs_selected = option;
      }
    } else {
      fc_fprintf(stderr, _("Unrecognized option: \"%s\"\n"), argv[i]);
      exit(EXIT_FAILURE);
    }

    i++;
  }
}

/**************************************************************************
  Main entry point for freeciv-ruleup
**************************************************************************/
int main(int argc, char **argv)
{
  enum log_level loglevel = LOG_NORMAL;

  /* Load win32 post-crash debugger */
#ifdef FREECIV_MSWINDOWS
# ifndef FREECIV_NDEBUG
  if (LoadLibrary("exchndl.dll") == NULL) {
#  ifdef FREECIV_DEBUG
    fprintf(stderr, "exchndl.dll could not be loaded, no crash debugger\n");
#  endif /* FREECIV_DEBUG */
  }
# endif /* FREECIV_NDEBUG */
#endif /* FREECIV_MSWINDOWS */

  init_nls();

  registry_module_init();
  init_character_encodings(FC_DEFAULT_DATA_ENCODING, FALSE);

  log_init(NULL, loglevel, NULL, NULL, -1);

  init_connections();

  settings_init(FALSE);

  game_init();
  i_am_server();

  rup_parse_cmdline(argc, argv);

  /* Set ruleset user requested to use */
  if (rs_selected == NULL) {
    rs_selected = GAME_DEFAULT_RULESETDIR;
  }
  sz_strlcpy(game.server.rulesetdir, rs_selected);

  /* Reset aifill to zero */
  game.info.aifill = 0;

  if (load_rulesets(NULL, TRUE, FALSE, TRUE)) {
    struct rule_data data;
    char tgt_dir[2048];

    data.nationlist = game.server.ruledit.nationlist;

    fc_snprintf(tgt_dir, sizeof(tgt_dir), "%s.ruleup", rs_selected);

    comments_load();
    save_ruleset(tgt_dir, rs_selected, &data);
    log_normal("Saved %s", tgt_dir);
    comments_free();
  } else {
    log_error(_("Can't load ruleset %s"), rs_selected);
  }

  registry_module_close();
  log_close();
  free_libfreeciv();
  free_nls();

  return EXIT_SUCCESS;
}
