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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* utility */
#include "bitvector.h"
#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "rand.h"
#include "registry.h"
#include "shared.h"
#include "support.h"

/* common */
#include "ai.h"
#include "bitvector.h"
#include "capability.h"
#include "citizens.h"
#include "city.h"
#include "game.h"
#include "government.h"
#include "idex.h"
#include "map.h"
#include "movement.h"
#include "packets.h"
#include "research.h"
#include "specialist.h"
#include "unit.h"
#include "unitlist.h"
#include "version.h"
#include "victory.h"

/* server */
#include "aiiface.h"
#include "barbarian.h"
#include "citizenshand.h"
#include "citytools.h"
#include "cityturn.h"
#include "diplhand.h"
#include "mapgen.h"
#include "maphand.h"
#include "meta.h"
#include "notify.h"
#include "plrhand.h"
#include "ruleset.h"
#include "savecompat.h"
#include "score.h"
#include "settings.h"
#include "spacerace.h"
#include "srv_main.h"
#include "stdinhand.h"
#include "techtools.h"
#include "unittools.h"

/* server/advisors */
#include "advdata.h"
#include "advbuilding.h"
#include "infracache.h"

/* server/generator */
#include "utilities.h"

/* server/scripting */
#include "script_server.h"

/* ai */
#include "aitraits.h"
#include "difficulty.h"

#include "legacysave.h"

#define TOKEN_SIZE 10

#define log_worker      log_verbose

/*
 * This loops over the entire map to save data. It collects all the data of
 * a line using GET_XY_CHAR and then executes the macro SECFILE_INSERT_LINE.
 * Internal variables map_x, map_y, nat_x, nat_y, and line are allocated
 * within the macro but definable by the caller of the macro.
 *
 * Parameters:
 *   line: buffer variable to hold a line of chars
 *   map_x, map_y: variables for internal map coordinates
 *   nat_x, nat_y: variables for output/native coordinates
 *   GET_XY_CHAR: macro returning the map character for each position
 *   SECFILE_INSERT_LINE: macro to output each processed line (line nat_y)
 *
 * Note: don't use this macro DIRECTLY, use 
 * SAVE_NORMAL_MAP_DATA or SAVE_PLAYER_MAP_DATA instead.
 */
#define SAVE_MAP_DATA(ptile, line,					    \
                      GET_XY_CHAR, SECFILE_INSERT_LINE)                     \
{                                                                           \
  char line[game.map.xsize + 1];                                            \
  int _nat_x, _nat_y;							    \
                                                                            \
  for (_nat_y = 0; _nat_y < game.map.ysize; _nat_y++) {			    \
    for (_nat_x = 0; _nat_x < game.map.xsize; _nat_x++) {		    \
      struct tile *ptile = native_pos_to_tile(_nat_x, _nat_y);		    \
      fc_assert_action(ptile != NULL, continue);                            \
      line[_nat_x] = (GET_XY_CHAR);                                         \
      fc_assert_msg(fc_isprint(line[_nat_x] & 0x7f),                        \
                    "Trying to write invalid map data: '%c' %d",            \
                    line[_nat_x], line[_nat_x]);                            \
    }                                                                       \
    line[game.map.xsize] = '\0';                                            \
    (SECFILE_INSERT_LINE);                                                  \
  }                                                                         \
}

/*
 * Wrappers for SAVE_MAP_DATA.
 *
 * SAVE_NORMAL_MAP_DATA saves a standard line of map data.
 *
 * SAVE_PLAYER_MAP_DATA saves a line of map data from a playermap.
 */
#define SAVE_NORMAL_MAP_DATA(ptile, secfile, secname, GET_XY_CHAR)	    \
  SAVE_MAP_DATA(ptile, _line, GET_XY_CHAR,				    \
		secfile_insert_str(secfile, _line, secname, _nat_y))

#define SAVE_PLAYER_MAP_DATA(ptile, secfile, secname, plrno,		    \
			     GET_XY_CHAR)                                   \
  SAVE_MAP_DATA(ptile, _line, GET_XY_CHAR,				    \
		secfile_insert_str(secfile, _line, secname, plrno, _nat_y))

/*
 * This loops over the entire map to load data. It inputs a line of data
 * using the macro SECFILE_LOOKUP_LINE and then loops using the macro
 * SET_XY_CHAR to load each char into the map at (map_x, map_y).  Internal
 * variables ch, map_x, map_y, nat_x, and nat_y are allocated within the
 * macro but definable by the caller.
 *
 * Parameters:
 *   ch: a variable to hold a char (data for a single position)
 *   map_x, map_y: variables for internal map coordinates
 *   nat_x, nat_y: variables for output/native coordinates
 *   SET_XY_CHAR: macro to load the map character at each (map_x, map_y)
 *   SECFILE_LOOKUP_LINE: macro to input the nat_y line for processing
 *
 * Note: some (but not all) of the code this is replacing used to
 * skip over lines that did not exist.  This allowed for
 * backward-compatibility.  We could add another parameter that
 * specified whether it was OK to skip the data, but there's not
 * really much advantage to exiting early in this case.  Instead,
 * we let any map data type to be empty, and just print an
 * informative warning message about it.
 */
