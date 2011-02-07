/**********************************************************************
 Freeciv - Copyright (C) 2005 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

/**************************************************************************
  Signals implementation.
  
  New signal types can be declared with script_signal_create. Each
  signal should have a unique name string.
  All signal declarations are in signals_create, for convenience.
  
  A signal may have any number of Lua callback functions connected to it
  at any given time.

  A signal emission invokes all associated callbacks in the order they were
  connected:

  * A callback can stop the current signal emission, preventing the callbacks
    connected after it from being invoked.

  * A callback can detach itself from its associated signal.
  
  Lua callbacks functions are able to do these via their return values.
  
  All Lua callback functions can return a value. Example:
    return false
    
  If the value is 'true' the current signal emission will be stopped.
**************************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <stdarg.h>

/* utility */
#include "log.h"
#include "mem.h"
#include "registry.h"

/* scripting */
#include "script_types.h"
#include "script_game.h"

#include "script_signal.h"

struct signal;
struct signal_callback;

/* get 'struct signal_callback_list' and related functions: */
#define SPECLIST_TAG signal_callback
#define SPECLIST_TYPE struct signal_callback
#include "speclist.h"

#define signal_callback_list_iterate(list, pcallback)                       \
  TYPED_LIST_ITERATE(struct signal_callback, list, pcallback)
#define signal_callback_list_iterate_end LIST_ITERATE_END

/**************************************************************************
  Signal datastructure.
**************************************************************************/
struct signal {
  int nargs;                              /* number of arguments to pass */
  enum api_types *arg_types;              /* argument types */
  struct signal_callback_list *callbacks; /* connected callbacks */
};

static void signal_destroy(struct signal *psignal);

/**************************************************************************
  Signal callback datastructure.
**************************************************************************/
struct signal_callback {
  char *name;                             /* callback function name */
};

/****************************************************************************
  Signal hash table.
****************************************************************************/
#define SPECHASH_TAG signal
#define SPECHASH_KEY_TYPE char *
#define SPECHASH_DATA_TYPE struct signal *
#define SPECHASH_KEY_VAL genhash_str_val_func
#define SPECHASH_KEY_COMP genhash_str_comp_func
#define SPECHASH_KEY_COPY genhash_str_copy_func
#define SPECHASH_KEY_FREE genhash_str_free_func
#define SPECHASH_DATA_FREE signal_destroy
#include "spechash.h"

static struct signal_hash *signals;

/****************************************************************************
  Create a new signal callback.
****************************************************************************/
static struct signal_callback *signal_callback_new(const char *name)
{
  struct signal_callback *pcallback = fc_malloc(sizeof(*pcallback));

  pcallback->name = fc_strdup(name);
  return pcallback;
}

/****************************************************************************
  Free a signal callback.
****************************************************************************/
static void signal_callback_destroy(struct signal_callback *pcallback)
{
  free(pcallback->name);
  free(pcallback);
}

/****************************************************************************
  Create a new signal.
****************************************************************************/
static struct signal *signal_new(int nargs, enum api_types *parg_types)
{
  struct signal *psignal = fc_malloc(sizeof(*psignal));

  psignal->nargs = nargs;
  psignal->arg_types = parg_types;
  psignal->callbacks =
      signal_callback_list_new_full(signal_callback_destroy);
  return psignal;
}

/****************************************************************************
  Free a signal.
****************************************************************************/
static void signal_destroy(struct signal *psignal)
{
  if (psignal->arg_types) {
    free(psignal->arg_types);
  }
  signal_callback_list_destroy(psignal->callbacks);
  free(psignal);
}

