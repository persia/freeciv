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

#include <stdlib.h>

/* utility */
#include "fciconv.h"
#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "netintf.h"
#include "registry.h"
#include "shared.h"

/* modinst */
#include "download.h"
#include "mpcmdline.h"
#include "mpdb.h"

#include "modinst.h"

struct fcmp_params fcmp = {
  .list_url = MODPACK_LIST_URL,
  .inst_prefix = NULL,
  .autoinstall = NULL
};

/**************************************************************************
  Progress indications from downloader
**************************************************************************/
static void msg_callback(const char *msg)
{
  log_normal("%s", msg);
}

/**************************************************************************
  Build main modpack list view
**************************************************************************/
static void setup_modpack_list(const char *name, const char *URL,
                               const char *version, const char *license,
                               enum modpack_type type, const char *subtype,
                               const char *notes)
{
  const char *type_str;
  const char *lic_str;
  const char *inst_str;

  if (modpack_type_is_valid(type)) {
    type_str = _(modpack_type_name(type));
  } else {
    /* TRANS: Unknown modpack type */
    type_str = _("?");
  }

  if (license != NULL) {
    lic_str = license;
  } else {
    /* TRANS: License of modpack is not known */
    lic_str = Q_("?license:Unknown");
  }

  inst_str = get_installed_version(name, type);
  if (inst_str == NULL) {
    inst_str = _("Not installed");
  }

  log_normal("%s", "");
  log_normal(_("Name=\"%s\""), name);
  log_normal(_("Version=\"%s\""), version);
  log_normal(_("Installed=\"%s\""), inst_str);
  log_normal(_("Type=\"%s\" / \"%s\""), type_str, subtype);
  log_normal(_("License=\"%s\""), lic_str);
  log_normal(_("URL=\"%s\""), URL);
}

/**************************************************************************
  Entry point of the freeciv-modpack program
**************************************************************************/
int main(int argc, char *argv[])
{
  int loglevel = LOG_NORMAL;
  int ui_options;

  init_nls();
  init_character_encodings(FC_DEFAULT_DATA_ENCODING, FALSE);
  registry_module_init();

  fc_init_network();

  log_init(NULL, loglevel, NULL, NULL, -1);

  /* This modifies argv! */
  ui_options = fcmp_parse_cmdline(argc, argv);

  if (ui_options != -1) {

    load_install_info_lists(&fcmp);

    log_normal(_("Freeciv modpack installer (command line version)"));

    if (fcmp.autoinstall == NULL) {
      download_modpack_list(&fcmp, setup_modpack_list, msg_callback);
    } else {
      const char *errmsg;

      errmsg = download_modpack(fcmp.autoinstall, &fcmp, msg_callback, NULL);

      if (errmsg == NULL) {
        log_normal(_("Modpack installed succesfully"));
      } else {
        log_error(_("Modpack install failed: %s"), errmsg);
      }
    }

    save_install_info_lists(&fcmp);
  }

  log_close();

  return EXIT_SUCCESS;
}