#define LOAD_MAP_DATA(ch, nat_y, ptile,					\
		      SECFILE_LOOKUP_LINE, SET_XY_CHAR)			\
{									\
  int _nat_x, _nat_y;							\
  bool _printed_warning = FALSE;					\
  for (_nat_y = 0; _nat_y < game.map.ysize; _nat_y++) {			\
    const int nat_y = _nat_y;						\
    const char *_line = (SECFILE_LOOKUP_LINE);				\
    if (NULL == _line) {						\
      log_verbose("Line not found='%s'", #SECFILE_LOOKUP_LINE);		\
      _printed_warning = TRUE;						\
      continue;								\
    } else if (strlen(_line) != game.map.xsize) {			\
      log_verbose("Line too short (expected %d got %lu)='%s'",		\
                  game.map.xsize, (unsigned long) strlen(_line),	\
                  #SECFILE_LOOKUP_LINE);				\
      _printed_warning = TRUE;						\
      continue;								\
    }									\
    for (_nat_x = 0; _nat_x < game.map.xsize; _nat_x++) {		\
      const char ch = _line[_nat_x];					\
      struct tile *ptile = native_pos_to_tile(_nat_x, _nat_y);		\
      (SET_XY_CHAR);							\
    }									\
  }									\
  if (_printed_warning) {						\
    fc_assert(0);							\
    /* TRANS: Minor error message. */					\
    log_error(_("Saved game contains incomplete map data. This can"	\
              " happen with old saved games, or it may indicate an"	\
              " invalid saved game file. Proceed at your own risk."));	\
  }									\
}

/* Iterate on the specials half-bytes */
#define special_halfbyte_iterate(s, num_specials_types)			    \
{									    \
  enum tile_special_type s;						    \
									    \
  for(s = 0; 4 * s < (num_specials_types); s++) {

#define special_halfbyte_iterate_end					    \
  }									    \
}

/* Iterate on the bases half-bytes */
#define bases_halfbyte_iterate(b, num_bases_types)			    \
{									    \
  int b;                                                                    \
									    \
  for(b = 0; 4 * b < (num_bases_types); b++) {

#define bases_halfbyte_iterate_end					    \
  }									    \
}

static const char num_chars[] =
  "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-+";

static bool load_river_overlay = FALSE;

static void set_savegame_special(struct tile *ptile, bv_extras *extras,
                                 char ch, const enum tile_special_type *index);

static void game_load_internal(struct section_file *file);

static struct player *identifier_to_player(char c);


static void worklist_load(struct section_file *file, struct worklist *pwl,
                          const char *path, ...)
                          fc__attribute((__format__ (__printf__, 3, 4)));

/****************************************************************************
  Converts single character into numerical value. This is not hex conversion.
****************************************************************************/
static int char2num(char ch)
{
  const char *pch;

  pch = strchr(num_chars, ch);

  fc_assert_ret_val_msg(NULL != pch, 0,
                        "Unknown ascii value for num: '%c' %d", ch, ch);

  return pch - num_chars;
}

/****************************************************************************
  Dereferences the terrain character.  See terrains[].identifier
    example: char2terrain('a') => T_ARCTIC
****************************************************************************/
static struct terrain *char2terrain(char ch)
{
  /* terrain_by_identifier plus fatal error */
  if (ch == TERRAIN_UNKNOWN_IDENTIFIER) {
    return T_UNKNOWN;
  }
  terrain_type_iterate(pterrain) {
    if (pterrain->identifier == ch) {
      return pterrain;
    }
  } terrain_type_iterate_end;

  log_fatal("Unknown terrain identifier '%c' in savegame.", ch);
  exit(EXIT_FAILURE);
}

/****************************************************************************
  Returns an order for a character identifier.  See also order2char.
****************************************************************************/
static enum unit_orders char2order(char order)
{
  switch (order) {
  case 'm':
  case 'M':
    return ORDER_MOVE;
  case 'w':
  case 'W':
    return ORDER_FULL_MP;
  case 'b':
  case 'B':
    return ORDER_OLD_BUILD_CITY;
  case 'a':
  case 'A':
    return ORDER_ACTIVITY;
  case 'd':
  case 'D':
    return ORDER_OLD_DISBAND;
  case 'u':
  case 'U':
    return ORDER_OLD_BUILD_WONDER;
  case 't':
  case 'T':
    return ORDER_OLD_TRADE_ROUTE;
  case 'h':
  case 'H':
    return ORDER_OLD_HOMECITY;
  }

  /* This can happen if the savegame is invalid. */
  return ORDER_LAST;
}

/****************************************************************************
  Returns a direction for a character identifier.  See also dir2char.
****************************************************************************/
static enum direction8 char2dir(char dir)
{
  /* Numberpad values for the directions. */
  switch (dir) {
  case '1':
    return DIR8_SOUTHWEST;
  case '2':
    return DIR8_SOUTH;
  case '3':
    return DIR8_SOUTHEAST;
  case '4':
    return DIR8_WEST;
  case '6':
    return DIR8_EAST;
  case '7':
    return DIR8_NORTHWEST;
  case '8':
    return DIR8_NORTH;
  case '9':
    return DIR8_NORTHEAST;
  }

  /* This can happen if the savegame is invalid. */
  return direction8_invalid();
}

/****************************************************************************
  Returns a character identifier for an activity.  See also char2activity.
****************************************************************************/
static char activity2char(enum unit_activity activity)
{
  switch (activity) {
  case ACTIVITY_IDLE:
    return 'w';
  case ACTIVITY_POLLUTION:
    return 'p';
  case ACTIVITY_OLD_ROAD:
    return 'r';
  case ACTIVITY_MINE:
    return 'm';
  case ACTIVITY_IRRIGATE:
    return 'i';
  case ACTIVITY_FORTIFIED:
    return 'f';
  case ACTIVITY_FORTRESS:
    return 't';
  case ACTIVITY_SENTRY:
    return 's';
  case ACTIVITY_OLD_RAILROAD:
    return 'l';
  case ACTIVITY_PILLAGE:
    return 'e';
  case ACTIVITY_GOTO:
    return 'g';
  case ACTIVITY_EXPLORE:
    return 'x';
  case ACTIVITY_TRANSFORM:
    return 'o';
  case ACTIVITY_AIRBASE:
    return 'a';
  case ACTIVITY_FORTIFYING:
    return 'y';
  case ACTIVITY_FALLOUT:
    return 'u';
  case ACTIVITY_BASE:
    return 'b';
  case ACTIVITY_GEN_ROAD:
    return 'R';
  case ACTIVITY_CONVERT:
    return 'c';
  case ACTIVITY_UNKNOWN:
  case ACTIVITY_PATROL_UNUSED:
    return '?';
  case ACTIVITY_LAST:
    break;
  }

  fc_assert(FALSE);
  return '?';
}

/****************************************************************************
  Returns an activity for a character identifier.  See also activity2char.
****************************************************************************/
static enum unit_activity char2activity(char activity)
{
  enum unit_activity a;

  for (a = 0; a < ACTIVITY_LAST; a++) {
    char achar = activity2char(a);

    if (activity == achar || activity == fc_toupper(achar)) {
      return a;
    }
  }

  /* This can happen if the savegame is invalid. */
  return ACTIVITY_LAST;
}

/****************************************************************************
  Setting compress type compability checks.
****************************************************************************/
static inline enum fz_method int2fz_method(int magic)
{
  switch (magic) {
  case 0:               /* Was FZ_PLAIN. */
    return FZ_PLAIN;
  case 1:               /* Was FZ_ZLIB. */
#ifdef FREECIV_HAVE_LIBZ
    return FZ_ZLIB;
#else
    log_verbose("Not compiled with zlib support, reverting to default.");
    break;
#endif /* FREECIV_HAVE_LIBZ */
  case 2:               /* Was FZ_BZIP2. */
#ifdef FREECIV_HAVE_LIBBZ2
    return FZ_BZIP2;
#else
    log_verbose("Not compiled with bzib2 support, reverting to default.");
    break;
#endif /* FREECIV_HAVE_LIBZ */
  case 3:
#ifdef FREECIV_HAVE_LIBLZMA
    return FZ_XZ;
#else
    log_verbose("Not compiled with xz support, reverting to default.");
    break;
#endif /* FREECIV_HAVE_LIBLZMA */
  }
  return GAME_DEFAULT_COMPRESS_TYPE;
}

/****************************************************************************
  Unquote a string. The unquoted data is written into dest. If the unquoted
  data will be larger than dest_length the function aborts. It returns the
  actual length of the unquoted block.
****************************************************************************/
static int unquote_block(const char *const quoted_, void *dest,
                         int dest_length)
{
  int i, length, parsed, tmp;
  char *endptr;
  const char *quoted = quoted_;

  parsed = sscanf(quoted, "%d", &length);
  fc_assert_ret_val(1 == parsed, 0);

  fc_assert_ret_val(length <= dest_length, 0);
  quoted = strchr(quoted, ':');
  fc_assert_ret_val(quoted != NULL, 0);
  quoted++;

  for (i = 0; i < length; i++) {
    tmp = strtol(quoted, &endptr, 16);
    fc_assert_ret_val((endptr - quoted) == 2, 0);
    fc_assert_ret_val(*endptr == ' ', 0);
    fc_assert_ret_val((tmp & 0xff) == tmp, 0);
    ((unsigned char *) dest)[i] = tmp;
    quoted += 3;
  }
  return length;
}


/****************************************************************************
  Load the worklist elements specified by path to the worklist pointed to
  by pwl.

  pwl should be a pointer to an existing worklist.

  path and ... give the prefix to load from, printf-style.
****************************************************************************/
static void worklist_load(struct section_file *file, struct worklist *pwl,
                          const char *path, ...)
{
  int i;
  const char *kind;
  const char *name;
  char path_str[1024];
  va_list ap;

  /* The first part of the registry path is taken from the varargs to the
   * function. */
  va_start(ap, path);
  fc_vsnprintf(path_str, sizeof(path_str), path, ap);
  va_end(ap);

  worklist_init(pwl);
  pwl->length = secfile_lookup_int_default(file, 0,
					   "%s.wl_length", path_str);

  for (i = 0; i < pwl->length; i++) {
    kind = secfile_lookup_str(file, "%s.wl_kind%d", path_str, i);
    if (!kind) {
      /* before 2.2.0 unit production was indicated by flag. */
      bool is_unit = secfile_lookup_bool_default(file, FALSE,
						 "%s.wl_is_unit%d",
						 path_str, i);
      kind = universals_n_name(is_unit ? VUT_UTYPE : VUT_IMPROVEMENT);
    }

    /* We lookup the production value by name.  An invalid entry isn't a
     * fatal error; we just truncate the worklist. */
    name = secfile_lookup_str_default(file, "-", "%s.wl_value%d",
				      path_str, i);
    pwl->entries[i] = universal_by_rule_name(kind, name);
    if (pwl->entries[i].kind == universals_n_invalid()) {
      log_error("%s.wl_value%d: unknown \"%s\" \"%s\".",
                path_str, i, kind, name);
      pwl->length = i;
      break;
    }
  }
}

/***************************************************************
load starting positions for the players from a savegame file
Now we don't know how many start positions there are nor how many
should be because rulesets are loaded later. So try to load as
many as they are; there should be at least enough for every
player.  This could be changed/improved in future.
***************************************************************/
static void map_load_startpos(struct section_file *file,
                              enum server_states state)
{
  int savegame_start_positions;
  int i;
  int nat_x, nat_y;

  /* Count entries. */
  for (savegame_start_positions = 0;
       secfile_lookup_int_default(file, -1, "map.r%dsx",
                                  savegame_start_positions) != -1;
       savegame_start_positions++) {
    /* Nothing. */
  }

  /* Load entries */
  for (i = 0; i < savegame_start_positions; i++) {
    const char *nation_name = secfile_lookup_str(file,
                                                 "map.r%dsnation", i);
    struct nation_type *pnation = NO_NATION_SELECTED;
    struct startpos *psp;
    struct tile *ptile;

    if (NULL != nation_name) {
      pnation = nation_by_rule_name(nation_name);
      if (NO_NATION_SELECTED == pnation) {
        log_error("Warning: Unknown nation %s for starting position %d",
                  nation_name, i);
      }
    }

    if (!secfile_lookup_int(file, &nat_x, "map.r%dsx", i)
        || !secfile_lookup_int(file, &nat_y, "map.r%dsy", i)) {
      log_error("%s", secfile_error());
      continue;
    }

    ptile = native_pos_to_tile(nat_x, nat_y);
    if (NULL == ptile) {
      log_error("Start position native coordinates (%d, %d) do not exist "
                "in this map. Skipping...", nat_x, nat_y);
      continue;
    }

    psp = map_startpos_new(native_pos_to_tile(nat_x, nat_y));
    if (NO_NATION_SELECTED != pnation) {
      startpos_allow(psp, pnation);
    }
  }

  if (0 < map_startpos_count()
      && state == S_S_INITIAL
      && map_startpos_count() < game.server.max_players) {
    log_verbose("Number of starts (%d) are lower than rules.max_players "
                "(%d), lowering rules.max_players.",
                map_startpos_count(), game.server.max_players);
    game.server.max_players = map_startpos_count();
  }
}

/***************************************************************
  Load the tile map from a savegame file
***************************************************************/
static void map_load_tiles(struct section_file *file)
{
  game.map.topology_id = secfile_lookup_int_default(file, MAP_ORIGINAL_TOPO,
                                                    "map.topology_id");

  /* In some cases we read these before, but not always, and
   * its safe to read them again:
   */
  if (!secfile_lookup_int(file, &game.map.xsize, "map.width")
      || !secfile_lookup_int(file, &game.map.ysize, "map.height")) {
    fc_assert_exit_msg(FALSE, "%s", secfile_error());
  }

  /* Initialize the map for the current topology. 'map.xsize' and
   * 'map.ysize' must be set. */
  map_init_topology();

  map_allocate();

  /* get the terrain type */
  LOAD_MAP_DATA(ch, line, ptile,
		secfile_lookup_str(file, "map.t%03d", line),
		ptile->terrain = char2terrain(ch));

  assign_continent_numbers();

  whole_map_iterate(ptile) {
    const char *spec_sprite;
    const char *label;
    int nat_x, nat_y;

    index_to_native_pos(&nat_x, &nat_y, tile_index(ptile));
    spec_sprite = secfile_lookup_str(file, "map.spec_sprite_%d_%d", nat_x,
                                     nat_y);
    label = secfile_lookup_str_default(file, NULL, "map.label_%d_%d", nat_x,
                                       nat_y);

    if (NULL != ptile->spec_sprite) {
      ptile->spec_sprite = fc_strdup(spec_sprite);
    }
    if (label != NULL) {
      tile_set_label(ptile, label);
    }
  } whole_map_iterate_end;
}

/* The order of the special bits, so that we can rebuild the
 * bitvector from old savegames (where the order of the bits is
 * unspecified). Here S_LAST means an unused bit (but see also resource
 * recovery). */
static const enum tile_special_type default_specials[] = {
  S_LAST, S_OLD_ROAD, S_IRRIGATION, S_OLD_RAILROAD,
  S_MINE, S_POLLUTION, S_HUT, S_OLD_FORTRESS,
  S_LAST, S_OLD_RIVER, S_FARMLAND, S_OLD_AIRBASE,
  S_FALLOUT, S_LAST, S_LAST, S_LAST
};

/****************************************************************************
  Load the rivers overlay map from a savegame file.

  A scenario may define the terrain of the map but not list the specials on
  it (thus allowing users to control the placement of specials).  However
  rivers are a special case and must be included in the map along with the
  scenario.  Thus in those cases this function should be called to load
  the river information separate from any other special data.

  This does not need to be called from map_load(), because map_load() loads
  the rivers overlay along with the rest of the specials.  Call this only
  if you've already called map_load_tiles(), and want to load only the
  rivers overlay but no other specials.  Scenarios that encode things this
  way should have the "riversoverlay" capability.
****************************************************************************/
static void map_load_rivers_overlay(struct section_file *file,
                                    const enum tile_special_type *special_order,
                                    int num_special_types)
{
  /* used by set_savegame_special */
  load_river_overlay = TRUE;

  if (special_order) {
    special_halfbyte_iterate(j, num_special_types) {
      char buf[16]; /* enough for sprintf() below */
      sprintf (buf, "map.spe%02d_%%03d", j);

      LOAD_MAP_DATA(ch, nat_y, ptile,
	secfile_lookup_str(file, buf, nat_y),
                    set_savegame_special(ptile, &ptile->extras, ch, special_order + 4 * j));
    } special_halfbyte_iterate_end;
  } else {
    /* Get the bits of the special flags which contain the river special
       and extract the rivers overlay from them. */
    fc_assert_ret(S_LAST <= 32);
    LOAD_MAP_DATA(ch, line, ptile,
      secfile_lookup_str(file, "map.n%03d", line),
                  set_savegame_special(ptile, &ptile->extras, ch, default_specials + 8));
  }

  load_river_overlay = FALSE;
}

/****************************************************************************
  Complicated helper function for loading specials from a savegame.

  'ch' gives the character loaded from the savegame.  Specials are packed
  in four to a character in hex notation.  'index' is a mapping of
  savegame bit -> special bit. S_LAST is used to mark unused savegame bits.
****************************************************************************/
static void set_savegame_special(struct tile *ptile, bv_extras *extras,
				 char ch,
				 const enum tile_special_type *index)
{
  int i, bin;
  const char *pch = strchr(hex_chars, ch);

  if (!pch || ch == '\0') {
    log_error("Unknown hex value: '%c' (%d)", ch, ch);
    bin = 0;
  } else {
    bin = pch - hex_chars;
  }

  for (i = 0; i < 4; i++) {
    enum tile_special_type sp = index[i];

    if (sp == S_LAST) {
      continue;
    }
    if (load_river_overlay && sp != S_OLD_RIVER) {
      continue;
    }

    if (bin & (1 << i)) {
      /* Pre 2.2 savegames have fortresses and airbases as part of specials */
      if (sp == S_OLD_FORTRESS) {
        struct base_type *pbase;

        pbase = get_base_by_gui_type(BASE_GUI_FORTRESS, NULL, NULL);
        if (pbase) {
          BV_SET(*extras, extra_index(base_extra_get(pbase)));
        }
      } else if (sp == S_OLD_AIRBASE) {
        struct base_type *pbase;

        pbase = get_base_by_gui_type(BASE_GUI_AIRBASE, NULL, NULL);
        if (pbase) {
          BV_SET(*extras, extra_index(base_extra_get(pbase)));
        }
      } else if (sp == S_OLD_ROAD) {
        struct road_type *proad;

        proad = road_by_compat_special(ROCO_ROAD);
        if (proad) {
          BV_SET(*extras, extra_index(road_extra_get(proad)));
        }
      } else if (sp == S_OLD_RAILROAD) {
        struct road_type *proad;

        proad = road_by_compat_special(ROCO_RAILROAD);
        if (proad) {
          BV_SET(*extras, extra_index(road_extra_get(proad)));
        }
      } else if (sp == S_OLD_RIVER) {
        struct road_type *proad;

        proad = road_by_compat_special(ROCO_RIVER);
        if (proad) {
          BV_SET(*extras, extra_index(road_extra_get(proad)));
        }
      } else {
        struct extra_type *pextra = NULL;
        enum extra_cause cause = EC_COUNT;

        /* Converting from old hardcoded specials to as sensible extra as we can */
        switch (sp) {
        case S_IRRIGATION:
        case S_FARMLAND:
          /* If old savegame has both irrigation and farmland, EC_IRRIGATION
           * gets applied twice, which hopefully has the correct result. */
          cause = EC_IRRIGATION;
          break;
        case S_MINE:
          cause = EC_MINE;
          break;
        case S_POLLUTION:
          cause = EC_POLLUTION;
          break;
        case S_HUT:
          cause = EC_HUT;
          break;
        case S_FALLOUT:
          cause = EC_FALLOUT;
          break;
        default:
          pextra = extra_type_by_rule_name(special_rule_name(sp));
          break;
        }

        if (cause != EC_COUNT) {
          struct tile *vtile = tile_virtual_new(ptile);

          /* Do not let the extras already set to the real tile mess with setup
           * of the player tiles if that's what we're doing. */
          vtile->extras = *extras;

          /* It's ok not to know which player or which unit originally built the extra -
           * in the rules used when specials were saved these could not have made any
           * difference. */
          pextra = next_extra_for_tile(vtile, cause, NULL, NULL);

          tile_virtual_destroy(vtile);
        }

        if (pextra) {
          BV_SET(*extras, extra_index(pextra));
        }
      }
    }
  }
}

/****************************************************************************
  Helper function for loading bases from a savegame.

  'ch' gives the character loaded from the savegame. Bases are packed
  in four to a character in hex notation.  'index' is a mapping of
  savegame bit -> base bit.
****************************************************************************/
static void set_savegame_bases(bv_extras *extras,
                               char ch,
                               struct base_type **index)
{
  int i, bin;
  const char *pch = strchr(hex_chars, ch);

  if (!pch || ch == '\0') {
    log_error("Unknown hex value: '%c' (%d)", ch, ch);
    bin = 0;
  } else {
    bin = pch - hex_chars;
  }

  for (i = 0; i < 4; i++) {
    struct base_type *pbase = index[i];

    if (pbase == NULL) {
      continue;
    }
    if (bin & (1 << i)) {
      BV_SET(*extras, extra_index(base_extra_get(pbase)));
    }
  }
}

/****************************************************************************
  Complicated helper function for reading resources from a savegame.
  This reads resources saved in the specials bitvector.
  ch is the character read from the map, and n is the number of the special
  (0 for special_1, 1 for special_2).
****************************************************************************/
static void set_savegame_old_resource(struct resource_type **r,
                                      const struct terrain *terrain,
                                      bv_extras *extras,
                                      char ch, int n)
{
  fc_assert_ret(n == 0 || n == 1);
  /* If resource is already set to non-NULL or there is no resource found
   * in this half-byte, then abort */
  if (*r || !(ascii_hex2bin (ch, 0) & 0x1) || !terrain->resources[0]) {
    return;
  }
  /* Note that we must handle the case of special_2 set when there is
   * only one resource defined for the terrain (example, Shields on
   * Grassland terrain where both resources are identical) */
  if (n == 0 || !terrain->resources[1]) {
    *r = terrain->resources[0];
  } else {
    *r = terrain->resources[1];
  }

  if ((*r) != NULL) {
    BV_SET(*extras, extra_index((*r)->self));
  }
}

/****************************************************************************
  Return the resource for the given identifier.
****************************************************************************/
static struct resource_type *identifier_to_resource(char c)
{
  /* speed common values */
  if (c == RESOURCE_NULL_IDENTIFIER
      || c == RESOURCE_NONE_IDENTIFIER) {
    return NULL;
  }
  return resource_by_identifier(c);
}

/****************************************************************************
  Return the player for the given identifier.
****************************************************************************/
static struct player *identifier_to_player(char c)
{
  if (c == ' ') {
    return NULL;
  }
  if ('0' <= c && c <= '9') {
    return player_by_number(c - '0');
  }
  return player_by_number(10 + c - 'A');
}

/***************************************************************
load a complete map from a savegame file
***************************************************************/
static void map_load(struct section_file *file,
                     enum server_states state,
                     const char *savefile_options,
                     const enum tile_special_type *special_order,
                     int num_special_types,
                     struct base_type **base_order,
                     int num_bases_types)
{
  /* map_init();
   * This is already called in game_init(), and calling it
   * here stomps on map.huts etc.  --dwp
   */

  map_load_tiles(file);
  if (secfile_lookup_bool_default(file, TRUE, "game.save_starts")) {
    map_load_startpos(file, state);
  }

  if (special_order) {
    /* First load the resources. This is straightforward. */
    LOAD_MAP_DATA(ch, nat_y, ptile,
		  secfile_lookup_str(file, "map.res%03d", nat_y),
		  ptile->resource = identifier_to_resource(ch));

    special_halfbyte_iterate(j, num_special_types) {
      char buf[16]; /* enough for sprintf() below */
      sprintf (buf, "map.spe%02d_%%03d", j);

      LOAD_MAP_DATA(ch, nat_y, ptile,
	  secfile_lookup_str(file, buf, nat_y),
                    set_savegame_special(ptile, &ptile->extras,
                                         ch, special_order + 4 * j));
    } special_halfbyte_iterate_end;
  } else {
    /* get 4-bit segments of 16-bit "special" field. */
    LOAD_MAP_DATA(ch, nat_y, ptile,
	    secfile_lookup_str(file, "map.l%03d", nat_y),
                  set_savegame_special(ptile, &ptile->extras,
                                       ch, default_specials + 0));
    LOAD_MAP_DATA(ch, nat_y, ptile,
	    secfile_lookup_str(file, "map.u%03d", nat_y),
                  set_savegame_special(ptile, &ptile->extras,
                                       ch, default_specials + 4));
    LOAD_MAP_DATA(ch, nat_y, ptile,
	    secfile_lookup_str(file, "map.n%03d", nat_y),
                  set_savegame_special(ptile, &ptile->extras,
                                       ch, default_specials + 8));
    LOAD_MAP_DATA(ch, nat_y, ptile,
	    secfile_lookup_str(file, "map.f%03d", nat_y),
                  set_savegame_special(ptile, &ptile->extras,
                                       ch, default_specials + 12));
    /* Setup resources (from half-bytes 1 and 3 of old savegames) */
    LOAD_MAP_DATA(ch, nat_y, ptile,
                  secfile_lookup_str(file, "map.l%03d", nat_y),
                  set_savegame_old_resource(&ptile->resource, ptile->terrain, &ptile->extras, ch, 0));
    LOAD_MAP_DATA(ch, nat_y, ptile,
                  secfile_lookup_str(file, "map.n%03d", nat_y),
                  set_savegame_old_resource(&ptile->resource, ptile->terrain, &ptile->extras, ch, 1));
  }

  /* after the resources are loaded, indicate those currently valid */
  whole_map_iterate(ptile) {
    if (NULL == ptile->resource) {
      continue;
    }

    if (ptile->terrain == NULL || !terrain_has_resource(ptile->terrain, ptile->resource)) {
      BV_CLR(ptile->extras, extra_index(ptile->resource->self));
    }
  } whole_map_iterate_end;

  if (has_capability("bases", savefile_options)) {
    char zeroline[game.map.xsize + 1];
    int i;

    /* This is needed when new bases has been added to ruleset, and                              
     * thus game.control.num_base_types is greater than, when game was saved. */
    for (i = 0; i < game.map.xsize; i++) {
      zeroline[i] = '0';
    }
    zeroline[i] = '\0';

    bases_halfbyte_iterate(j, num_bases_types) {
      char buf[16]; /* enough for sprintf() below */
      sprintf(buf, "map.b%02d_%%03d", j);

      LOAD_MAP_DATA(ch, nat_y, ptile,
                    secfile_lookup_str_default(file, zeroline, buf, nat_y),
                    set_savegame_bases(&ptile->extras, ch, base_order + 4 * j));
    } bases_halfbyte_iterate_end;
  }
}

/****************************************************************************
  Load data about tiles owners. This must be after players are allocated.
****************************************************************************/
static void map_load_owner(struct section_file *file,
                           const char *savefile_options)
{
  /* Owner and ownership source are stored as plain numbers */
  if (has_capability("new_owner_map", savefile_options)) {
    int x, y;
    struct player *owner = NULL;
    struct tile *claimer = NULL;

    for (y = 0; y < game.map.ysize; y++) {
      const char *buffer1 = secfile_lookup_str(file, "map.owner%03d", y);
      const char *buffer2 = secfile_lookup_str(file, "map.source%03d", y);
      const char *ptr1 = buffer1;
      const char *ptr2 = buffer2;

      fc_assert_exit_msg(NULL != buffer1,
                         "Savegame corrupt - map line %d not found.", y);
      fc_assert_exit_msg(NULL != buffer2,
                         "Savegame corrupt - map line %d not found.", y);
      for (x = 0; x < game.map.xsize; x++) {
        char token1[TOKEN_SIZE];
        char token2[TOKEN_SIZE];
        int number;
        struct tile *ptile = native_pos_to_tile(x, y);

        scanin(&ptr1, ",", token1, sizeof(token1));
        scanin(&ptr2, ",", token2, sizeof(token2));
        fc_assert_exit_msg('\0' != token1[0],
                           "Savegame corrupt - map size not correct.");
        fc_assert_exit_msg('\0' != token2[0],
                           "Savegame corrupt - map size not correct.");
        if (strcmp(token1, "-") == 0) {
          owner = NULL;
        } else {
          fc_assert_exit_msg(str_to_int(token1, &number),
                             "Savegame corrupt - got map owner %s "
                             "in (%d, %d).", token1, x, y);
          owner = player_by_number(number);
        }
        if (strcmp(token2, "-") == 0) {
          claimer = NULL;
        } else {
          fc_assert_exit_msg(str_to_int(token2, &number),
                             "Savegame corrupt - got map source %s "
                             "in (%d, %d).", token2, x, y);
          claimer = index_to_tile(number);
        }

        map_claim_ownership(ptile, owner, claimer, TRUE);
      }
    }
  }
}

/****************************************************************************
  Load data about known tiles. This must be after players are allocated.
****************************************************************************/
static void map_load_known(struct section_file *file,
                           const char *savefile_options)
{
  if (secfile_lookup_bool_default(file, TRUE, "game.save_known")) {
    int *known = fc_calloc(MAP_INDEX_SIZE, sizeof(int));

    /* get 4-bit segments of the first half of the 32-bit "known" field */
    LOAD_MAP_DATA(ch, nat_y, ptile,
		  secfile_lookup_str(file, "map.a%03d", nat_y),
		  known[tile_index(ptile)] = ascii_hex2bin(ch, 0));
    LOAD_MAP_DATA(ch, nat_y, ptile,
		  secfile_lookup_str(file, "map.b%03d", nat_y),
		  known[tile_index(ptile)] |= ascii_hex2bin(ch, 1));
    LOAD_MAP_DATA(ch, nat_y, ptile,
		  secfile_lookup_str(file, "map.c%03d", nat_y),
		  known[tile_index(ptile)] |= ascii_hex2bin(ch, 2));
    LOAD_MAP_DATA(ch, nat_y, ptile,
		  secfile_lookup_str(file, "map.d%03d", nat_y),
		  known[tile_index(ptile)] |= ascii_hex2bin(ch, 3));

    if (has_capability("known32fix", savefile_options)) {
      /* get 4-bit segments of the second half of the 32-bit "known" field */
      LOAD_MAP_DATA(ch, nat_y, ptile,
		    secfile_lookup_str(file, "map.e%03d", nat_y),
		    known[tile_index(ptile)] |= ascii_hex2bin(ch, 4));
      LOAD_MAP_DATA(ch, nat_y, ptile,
		    secfile_lookup_str(file, "map.g%03d", nat_y),
		    known[tile_index(ptile)] |= ascii_hex2bin(ch, 5));
      LOAD_MAP_DATA(ch, nat_y, ptile,
		    secfile_lookup_str(file, "map.h%03d", nat_y),
		    known[tile_index(ptile)] |= ascii_hex2bin(ch, 6));
      LOAD_MAP_DATA(ch, nat_y, ptile,
		    secfile_lookup_str(file, "map.i%03d", nat_y),
		    known[tile_index(ptile)] |= ascii_hex2bin(ch, 7));
    }

    players_iterate(pplayer) {
      dbv_clr_all(&pplayer->tile_known);
    } players_iterate_end;

    /* HACK: we read the known data from hex into a 32-bit integer, and
     * now we convert it to bv_player. */
    whole_map_iterate(ptile) {
      players_iterate(pplayer) {
        if (known[tile_index(ptile)] & (1u << player_index(pplayer))) {
          map_set_known(ptile, pplayer);
        }
      } players_iterate_end;
    } whole_map_iterate_end;
    FC_FREE(known);
  }
  game.map.server.have_resources = TRUE;
}

/*****************************************************************************
  Load technology from path_name and if doesn't exist (because savegame
  is too old) load from path.
*****************************************************************************/
static Tech_type_id technology_load(struct section_file *file,
                                    const char* path, int plrno)
{
  char path_with_name[128];
  const char* name;
  struct advance *padvance;

  fc_snprintf(path_with_name, sizeof(path_with_name),
              "%s_name", path);

  name = secfile_lookup_str(file, path_with_name, plrno);
  if (!name) {
    log_fatal("%s: no tech name", path_with_name);
    exit(EXIT_FAILURE);
  }

  if (fc_strcasecmp(name, "A_FUTURE") == 0) {
    return A_FUTURE;
  }
  if (fc_strcasecmp(name, "A_NONE") == 0) {
    return A_NONE;
  }
  if (fc_strcasecmp(name, "A_UNSET") == 0) {
    return A_UNSET;
  }
  if (name[0] == '\0') {
    /* used by researching_saved */
    return A_UNKNOWN;
  }

  padvance = advance_by_rule_name(name);
  if (NULL == padvance) {
    log_fatal("%s: unknown technology \"%s\".", path_with_name, name);
    exit(EXIT_FAILURE);    
  }
  return advance_number(padvance);
}

/****************************************************************************
  Loads the units for the given player.
****************************************************************************/
static void player_load_units(struct player *plr, int plrno,
                              struct section_file *file,
                              const char *savefile_options,
                              const enum tile_special_type *special_order,
                              int num_special_types,
                              struct base_type **base_order,
                              int num_base_types)
{
  int nunits, i, j;
  enum unit_activity activity;

  fc_assert_exit_msg(secfile_lookup_int(file, &nunits,
                                        "player%d.nunits", plrno),
                     "%s", secfile_error());
  if (!plr->is_alive && nunits > 0) {
    nunits = 0; /* Some old savegames may be buggy. */
  }
  
  for (i = 0; i < nunits; i++) {
    struct unit *punit;
    struct city *pcity;
    int nat_x, nat_y;
    const char* type_name;
    struct unit_type *type;
    int veteran;
    enum tile_special_type target;
    struct base_type *pbase = NULL;
    struct road_type *proad = NULL;
    int base;
    char unitstr[32];
    int ei;
    struct tile *ptile;

    type_name = secfile_lookup_str(file, "player%d.u%d.type_by_name",
                                   plrno, i);
    if (!type_name) {
      log_fatal("Player %d, unit %d, no type", plrno, i);
      exit(EXIT_FAILURE);
    }
    
    type = unit_type_by_rule_name(type_name);
    if (!type) {
      log_fatal("player%d.u%d: unknown unit type \"%s\".",
                plrno, i, type_name);
      exit(EXIT_FAILURE);
    }

    veteran
      = secfile_lookup_int_default(file, 0, "player%d.u%d.veteran", plrno, i);
    {
      /* Protect against change in veteran system in ruleset */
      const int levels = utype_veteran_levels(type);
      if (veteran >= levels) {
        fc_assert(levels >= 1);
        veteran = levels - 1;
      }
    }
    
    punit = unit_virtual_create(plr, NULL, type, veteran);
    fc_assert_exit_msg(secfile_lookup_int(file, &punit->id,
                                          "player%d.u%d.id", plrno, i),
                       "%s", secfile_error());
    identity_number_reserve(punit->id);
    idex_register_unit(punit);

    fc_assert_exit_msg(secfile_lookup_int(file, &nat_x, "player%d.u%d.x",
                                          plrno, i), "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &nat_y, "player%d.u%d.y",
                                          plrno, i), "%s", secfile_error());
    unit_tile_set(punit, native_pos_to_tile(nat_x, nat_y));
    ptile = unit_tile(punit);
    if (NULL == ptile) {
      log_fatal("player%d.u%d invalid tile (%d, %d)",
                plrno, i, nat_x, nat_y);
      exit(EXIT_FAILURE);
    }

    punit->nationality = plr;

    /* Avoid warning when loading pre-2.1 saves containing foul status */
    (void) secfile_entry_lookup(file, "player%d.u%d.foul", plrno, i);

    fc_assert_exit_msg(secfile_lookup_int(file, &punit->homecity,
                                          "player%d.u%d.homecity", plrno, i),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &punit->moves_left,
                                          "player%d.u%d.moves", plrno, i),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &punit->fuel,
                                          "player%d.u%d.fuel", plrno, i),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &ei,
                                          "player%d.u%d.activity", plrno, i),
                       "%s", secfile_error());
    activity = ei;

    if ((pcity = game_city_by_number(punit->homecity))) {
      unit_list_prepend(pcity->units_supported, punit);
    } else if (punit->homecity > IDENTITY_NUMBER_ZERO) {
      log_error("player%d.u%d: bad home city %d.",
                plrno, i, punit->homecity);
      punit->homecity = IDENTITY_NUMBER_ZERO;
    }

    punit->server.birth_turn = secfile_lookup_int_default(file, game.info.turn,
                                                   "player%d.u%d.born", plrno, i);
    base = secfile_lookup_int_default(file, -1,
                                      "player%d.u%d.activity_base", plrno, i);
    if (base >= 0 && base < num_base_types) {
      pbase = base_order[base];
    }

    if (special_order) {
      int tgt_no
        = secfile_lookup_int_default(file, num_special_types /* S_LAST */,
                                     "player%d.u%d.activity_target", plrno, i);
      if (tgt_no >= 0 && tgt_no < num_special_types) {
        target = special_order[tgt_no];
      } else {
        target = S_LAST;
      }
    } else {
      /* (Perhaps targeted pillaging post-dates default_specials, but better
       * safe than sorry.) */
      int tgt_no
        = secfile_lookup_int_default(file, ARRAY_SIZE(default_specials),
                                     "player%d.u%d.activity_target", plrno, i);
      if (tgt_no >= 0 && tgt_no < ARRAY_SIZE(default_specials)) {
        target = default_specials[tgt_no];
      } else {
        target = S_LAST;
      }
    }
    if (target == S_OLD_FORTRESS) {
      target = S_LAST;
      pbase = base_type_by_rule_name("Fortress");
    } else if (target == S_OLD_AIRBASE) {
      target = S_LAST;
      pbase = base_type_by_rule_name("Airbase");
    }

    if (target == S_OLD_ROAD) {
      target = S_LAST;
      proad = road_by_compat_special(ROCO_ROAD);
    } else if (target == S_OLD_RAILROAD) {
      target = S_LAST;
      proad = road_by_compat_special(ROCO_RAILROAD);
    }

    if (activity == ACTIVITY_PATROL_UNUSED) {
      /* Previously ACTIVITY_PATROL and ACTIVITY_GOTO were used for
       * client-side goto.  Now client-side goto is handled by setting
       * a special flag, and units with orders generally have ACTIVITY_IDLE.
       * Old orders are lost.  Old client-side goto units will still have
       * ACTIVITY_GOTO and will goto the correct position via server goto.
       * Old client-side patrol units lose their patrol routes and are put
       * into idle mode. */
      activity = ACTIVITY_IDLE;
    }

    if (activity == ACTIVITY_FORTRESS) {
      activity = ACTIVITY_BASE;
      pbase = get_base_by_gui_type(BASE_GUI_FORTRESS, punit, ptile);
    } else if (activity == ACTIVITY_AIRBASE) {
      activity = ACTIVITY_BASE;
      pbase = get_base_by_gui_type(BASE_GUI_AIRBASE, punit, ptile);
    }

    if (activity == ACTIVITY_OLD_ROAD) {
      activity = ACTIVITY_GEN_ROAD;
      proad = road_by_compat_special(ROCO_ROAD);
    } else if (activity == ACTIVITY_OLD_RAILROAD) {
      activity = ACTIVITY_GEN_ROAD;
      proad = road_by_compat_special(ROCO_RAILROAD);
    }

    if (activity == ACTIVITY_IRRIGATE) {
      struct extra_type *tgt = next_extra_for_tile(unit_tile(punit),
                                                   EC_IRRIGATION,
                                                   unit_owner(punit),
                                                   punit);
      if (tgt != NULL) {
        set_unit_activity_targeted(punit, ACTIVITY_IRRIGATE, tgt);
      } else {
        set_unit_activity_targeted(punit, ACTIVITY_IRRIGATE, NULL);
      }
    } else if (activity == ACTIVITY_MINE) {
      struct extra_type *tgt = next_extra_for_tile(unit_tile(punit),
                                                   EC_MINE,
                                                   unit_owner(punit),
                                                   punit);
      if (tgt != NULL) {
        set_unit_activity_targeted(punit, ACTIVITY_MINE, tgt);
      } else {
        set_unit_activity_targeted(punit, ACTIVITY_MINE, NULL);
      }
    } else if (activity == ACTIVITY_POLLUTION) {
      struct extra_type *tgt = prev_extra_in_tile(unit_tile(punit),
                                                  ERM_CLEANPOLLUTION,
                                                  unit_owner(punit),
                                                  punit);
      if (tgt != NULL) {
        set_unit_activity_targeted(punit, ACTIVITY_POLLUTION, tgt);
      } else {
        set_unit_activity_targeted(punit, ACTIVITY_POLLUTION, NULL);
      }
    } else if (activity == ACTIVITY_FALLOUT) {
      struct extra_type *tgt = prev_extra_in_tile(unit_tile(punit),
                                                  ERM_CLEANFALLOUT,
                                                  unit_owner(punit),
                                                  punit);
      if (tgt != NULL) {
        set_unit_activity_targeted(punit, ACTIVITY_FALLOUT, tgt);
      } else {
        set_unit_activity_targeted(punit, ACTIVITY_FALLOUT, NULL);
      }
    } else if (activity == ACTIVITY_BASE) {
      if (pbase) {
        set_unit_activity_base(punit, base_number(pbase));
      } else {
        log_error("Cannot find base %d for %s to build",
                  base, unit_rule_name(punit));
        set_unit_activity(punit, ACTIVITY_IDLE);
      }
    } else if (activity == ACTIVITY_GEN_ROAD) {
      if (proad) {
        set_unit_activity_road(punit, road_number(proad));
      } else {
        log_error("Cannot find road for %s to build",
                  unit_rule_name(punit));
        set_unit_activity(punit, ACTIVITY_IDLE);
      }
    } else if (activity == ACTIVITY_PILLAGE) {
      struct extra_type *a_target;

      if (target != S_LAST) {
        a_target = special_extra_get(target);
      } else if (pbase != NULL) {
        a_target = base_extra_get(pbase);
      } else if (proad != NULL) {
        a_target = road_extra_get(proad);
      } else {
        a_target = NULL;
      }
      /* An out-of-range base number is seen with old savegames. We take
       * it as indicating undirected pillaging. We will assign pillage
       * targets before play starts. */
      set_unit_activity_targeted(punit, activity, a_target);
    } else {
      set_unit_activity(punit, activity);
    }

    /* need to do this to assign/deassign settlers correctly -- Syela
     *
     * was punit->activity=secfile_lookup_int(file,
     *                             "player%d.u%d.activity",plrno, i); */
    fc_assert_exit_msg(secfile_lookup_int(file, &punit->activity_count,
                       "player%d.u%d.activity_count", plrno, i),
                       "%s", secfile_error());

    /* Special case: for a long time, we accidentally incremented
     * activity_count while a unit was sentried, so it could increase
     * without bound (bug #20641) and be saved in old savefiles.
     * We zero it to prevent potential trouble overflowing the range
     * in network packets, etc. */
    if (activity == ACTIVITY_SENTRY) {
      punit->activity_count = 0;
    }

    punit->done_moving = secfile_lookup_bool_default(file,
	(punit->moves_left == 0), "player%d.u%d.done_moving", plrno, i);

    punit->battlegroup
      = secfile_lookup_int_default(file, BATTLEGROUP_NONE,
				   "player%d.u%d.battlegroup", plrno, i);

    /* Load the goto information.  Older savegames will not have the
     * "go" field, so we just load the goto destination by default. */
    if (secfile_lookup_bool_default(file, TRUE,
                                    "player%d.u%d.go", plrno, i)) {
      int gnat_x, gnat_y;

      fc_assert_exit_msg(secfile_lookup_int(file, &gnat_x,
                                            "player%d.u%d.goto_x", plrno, i),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &gnat_y,
                                            "player%d.u%d.goto_y", plrno, i),
                         "%s", secfile_error());

      punit->goto_tile = native_pos_to_tile(gnat_x, gnat_y);
    } else {
      punit->goto_tile = NULL;
    }

    /* Load AI data of the unit. */
    fc_snprintf(unitstr, sizeof(unitstr), "player%d.u%d", plrno, i);
    CALL_FUNC_EACH_AI(unit_load, file, punit, unitstr);

    fc_assert_exit_msg(secfile_lookup_bool(file, &punit->ai_controlled,
                                           "player%d.u%d.ai", plrno, i),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &punit->hp,
                                          "player%d.u%d.hp", plrno, i),
                       "%s", secfile_error());

    punit->server.ord_map
      = secfile_lookup_int_default(file, 0,
				   "player%d.u%d.ord_map", plrno, i);
    punit->server.ord_city
      = secfile_lookup_int_default(file, 0,
				   "player%d.u%d.ord_city", plrno, i);
    punit->moved
      = secfile_lookup_bool_default(file, FALSE,
				    "player%d.u%d.moved", plrno, i);
    punit->paradropped
      = secfile_lookup_bool_default(file, FALSE,
				    "player%d.u%d.paradropped", plrno, i);

    /* 'transported_by' is loaded later. */

    /* Initialize upkeep values: these are hopefully initialized
       elsewhere before use (specifically, in city_support(); but
       fixme: check whether always correctly initialized?).
       Below is mainly for units which don't have homecity --
       otherwise these don't get initialized (and AI calculations
       etc may use junk values).
    */
    output_type_iterate(o) {
      punit->upkeep[o] = utype_upkeep_cost(type, plr, o);
    } output_type_iterate_end;

    /* load the unit orders */
    if (has_capability("orders", savefile_options)) {
      int road_idx = road_index(road_by_compat_special(ROCO_ROAD));
      int rail_idx = road_index(road_by_compat_special(ROCO_RAILROAD));

      int len = secfile_lookup_int_default(file, 0,
			"player%d.u%d.orders_length", plrno, i);
      if (len > 0) {
        const char *orders_buf, *dir_buf, *act_buf, *base_buf;

	punit->orders.list = fc_malloc(len * sizeof(*(punit->orders.list)));
	punit->orders.length = len;
	punit->orders.index = secfile_lookup_int_default(file, 0,
			"player%d.u%d.orders_index", plrno, i);
	punit->orders.repeat = secfile_lookup_bool_default(file, FALSE,
			"player%d.u%d.orders_repeat", plrno, i);
	punit->orders.vigilant = secfile_lookup_bool_default(file, FALSE,
			"player%d.u%d.orders_vigilant", plrno, i);

	orders_buf = secfile_lookup_str_default(file, "",
			"player%d.u%d.orders_list", plrno, i);
	dir_buf = secfile_lookup_str_default(file, "",
			"player%d.u%d.dir_list", plrno, i);
	act_buf = secfile_lookup_str_default(file, "",
			"player%d.u%d.activity_list", plrno, i);
        base_buf = secfile_lookup_str(file, "player%d.u%d.base_list",
                                      plrno, i);
	punit->has_orders = TRUE;
	for (j = 0; j < len; j++) {
	  struct unit_order *order = &punit->orders.list[j];
          struct base_type *order_base = NULL;

	  if (orders_buf[j] == '\0' || dir_buf[j] == '\0'
	      || act_buf[j] == '\0') {
            log_error("Invalid unit orders.");
	    free_unit_orders(punit);
	    break;
	  }
	  order->order = char2order(orders_buf[j]);
	  order->dir = char2dir(dir_buf[j]);
	  order->activity = char2activity(act_buf[j]);

	  if (order->order == ORDER_LAST
	      || (order->order == ORDER_MOVE && !direction8_is_valid(order->dir))
	      || (order->order == ORDER_ACTIVITY
		  && order->activity == ACTIVITY_LAST)) {
	    /* An invalid order.  Just drop the orders for this unit. */
	    free(punit->orders.list);
	    punit->orders.list = NULL;
	    punit->has_orders = FALSE;
	    break;
	  }

          /* The order may have been replaced by the perform action order */
          order->action = sg_order_to_action(order->order, punit,
                                             punit->goto_tile);
          if (order->action != ACTION_COUNT) {
            /* The order was converted by order_to_action */
            order->order = ORDER_PERFORM_ACTION;
          }

          /* Pre 2.2 savegames had activities ACTIVITY_FORTRESS and ACTIVITY_AIRBASE */
          if (order->activity == ACTIVITY_FORTRESS) {
            order_base = get_base_by_gui_type(BASE_GUI_FORTRESS, NULL, NULL);
            order->activity = ACTIVITY_IDLE; /* In case no matching gui_type found */
          } else if (order->activity == ACTIVITY_AIRBASE) {
            order_base = get_base_by_gui_type(BASE_GUI_AIRBASE, NULL, NULL);
            order->activity = ACTIVITY_IDLE; /* In case no matching gui_type found */
          }
          if (order_base != NULL) {
            /* Either ACTIVITY_FORTRESS or ACTIVITY_AIRBASE */
            order->activity = ACTIVITY_BASE;
            order->target = extra_number(base_extra_get(order_base));
          } else if (base_buf && base_buf[j] != '?') {
            base = char2num(base_buf[j]);

            if (base >= 0 && base < num_base_types) {
              order_base = base_order[base];
            } else {
              log_error("Cannot find base %d for %s to build",
                        base, unit_rule_name(punit));
              base = base_number(get_base_by_gui_type(BASE_GUI_FORTRESS, NULL, NULL));
            }

            order->target = extra_number(base_extra_get(base_by_number(base)));
          } else {
            order->target = EXTRA_NONE;
          }

          if (order->activity == ACTIVITY_OLD_ROAD) {
            order->activity = ACTIVITY_GEN_ROAD;
            order->target = extra_number(road_extra_get(road_by_number(road_idx)));
          } else if (order->activity == ACTIVITY_OLD_RAILROAD) {
            order->activity = ACTIVITY_GEN_ROAD;
            order->target = extra_number(road_extra_get(road_by_number(rail_idx)));
          }
	}
        if (punit->orders.list[len - 1].order == ORDER_MOVE) {
          /* Last move never safe in old versions. */
          punit->orders.list[len - 1].order = ORDER_ACTION_MOVE;
        }
      } else {
	punit->has_orders = FALSE;
	punit->orders.list = NULL;
      }
    } else {
      /* Old-style goto routes get discarded. */
      punit->has_orders = FALSE;
      punit->orders.list = NULL;
    }

    /* allocate the unit's contribution to fog of war */
    punit->server.vision = vision_new(unit_owner(punit), ptile);
    unit_refresh_vision(punit);
    /* NOTE: There used to be some map_set_known calls here.  These were
     * unneeded since unfogging the tile when the unit sees it will
     * automatically reveal that tile. */

    unit_list_append(plr->units, punit);

    unit_list_prepend(ptile->units, punit);

    /* Claim ownership of fortress? */
    if ((extra_owner(ptile) == NULL
         || pplayers_at_war(extra_owner(ptile), plr))
        && tile_has_claimable_base(ptile, unit_type_get(punit))) {
      tile_claim_bases(ptile, plr);
    }
  }
}