/**************************************************************************
  Declare any new signal types you need here.
**************************************************************************/
static void signals_create(void)
{
  script_signal_create("turn_started", 2, API_TYPE_INT, API_TYPE_INT);
  script_signal_create("unit_moved",
		       3, API_TYPE_UNIT, API_TYPE_TILE, API_TYPE_TILE);

  /* Includes all newly-built cities. */
  script_signal_create("city_built", 1, API_TYPE_CITY);

  script_signal_create("city_growth", 2, API_TYPE_CITY, API_TYPE_INT);

  /* Only includes units built in cities, for now. */
  script_signal_create("unit_built", 2, API_TYPE_UNIT, API_TYPE_CITY);
  script_signal_create("building_built",
		       2, API_TYPE_BUILDING_TYPE, API_TYPE_CITY);

  /* These can happen for various reasons; the third argument gives the
   * reason (a simple string identifier).  Example identifiers:
   * "pop_cost", "need_tech", "need_building", "need_special",
   * "need_terrain", "need_government", "need_nation", "never",
   * "unavailable". */
  script_signal_create("unit_cant_be_built",
		       3, API_TYPE_UNIT_TYPE, API_TYPE_CITY, API_TYPE_STRING);
  script_signal_create("building_cant_be_built",
		       3, API_TYPE_BUILDING_TYPE, API_TYPE_CITY,
		       API_TYPE_STRING);

  /* The third argument contains the source: "researched", "traded",
   * "stolen", "hut". */
  script_signal_create("tech_researched",
		       3, API_TYPE_TECH_TYPE, API_TYPE_PLAYER,
		       API_TYPE_STRING);

  /* First player is city owner, second is enemy. */
  script_signal_create("city_destroyed",
                       3, API_TYPE_CITY, API_TYPE_PLAYER, API_TYPE_PLAYER);
  script_signal_create("city_lost",
                       3, API_TYPE_CITY, API_TYPE_PLAYER, API_TYPE_PLAYER);

  script_signal_create("hut_enter", 1, API_TYPE_UNIT);

  script_signal_create("unit_lost", 2, API_TYPE_UNIT, API_TYPE_PLAYER);
}

/**************************************************************************
  Invoke all the callback functions attached to a given signal.
**************************************************************************/
void script_signal_emit(const char *signal_name, int nargs, ...)
{
  struct signal *psignal;
  va_list args;

  if (signal_hash_lookup(signals, signal_name, &psignal)) {
    if (psignal->nargs != nargs) {
      log_error("Signal \"%s\" requires %d args, was passed %d on invoke.",
                signal_name, psignal->nargs, nargs);
    } else {
      signal_callback_list_iterate(psignal->callbacks, pcallback) {
        va_start(args, nargs);
        if (script_callback_invoke(pcallback->name, nargs, psignal->arg_types,
                                   args)) {
          va_end(args);
          break;
        }
        va_end(args);
      } signal_callback_list_iterate_end;
    }
  } else {
    log_error("Signal \"%s\" does not exist, so cannot be invoked.",
              signal_name);
  }
}

/**************************************************************************
  Create a new signal type.
**************************************************************************/
void script_signal_create_valist(const char *signal_name, int nargs,
                                 va_list args)
{
  struct signal *psignal;

  if (signal_hash_lookup(signals, signal_name, &psignal)) {
    log_error("Signal \"%s\" was already created.", signal_name);
  } else {
    enum api_types *parg_types = fc_calloc(nargs, sizeof(*parg_types));
    int i;

    for (i = 0; i < nargs; i++) {
      *(parg_types + i) = va_arg(args, int);
    }
    signal_hash_insert(signals, signal_name, signal_new(nargs, parg_types));
  }
}

/**************************************************************************
  Create a new signal type.
**************************************************************************/
void script_signal_create(const char *signal_name, int nargs, ...)
{
  va_list args;

  va_start(args, nargs);
  script_signal_create_valist(signal_name, nargs, args);
  va_end(args);
}

/**************************************************************************
  Connects a callback function to a certain signal.
**************************************************************************/
void script_signal_connect(const char *signal_name,
                           const char *callback_name)
{
  SCRIPT_CHECK_ARG_NIL(signal_name, 1, string);
  SCRIPT_CHECK_ARG_NIL(callback_name, 2, string);

  {
    struct signal *psignal;
    bool duplicate = FALSE;

    if (signal_hash_lookup(signals, signal_name, &psignal)) {
      /* check for a duplicate callback */
      signal_callback_list_iterate(psignal->callbacks, pcallback) {
        if (!strcmp(pcallback->name, callback_name)) {
          duplicate = TRUE;
          break;
        }
      } signal_callback_list_iterate_end;

      if (duplicate) {
        script_error("Signal \"%s\" already has a callback called \"%s\".",
                     signal_name, callback_name);
      } else {
        signal_callback_list_append(psignal->callbacks,
                                    signal_callback_new(callback_name));
      }
    } else {
      script_error("Signal \"%s\" does not exist.", signal_name);
    }
  }
}

/**************************************************************************
  Initialize script signals and callbacks.
**************************************************************************/
void script_signals_init(void)
{
  if (NULL == signals) {
    signals = signal_hash_new();

    signals_create();
  }
}

/**************************************************************************
  Free script signals and callbacks.
**************************************************************************/
void script_signals_free(void)
{
  if (NULL != signals) {
    signal_hash_destroy(signals);
    signals = NULL;
  }
}
