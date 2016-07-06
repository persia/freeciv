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

/* utility */
#include "log.h"

/* common */
#include "ai.h"
#include "city.h"
#include "game.h"
#include "unit.h"

/* server/advisors */
#include "infracache.h"

/* ai/default */
#include "aiplayer.h"

/* ai/threxpr */
#include "texaicity.h"

#include "texaiplayer.h"

/* What level of operation we should abort because
 * of received messages. Lower is more critical;
 * TEXAI_ABORT_EXIT means that whole thread should exit,
 * TEXAI_ABORT_NONE means that we can continue what we were doing */
enum texai_abort_msg_class
{
  TEXAI_ABORT_EXIT,
  TEXAI_ABORT_PHASE_END,
  TEXAI_ABORT_NONE
};

static enum texai_abort_msg_class texai_check_messages(void);

struct texai_thr
{
  int num_players;
  struct texai_msgs msgs_to;
  struct texai_reqs reqs_from;
  bool thread_running;
  fc_thread ait;
} exthrai;

/**************************************************************************
  Initialize ai thread.
**************************************************************************/
void texai_init_threading(void)
{
  exthrai.thread_running = FALSE;

  exthrai.num_players = 0;
}

/**************************************************************************
  This is main function of ai thread.
**************************************************************************/
static void texai_thread_start(void *arg)
{
  bool finished = FALSE;

  log_debug("New AI thread launched");

  /* Just wait until we are signaled to shutdown */
  fc_allocate_mutex(&exthrai.msgs_to.mutex);
  while (!finished) {
    fc_thread_cond_wait(&exthrai.msgs_to.thr_cond, &exthrai.msgs_to.mutex);

    if (texai_check_messages() <= TEXAI_ABORT_EXIT) {
      finished = TRUE;
    }
  }
  fc_release_mutex(&exthrai.msgs_to.mutex);

  log_debug("AI thread exiting");
}

/**************************************************************************
  Handle messages from message queue.
**************************************************************************/
static enum texai_abort_msg_class texai_check_messages(void)
{
  enum texai_abort_msg_class ret_abort= TEXAI_ABORT_NONE;

  texaimsg_list_allocate_mutex(exthrai.msgs_to.msglist);
  while (texaimsg_list_size(exthrai.msgs_to.msglist) > 0) {
    struct texai_msg *msg;
    enum texai_abort_msg_class new_abort = TEXAI_ABORT_NONE;

    msg = texaimsg_list_get(exthrai.msgs_to.msglist, 0);
    texaimsg_list_remove(exthrai.msgs_to.msglist, msg);
    texaimsg_list_release_mutex(exthrai.msgs_to.msglist);

    log_debug("Plr thr got %s", texaimsgtype_name(msg->type));

    switch(msg->type) {
    case TEXAI_MSG_FIRST_ACTIVITIES:
      fc_allocate_mutex(&game.server.mutexes.city_list);

      initialize_infrastructure_cache(msg->plr);

      /* Use _safe iterate in case the main thread
       * destroyes cities while we are iterating through these. */
      city_list_iterate_safe(msg->plr->cities, pcity) {
        texai_city_worker_requests_create(msg->plr, pcity);

        /* Release mutex for a second in case main thread
         * wants to do something to city list. */
        fc_release_mutex(&game.server.mutexes.city_list);

        /* Recursive message check in case phase is finished. */
        new_abort = texai_check_messages();
        fc_allocate_mutex(&game.server.mutexes.city_list);
        if (new_abort < TEXAI_ABORT_NONE) {
          break;
        }
      } city_list_iterate_safe_end;
      fc_release_mutex(&game.server.mutexes.city_list);

      texai_send_req(TEXAI_REQ_TURN_DONE, msg->plr, NULL);

      break;
    case TEXAI_MSG_PHASE_FINISHED:
      new_abort = TEXAI_ABORT_PHASE_END;
      break;
    case TEXAI_MSG_THR_EXIT:
      new_abort = TEXAI_ABORT_EXIT;
      break;
    default:
      log_error("Illegal message type %s (%d) for threaded ai!",
                texaimsgtype_name(msg->type), msg->type);
      break;
    }

    if (new_abort < ret_abort) {
      ret_abort = new_abort;
    }

    FC_FREE(msg);

    texaimsg_list_allocate_mutex(exthrai.msgs_to.msglist);
  }
  texaimsg_list_release_mutex(exthrai.msgs_to.msglist);

  return ret_abort;
}