/****************************************************************************
  Load transporter status. This must be after _all_ units are loaded due to
  possible allied units on transports.
****************************************************************************/
static void player_load_units_transporter(struct player *plr,
                                          struct section_file *file)
{
  int nunits, i, plrno = player_index(plr);

  /* Copied from player_load_unit(). */
  fc_assert_exit_msg(secfile_lookup_int(file, &nunits,
                                        "player%d.nunits", plrno),
                     "%s", secfile_error());
  if (!plr->is_alive && nunits > 0) {
    nunits = 0; /* Some old savegames may be buggy. */
  }

  for (i = 0; i < nunits; i++) {
    int id_unit, id_trans;
    struct unit *punit, *ptrans;

    id_unit = secfile_lookup_int_default(file, -1,
                                         "player%d.u%d.id",
                                         plrno, i);
    punit = player_unit_by_number(plr, id_unit);
    fc_assert_action(punit != NULL, continue);

    id_trans = secfile_lookup_int_default(file, -1,
                                          "player%d.u%d.transported_by",
                                          plrno, i);
    if (id_trans == -1) {
      /* Not transported. */
      continue;
    }

    ptrans = game_unit_by_number(id_trans);
    fc_assert_action(id_trans == -1 || ptrans != NULL, continue);

    if (ptrans) {
      fc_assert_action(unit_transport_load(punit, ptrans, TRUE), continue);
    }
  }
}

/****************************************************************************
  Load all information about player "plrno" into the structure pointed to
  by "plr".
****************************************************************************/
static void player_load_main(struct player *plr, int plrno,
                             struct section_file *file,
                             const char *savefile_options,
                             const char **technology_order,
                             size_t technology_order_size)
{
  int i, k, c_s;
  const char *p;
  const char *name;
  struct government *gov;
  struct research *research;
  struct nation_type *pnation;
  struct nation_style *style;
  int old_barb_type;
  bool ai_controlled;

  research = research_get(plr);

  old_barb_type = secfile_lookup_int_default(file, 0,
                                             "player%d.ai.is_barbarian",
                                             plrno);

  plr->ai_common.barbarian_type = barb_type_convert(old_barb_type);

  if (is_barbarian(plr)) {
    server.nbarbarians++;
  }

  server_player_set_name(plr, secfile_lookup_str(file, "player%d.name",
                                                 plrno));
  sz_strlcpy(plr->username,
	     secfile_lookup_str_default(file, "", "player%d.username", plrno));
  plr->unassigned_user = (!strcmp(plr->username, ANON_USER_NAME));
  sz_strlcpy(plr->ranked_username,
	     secfile_lookup_str_default(file, "", "player%d.ranked_username", 
             plrno));
  plr->unassigned_ranked = (!strcmp(plr->ranked_username, ANON_USER_NAME));

  /* Nations stored by name. */
  p = secfile_lookup_str(file, "player%d.nation", plrno);
  if (p != NULL) {
    pnation = nation_by_rule_name(p);
  } else {
    pnation = NO_NATION_SELECTED;
  }

  if (pnation != NO_NATION_SELECTED) {

    /* 2.1 and earlier savegames have same nation for both barbarian players.
     * Reassign correct nations for such barbarians. */
    enum barbarian_type nat_barb_type = nation_barbarian_type(pnation);

    if ((is_sea_barbarian(plr) && nat_barb_type != SEA_BARBARIAN)
        || (is_land_barbarian(plr) && nat_barb_type != LAND_BARBARIAN)) {
      log_verbose("Reassigning barbarian nation for %s", player_name(plr));
      plr->nation = NO_NATION_SELECTED;
    } else {
      player_set_nation(plr, pnation);
    }
  } else {
    plr->nation = NO_NATION_SELECTED;
  }
  /* Nation may be unselected at this point; we check for this later and
   * reassign nations to players who don't have them. */

  init_tech(research, FALSE);
  /* We used to call give_initial_techs here, but that shouldn't be
   * necessary.  The savegame should already mark those techs as known.
   * give_initial_techs will crash if the nation is unset. */

  /* It is important that barbarian_type is loaded before
   * calling is_barbarian() and is_land_barbarian() */
  if (is_barbarian(plr) && plr->nation == NO_NATION_SELECTED) {
    if (is_land_barbarian(plr)) {
      pnation = pick_a_nation(NULL, FALSE, FALSE, LAND_BARBARIAN);
    } else {
      pnation = pick_a_nation(NULL, FALSE, FALSE, SEA_BARBARIAN);
    }
    player_set_nation(plr, pnation);
  }

  /* government */
  name = secfile_lookup_str(file, "player%d.government_name", plrno);
  if (!name) {
    log_fatal("Player%d: no government.", plrno);
    exit(EXIT_FAILURE);
  }
  gov = government_by_rule_name(name);
  if (gov == NULL) {
    log_fatal("Player%d: unsupported government \"%s\".", plrno, name);
    exit(EXIT_FAILURE);
  }
  plr->government = gov;

  /* Target government */
  name = secfile_lookup_str(file, "player%d.target_government_name", plrno);
  if (name) {
    gov = government_by_rule_name(name);
  } else {
    gov = NULL;
  }
  if (gov) {
    plr->target_government = gov;
  } else {
    /* Old servers didn't have this value. */
    plr->target_government = government_of_player(plr);
  }

  /* Multipliers: post-date version 1 savefiles, so use ruleset defaults */
  multipliers_iterate(pmul) {
    plr->multipliers[multiplier_index(pmul)]
      = plr->multipliers_target[multiplier_index(pmul)] = pmul->def;
  } multipliers_iterate_end;

  p = secfile_lookup_str(file, "player%d.city_style_by_name", plrno);
  if (!p) {
    char* old_order[4] = {"European", "Classical", "Tropical", "Asian"};

    c_s = secfile_lookup_int_default(file, 0, "player%d.city_style", plrno);
    if (c_s < 0 || c_s > 3) {
      log_error("Player%d: unsupported city_style %d. Changed to \"%s\".",
                plrno, c_s, old_order[0]);
      c_s = 0;
    }
    p = old_order[c_s];
  }
  style = style_by_rule_name(p);
  if (style == NULL) {
    style = style_by_number(0);
    log_error("Player%d: unsupported city_style_name \"%s\". "
              "Changed to \"%s\".", plrno, p, style_rule_name(style));
  }
  plr->style = style;

  plr->nturns_idle = 0;
  plr->is_male = secfile_lookup_bool_default(file, TRUE, "player%d.is_male", plrno);
  fc_assert_exit_msg(secfile_lookup_bool(file, &plr->is_alive,
                                         "player%d.is_alive", plrno),
                     "%s", secfile_error());
  plr->turns_alive = secfile_lookup_int_default(file, game.info.turn,
                                                "player%d.turns_alive", plrno);
  fc_assert_exit_msg(secfile_lookup_bool(file, &ai_controlled,
                                         "player%d.ai.control", plrno),
                     "%s", secfile_error());
  if (ai_controlled) {
    set_as_ai(player_by_number(plrno));
  } else {
    set_as_human(player_by_number(plrno));
  }
  plr->phase_done = FALSE;

  /* Backwards-compatibility: the tech goal value is still stored in the
   * "ai" section even though it was moved into the research struct. */
  research->tech_goal =
    technology_load(file, "player%d.ai.tech_goal", plrno);

  if (research->tech_goal == A_NONE
      || NULL == valid_advance_by_number(research->tech_goal)) {
    /* Old servers (1.14.1) saved both A_UNSET and A_NONE by 0
     * Here 0 means A_UNSET
     */
    research->tech_goal = A_UNSET;
  }
  /* Some sane defaults */
  plr->ai_common.fuzzy = 0;		 /* set later */
  plr->ai_common.expand = 100;		 /* set later */
  plr->ai_common.science_cost = 100;	 /* set later */
  plr->ai_common.skill_level =
    ai_level_convert(secfile_lookup_int_default(file, game.info.skill_level,
                                                "player%d.ai.skill_level", plrno));
  if (is_ai(plr)) {
    if (!ai_level_is_valid(plr->ai_common.skill_level)) {
      plr->ai_common.skill_level = ai_level_convert(GAME_OLD_DEFAULT_SKILL_LEVEL);
    }

    /* Set AI parameters */
    set_ai_level_directer(plr, plr->ai_common.skill_level);
  }

  fc_assert_exit_msg(secfile_lookup_int(file, &plr->economic.gold,
                                        "player%d.gold", plrno),
                     "%s", secfile_error());
  fc_assert_exit_msg(secfile_lookup_int(file, &plr->economic.tax,
                                        "player%d.tax", plrno),
                     "%s", secfile_error());
  fc_assert_exit_msg(secfile_lookup_int(file, &plr->economic.science,
                                        "player%d.science", plrno),
                     "%s", secfile_error());
  fc_assert_exit_msg(secfile_lookup_int(file, &plr->economic.luxury,
                                        "player%d.luxury", plrno),
                     "%s", secfile_error());

  plr->server.bulbs_last_turn =
    secfile_lookup_int_default(file, 0,
                               "player%d.bulbs_last_turn", plrno);

  /* The number of techs and future techs the player has
   * researched/acquired. */
  fc_assert_exit_msg(secfile_lookup_int(file, &research->techs_researched,
                                        "player%d.researchpoints", plrno),
                     "%s", secfile_error());
  fc_assert_exit_msg(secfile_lookup_int(file, &research->future_tech,
                                        "player%d.futuretech", plrno),
                     "%s", secfile_error());
  fc_assert_exit_msg(secfile_lookup_int(file, &research->bulbs_researched,
                                        "player%d.researched", plrno),
                     "%s", secfile_error());

  /* We use default values for bulbs_researching_saved, researching_saved,
   * and got_tech to preserve backwards-compatibility with save files
   * that didn't store this information.  But the local variables have
   * changed to reflect such minor differences. */
  research->bulbs_researching_saved =
    secfile_lookup_int_default(file, 0,
                               "player%d.researched_before", plrno);
  research->researching_saved =
    technology_load(file, "player%d.research_changed_from", plrno);

  research->researching =
    technology_load(file, "player%d.researching", plrno);

  if (research->researching == A_NONE
      || NULL == valid_advance_by_number(research->researching)) {
    /* Old servers (1.14.1) used to save A_FUTURE by 0 
     * This has to be interpreted from context because A_NONE was also
     * saved by 0
     */
    research->researching = A_FUTURE;
  }

  research->got_tech =
    secfile_lookup_bool_default(file, FALSE,
                                "player%d.research_got_tech", plrno);

  /* For new savegames using the technology_order[] list, any unknown
   * inventions are ignored.  Older games are more strictly enforced,
   * as an invalid index is probably indication of corruption.
   */
  p = secfile_lookup_str(file, "player%d.invs_new", plrno);
  if (!p) {
    log_fatal("Player %d: no inventions info", plrno);
    exit(EXIT_FAILURE);
  } else {
    for (k = 0; k < technology_order_size && p[k]; k++) {
      if (p[k] == '1') {
        struct advance *padvance = advance_by_rule_name(technology_order[k]);
        if (padvance) {
          research_invention_set(research, advance_number(padvance),
                                 TECH_KNOWN);
        }
      }
    }
  }

  fc_assert_exit_msg(secfile_lookup_bool(file, &plr->server.got_first_city,
                                         "player%d.capital", plrno),
                     "%s", secfile_error());

  {
    /* The old-style "revolution" value indicates the number of turns until
     * the revolution is complete, or 0 if there is no revolution.  The
     * new-style "revolution_finishes" value indicates the turn in which
     * the revolution will complete (which may be less than the current
     * turn) or -1 if there is no revolution. */
    int revolution = secfile_lookup_int_default(file, 0, "player%d.revolution",
						plrno);

    if (revolution == 0) {
      if (government_of_player(plr) != game.government_during_revolution) {
        revolution = -1;
      } else {
        /* some old savegames may be buggy */
        revolution = game.info.turn + 1;
      }
    } else {
      revolution = game.info.turn + revolution;
    }
    plr->revolution_finishes
      = secfile_lookup_int_default(file, revolution,
				   "player%d.revolution_finishes", plrno);
  }

  /* Unit statistics. */
  plr->score.units_built =
      secfile_lookup_int_default(file, 0, "player%d.units_built", plrno);
  plr->score.units_killed =
      secfile_lookup_int_default(file, 0, "player%d.units_killed", plrno);
  plr->score.units_lost =
      secfile_lookup_int_default(file, 0, "player%d.units_lost", plrno);

  { /* spacerace */
    struct player_spaceship *ship = &plr->spaceship;
    char prefix[32];
    const char *st;
    int ei;
    
    fc_snprintf(prefix, sizeof(prefix), "player%d.spaceship", plrno);
    spaceship_init(ship);
    fc_assert_exit_msg(secfile_lookup_int(file, &ei,
                                          "%s.state", prefix),
                       "%s", secfile_error());
    ship->state = ei;

    if (ship->state != SSHIP_NONE) {
      fc_assert_exit_msg(secfile_lookup_int(file, &ship->structurals,
                                            "%s.structurals", prefix),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &ship->components,
                                            "%s.components", prefix),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &ship->modules,
                                            "%s.modules", prefix),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &ship->fuel,
                                            "%s.fuel", prefix),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &ship->propulsion,
                                            "%s.propulsion", prefix),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &ship->habitation,
                                            "%s.habitation", prefix),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &ship->life_support,
                                            "%s.life_support", prefix),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &ship->solar_panels,
                                            "%s.solar_panels", prefix),
                         "%s", secfile_error());

      st = secfile_lookup_str(file, "%s.structure", prefix);
      for (i = 0; i < NUM_SS_STRUCTURALS; i++) {
	if (st[i] == '0') {
	  /* Already not set */
	} else if (st[i] == '1') {
	  BV_SET(ship->structure, i);
	} else {
	  log_error("invalid spaceship structure '%c' (%d)", st[i], st[i]);
	}
      }
      if (ship->state >= SSHIP_LAUNCHED) {
        fc_assert_exit_msg(secfile_lookup_int(file, &ship->launch_year,
                                              "%s.launch_year", prefix),
                           "%s", secfile_error());
      }
      spaceship_calc_derived(ship);
    }
  }

  BV_CLR_ALL(plr->real_embassy);
  if (has_capability("embassies2", savefile_options)) {
    /* done while loading diplstates data */
  } else if (has_capability("embassies", savefile_options)) {
    players_iterate(pother) {
      if (secfile_lookup_bool_default(file, FALSE, "player%d.embassy%d",
                                      plrno, player_index(pother))) {
        BV_SET(plr->real_embassy, player_index(pother));
      }
    } players_iterate_end;
  } else {
    /* Required for 2.0 and earlier savegames.  Remove eventually and make
     * the cap check mandatory. */
    int embassy = secfile_lookup_int_default(file, 0,
                                             "player%d.embassy", plrno);

    players_iterate(pother) {
      if (embassy & (1 << player_index(pother))) {
        BV_SET(plr->real_embassy, player_index(pother));
      }
    } players_iterate_end;
  }

  /* "Old" observer players will still be loaded but are considered dead. */
  players_iterate(aplayer) {
    struct player_diplstate *ds = player_diplstate_get(plr, aplayer);
    i = player_index(aplayer);

    /* ai data */
    plr->ai_common.love[i]
         = secfile_lookup_int_default(file, 1, "player%d.ai%d.love", plrno, i);

    /* diplomatic state */

    ds->type =
      secfile_lookup_int_default(file, DS_WAR,
                                 "player%d.diplstate%d.type", plrno, i);
    ds->max_state =
      secfile_lookup_int_default(file, DS_WAR,
                                 "player%d.diplstate%d.max_state", plrno, i);
    ds->first_contact_turn =
      secfile_lookup_int_default(file, 0,
                                 "player%d.diplstate%d.first_contact_turn", plrno, i);
    ds->turns_left =
      secfile_lookup_int_default(file, -2,
                                 "player%d.diplstate%d.turns_left", plrno, i);
    ds->has_reason_to_cancel =
      secfile_lookup_int_default(file, 0,
                                 "player%d.diplstate%d.has_reason_to_cancel",
                                 plrno, i);
    ds->contact_turns_left =
      secfile_lookup_int_default(file, 0,
                           "player%d.diplstate%d.contact_turns_left", plrno, i);

    if (has_capability("embassies2", savefile_options)
        && secfile_lookup_bool_default(file, FALSE,
                                       "player%d.diplstate%d.embassy",
                                       plrno, i)) {
      BV_SET(plr->real_embassy, player_index(aplayer));
    }
  } players_iterate_end;

  CALL_FUNC_EACH_AI(player_load, plr, file, plrno);
}

/****************************************************************************
  Load one tile of the city map. Returns FALSE if the end of the string is
  reached.

  TODO: Compatibility function for savegame of version 2.2.x. Remove if
        backwards compatibility is removed.
****************************************************************************/
/* enum city_tile_type string constants for savegame */
#define S_TILE_EMPTY		'0'
#define S_TILE_WORKER		'1'
#define S_TILE_UNAVAILABLE	'2'
#define S_TILE_UNKNOWN		'?'
static bool player_load_city_tile_S22(int plrno, int i, struct city *pcity,
                                      int x, int y, char tile_status,
                                      int *citizen_count)
{
  int radius_sq = city_map_radius_sq_get(pcity);
  struct city *pwork = NULL;
  struct tile *ptile = is_valid_city_coords(radius_sq, x, y)
                       ? city_map_to_tile(city_tile(pcity), radius_sq, x, y)
                       : NULL;

  switch (tile_status) {
  case '\0':
    /* too short, will yield very odd results! */
    return FALSE;
    break;

  case S_TILE_EMPTY:
    if (NULL == ptile) {
      log_worker("player%d.c%d.workers {%d, %d} '%c' not valid for "
                 "(%d, %d) \"%s\"[%d], ignoring",
                 plrno, i, x, y, tile_status,
                 TILE_XY(city_tile(pcity)), city_name(pcity),
                 city_size_get(pcity));
    } else if (NULL != (pwork = tile_worked(ptile))) {
      log_worker("player%d.c%d.workers {%d, %d} '%c' conflict at "
                 "(%d, %d) for (%d ,%d) \"%s\"[%d] with (%d, %d) "
                 "\"%s\"[%d], converting to unavailable",
                 plrno, i, x, y, tile_status, TILE_XY(ptile),
                 TILE_XY(city_tile(pcity)), city_name(pcity),
                 city_size_get(pcity),
                 TILE_XY(city_tile(pwork)), city_name(pwork),
                 city_size_get(pwork));
    }
    break;

  case S_TILE_WORKER:
    if (NULL == ptile) {
      log_worker("player%d.c%d.workers {%d, %d} '%c' not valid for "
                 "(%d, %d) \"%s\"[%d], ignoring",
                 plrno, i, x, y, tile_status,
                 TILE_XY(city_tile(pcity)), city_name(pcity),
                 city_size_get(pcity));
    } else if (NULL != (pwork = tile_worked(ptile))) {
      log_worker("player%d.c%d.workers {%d, %d} '%c' conflict at "
                 "(%d, %d) for (%d, %d) \"%s\"[%d] with (%d, %d) "
                 "\"%s\"[%d], converting to default specialist",
                 plrno, i, x, y, tile_status, TILE_XY(ptile),
                 TILE_XY(city_tile(pcity)), city_name(pcity),
                 city_size_get(pcity),
                 TILE_XY(city_tile(pwork)), city_name(pwork),
                 city_size_get(pwork));

      pcity->specialists[DEFAULT_SPECIALIST]++;
      auto_arrange_workers(pcity);
      (*citizen_count)++;
    } else {
      tile_set_worked(ptile, pcity);
      (*citizen_count)++;
    }
    break;

  case S_TILE_UNAVAILABLE:
    /* nothing */
    break;

  case S_TILE_UNKNOWN:
    if (NULL == ptile) {
      /* already set C_TILE_UNUSABLE */
    } else {
      log_worker("player%d.c%d.workers {%d, %d} '%c' not valid at "
                 "(%d, %d) for (%d, %d) \"%s\"[%d], converting to "
                 "unavailable", plrno, i, x, y, tile_status, TILE_XY(ptile),
                 TILE_XY(city_tile(pcity)), city_name(pcity), city_size_get(pcity));
    }
    break;

  default:
    log_worker("player%d.c%d.workers {%d, %d} '%c' not valid for "
               "(%d, %d) \"%s\"[%d], ignoring", plrno, i, x, y, tile_status,
               TILE_XY(city_tile(pcity)), city_name(pcity), city_size_get(pcity));
    break;
  };

  return TRUE;
}
#undef S_TILE_EMPTY
#undef S_TILE_WORKER
#undef S_TILE_UNAVAILABLE
#undef S_TILE_UNKNOWN

/****************************************************************************
  Load map of tiles worked by cities. The return pointer has to be freed by
  the caller.
****************************************************************************/
static int *player_load_cities_worked_map(struct section_file *file,
                                          const char *savefile_options)
{
  int *worked_tiles = NULL;
  int x, y;

  if (!has_capability("tile_worked", savefile_options)) {
    return NULL;
  }

  worked_tiles = fc_malloc(MAP_INDEX_SIZE * sizeof(*worked_tiles));

  for (y = 0; y < game.map.ysize; y++) {
    const char *buffer = secfile_lookup_str(file, "map.worked%03d", y);
    const char *ptr = buffer;

    fc_assert_exit_msg(NULL != buffer,
                       "Savegame corrupt - map line %d not found.", y);
    for (x = 0; x < game.map.xsize; x++) {
      char token[TOKEN_SIZE];
      int number;
      struct tile *ptile = native_pos_to_tile(x, y);

      scanin(&ptr, ",", token, sizeof(token));
      fc_assert_exit_msg('\0' != token[0],
                         "Savegame corrupt - map size not correct.");
      if (strcmp(token, "-") == 0) {
        number = -1;
      } else {
        fc_assert_exit_msg(str_to_int(token, &number) && 0 < number,
                           "Savegame corrupt - got tile worked by city "
                           "id=%s in (%d, %d).", token, x, y);
      }

      worked_tiles[ptile->index] = number;
    }
  }

  return worked_tiles;
}