/**************************************************************************
  Initialize player for use with threxpr AI.
**************************************************************************/
void texai_player_alloc(struct ai_type *ait, struct player *pplayer)
{
  struct texai_plr *player_data = fc_calloc(1, sizeof(struct texai_plr));

  player_set_ai_data(pplayer, ait, player_data);

  /* Default AI */
  dai_data_init(ait, pplayer);
}

/**************************************************************************
  Free player from use with threxpr AI.
**************************************************************************/
void texai_player_free(struct ai_type *ait, struct player *pplayer)
{
  struct texai_plr *player_data = player_ai_data(pplayer, ait);

  /* Default AI */
  dai_data_close(ait, pplayer);

  if (player_data != NULL) {
    player_set_ai_data(pplayer, ait, NULL);
    FC_FREE(player_data);
  }
}

/**************************************************************************
  We actually control the player
**************************************************************************/
void texai_control_gained(struct ai_type *ait, struct player *pplayer)
{
  exthrai.num_players++;

  log_debug("%s now under threaded AI (%d)", pplayer->name,
            exthrai.num_players);

  if (!exthrai.thread_running) {
    exthrai.msgs_to.msglist = texaimsg_list_new();
    exthrai.reqs_from.reqlist = texaireq_list_new();

    exthrai.thread_running = TRUE;
 
    fc_thread_cond_init(&exthrai.msgs_to.thr_cond);
    fc_init_mutex(&exthrai.msgs_to.mutex);
    fc_thread_start(&exthrai.ait, texai_thread_start, NULL);
  }
}

/**************************************************************************
  We no longer control the player
**************************************************************************/
void texai_control_lost(struct ai_type *ait, struct player *pplayer)
{
  exthrai.num_players--;

  log_debug("%s no longer under threaded AI (%d)", pplayer->name,
            exthrai.num_players);

  if (exthrai.num_players <= 0) {
    texai_send_msg(TEXAI_MSG_THR_EXIT, pplayer, NULL);

    fc_thread_wait(&exthrai.ait);
    exthrai.thread_running = FALSE;

    fc_thread_cond_destroy(&exthrai.msgs_to.thr_cond);
    fc_destroy_mutex(&exthrai.msgs_to.mutex);
    texaimsg_list_destroy(exthrai.msgs_to.msglist);
    texaireq_list_destroy(exthrai.reqs_from.reqlist);
  }
}

/**************************************************************************
  Check for messages sent by player thread
**************************************************************************/
void texai_refresh(struct ai_type *ait, struct player *pplayer)
{
  if (exthrai.thread_running) {
    texaireq_list_allocate_mutex(exthrai.reqs_from.reqlist);
    while (texaireq_list_size(exthrai.reqs_from.reqlist) > 0) {
       struct texai_req *req;

       req = texaireq_list_get(exthrai.reqs_from.reqlist, 0);
       texaireq_list_remove(exthrai.reqs_from.reqlist, req);

       texaireq_list_release_mutex(exthrai.reqs_from.reqlist);

       log_debug("Plr thr sent %s", texaireqtype_name(req->type));

       switch(req->type) {
       case TEXAI_REQ_WORKER_TASK:
         texai_req_worker_task_rcv(req);
         break;
       case TEXAI_REQ_TURN_DONE:
         req->plr->ai_phase_done = TRUE;
         break;
       }

       FC_FREE(req);

       texaireq_list_allocate_mutex(exthrai.reqs_from.reqlist);
     }
    texaireq_list_release_mutex(exthrai.reqs_from.reqlist);
  }
}

/**************************************************************************
  Send message to thread. Be sure that thread is running so that messages
  are not just piling up to the list without anybody reading them.
**************************************************************************/
void texai_msg_to_thr(struct texai_msg *msg)
{
  fc_allocate_mutex(&exthrai.msgs_to.mutex);
  texaimsg_list_append(exthrai.msgs_to.msglist, msg);
  fc_thread_cond_signal(&exthrai.msgs_to.thr_cond);
  fc_release_mutex(&exthrai.msgs_to.mutex);
}

/**************************************************************************
  Thread sends message.
**************************************************************************/
void texai_req_from_thr(struct texai_req *req)
{
  texaireq_list_allocate_mutex(exthrai.reqs_from.reqlist);
  texaireq_list_append(exthrai.reqs_from.reqlist, req);
  texaireq_list_release_mutex(exthrai.reqs_from.reqlist);
}

/**************************************************************************
  Return whether player thread is running
**************************************************************************/
bool texai_thread_running(void)
{
  return exthrai.thread_running;
}