/****************************************************************************
  Load the cities of a player.
****************************************************************************/
static void player_load_cities(struct player *plr, int plrno,
                               struct section_file *file,
                               const char *savefile_options,
                               const char **improvement_order,
                               size_t improvement_order_size,
                               int *worked_tiles)
{
  char named[MAX_LEN_NAME], citystr[32];
  struct player *past;
  struct city *pcity;
  const char *kind;
  const char *name;
  const char *p;
  int id, i, j, k;
  int ncities;

  fc_assert_exit_msg(secfile_lookup_int(file, &ncities,
                                        "player%d.ncities", plrno),
                     "%s", secfile_error());

  if (!plr->is_alive && ncities > 0) {
    log_error("player%d.ncities=%d for dead player!", plrno, ncities);
    ncities = 0; /* Some old savegames may be buggy. */
  }

  if (!plr->server.got_first_city && ncities > 0) {
    /* Probably barbarians in an old savegame; fix up */
    plr->server.got_first_city = TRUE;
  }

  for (i = 0; i < ncities; i++) { /* read the cities */
    int sp_count = 0, workers = 0;
    int nat_x, nat_y;
    struct tile *pcenter;

    fc_assert_exit_msg(secfile_lookup_int(file, &nat_x, "player%d.c%d.x",
                                          plrno, i),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &nat_y,
                                          "player%d.c%d.y", plrno, i),
                       "%s", secfile_error());

    pcenter = native_pos_to_tile(nat_x, nat_y);
    if (NULL == pcenter) {
      log_fatal("player%d.c%d invalid tile (%d, %d)",
                plrno, i, nat_x, nat_y);
      exit(EXIT_FAILURE);
    }

    if (NULL != tile_city(pcenter)) {
      log_fatal("player%d.c%d duplicate city (%d, %d)",
                plrno, i, nat_x, nat_y);
      exit(EXIT_FAILURE);
    }

    /* lookup name out of order */
    fc_snprintf(named, sizeof(named), "player%d.c%d.name", plrno, i);
    /* instead of dying, use name string for damaged name */
    name = secfile_lookup_str_default(file, named, "%s", named);
    /* copied into city->name */
    pcity = create_city_virtual(plr, pcenter, name);
    adv_city_alloc(pcity);
    citizens_init(pcity);

    fc_assert_exit_msg(secfile_lookup_int(file, &pcity->id,
                                          "player%d.c%d.id", plrno, i),
                       "%s", secfile_error());
    identity_number_reserve(pcity->id);
    idex_register_city(pcity);

    id = secfile_lookup_int_default(file, plrno,
				    "player%d.c%d.original", plrno, i);
    past = player_by_number(id);
    if (NULL != past) {
      pcity->original = past;
    }

    /* no city_choose_build_default(), values loaded below! */

    {
      int value;
      citizens size;
      fc_assert_exit_msg(secfile_lookup_int(file, &value,
                                            "player%d.c%d.size", plrno, i),
                         "%s", secfile_error());
      size = (citizens)value; /* set the correct type */
      if (value != (int)size) {
        log_error("Invalid city size: %d; set to %d.", value, size);
      }
      city_size_set(pcity, size);
    }

    specialist_type_iterate(sp) {
      int value;
      fc_assert_exit_msg(secfile_lookup_int(file, &value,
                                            "player%d.c%d.n%s", plrno, i,
                                            specialist_rule_name
                                            (specialist_by_number(sp))),
                         "%s", secfile_error());
      pcity->specialists[sp] = (citizens)value; /* set the correct type */
      if (value != (int)pcity->specialists[sp]) {
        log_error("Invalid number of specialists: %d; set to %d.", value,
                  pcity->specialists[sp]);
      }
      sp_count += pcity->specialists[sp];
    } specialist_type_iterate_end;

    for (j = 0; j < MAX_TRADE_ROUTES; j++) {
      int partner = secfile_lookup_int_default(file, 0,
                                               "player%d.c%d.traderoute%d",
                                               plrno, i, j);
      if (partner != 0) {
        struct trade_route *proute = fc_malloc(sizeof(struct trade_route));

        proute->partner = partner;
        proute->dir = RDIR_BIDIRECTIONAL;
        proute->goods = goods_by_number(0); /* First good */

        trade_route_list_append(pcity->routes, proute);
      }
    }

    fc_assert_exit_msg(secfile_lookup_int(file, &pcity->food_stock,
                                          "player%d.c%d.food_stock",
                                          plrno, i),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &pcity->shield_stock,
                                          "player%d.c%d.shield_stock",
                                          plrno, i),
                       "%s", secfile_error());

    pcity->airlift =
      secfile_lookup_int_default(file, 0, "player%d.c%d.airlift", plrno,i);
    pcity->was_happy =
      secfile_lookup_bool_default(file, FALSE, "player%d.c%d.was_happy",
                                  plrno,i);
    pcity->turn_plague =
      secfile_lookup_int_default(file, 0, "player%d.c%d.turn_plague",
                                 plrno,i);
    fc_assert_exit_msg(secfile_lookup_int(file, &pcity->anarchy,
                                          "player%d.c%d.anarchy", plrno, i),
                       "%s", secfile_error());
    pcity->rapture =
      secfile_lookup_int_default(file, 0, "player%d.c%d.rapture",
                                 plrno,i);
    pcity->server.steal =
      secfile_lookup_int_default(file, 0, "player%d.c%d.steal",
                                 plrno,i);

    /* before did_buy for undocumented hack */
    pcity->turn_founded =
      secfile_lookup_int_default(file, -2, "player%d.c%d.turn_founded",
                                 plrno, i);
    fc_assert_exit_msg(secfile_lookup_int(file, &j, "player%d.c%d.did_buy",
                                          plrno, i),
                       "%s", secfile_error());
    pcity->did_buy = (j != 0);
    if (j == -1 && pcity->turn_founded == -2) {
      /* undocumented hack */
      pcity->turn_founded = game.info.turn;
    }
    pcity->did_sell =
      secfile_lookup_bool_default(file, FALSE, "player%d.c%d.did_sell", plrno,i);

    if (has_capability("turn_last_built", savefile_options)) {
      fc_assert_exit_msg(secfile_lookup_int(file, &pcity->turn_last_built,
                                            "player%d.c%d.turn_last_built",
                                            plrno, i),
                         "%s", secfile_error());
    } else {
      /* Before, turn_last_built was stored as a year.  There is no easy
       * way to convert this into a turn value. */
      pcity->turn_last_built = 0;
    }

    kind = secfile_lookup_str(file, "player%d.c%d.currently_building_kind",
                              plrno, i);
    if (!kind) {
      /* before 2.2.0 unit production was indicated by flag. */
      bool is_unit = secfile_lookup_bool_default(file, FALSE,
						 "player%d.c%d.is_building_unit",
						 plrno, i);
      kind = universals_n_name(is_unit ? VUT_UTYPE : VUT_IMPROVEMENT);
    }

    name = secfile_lookup_str(file, "player%d.c%d.currently_building_name",
                              plrno, i);
    if (!name) {
      log_fatal("Player %d, city %d: No production name", plrno, i);
      exit(EXIT_FAILURE);
    }
    pcity->production = universal_by_rule_name(kind, name);
    if (pcity->production.kind == universals_n_invalid()) {
      log_fatal("player%d.c%d.currently_building: unknown \"%s\" \"%s\".",
                plrno, i, kind, name);
      exit(EXIT_FAILURE);
    }

    kind = secfile_lookup_str(file, "player%d.c%d.changed_from_kind",
                              plrno, i);
    if (!kind) {
      /* before 2.2.0 unit production was indicated by flag. */
      bool is_unit = secfile_lookup_bool_default(file, FALSE,
						 "player%d.c%d.changed_from_is_unit",
						 plrno, i);
      kind = universals_n_name(is_unit ? VUT_UTYPE : VUT_IMPROVEMENT);
    }

    name = secfile_lookup_str(file, "player%d.c%d.changed_from_name",
                              plrno, i);
    if (!name) {
      log_fatal("Player %d, city %d: No old production name",
                plrno, i);
      exit(EXIT_FAILURE);
    }
    pcity->changed_from = universal_by_rule_name(kind, name);
    if (pcity->changed_from.kind == universals_n_invalid()) {
      log_fatal("player%d.c%d.changed_from: unknown \"%s\" \"%s\".",
                plrno, i, kind, name);
      exit(EXIT_FAILURE);
    }

    pcity->before_change_shields =
      secfile_lookup_int_default(file, pcity->shield_stock,
				 "player%d.c%d.before_change_shields", plrno, i);
    pcity->caravan_shields =
      secfile_lookup_int_default(file, 0,
				 "player%d.c%d.caravan_shields", plrno, i);
    pcity->disbanded_shields =
      secfile_lookup_int_default(file, 0,
				 "player%d.c%d.disbanded_shields", plrno, i);
    pcity->last_turns_shield_surplus =
      secfile_lookup_int_default(file, 0,
				 "player%d.c%d.last_turns_shield_surplus",
				 plrno, i);

    pcity->style = city_style(pcity);

    pcity->server.synced = FALSE; /* must re-sync with clients */

    /* Fix for old buggy savegames. */
    if (!has_capability("known32fix", savefile_options)
        && plrno >= 16) {
      city_tile_iterate(city_map_radius_sq_get(pcity), pcenter, tile1) {
        map_set_known(tile1, plr);
      } city_tile_iterate_end;
    }

    city_freeze_workers(pcity);

    if (worked_tiles == NULL) {
      /* 2.2.x savegame */
#define CITY_MAP_OLD_RADIUS 2
#define CITY_MAP_OLD_SIZE (CITY_MAP_OLD_RADIUS * 2 + 1)
      city_map_radius_sq_set(pcity, CITY_MAP_OLD_RADIUS
                                    * CITY_MAP_OLD_RADIUS + 1);
      p = secfile_lookup_str_default(file, "", "player%d.c%d.workers",
                                     plrno, i);

      if (strlen(p) != CITY_MAP_OLD_SIZE * CITY_MAP_OLD_SIZE) {
        /* FIXME: somehow need to rearrange */
        log_worker("player%d.c%d.workers length %lu not equal city"
                   " map size %lu, needs rearranging!", plrno, i,
                   (unsigned long)strlen(p),
                   (unsigned long)(CITY_MAP_OLD_SIZE * CITY_MAP_OLD_SIZE));
      }

      bool ok = TRUE;
      /* start the loop at offset xy_min compared to the new max
       * city radius and end at xy_max */
      int xy_min = CITY_MAP_MAX_RADIUS - CITY_MAP_OLD_RADIUS;
      int xy_max = CITY_MAP_MAX_RADIUS + CITY_MAP_OLD_RADIUS + 1;
      int x, y;
      for(y = xy_min; y < xy_max; y++) {
        if (!ok) {
          break;
        }

        for(x = xy_min; x < xy_max; x++) {
          if (ok) {
            ok = player_load_city_tile_S22(plrno, i, pcity, x, y, *p,
                                           &workers);
            p++;
          }
        }
      }
#undef CITY_MAP_OLD_SIZE
#undef CITY_MAP_OLD_RADIUS
    } else {
      /* load new savegame with variable (squared) city radius and
       * worked tiles map */

      /* FIXME: Dummy to mark this value as used. It is needed to prevent
       *        old servers from crashing if a new savegame is loaded. */
      (void) secfile_entry_lookup(file, "player%d.c%d.workers", plrno, i);

      int radius_sq
        = secfile_lookup_int_default(file, -1, "player%d.c%d.city_radius_sq",
                                     plrno, i);
      city_map_radius_sq_set(pcity, radius_sq);

      city_tile_iterate(radius_sq, city_tile(pcity), ptile) {
        if (worked_tiles[ptile->index] == pcity->id) {
          tile_set_worked(ptile, pcity);
          workers++;

#ifdef DEBUG
          /* set this tile to unused; a check for not resetted tiles is
           * included in game_load_internal() */
          worked_tiles[ptile->index] = -1;
#endif /* DEBUG */
        }
      } city_tile_iterate_end;
    }

    if (tile_worked(pcenter) != pcity) {
      struct city *pwork = tile_worked(pcenter);

      if (NULL != pwork) {
        log_error("[player%d.c%d] city center of '%s' (%d,%d) [%d] is "
                  "worked by '%s' (%d,%d) [%d]; repairing ", plrno, i,
                  city_name(pcity), TILE_XY(pcenter), city_size_get(pcity),
                  city_name(pwork), TILE_XY(city_tile(pwork)),
                  city_size_get(pwork));

        tile_set_worked(pcenter, NULL); /* remove tile from pwork */
        pwork->specialists[DEFAULT_SPECIALIST]++;
        auto_arrange_workers(pwork);
      } else {
        log_error("[player%d.c%d] city center of '%s' (%d,%d) [%d] is "
                  "empty; repairing ", plrno, i, city_name(pcity),
                  TILE_XY(pcenter), city_size_get(pcity));
      }

      /* repair pcity */
      tile_set_worked(pcenter, pcity);
      city_repair_size(pcity, -1);
    }

    k = city_size_get(pcity) - sp_count - (workers - FREE_WORKED_TILES);
    if (0 != k) {
      log_error("[player%d.c%d] size mismatch for '%s' (%d,%d): "
                "size [%d] != (workers [%d] - free worked tiles [%d]) + "
                "specialists [%d]",
                plrno, i, city_name(pcity), TILE_XY(pcenter),
                city_size_get(pcity),
                workers, FREE_WORKED_TILES, sp_count);

      /* repair pcity */
      city_repair_size(pcity, k);
    }

    /* Initialise list of improvements with City- and Building-wide
       equiv_ranges */
    for (k = 0; k < ARRAY_SIZE(pcity->built); k++) {
      pcity->built[k].turn = I_NEVER;
    }

    /* For new savegames using the improvement_order[] list, any unknown
     * improvements are ignored.  Older games are more strictly enforced,
     * as an invalid index is probably indication of corruption.
     */
    p = secfile_lookup_str(file, "player%d.c%d.improvements_new",
                           plrno, i);
    if (!p) {
      log_fatal("Player %d, city %d: No buildings information",
                plrno, i);
      exit(EXIT_FAILURE);
    } else {
      for (k = 0; k < improvement_order_size && p[k]; k++) {
        if (p[k] == '1') {
          struct impr_type *pimprove =
              improvement_by_rule_name(improvement_order[k]);
          if (pimprove) {
            city_add_improvement(pcity, pimprove);
          }
        }
      }
    }

    /* worklist_init() done in create_city_virtual() */
    worklist_load(file, &pcity->worklist, "player%d.c%d", plrno, i);

    /* after 2.1.0 new options format.  Old options are lost on upgrade. */
    BV_CLR_ALL(pcity->city_options);
    for (j = 0; j < CITYO_LAST; j++) {
      if (secfile_lookup_bool_default(file, FALSE,
				      "player%d.c%d.option%d",
				      plrno, i, j)) {
	BV_SET(pcity->city_options, j);
      }
    }

    /* Create new citizens info, since these old savegames do not contain
     * it. */
    citizens_update(pcity, NULL);

    fc_snprintf(citystr, sizeof(citystr), "player%d.c%d", plrno, i);
    CALL_FUNC_EACH_AI(city_load, file, pcity, citystr);

    /* After everything is loaded, but before vision. */
    map_claim_ownership(pcenter, plr, pcenter, TRUE);

    /* adding the city contribution to fog-of-war */
    pcity->server.vision = vision_new(plr, pcenter);
    vision_reveal_tiles(pcity->server.vision, game.server.vision_reveal_tiles);
    city_refresh_vision(pcity);

    city_list_append(plr->cities, pcity);
  }
}

/****************************************************************************
  Load player custom (client specific) attribute data
****************************************************************************/
static void player_load_attributes(struct player *plr, int plrno,
				   struct section_file *file)
{
  /* Toss any existing attribute_block (should not exist) */
  if (plr->attribute_block.data) {
    free(plr->attribute_block.data);
    plr->attribute_block.data = NULL;
  }

  /* This is a big heap of opaque data for the client, check everything! */
  plr->attribute_block.length = secfile_lookup_int_default(
      file, 0, "player%d.attribute_v2_block_length", plrno);

  if (0 > plr->attribute_block.length) {
    log_error("player%d.attribute_v2_block_length=%d too small", plrno,
              plr->attribute_block.length);
    plr->attribute_block.length = 0;
  } else if (MAX_ATTRIBUTE_BLOCK < plr->attribute_block.length) {
    log_error("player%d.attribute_v2_block_length=%d too big (max %d)",
              plrno, plr->attribute_block.length, MAX_ATTRIBUTE_BLOCK);
    plr->attribute_block.length = 0;
  } else if (0 < plr->attribute_block.length) {
    int part_nr, parts;
    size_t actual_length;
    int quoted_length;
    char *quoted;

    plr->attribute_block.data = fc_malloc(plr->attribute_block.length);

    fc_assert_exit_msg(secfile_lookup_int(file, &quoted_length,
                       "player%d.attribute_v2_block_length_quoted", plrno),
                       "%s", secfile_error());
    quoted = fc_malloc(quoted_length + 1);
    quoted[0] = '\0';

    fc_assert_exit_msg(secfile_lookup_int(file, &parts,
                       "player%d.attribute_v2_block_parts", plrno),
                       "%s", secfile_error());

    for (part_nr = 0; part_nr < parts; part_nr++) {
      const char *current =
          secfile_lookup_str(file, "player%d.attribute_v2_block_data.part%d",
                             plrno, part_nr);
      if (!current) {
        log_error("attribute_v2_block_parts=%d actual=%d", parts, part_nr);
        break;
      }
      log_debug("attribute_v2_block_length_quoted=%lu have=%lu part=%lu",
                (unsigned long) quoted_length,
                (unsigned long) strlen(quoted),
                (unsigned long) strlen(current));
      fc_assert(strlen(quoted) + strlen(current) <= quoted_length);
      strcat(quoted, current);
    }
    fc_assert_msg(quoted_length == strlen(quoted),
                  "attribute_v2_block_length_quoted=%lu actual=%lu",
                  (unsigned long) quoted_length,
                  (unsigned long) strlen(quoted));

    actual_length =
	unquote_block(quoted,
		      plr->attribute_block.data,
		      plr->attribute_block.length);
    fc_assert(actual_length == plr->attribute_block.length);
    free(quoted);
  }
}

/****************************************************************************
  Load the private vision map for fog of war
****************************************************************************/
static void player_load_vision(struct player *plr, int plrno,
                               struct section_file *file,
                               const char *savefile_options,
                               const enum tile_special_type *special_order,
                               size_t num_special_types,
                               const char **improvement_order,
                               size_t improvement_order_size,
                               struct base_type **base_order,
                               size_t num_base_types)
{
  const char *p;
  int i, k, id;
  int nat_x, nat_y;
  int total_ncities =
    secfile_lookup_int_default(file, -1, "player%d.total_ncities", plrno);

  if (!plr->is_alive)
    map_know_and_see_all(plr);

  /* load map if:
   * - it from a fog of war build;
   * - fog of war was on (otherwise the private map wasn't saved);
   * - is not from a "unit only" fog of war save file;
   * - game.save_private_map is not set to FALSE in the scenario / savegame
   */
  if (NULL != secfile_entry_by_path(file, "game.fogofwar")
      && game.info.fogofwar
      && -1 != total_ncities
      && secfile_lookup_bool_default(file, TRUE, "game.save_private_map")) {
    LOAD_MAP_DATA(ch, vnat_y, ptile,
                  secfile_lookup_str(file, "player%d.map_t%03d",
                                     plrno, vnat_y),
                  map_get_player_tile(ptile, plr)->terrain =
                  char2terrain(ch));

    if (special_order) {
      LOAD_MAP_DATA(ch, vnat_y, ptile,
	secfile_lookup_str(file, "player%d.map_res%03d", plrno, vnat_y),
	map_get_player_tile(ptile, plr)->resource
		    = identifier_to_resource(ch));

      special_halfbyte_iterate(j, num_special_types) {
	char buf[32]; /* enough for sprintf() below */
	sprintf (buf, "player%d.map_spe%02d_%%03d", plrno, j);
	LOAD_MAP_DATA(ch, vnat_y, ptile,
                      secfile_lookup_str(file, buf, vnat_y),
                      set_savegame_special(ptile, &map_get_player_tile(ptile, plr)->extras,
                                 ch, special_order + 4 * j));
      } special_halfbyte_iterate_end;
    } else {
      /* get 4-bit segments of 12-bit "special" field. */
      LOAD_MAP_DATA(ch, vnat_y, ptile,
                    secfile_lookup_str(file, "player%d.map_l%03d", plrno, vnat_y),
                    set_savegame_special(ptile, &map_get_player_tile(ptile, plr)->extras,
                                         ch, default_specials + 0));
      LOAD_MAP_DATA(ch, vnat_y, ptile,
                    secfile_lookup_str(file, "player%d.map_u%03d", plrno, vnat_y),
                    set_savegame_special(ptile, &map_get_player_tile(ptile, plr)->extras,
                                         ch, default_specials + 4));
      LOAD_MAP_DATA(ch, vnat_y, ptile,
                    secfile_lookup_str_default (file, NULL, "player%d.map_n%03d",
                                                plrno, vnat_y),
                    set_savegame_special(ptile, &map_get_player_tile(ptile, plr)->extras,
                                         ch, default_specials + 8));
      LOAD_MAP_DATA(ch, vnat_y, ptile,
                    secfile_lookup_str(file, "map.l%03d", vnat_y),
                    set_savegame_old_resource(&map_get_player_tile(ptile, plr)->resource,
                                              ptile->terrain,
                                              &map_get_player_tile(ptile, plr)->extras, ch, 0));
      LOAD_MAP_DATA(ch, vnat_y, ptile,
                    secfile_lookup_str(file, "map.n%03d", vnat_y),
                    set_savegame_old_resource(&map_get_player_tile(ptile, plr)->resource,
                                              ptile->terrain,
                                              &map_get_player_tile(ptile, plr)->extras, ch, 1));
    }

    if (has_capability("bases", savefile_options)) {
      char zeroline[game.map.xsize + 1];
      int xi;

      /* This is needed when new bases has been added to ruleset, and
       * thus game.control.num_base_types is greater than, when game was saved. */
      for(xi = 0; xi < game.map.xsize; xi++) {
        zeroline[xi] = '0';
      }
      zeroline[xi]= '\0';

      bases_halfbyte_iterate(j, num_base_types) {
        char buf[32]; /* should be enough for snprintf() below */

        fc_snprintf(buf, sizeof(buf), "player%d.map_b%02d_%%03d", plrno, j);

        LOAD_MAP_DATA(ch, vnat_y, ptile,
                      secfile_lookup_str_default(file, zeroline, buf, vnat_y),
                      set_savegame_bases(&map_get_player_tile(ptile, plr)->extras,
                                         ch, base_order + 4 * j));
      } bases_halfbyte_iterate_end;
    } else {
      /* Already loaded fortresses and airbases as part of specials */
    }

    if (game.server.foggedborders) {
      LOAD_MAP_DATA(ch, vnat_y, ptile,
                    secfile_lookup_str(file, "player%d.map_owner%03d", plrno, vnat_y),
                    map_get_player_tile(ptile, plr)->owner = identifier_to_player(ch));
    }

    /* get 4-bit segments of 16-bit "updated" field */
    LOAD_MAP_DATA(ch, vnat_y, ptile,
                  secfile_lookup_str
                  (file, "player%d.map_ua%03d", plrno, vnat_y),
		  map_get_player_tile(ptile, plr)->last_updated =
                  ascii_hex2bin(ch, 0));
    LOAD_MAP_DATA(ch, vnat_y, ptile,
                  secfile_lookup_str
                  (file, "player%d.map_ub%03d", plrno, vnat_y),
                  map_get_player_tile(ptile, plr)->last_updated |=
                  ascii_hex2bin(ch, 1));
    LOAD_MAP_DATA(ch, vnat_y, ptile,
                  secfile_lookup_str
                  (file, "player%d.map_uc%03d", plrno, vnat_y),
                  map_get_player_tile(ptile, plr)->last_updated |=
                  ascii_hex2bin(ch, 2));
    LOAD_MAP_DATA(ch, vnat_y, ptile,
                  secfile_lookup_str
                  (file, "player%d.map_ud%03d", plrno, vnat_y),
                  map_get_player_tile(ptile, plr)->last_updated |=
                  ascii_hex2bin(ch, 3));

    for (i = 0; i < total_ncities; i++) {
      /* similar to vision_site_new() */
      struct vision_site *pdcity = vision_site_new(0, NULL, NULL);

      fc_assert_exit_msg(secfile_lookup_int(file, &nat_x, "player%d.dc%d.x",
                                            plrno, i),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &nat_y, "player%d.dc%d.y",
                                            plrno, i),
                         "%s", secfile_error());
      pdcity->location = native_pos_to_tile(nat_x, nat_y);
      if (NULL == pdcity->location) {
        log_error("player%d.dc%d invalid tile (%d,%d)",
                  plrno, i, nat_x, nat_y);
        free(pdcity);
        continue;
      }

      fc_assert_exit_msg(secfile_lookup_int(file, &pdcity->identity,
                                            "player%d.dc%d.id", plrno, i),
                         "%s", secfile_error());
      if (IDENTITY_NUMBER_ZERO >= pdcity->identity) {
        log_error("player%d.dc%d has invalid id (%d); skipping.",
                  plrno, i, pdcity->identity);
        free(pdcity);
        continue;
      }

      fc_assert_exit_msg(secfile_lookup_int(file, &id, "player%d.dc%d.owner",
                                            plrno, i),
                         "%s", secfile_error());
      if (id == plrno) {
        /* Earlier versions redundantly saved the dummy for their own cities.
         * Since 2.2.0, map_claim_ownership() rebuilds them at city load time.
         */
        log_verbose("player%d.dc%d has same owner (%d); skipping.",
                    plrno, i, id);
        free(pdcity);
        continue;
      }

      pdcity->owner = player_by_number(id);
      if (NULL == vision_site_owner(pdcity)) {
        log_error("player%d.dc%d has invalid owner (%d); skipping.",
                  plrno, i, id);
        free(pdcity);
        continue;
      }

      {
        int size;
        citizens city_size;
        fc_assert_exit_msg(secfile_lookup_int(file, &size,
                                              "player%d.dc%d.size", plrno, i),
                         "%s", secfile_error());
        city_size = (citizens)size; /* set the correct type */
        if (size != (int)city_size) {
          log_error("Invalid city size: %d; set to %d.", size, city_size);
        }
        vision_site_size_set(pdcity, size);
      }

      pdcity->occupied = secfile_lookup_bool_default(file, FALSE,
                                      "player%d.dc%d.occupied", plrno, i);
      pdcity->walls = secfile_lookup_bool_default(file, FALSE,
                                      "player%d.dc%d.walls", plrno, i);
      pdcity->happy = secfile_lookup_bool_default(file, FALSE,
                                      "player%d.dc%d.happy", plrno, i);
      pdcity->unhappy = secfile_lookup_bool_default(file, FALSE,
                                      "player%d.dc%d.unhappy", plrno, i);

      pdcity->style = 0;

      /* Old savegames never have real city image information, so set
       * it to special value "server doesn't know" */
      pdcity->city_image = -100;

      /* Initialise list of improvements */
      BV_CLR_ALL(pdcity->improvements);

      p = secfile_lookup_str(file, "player%d.dc%d.improvements",
                             plrno, i);
      if (!p) {
        /* old savegames */
      } else {
        for (k = 0; k < improvement_order_size && p[k]; k++) {
          if (p[k] == '1') {
            struct impr_type *pimprove =
                improvement_by_rule_name(improvement_order[k]);
            if (pimprove) {
              BV_SET(pdcity->improvements, improvement_index(pimprove));
            }
          }
        }
      }
      sz_strlcpy(pdcity->name, secfile_lookup_str(file, "player%d.dc%d.name", plrno, i));

      change_playertile_site(map_get_player_tile(pdcity->location, plr),
                             pdcity);
      identity_number_reserve(pdcity->identity);
    }

    /* Repair inconsistent player maps. There was a bug in some pre-1.11
       savegames, and possibly other versions, and border support changed
       from time to time. Anyway, it shouldn't hurt. */
    whole_map_iterate(ptile) {
      if (map_is_known_and_seen(ptile, plr, V_MAIN)) {
	struct city *pcity = tile_city(ptile);

	update_player_tile_knowledge(plr, ptile);
	reality_check_city(plr, ptile);

	if (NULL != pcity) {
	  update_dumb_city(plr, pcity);
	}
      }
    } whole_map_iterate_end;
  } else {
    /* We have an old savegame or fog of war was turned off; the
       players private knowledge is set to be what he could see
       without fog of war */
    whole_map_iterate(ptile) {
      if (map_is_known(ptile, plr)) {
	struct city *pcity = tile_city(ptile);

	update_player_tile_last_seen(plr, ptile);
	update_player_tile_knowledge(plr, ptile);

	if (NULL != pcity) {
	  update_dumb_city(plr, pcity);
	}
      }
    } whole_map_iterate_end;
  }
}

/***************************************************************
 For each city and tile, sort unit lists according to
 ord_city and ord_map values.
***************************************************************/
static void apply_unit_ordering(void)
{
  players_iterate(pplayer) {
    city_list_iterate(pplayer->cities, pcity) {
      unit_list_sort_ord_city(pcity->units_supported);
    }
    city_list_iterate_end;
  } players_iterate_end;

  whole_map_iterate(ptile) {
    unit_list_sort_ord_map(ptile->units);
  } whole_map_iterate_end;
}

/***************************************************************
  Called only in server/stdinhand.c load_command()
  Entire ruleset is always sent afterward.
***************************************************************/
void legacy_game_load(struct section_file *file)
{
  bool was_send_city_suppressed = send_city_suppression(TRUE);
  bool was_send_tile_suppressed = send_tile_suppression(TRUE);

  game_load_internal(file);
  /* load script state last so we have access to all game data */
  script_server_state_load(file);

  send_tile_suppression(was_send_tile_suppressed);
  send_city_suppression(was_send_city_suppressed);
}

/***************************************************************
  Real game_load function.
***************************************************************/
static void game_load_internal(struct section_file *file)
{
  int i, k;
  int game_version;
  enum server_states tmp_server_state;
  RANDOM_STATE rstate;
  const char *string;
  size_t improvement_order_size = 0;
  size_t technology_order_size = 0;
  int civstyle = 0;
  const char *scen_text;
  const char **improvement_order = NULL;
  const char **technology_order = NULL;
  enum tile_special_type *special_order = NULL;
  size_t num_special_types = 0;
  struct base_type **base_order = NULL;
  size_t num_base_types = 0;
  const char *savefile_options = secfile_lookup_str(file, "savefile.options");
  bool bval;
  const struct entry *pentry;

  /* [savefile] */

  /* We don't need savefile.reason, but read it anyway to avoid
   * warnings about unread secfile entries. */
  (void) secfile_entry_by_path(file, "savefile.reason");

  if (has_capability("improvement_order", savefile_options)) {
    improvement_order = secfile_lookup_str_vec(file, &improvement_order_size,
                                               "savefile.improvement_order");
  }
  if (has_capability("technology_order", savefile_options)) {
    technology_order = secfile_lookup_str_vec(file, &technology_order_size,
                                              "savefile.technology_order");
  }

  /* [scenario] */
  scen_text = secfile_lookup_str_default(file, "", "scenario.name");
  if (scen_text[0] != '\0') {
    game.scenario.is_scenario = TRUE;
    sz_strlcpy(game.scenario.name, scen_text);
    game.scenario.authors[0] = '\0';
    scen_text = secfile_lookup_str_default(file, "",
                                           "scenario.description");
    if (scen_text[0] != '\0') {
      sz_strlcpy(game.scenario.description, scen_text);
    } else {
      game.scenario.description[0] = '\0';
    }
    game.scenario.players
      = secfile_lookup_bool_default(file, TRUE, "scenario.save_players");

    /* Old scenarios couldn't prevent cities */
    game.scenario.prevent_new_cities = FALSE;

    /* Assume any scenario from pre-editor time handmade with special settings. */
    game.scenario.handmade = TRUE;
  } else {
    game.scenario.is_scenario = FALSE;
  }

  /* [rulesets] */
  if ((string = secfile_lookup_str(file, "game.rulesetdir"))) {
    /* A ruleset was explicitly required, let's ignore the "rulesetdir"
     * capability then. */
    if (!strcmp("default", string)) {
      sz_strlcpy(game.server.rulesetdir, "classic");
    } else {
      sz_strlcpy(game.server.rulesetdir, string);
    }
  } else {
    civstyle = secfile_lookup_int_default(file, 2, "game.civstyle");
    string = (civstyle == 1) ? "civ1" : "classic";

    if (!has_capability("rulesetdir", savefile_options)) {
      const char *str2, *str =
        secfile_lookup_str_default(file, "classic", "game.info.t.techs");

      /* Savegame is missing "rulesetdir" capability, so this is ancient
       * terrain ruleset known as "classic", not to be confused with current
       * "classic" ruleset. */
      if (strcmp("classic",
                 secfile_lookup_str_default(file, "foobar",
                                            "game.info.t.terrain")) == 0) {
        /* TRANS: Fatal error message. */
        log_fatal(_("Saved game uses the ancient format terrain"
                    " ruleset, and is no longer supported."));
        exit(EXIT_FAILURE);
      }

#define T(x) \
    str2 = secfile_lookup_str_default(file, "classic", x);                   \
    if (strcmp(str, str2) != 0) {                                            \
      log_normal(_("Warning: Different rulesetdirs ('%s' and '%s') are "     \
                   "no longer supported. Using '%s'."), str, str2, str);     \
    }

      T("game.info.t.units");
      T("game.info.t.buildings");
      T("game.info.t.terrain");
      T("game.info.t.governments");
      T("game.info.t.nations");
      T("game.info.t.cities");
      T("game.info.t.game");
#undef T

      if (!strcmp("default", str)) {
        sz_strlcpy(game.server.rulesetdir, "classic");
      } else {
        sz_strlcpy(game.server.rulesetdir, str);
      }
    } else {
      sz_strlcpy(game.server.rulesetdir, string);
    }
  }

  /* Determine if the game is a new one (ex. scenario) or a savegame. */
  if (secfile_lookup_bool(file, &game.info.is_new_game,
                          "game.save_players")) {
    game.info.is_new_game = !game.info.is_new_game;
  } else {
    /* "game.save_players" is missing, try to get the number of players. */
    game.info.is_new_game = (!secfile_lookup_int(file, &i, "game.nplayers")
                             || 0 == i);
  }

  /* load rulesets */
  if (!load_rulesets(NULL, FALSE, TRUE, FALSE)) {
    /* Failed to load correct ruleset */
    exit(EXIT_FAILURE);
  }

  /* [settings]; must be *after* loading the ruleset */
  settings_game_load(file, "settings");

  /* [game] */
  game_version = secfile_lookup_int_default(file, 0, "game.version");

  /* we require at least version 2.0.0 */
  if (20000 > game_version) {
    /* TRANS: Fatal error message. */
    log_fatal(_("Saved game is too old, at least version 2.0.0 required."));
    exit(EXIT_FAILURE);
  }

  if (has_capability("resources", savefile_options)) {
    const char **modname;
    size_t nmod;
    enum tile_special_type j;

    modname = secfile_lookup_str_vec(file, &num_special_types,
				     "savefile.specials");
    /* make sure that the size of the array is divisible by 4 */
    /* Allocating extra 4 slots, just a couple of bytes, in case of
     * num_special_types being divisible by 4 already is intentional.
     * Added complexity would cost those couple of bytes in code size alone,
     * and we actually need at least one slot immediately after last valid
     * one. That's where S_LAST is (or was in version that saved the game)
     * and in some cases S_LAST gets written to savegame, at least as
     * activity target special when activity targets some base instead.
     * By having current S_LAST in that index allows us to map that old
     * S_LAST to current S_LAST, just like any real special within range
     * gets mapped. */
    nmod = num_special_types + (4 - (num_special_types % 4));
    special_order = fc_calloc(nmod, sizeof(*special_order));
    for (j = 0; j < num_special_types; j++) {
      if (!strcasecmp("Road", modname[j])) {
        special_order[j] = S_OLD_ROAD;
      } else if (!strcasecmp("Railroad", modname[j])) {
        special_order[j] = S_OLD_RAILROAD;
      } else if (!strcasecmp("River", modname[j])) {
        special_order[j] = S_OLD_RIVER;
      } else if (!strcasecmp("Fortress", modname[j])) {
        special_order[j] = S_OLD_FORTRESS;
      } else if (!strcasecmp("Airbase", modname[j])) {
        special_order[j] = S_OLD_AIRBASE;
      } else {
        special_order[j] = special_by_rule_name(modname[j]);
      }
    }
    free(modname);
    for (; j < nmod; j++) {
      special_order[j] = S_LAST;
    }
  }

  tmp_server_state = server_states_invalid();
  if ((pentry = secfile_entry_lookup(file, "game.server_state"))) {
    switch (entry_type(pentry)) {
    case ENTRY_STR:
      /* New in 2.2: server_state as string; see srv_main.h */
      {
        const char *state_str;

        if (entry_str_get(pentry, &state_str)) {
          tmp_server_state = server_states_by_name(state_str, strcmp);
        }
      }
      break;
    case ENTRY_INT:
      /* In former savegames, server states are saved as magic numbers. */
      {
        enum old_server_states {
          OLD_S_S_INITIAL = 0,
          OLD_S_S_GENERATING_WAITING = 1,
          OLD_S_S_RUNNING = 2,
          OLD_S_S_OVER = 3
        } saved_state;

        int ei;

        if (entry_int_get(pentry, &ei)) {
          saved_state = ei;
          switch (saved_state) {
          case OLD_S_S_INITIAL:
          case OLD_S_S_GENERATING_WAITING:
            tmp_server_state = S_S_INITIAL;
            break;
          case OLD_S_S_RUNNING:
            tmp_server_state = S_S_RUNNING;
            break;
          case OLD_S_S_OVER:
            tmp_server_state = S_S_OVER;
            break;
          }
        }
      }
      break;
    case ENTRY_BOOL:
    case ENTRY_FLOAT:
      break;
    }
  }
  if (!server_states_is_valid(tmp_server_state)) {
    /* Don't take any risk! */
    tmp_server_state = S_S_INITIAL;
  }

  if (game.scenario.is_scenario == TRUE
      && (tmp_server_state != S_S_INITIAL
          || (tmp_server_state == S_S_RUNNING
              && game.scenario.players == TRUE))) {
    log_fatal("Invalid scenario definition (server state '%d' and "
              "players are %s).", tmp_server_state,
              game.scenario.players ? "saved" : "not saved");
    exit(EXIT_FAILURE);
  }

  if (game.scenario.is_scenario) {
    /* Remove all defined players. They are recreated with the skill level
     * defined by the scenario. */
    (void) aifill(0);
  }

  {
    bool spacerace;
    int level;

    set_meta_patches_string(secfile_lookup_str_default(file, 
                                                default_meta_patches_string(),
                                                "game.metapatches"));
    game.server.meta_info.user_message_set =
      secfile_lookup_bool_default(file, FALSE, "game.user_metamessage");
    if (game.server.meta_info.user_message_set) {
      set_user_meta_message_string(secfile_lookup_str_default(file, 
                                                default_meta_message_string(),
                                                "game.metamessage"));
    } else {
      /* To avoid warnings when loading pre-2.1 savegames */
      (void) secfile_entry_by_path(file, "game.metamessage");
    }

    if (0 == strcmp(DEFAULT_META_SERVER_ADDR, srvarg.metaserver_addr)) {
      /* Do not overwrite this if the user requested a specific metaserver
       * from the command line (option --Metaserver). */
      sz_strlcpy(srvarg.metaserver_addr,
                 secfile_lookup_str_default(file, DEFAULT_META_SERVER_ADDR,
                                            "game.metaserver"));
    }
    if ('\0' == srvarg.serverid[0]) {
      /* Do not overwrite this if the user requested a specific metaserver
       * from the command line (option --serverid). */
      sz_strlcpy(srvarg.serverid,
                 secfile_lookup_str_default(file, "", "game.serverid"));
    }

    fc_assert_exit_msg(secfile_lookup_int(file, &game.info.gold, 
                                          "game.gold"),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &game.info.tech,
                                          "game.tech"),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &level,
                                          "game.skill_level"),
                       "%s", secfile_error());
    game.info.skill_level = ai_level_convert(level);
    fc_assert_exit_msg(secfile_lookup_int(file, &game.info.timeout,
                                          "game.timeout"),
                       "%s", secfile_error());
    if (!ai_level_is_valid(game.info.skill_level)) {
      game.info.skill_level = GAME_OLD_DEFAULT_SKILL_LEVEL;
    }

    game.server.timeoutint =
      secfile_lookup_int_default(file, GAME_DEFAULT_TIMEOUTINT,
                                 "game.timeoutint");
    game.server.timeoutintinc =
	secfile_lookup_int_default(file, GAME_DEFAULT_TIMEOUTINTINC,
				   "game.timeoutintinc");
    game.server.timeoutinc =
	secfile_lookup_int_default(file, GAME_DEFAULT_TIMEOUTINC,
				   "game.timeoutinc");
    game.server.timeoutincmult =
	secfile_lookup_int_default(file, GAME_DEFAULT_TIMEOUTINCMULT,
				   "game.timeoutincmult");
    game.server.timeoutcounter =
	secfile_lookup_int_default(file, 1, "game.timeoutcounter");

    game.server.timeoutaddenemymove
      = secfile_lookup_int_default(file, game.server.timeoutaddenemymove,
				   "game.timeoutaddenemymove");

    game.server.unitwaittime
      = secfile_lookup_int_default(file, GAME_DEFAULT_UNITWAITTIME,
                                   "game.unitwaittime");

    game.server.end_turn      = secfile_lookup_int_default(file, 5000,
                                                         "game.end_turn");
    game.info.shieldbox
      = secfile_lookup_int_default(file, GAME_DEFAULT_SHIELDBOX,
				   "game.box_shield");
    game.info.sciencebox
      = secfile_lookup_int_default(file, 0, "game.box_science");
    if (game.info.sciencebox == 0) {
      /* Researchcost was used for 2.0 and earlier servers. */
      game.info.sciencebox
	= 5 * secfile_lookup_int_default(file, 0, "game.researchcost");
      if (game.info.sciencebox == 0) {
	/* With even earlier servers (?) techlevel was used for this info. */
	game.info.sciencebox = 5 * secfile_lookup_int_default(file, 0, "game.techlevel");
      }
    }

    fc_assert_exit_msg(secfile_lookup_int(file, &game.info.year,
                                          "game.year"),
                       "%s", secfile_error());
    game.info.year_0_hack   = secfile_lookup_bool_default(file, FALSE,
                                                          "game.year_0_hack");

    if (has_capability("turn", savefile_options)) {
      game.info.turn = secfile_lookup_int_default(file, 0, "game.turn");
      if (0 > game.info.turn) {
        /* Some old scenarios claims the game starts at turn -2 for unknown
         * reasons. However, since the values -1 (I_NEVER) and -2
         * (I_DESTROYED) are used at turn number for the city improvements,
         * we cannot use that strange hack anymore. */
        game.info.turn = 0;
      }
    } else {
      game.info.turn = 0;
    }

    if (secfile_lookup_bool(file, &bval, "game.simultaneous_phases_now")) {
      game.info.phase_mode = (bval ? PMT_CONCURRENT
                              : PMT_PLAYERS_ALTERNATE);

    } else {
      game.info.phase_mode = GAME_DEFAULT_PHASE_MODE;
    }

    if (secfile_lookup_bool(file, &bval,
                            "game.simultaneous_phases_stored")) {
      game.server.phase_mode_stored = (bval ? PMT_CONCURRENT
                                       : PMT_PLAYERS_ALTERNATE);
    } else {
      game.server.phase_mode_stored = game.info.phase_mode;
    }

    game.info.phase_mode
      = secfile_lookup_int_default(file, game.info.phase_mode,
                                   "game.phase_mode");
    game.server.phase_mode_stored
      = secfile_lookup_int_default(file, game.server.phase_mode_stored,
                                   "game.phase_mode_stored");

    fc_assert_exit_msg(secfile_lookup_int(file, &game.server.min_players,
                                          "game.min_players"),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &game.server.max_players,
                                          "game.max_players"),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &game.info.globalwarming,
                                          "game.globalwarming"),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &game.info.warminglevel,
                                          "game.warminglevel"),
                       "%s", secfile_error());

    game.info.heating = secfile_lookup_int_default(file, 0, "game.heating");
    game.info.nuclearwinter = secfile_lookup_int_default(file, 0, "game.nuclearwinter");
    game.info.cooling = secfile_lookup_int_default(file, 0, "game.cooling");
    game.info.coolinglevel = secfile_lookup_int_default(file, 8, "game.coolinglevel");
    game.info.notradesize =
      secfile_lookup_int_default(file, GAME_DEFAULT_NOTRADESIZE, "game.notradesize");
    game.info.fulltradesize =
      secfile_lookup_int_default(file, GAME_DEFAULT_FULLTRADESIZE, "game.fulltradesize");

    game.info.trading_city =
      secfile_lookup_bool_default(file, GAME_DEFAULT_TRADING_CITY,
                                  "game.trading_city");
    game.info.trading_tech =
      secfile_lookup_bool_default(file, GAME_DEFAULT_TRADING_TECH,
                                  "game.trading_tech");
    game.info.trading_gold =
      secfile_lookup_bool_default(file, GAME_DEFAULT_TRADING_GOLD,
                                  "game.trading_gold");

    game.info.trademindist =
      secfile_lookup_int_default(file, GAME_DEFAULT_TRADEMINDIST, "game.trademindist");
    game.info.angrycitizen =
      secfile_lookup_bool_default(file, GAME_DEFAULT_ANGRYCITIZEN, "game.angrycitizen");
    {
      /* Backwards compatibility. Prior to 2.4, citymindist==0 meant to take
       * it from ruleset min_dist_bw_cities, but that no longer exists. */
      int citymindist =
        secfile_lookup_int_default(file, GAME_DEFAULT_CITYMINDIST,
                                   "game.citymindist");
      if (citymindist != 0) {
        game.info.citymindist = citymindist;
      } /* else leave it at ruleset/server default */
    }
    game.info.rapturedelay =
      secfile_lookup_int_default(file, GAME_DEFAULT_RAPTUREDELAY, "game.rapturedelay");
    game.server.diplbulbcost =
      secfile_lookup_int_default(file, GAME_DEFAULT_DIPLBULBCOST, "game.diplcost");
    game.server.diplgoldcost = game.server.diplbulbcost; /* They used to be one value */
    game.server.freecost =
      secfile_lookup_int_default(file, GAME_DEFAULT_FREECOST, "game.freecost");
    game.server.conquercost =
      secfile_lookup_int_default(file, GAME_DEFAULT_CONQUERCOST, "game.conquercost");

    game.info.foodbox = secfile_lookup_int_default(file, 0, "game.box_food");
    if (game.info.foodbox == 0) {
      /* foodbox was used for 2.0 and earlier servers. */
      game.info.foodbox = 10 * secfile_lookup_int_default(file, 100, "game.foodbox");
    }
    game.server.techpenalty =
      secfile_lookup_int_default(file, GAME_DEFAULT_TECHPENALTY, "game.techpenalty");
    game.server.razechance =
      secfile_lookup_int_default(file, GAME_DEFAULT_RAZECHANCE, "game.razechance");

    game.server.save_nturns =
      secfile_lookup_int_default(file, GAME_DEFAULT_SAVETURNS, "game.save_nturns");

    /* suppress warnings about unused entries in old savegames: */
    (void) secfile_entry_lookup(file, "game.rail_food");
    (void) secfile_entry_lookup(file, "game.rail_prod");
    (void) secfile_entry_lookup(file, "game.rail_trade");
    (void) secfile_entry_lookup(file, "game.farmfood");

    /* National borders setting. */
    game.info.borders = secfile_lookup_int_default(file, BORDERS_DISABLED,
                                                   "game.borders");
    if (BORDERS_EXPAND < game.info.borders) {
      game.info.borders = BORDERS_ENABLED;
    }
    if (secfile_lookup_bool_default(file, FALSE, "game.happyborders")) {
      game.info.happyborders = HB_NATIONAL;
    } else {
      game.info.happyborders = HB_DISABLED;
    }

    /* Diplomacy. */
    game.info.diplomacy = secfile_lookup_int_default(file, GAME_DEFAULT_DIPLOMACY, 
                                                "game.diplomacy");

    sz_strlcpy(game.server.save_name,
               secfile_lookup_str_default(file, GAME_DEFAULT_SAVE_NAME,
                                          "game.save_name"));
    game.server.save_compress_level
      = secfile_lookup_int_default(file, GAME_DEFAULT_COMPRESS_LEVEL,
                                   "game.save_compress_level");
    game.server.save_compress_type =
        int2fz_method(secfile_lookup_int_default(file, -1,
                                                 "game.save_compress_type"));

    game.info.aifill = secfile_lookup_int_default(file, 0, "game.aifill");

    game.server.scorelog = secfile_lookup_bool_default(file, FALSE,
                                                       "game.scorelog");
    game.server.scoreturn =
      secfile_lookup_int_default(file,
                                 game.info.turn + GAME_DEFAULT_SCORETURN,
                                 "game.scoreturn");
    sz_strlcpy(server.game_identifier,
               secfile_lookup_str_default(file, "", "game.id"));
    /* We are not checking game_identifier legality just yet.
     * That's done when we are sure that rand seed has been initialized,
     * so that we can generate new game_identifier, if needed. */

    game.info.fogofwar = secfile_lookup_bool_default(file, FALSE, "game.fogofwar");
    game.server.fogofwar_old = game.info.fogofwar;

    game.server.foggedborders
      = secfile_lookup_bool_default(file, GAME_DEFAULT_FOGGEDBORDERS,
                                    "game.foggedborders");

    game.info.global_warming
      = secfile_lookup_bool_default(file, GAME_DEFAULT_GLOBAL_WARMING,
                                    "game.global_warming");
    game.info.nuclear_winter
      = secfile_lookup_bool_default(file, GAME_DEFAULT_NUCLEAR_WINTER,
                                    "game.nuclear_winter");

    game.server.civilwarsize =
      secfile_lookup_int_default(file, GAME_DEFAULT_CIVILWARSIZE,
				 "game.civilwarsize");
    game.info.restrictinfra =
      secfile_lookup_bool_default(file, GAME_DEFAULT_RESTRICTINFRA,
                                  "game.restrictinfra");
    game.server.contactturns =
      secfile_lookup_int_default(file, GAME_DEFAULT_CONTACTTURNS,
				 "game.contactturns");
  
    if(has_capability("diplchance_percent", savefile_options)) {
      game.server.diplchance = secfile_lookup_int_default(file, game.server.diplchance,
						   "game.diplchance");
    } else {
      game.server.diplchance = secfile_lookup_int_default(file, 3, /* old default */
						   "game.diplchance");
      if (game.server.diplchance < 2) {
	game.server.diplchance = GAME_MAX_DIPLCHANCE;
      } else if (game.server.diplchance > 10) {
	game.server.diplchance = GAME_MIN_DIPLCHANCE;
      } else {
	game.server.diplchance = 100 - (10 * (game.server.diplchance - 1));
      }
    }

    game.server.aqueductloss =
      secfile_lookup_int_default(file, game.server.aqueductloss,
                                 "game.aqueductloss");

    /* Lowest bit of old killcitizen value indicates if
     * land units should kill citizens. We take that as
     * new boolean killcitizen value. */
    game.info.killcitizen =
      (secfile_lookup_int_default(file, 1, "game.killcitizen")
       & 0x1);
    game.server.killunhomed =
      secfile_lookup_int_default(file, game.server.killunhomed,
                                 "game.killunhomed");
    game.server.savepalace =
      secfile_lookup_bool_default(file, game.server.savepalace,
                                  "game.savepalace");
    game.server.homecaughtunits =
      secfile_lookup_bool_default(file, game.server.homecaughtunits,
                                  "game.homecaughtunits");
    game.server.turnblock =
      secfile_lookup_bool_default(file, game.server.turnblock,
                                  "game.turnblock");
    game.server.fixedlength =
      secfile_lookup_bool_default(file, game.server.fixedlength,
                                  "game.fixedlength");
    game.server.barbarianrate =
      secfile_lookup_int_default(file, game.server.barbarianrate,
                                 "game.barbarians");
    game.server.onsetbarbarian =
      secfile_lookup_int_default(file, game.server.onsetbarbarian,
                                 "game.onsetbarbs");
    /* old savegames use the year while newer savegames use the turn. */
    game.server.onsetbarbarian = MAX(0, game.server.onsetbarbarian);

    game.server.revolution_length =
      secfile_lookup_int_default(file, game.server.revolution_length,
                                 "game.revolen");
    if (game.server.revolution_length == 0) {
      /* Value 0 meant RANDOM 1-5 */
      game.server.revolution_length = 5;
      game.info.revolentype = REVOLEN_RANDOM;
    } else {
      game.info.revolentype = REVOLEN_FIXED;
    }

    game.server.occupychance =
      secfile_lookup_int_default(file, game.server.occupychance,
                                 "game.occupychance");
    game.server.autoattack =
      secfile_lookup_bool_default(file, GAME_DEFAULT_AUTOATTACK,
                                  "game.autoattack");
    game.server.seed = game.server.seed_setting =
      secfile_lookup_int_default(file, game.server.seed_setting,
                                 "game.randseed");
    game.server.allowed_city_names =
      secfile_lookup_int_default(file, game.server.allowed_city_names,
                                 "game.allowed_city_names"); 
    game.server.migration =
      secfile_lookup_bool_default(file, game.server.migration,
                                  "game.migration");
    game.server.mgr_turninterval =
      secfile_lookup_int_default(file, game.server.mgr_turninterval,
                                 "game.mgr_turninterval");
    game.server.mgr_foodneeded =
      secfile_lookup_bool_default(file, game.server.mgr_foodneeded,
                                 "game.mgr_foodneeded");
    game.server.mgr_distance =
      secfile_lookup_int_default(file, game.server.mgr_distance,
                                 "game.mgr_distance");
    game.server.mgr_nationchance =
      secfile_lookup_int_default(file, game.server.mgr_nationchance,
                                 "game.mgr_nationchance");
    game.server.mgr_worldchance =
      secfile_lookup_int_default(file, game.server.mgr_worldchance,
                                 "game.mgr_worldchance");

    game.server.techlost_donor =
      secfile_lookup_int_default(file, game.server.techlost_donor,
                                 "game.techlost_donor");
    game.server.techlost_recv =
      secfile_lookup_int_default(file, game.server.techlost_recv,
                                 "game.techlost_recv");
    game.info.team_pooled_research =
      secfile_lookup_int_default(file, game.info.team_pooled_research,
                                 "game.team_pooled_research");

    sz_strlcpy(game.server.demography,
	       secfile_lookup_str_default(file, GAME_DEFAULT_DEMOGRAPHY,
					  "game.demography"));
    sz_strlcpy(game.server.allow_take,
	       secfile_lookup_str_default(file, GAME_DEFAULT_ALLOW_TAKE,
					  "game.allow_take"));

    spacerace = secfile_lookup_bool_default(file, victory_enabled(VC_SPACERACE),
                                            "game.spacerace");
    if (spacerace) {
      game.info.victory_conditions |= (1 << VC_SPACERACE);
    } else {
      game.info.victory_conditions &= ~(1 << VC_SPACERACE);
    }

    game.server.endspaceship =
      secfile_lookup_bool_default(file, game.server.endspaceship,
                                  "game.endspaceship");

    game.server.auto_ai_toggle = 
      secfile_lookup_bool_default(file, game.server.auto_ai_toggle, 
                                  "game.auto_ai_toggle");

    game.server.event_cache.turns =
      secfile_lookup_int_default(file, game.server.event_cache.turns,
                                 "game.event_cache.turns");
    game.server.event_cache.max_size =
      secfile_lookup_int_default(file, game.server.event_cache.max_size,
                                 "game.event_cache.max_size");
    game.server.event_cache.chat =
      secfile_lookup_bool_default(file, game.server.event_cache.chat,
                                 "game.event_cache.chat");
    game.server.event_cache.info =
      secfile_lookup_bool_default(file, game.server.event_cache.info,
                                 "game.event_cache.info");
  }

  if (has_capability("bases", savefile_options)) {
    const char **modname = NULL;
    int j;

    num_base_types = secfile_lookup_int_default(file, 0,
                                                "savefile.num_bases");

    if (num_base_types > 0) {
      modname = secfile_lookup_str_vec(file, &num_base_types,
                                       "savefile.bases");
    }

    /* make sure that the size of the array is divisible by 4 */
    base_order = fc_calloc(4 * ((num_base_types + 3) / 4),
                           sizeof(*base_order));
    for (j = 0; j < num_base_types; j++) {
      base_order[j] = base_type_by_rule_name(modname[j]);
    }
    free(modname);
  }

  {
    {
      int ei;

      {
	if (!has_capability("startunits", savefile_options)) {
          int settlers = secfile_lookup_int_default(file, 0, "game.settlers");
          int explorer = secfile_lookup_int_default(file, 0, "game.explorer");
          int su;

          for (su = 0; settlers > 0 && su < (MAX_LEN_STARTUNIT - 1) ; su++, settlers--) {
            game.server.start_units[su] = 'c';
          }
          for (; explorer > 0 && su < (MAX_LEN_STARTUNIT - 1) ; su++, explorer--) {
            game.server.start_units[su] = 'x';
          }
          game.server.start_units[su] = '\0';
        } else {
	  sz_strlcpy(game.server.start_units,
		     secfile_lookup_str_default(file,
						GAME_DEFAULT_START_UNITS,
						"game.start_units"));
	}
	game.server.dispersion =
	  secfile_lookup_int_default(file, GAME_DEFAULT_DISPERSION,
				     "game.dispersion");
      }

      game.map.topology_id = secfile_lookup_int_default(file, MAP_ORIGINAL_TOPO,
                                                        "map.topology_id");
      game.map.server.mapsize
        = secfile_lookup_int_default(file, MAP_DEFAULT_MAPSIZE,
                                     "map.mapsize");
      game.map.server.size = secfile_lookup_int_default(file, MAP_DEFAULT_SIZE,
                                                        "map.size");
      game.map.server.tilesperplayer
        = secfile_lookup_int_default(file, MAP_DEFAULT_TILESPERPLAYER,
                                     "map.tilesperplayer");
      game.map.server.startpos = secfile_lookup_int_default(file,
                                                            MAP_DEFAULT_STARTPOS,
                                                            "map.startpos");
      fc_assert_exit_msg(secfile_lookup_int(file, &game.map.server.riches,
                                            "map.riches"),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file, &game.map.server.huts,
                                            "map.huts"),
                         "%s", secfile_error());
      fc_assert_exit_msg(secfile_lookup_int(file,
                                            &ei,
                                            "map.generator"),
                         "%s", secfile_error());
      game.map.server.generator = ei;
      fc_assert_exit_msg(secfile_lookup_int(file, &game.map.server.seed_setting,
                                            "map.seed"),
                         "%s", secfile_error());
      game.map.server.seed = game.map.server.seed_setting;
      fc_assert_exit_msg(secfile_lookup_int(file, &game.map.server.landpercent,
                                            "map.landpercent"),
                         "%s", secfile_error());

      game.map.server.wetness =
        secfile_lookup_int_default(file, MAP_DEFAULT_WETNESS, "map.wetness");
      game.map.server.steepness =
        secfile_lookup_int_default(file, MAP_DEFAULT_STEEPNESS,
                                   "map.steepness");
      game.map.server.have_huts = secfile_lookup_bool_default(file, TRUE,
                                                              "map.have_huts");
      game.map.server.temperature =
	secfile_lookup_int_default(file, MAP_DEFAULT_TEMPERATURE,
                                   "map.temperature");
      game.map.server.alltemperate
	= secfile_lookup_bool_default(file, MAP_DEFAULT_ALLTEMPERATE,
				      "map.alltemperate");
      game.map.server.tinyisles
	= secfile_lookup_bool_default(file, MAP_DEFAULT_TINYISLES,
				      "map.tinyisles");
      game.map.server.separatepoles
	= secfile_lookup_bool_default(file, MAP_DEFAULT_SEPARATE_POLES,
				      "map.separatepoles");

      if (has_capability("startoptions", savefile_options)) {
        fc_assert_exit_msg(secfile_lookup_int(file, &game.map.xsize,
                                              "map.width"),
                           "%s", secfile_error());
        fc_assert_exit_msg(secfile_lookup_int(file, &game.map.ysize,
                                              "map.height"),
                           "%s", secfile_error());
      } else {
        /* old versions saved with these names in S_S_INITIAL: */
        fc_assert_exit_msg(secfile_lookup_int(file, &game.map.xsize, "map.xsize"),
                           "%s", secfile_error());
        fc_assert_exit_msg(secfile_lookup_int(file, &game.map.ysize, "map.ysize"),
                           "%s", secfile_error());
      }

      if (S_S_INITIAL == tmp_server_state
          && MAPGEN_SCENARIO == game.map.server.generator) {
        /* generator 0 = map done with a map editor aka a "scenario" */
        if (has_capability("specials",savefile_options)) {
          map_load(file, tmp_server_state, savefile_options,
                   special_order, num_special_types,
                   base_order, num_base_types);
          return;
        }
        map_load_tiles(file);
        if (has_capability("riversoverlay",savefile_options)) {
	  map_load_rivers_overlay(file, special_order, num_special_types);
	}
        if (has_capability("startpos", savefile_options)) {
          map_load_startpos(file, tmp_server_state);
          return;
        }
	return;
      }
    }
    if (S_S_INITIAL == tmp_server_state) {
      return;
    }
  }

  /* We check
     1) if the block exists at all.
     2) if it is saved. */
  if (NULL != secfile_entry_lookup(file, "random.index_J")
      && secfile_lookup_bool_default(file, TRUE, "game.save_random")) {
    fc_assert_exit_msg(secfile_lookup_int(file, &rstate.j, "random.index_J"),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &rstate.k, "random.index_K"),
                       "%s", secfile_error());
    fc_assert_exit_msg(secfile_lookup_int(file, &rstate.x, "random.index_X"),
                       "%s", secfile_error());

    /* Since random state was previously saved, save it also when resaving.
     *
     * Do NOT touch this in case of regular savegame. They always have random
     * saved, but if one starts to make scenario based on a savegame, we want
     * default scenario settings in the beginning (default save_random = FALSE).
     */
    if (game.scenario.is_scenario) {
      game.scenario.save_random = TRUE;
    }

    for (i = 0; i < 8; i++) {
      string = secfile_lookup_str(file, "random.table%d",i);
      fc_assert(NULL != string);
      sscanf(string, "%8x %8x %8x %8x %8x %8x %8x", &rstate.v[7*i],
             &rstate.v[7*i+1], &rstate.v[7*i+2], &rstate.v[7*i+3],
             &rstate.v[7*i+4], &rstate.v[7*i+5], &rstate.v[7*i+6]);
    }
    rstate.is_init = TRUE;
    fc_rand_set_state(rstate);
  } else {
    /* mark it */
    (void) secfile_entry_by_path(file, "game.save_random");

    /* We're loading a running game without a seed (which is okay, if it's
     * a scenario).  We need to generate the game seed now because it will
     * be needed later during the load. */
    if (S_S_RUNNING <= tmp_server_state) {
      init_game_seed();
      rstate = fc_rand_state();
    }
  }

  if (0 == strlen(server.game_identifier)
      || !is_base64url(server.game_identifier)) {
    /* This uses fc_rand(), so random state has to be initialized
     * before this. */
    randomize_base64url_string(server.game_identifier,
                               sizeof(server.game_identifier));
  }

  settings_consider_all_changed();

  map_load(file, tmp_server_state, savefile_options,
           special_order, num_special_types,
           base_order, num_base_types);

  if (!game.info.is_new_game) {
    int *worked_tiles = NULL; /* temporary map for worked tiles */
    int loaded_players = 0;
    int players;

    /* destroyed wonders: */
    string = secfile_lookup_str(file, "game.destroyed_wonders_new");
    if (!string) {
      log_fatal("No destroyed wonders information");
      exit(EXIT_FAILURE);
    } else {
      for (k = 0; k < improvement_order_size && string[k]; k++) {
        if (string[k] == '1') {
          struct impr_type *pimprove =
              improvement_by_rule_name(improvement_order[k]);
          if (pimprove) {
            game.info.great_wonder_owners[improvement_index(pimprove)] =
                WONDER_DESTROYED;
          }
        }
      }
    }

    server.identity_number =
      secfile_lookup_int_default(file, server.identity_number,
                                 "game.identity_number_used");

    /* Initialize nations we loaded from rulesets. This has to be after
     * map loading and before we seek nations for players */
    update_nations_with_startpos();

    /* Load worked tiles map */
    worked_tiles = player_load_cities_worked_map(file, savefile_options);

    /* Now, load the players from the savefile. */
    player_slots_iterate(pslot) {
      struct player *pplayer;

      if (NULL == secfile_section_lookup(file, "player%d",
                                         player_slot_index(pslot))) {
        continue;
      }

      /* Create player */
      pplayer = server_create_player(player_slot_index(pslot),
                                     default_ai_type_name(), NULL,
                                     FALSE);
      server_player_init(pplayer, FALSE, FALSE);
      loaded_players++;
    } player_slots_iterate_end;

    /* check number of players */
    {
      int nplayers = secfile_lookup_int_default(file, 0, "game.nplayers");
      fc_assert_ret(player_count() == nplayers);
    }

    /* Load team informations. All players should now have teams. This is
     * not the case with old savegames. Assign first the requested teams,
     * then find empty teams for other players. */
    players_iterate(pplayer) {
      int team;
      struct team_slot *tslot;

      if (secfile_lookup_int(file, &team, "player%d.team_no",
                             player_number(pplayer))
          && (tslot = team_slot_by_number(team))) {
        team_add_player(pplayer, team_new(tslot));
      }
    } players_iterate_end;
    players_iterate(pplayer) {
      if (NULL == pplayer->team) {
        team_add_player(pplayer, team_new(NULL));
      }
    } players_iterate_end;

    /* Load map player data. */
    players_iterate(pplayer) {
      player_map_init(pplayer);

      /* Just in case savecompat starts adding it in the future. */
      pplayer->server.border_vision =
          secfile_lookup_bool_default(file, FALSE,
                                      "player%d.border_vision",
                                      player_number(pplayer));
    } players_iterate_end;
    map_load_known(file, savefile_options);
    map_load_owner(file, savefile_options);

    /* Now, load each players data about other players. Thus must be after
     * the player slots are activated. */
    players_iterate(pplayer) {
      int plrno = player_number(pplayer);

      player_load_main(pplayer, plrno, file, savefile_options,
                       technology_order, technology_order_size);
      player_load_cities(pplayer, plrno, file, savefile_options,
                         improvement_order, improvement_order_size,
                         worked_tiles);
      player_load_units(pplayer, plrno, file, savefile_options,
                        special_order, num_special_types,
                        base_order, num_base_types);
      player_load_attributes(pplayer, plrno, file);

      /* print out some informations */
      if (is_ai(pplayer)) {
        CALL_PLR_AI_FUNC(gained_control, pplayer, pplayer);
        log_normal(_("%s has been added as %s level AI-controlled player "
                     "(%s)."), player_name(pplayer),
                   ai_level_translated_name(pplayer->ai_common.skill_level),
                   ai_name(pplayer->ai));
      } else {
        log_normal(_("%s has been added as human player."),
                   player_name(pplayer));
      }
    } players_iterate_end;

    players_iterate(pplayer) {
      /* Backward compatibility: if we had any open-ended orders (pillage)
       * in the savegame, assign specific targets now */
      unit_list_iterate(pplayer->units, punit) {
        unit_assign_specific_activity_target(punit,
                                             &punit->activity,
                                             &punit->activity_target);
      } unit_list_iterate_end;
      /* Load transporter status. */
      player_load_units_transporter(pplayer, file);
      /* Update cached city illness. This can depend on trade routes,
       * so can't be calculated until all players have been loaded. */
      if (game.info.illness_on) {
        city_list_iterate(pplayer->cities, pcity) {
          pcity->server.illness
            = city_illness_calc(pcity, NULL, NULL,
                                &(pcity->illness_trade), NULL);
        } city_list_iterate_end;
      }

    } players_iterate_end;

    /* Free worked tiles map */
    if (worked_tiles != NULL) {
#ifdef DEBUG
      /* check the entire map for unused worked tiles */
      whole_map_iterate(ptile) {
        if (worked_tiles[ptile->index] != -1) {
          log_error("[city id: %d] Unused worked tile at (%d, %d).",
                    worked_tiles[ptile->index], TILE_XY(ptile));
        }
      } whole_map_iterate_end;
#endif /* DEBUG */
      FC_FREE(worked_tiles);
    }

    /* In case of tech_leakage, we can update research only after all
     * the players have been loaded */
    researches_iterate(presearch) {
      /* Mark the reachable techs */
      research_update(presearch);

      /* Check researching technology and goal. */
      if (presearch->researching != A_UNSET
          && !is_future_tech(presearch->researching)
          && (valid_advance_by_number(presearch->researching) == NULL
              || (research_invention_state(presearch, presearch->researching)
                  != TECH_PREREQS_KNOWN))) {
        log_error(_("%s had invalid researching technology."),
                  research_name_translation(presearch));
        presearch->researching = A_UNSET;
      }
      if (presearch->tech_goal != A_UNSET
          && !is_future_tech(presearch->tech_goal)
          && (valid_advance_by_number(presearch->researching) == NULL
              || !research_invention_reachable(presearch,
                                               presearch->tech_goal)
              || (research_invention_state(presearch, presearch->tech_goal)
                  == TECH_KNOWN))) {
        log_error(_("%s had invalid technology goal."),
                  research_name_translation(presearch));
        presearch->tech_goal = A_UNSET;
      }
    } researches_iterate_end;

    /* The savegame may contain valid nations that are not included in
     * the current default nationset for the ruleset, since it predates
     * nationsets. Try to reconcile these. */
    fit_nationset_to_players();

    /* Some players may have invalid nations in the ruleset.  Once all 
     * players are loaded, pick one of the remaining nations for them. */
    players_iterate(pplayer) {
      if (pplayer->nation == NO_NATION_SELECTED) {
        player_set_nation(pplayer, pick_a_nation(NULL, FALSE, TRUE,
                                                 NOT_A_BARBARIAN));
        /* TRANS: Minor error message: <Leader> ... <Poles>. */
        log_error(_("%s had invalid nation; changing to %s."),
                  player_name(pplayer), nation_plural_for_player(pplayer));
      }

      ai_traits_init(pplayer);
    } players_iterate_end;

    /* Sanity check alliances, prevent allied-with-ally-of-enemy */
    players_iterate_alive(plr) {
      players_iterate_alive(aplayer) {
        if (pplayers_allied(plr, aplayer)) {
          enum dipl_reason can_ally = pplayer_can_make_treaty(plr, aplayer,
                                                              DS_ALLIANCE);
          if (can_ally == DIPL_ALLIANCE_PROBLEM_US
              || can_ally == DIPL_ALLIANCE_PROBLEM_THEM) {
            log_error("Illegal alliance structure detected: "
                      "%s alliance to %s reduced to peace treaty.",
                      nation_rule_name(nation_of_player(plr)),
                      nation_rule_name(nation_of_player(aplayer)));
            player_diplstate_get(plr, aplayer)->type = DS_PEACE;
            player_diplstate_get(aplayer, plr)->type = DS_PEACE;
          }
        }
      } players_iterate_alive_end;
    } players_iterate_alive_end;

    /* Update all city information.  This must come after all cities are
     * loaded (in player_load) but before player (dumb) cities are loaded
     * in player_load_vision(). */
    players_iterate(plr) {
      city_list_iterate(plr->cities, pcity) {
        city_refresh(pcity);
        CALL_PLR_AI_FUNC(city_got, plr, plr, pcity);
      } city_list_iterate_end;
    } players_iterate_end;

    /* Since the cities must be placed on the map to put them on the
       player map we do this afterwards */
    players_iterate(pplayer) {
      int n = player_index(pplayer);
      player_load_vision(pplayer, n, file, savefile_options,
                         special_order, num_special_types,
                         improvement_order, improvement_order_size,
                         base_order, num_base_types);
    } players_iterate_end;

    /* We do this here since if the did it in player_load, player 1
       would try to unfog (unloaded) player 2's map when player 1's units
       were loaded */
    players_iterate(pplayer) {
      BV_CLR_ALL(pplayer->gives_shared_vision);
      BV_CLR_ALL(pplayer->server.really_gives_vision);
    } players_iterate_end;

    players_iterate(pplayer) {
      int n = player_index(pplayer);

      if (has_capability("vision", savefile_options)) {
        players_iterate(pplayer2) {
          if (secfile_lookup_bool_default(file, FALSE,
                "player%d.diplstate%d.gives_shared_vision",
                n, player_index(pplayer2))) {
            give_shared_vision(pplayer, pplayer2);
         }
        } players_iterate_end;
      } else {
        const char *vision = secfile_lookup_str(file,
                                "player%d.gives_shared_vision", n);

         if (vision) {
          players_iterate(pplayer2) {
            if (vision[player_index(pplayer2)] == '1') {
              give_shared_vision(pplayer, pplayer2);
            }
          } players_iterate_end;
        }
      }
    } players_iterate_end;

    initialize_globals();
    apply_unit_ordering();

    /* all vision is ready */
    map_calculate_borders(); /* does city_thaw_workers_queue() */
    /* city_refresh() below */

    /* Make sure everything is consistent. */

    /* Old savegames may have maxplayers lower than current player count,
     * fix. */
    players = normal_player_count();
    if (game.server.max_players < players) {
      log_verbose("Max players lower than current players, fixing");
      game.server.max_players = players;
    }

    players_iterate(pplayer) {
      unit_list_iterate(pplayer->units, punit) {
        if (!can_unit_continue_current_activity(punit)) {
          log_error("Unit doing illegal activity in savegame!");
          punit->activity = ACTIVITY_IDLE;
        }
      } unit_list_iterate_end;
    } players_iterate_end;

    cities_iterate(pcity) {
      city_refresh(pcity); /* again */
      city_thaw_workers(pcity); /* may auto_arrange_workers() */
    } cities_iterate_end;
  }

  if (secfile_lookup_int_default(file, -1,
				 "game.shuffled_player_%d", 0) >= 0) {
    int shuffled_players[player_slot_count()];

    /* players_iterate() not used here */
    for (i = 0; i < player_slot_count(); i++) {
      shuffled_players[i] = secfile_lookup_int_default(file,
          i, "game.shuffled_player_%d", i);
    }
    set_shuffled_players(shuffled_players);
  } else {
    /* No shuffled players included, so shuffle them (this may include
     * scenarios). */
    shuffle_players();
  }

  /* Fix ferrying sanity */
  players_iterate(pplayer) {
    unit_list_iterate_safe(pplayer->units, punit) {
      if (!unit_transport_get(punit)
          && !can_unit_exist_at_tile(punit, unit_tile(punit))) {
        log_error("Removing %s unferried %s in %s at (%d, %d)",
                  nation_rule_name(nation_of_player(pplayer)),
                  unit_rule_name(punit),
                  terrain_rule_name(unit_tile(punit)->terrain),
                  TILE_XY(unit_tile(punit)));
        bounce_unit(punit, TRUE);
      }
    } unit_list_iterate_safe_end;
  } players_iterate_end;

  /* Fix stacking issues.  We don't rely on the savegame preserving
   * alliance invariants (old savegames often did not) so if there are any
   * unallied units on the same tile we just bounce them. */
  players_iterate(pplayer) {
    players_iterate(aplayer) {
      resolve_unit_stacks(pplayer, aplayer, TRUE);
    } players_iterate_end;
  } players_iterate_end;

  players_iterate(pplayer) {
    calc_civ_score(pplayer);
  } players_iterate_end;

  /* Recalculate the potential buildings for each city.  
   * Has caused some problems with game random state. */
  players_iterate(pplayer) {
    bool saved_ai_control = is_ai(pplayer);

    /* Recalculate for all players. */
    set_as_human(pplayer);

    /* Building advisor needs data phase open in order to work */
    adv_data_phase_init(pplayer, FALSE);
    building_advisor(pplayer);
    /* Close data phase again so it can be opened again when game starts. */
    adv_data_phase_done(pplayer);

    if (saved_ai_control) {
      set_as_ai(pplayer);
    }
  } players_iterate_end;

  /* Player colors are always needed once game has started. Pre-2.4 savegames
   * lack them. */
  if (game_was_started()) {
    assign_player_colors();
  }

  /* Restore game random state, just in case various initialization code
   * inexplicably altered the previously existing state. */
  if (!game.info.is_new_game) {
    fc_rand_set_state(rstate);
  }

  /* load event cache */
  event_cache_load(file, "event_cache");

  /* Free load data. */
  if (NULL != base_order) {
    free(base_order);
  }
  if (NULL != special_order) {
    free(special_order);
  }
  if (NULL != improvement_order) {
    free(improvement_order);
  }
  if (NULL != technology_order) {
    free(technology_order);
  }
}
