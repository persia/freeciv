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

/**********************************************************************
                          gui_id.h  -  description
                             -------------------
    begin                : Mon Jul 15 2002
    copyright            : (C) 2002 by Rafa� Bursig
    email                : Rafa� Bursig <bursig@poczta.fm>
 **********************************************************************/

#ifndef FC__SDL_GUI_ID_H
#define FC__SDL_GUI_ID_H

enum GUI_ID {
  ID_ERROR = 0,
  ID_LABEL = 1,
  ID_SEPARATOR,
  ID_BUTTON,
  ID_WINDOW,
  ID_MOVED_WINDOW,
  ID_SCROLLBAR,
  ID_ICON,
  ID_EDIT,
  ID_CHECKBOX,
  ID_CLIENT_OPTIONS,
  ID_WAITING_LABEL,
  ID_START_NEW_GAME,
  ID_LOAD_GAME,
  ID_JOIN_GAME,
  ID_JOIN_META_GAME,
  ID_CLIENT_OPTIONS_BUTTON,
  ID_QUIT,
  ID_PLAYER_NAME_EDIT,
  ID_SERVER_NAME_EDIT,
  ID_PORT_EDIT,
  ID_CONNECT_BUTTON,
  ID_META_SERVERS_BUTTON,
  ID_META_SERVERS_WINDOW,
  ID_LAN_SERVERS_WINDOW,
  ID_CANCEL_BUTTON,
  ID_CHATLINE_UP_BUTTON,
  ID_CHATLINE_DOWN_BUTTON,
  ID_CHATLINE_VSCROLLBAR,
  ID_CHATLINE_WINDOW,
  ID_CHATLINE_TOGGLE_LOG_WINDOW_BUTTON,
  ID_CHATLINE_INPUT_EDIT,
  ID_NATION_WIZARD_WINDOW,
  ID_NATION_WIZARD_START_BUTTON,
  ID_NATION_WIZARD_BACK_BUTTON,
  ID_NATION_WIZARD_EXIT_BUTTON,
  ID_NATION_WIZARD_DISCONNECT_BUTTON,
  ID_NATION_WIZARD_LEADER_NAME_EDIT,
  ID_NATION_WIZARD_NEXT_LEADER_NAME_BUTTON,
  ID_NATION_WIZARD_PREV_LEADER_NAME_BUTTON,
  ID_NATION_WIZARD_CHANGE_SEX_BUTTON,
  ID_OPTIONS_WINDOW,
  ID_OPTIONS_VIDEO_BUTTON,
  ID_OPTIONS_RESOLUTION_LABEL,
  ID_OPTIONS_TOGGLE_FULLSCREEN_CHECKBOX,
  ID_OPTIONS_FULLSCREEN_LABEL,
  ID_OPTIONS_SOUND_BUTTON,
  ID_OPTIONS_LOCAL_BUTTON,
  ID_OPTIONS_LOCAL_SOUND_LABEL,
  ID_OPTIONS_LOCAL_SOUND_CHECKBOX,
  ID_OPTIONS_LOCAL_MOVE_LABEL,
  ID_OPTIONS_LOCAL_MOVE_CHECKBOX,
  ID_OPTIONS_LOCAL_MOVE_STEP_LABEL,
  ID_OPTIONS_LOCAL_MOVE_STEP_EDIT,
  ID_OPTIONS_LOCAL_COMBAT_LABEL,
  ID_OPTIONS_LOCAL_COMBAT_CHECKBOX,
  ID_OPTIONS_LOCAL_ACENTER_LABEL,
  ID_OPTIONS_LOCAL_ACENTER_CHECKBOX,
  ID_OPTIONS_LOCAL_COMBAT_CENTER_LABEL,
  ID_OPTIONS_LOCAL_COMBAT_CENTER_CHECKBOX,
  ID_OPTIONS_LOCAL_ACTIVE_UNITS_LABEL,
  ID_OPTIONS_LOCAL_ACTIVE_UNITS_CHECKBOX,
  ID_OPTIONS_LOCAL_CITY_CENTER_LABEL,
  ID_OPTIONS_LOCAL_CITY_CENTER_CHECKBOX,
  ID_OPTIONS_LOCAL_END_TURN_LABEL,
  ID_OPTIONS_LOCAL_END_TURN_CHECKBOX,
  ID_OPTIONS_MAP_BUTTON,
  ID_OPTIONS_MAP_GRID_LABEL,
  ID_OPTIONS_MAP_GRID_CHECKBOX,
  ID_OPTIONS_MAP_CITY_NAMES_LABEL,
  ID_OPTIONS_MAP_CITY_NAMES_CHECKBOX,
  ID_OPTIONS_MAP_CITY_PROD_LABEL,
  ID_OPTIONS_MAP_CITY_PROD_CHECKBOX,
  ID_OPTIONS_MAP_BORDERS_CHECKBOX,
  ID_OPTIONS_MAP_BORDERS_LABEL,
  ID_OPTIONS_MAP_CITY_CIV3_TEXT_STYLE_LABEL,
  ID_OPTIONS_MAP_CITY_CIV3_TEXT_STYLE_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_LABEL,
  ID_OPTIONS_MAP_TERRAIN_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_COAST_LABEL,
  ID_OPTIONS_MAP_TERRAIN_COAST_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_INPR_LABEL,
  ID_OPTIONS_MAP_TERRAIN_RR_LABEL,
  ID_OPTIONS_MAP_TERRAIN_RR_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_IR_LABEL,
  ID_OPTIONS_MAP_TERRAIN_IR_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_M_LABEL,
  ID_OPTIONS_MAP_TERRAIN_M_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_FA_LABEL,
  ID_OPTIONS_MAP_TERRAIN_FA_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_SPEC_LABEL,
  ID_OPTIONS_MAP_TERRAIN_SPEC_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_POLL_LABEL,
  ID_OPTIONS_MAP_TERRAIN_POLL_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_CITY_LABEL,
  ID_OPTIONS_MAP_TERRAIN_CITY_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_UNITS_LABEL,
  ID_OPTIONS_MAP_TERRAIN_UNITS_CHECKBOX,
  ID_OPTIONS_MAP_TERRAIN_FOG_LABEL,
  ID_OPTIONS_MAP_TERRAIN_FOG_CHECKBOX,
  ID_OPTIONS_WORKLIST_BUTTON,
  ID_OPTIONS_DISC_BUTTON,
  ID_OPTIONS_BACK_BUTTON,
  ID_OPTIONS_EXIT_BUTTON,
  ID_UNITS_WINDOW,
  ID_TOGGLE_UNITS_WINDOW_BUTTON,
  ID_MINI_MAP_WINDOW,
  ID_TOGGLE_MAP_WINDOW_BUTTON,
  ID_UNIT_ORDER_BUILD_CITY,
  ID_UNIT_ORDER_BUILD_WONDER,
  ID_UNIT_ORDER_ROAD,
  ID_UNIT_ORDER_TRADEROUTE,
  ID_UNIT_ORDER_IRRIGATE,
  ID_UNIT_ORDER_MINE,
  ID_UNIT_ORDER_TRANSFORM,
  ID_UNIT_ORDER_FORTRESS,
  ID_UNIT_ORDER_FORTIFY,
  ID_UNIT_ORDER_AIRBASE,
  ID_UNIT_ORDER_POLLUTION,
  ID_UNIT_ORDER_PARADROP,
  ID_UNIT_ORDER_FALLOUT,
  ID_UNIT_ORDER_SENTRY,
  ID_UNIT_ORDER_PILLAGE,
  ID_UNIT_ORDER_HOMECITY,
  ID_UNIT_ORDER_UNLOAD,
  ID_UNIT_ORDER_WAKEUP_OTHERS,
  ID_UNIT_ORDER_AUTOMATION,
  ID_UNIT_ORDER_AUTO_EXPLORE,
  ID_UNIT_ORDER_CONNECT,
  ID_UNIT_ORDER_PATROL,
  ID_UNIT_ORDER_GOTO,
  ID_UNIT_ORDER_GOTO_CITY,
  ID_UNIT_ORDER_AIRLIFT,
  ID_UNIT_ORDER_RETURN,
  ID_UNIT_ORDER_UPGRADE,
  ID_UNIT_ORDER_DISBAND,
  ID_UNIT_ORDER_DIPLOMAT_DLG,
  ID_UNIT_ORDER_NUKE,
  ID_UNIT_ORDER_WAIT,
  ID_UNIT_ORDER_DONE,
  ID_NEWCITY_NAME_WINDOW,
  ID_NEWCITY_NAME_EDIT,
  ID_NEWCITY_NAME_OK_BUTTON,
  ID_NEWCITY_NAME_CANCEL_BUTTON,
  ID_NEWCITY_NAME_LABEL,
  ID_CITIES,
  ID_PLAYERS,
  ID_UNITS,
  ID_REVOLUTION,
  ID_ECONOMY,
  ID_ECONOMY_DIALOG_WINDOW,
  ID_UNITS_DIALOG_WINDOW,
  ID_RESEARCH,
  ID_NEW_TURN,
  ID_COOLING_ICON,
  ID_WARMING_ICON,
  ID_REVOLUTION_DLG_WINDOW,
  ID_REVOLUTION_DLG_LABEL,
  ID_REVOLUTION_DLG_OK_BUTTON,
  ID_REVOLUTION_DLG_CANCEL_BUTTON,
  ID_GOVERNMENT_DLG_WINDOW,
  ID_CITY_DLG_WINDOW,
  ID_CITY_DLG_EXIT_BUTTON,
  ID_CITY_DLG_CITIZENS_LABEL,
  ID_CITY_DLG_MAP_LABEL,
  ID_CITY_DLG_INFO_BUTTON,
  ID_CITY_DLG_HAPPY_BUTTON,
  ID_CITY_DLG_ARMY_BUTTON,
  ID_CITY_DLG_SUPPORT_BUTTON,
  ID_CITY_DLG_CURRENTLY_BUILDING_LABEL,
  ID_CITY_DLG_BUILDING_LABEL,
  ID_CITY_DLG_BUILDING_UP_BUTTON,
  ID_CITY_DLG_BUILDING_DOWN_BUTTON,
  ID_CITY_DLG_BUILDING_VSCROLLBAR,
  ID_CITY_DLG_RESOURCE_MAP,
  ID_CITY_DLG_CHANGE_PROD_BUTTON,
  ID_CITY_DLG_PROD_Q_BUTTON,
  ID_CITY_DLG_PROD_BUY_BUTTON,
  ID_CITY_DLG_CMA_BUTTON,
  ID_CITY_DLG_PREV_BUTTON,
  ID_CITY_DLG_NEXT_BUTTON,
  ID_CITY_DLG_NAME_EDIT,
  ID_CITY_DLG_OPTIONS_BUTTON,
  ID_CHANGE_PROD_DLG_WINDOW,
  ID_CHANGE_PROD_DLG_UP_BUTTON,
  ID_CHANGE_PROD_DLG_DOWN_BUTTON,
  ID_CHANGE_PROD_DLG_VSCROLLBAR,
  ID_CHANGE_PROD_DLG_EXIT_BUTTON,
  ID_CHANGE_TAXRATE_DLG_WINDDOW,
  ID_CHANGE_TAXRATE_DLG_TAX_SCROLLBAR,
  ID_CHANGE_TAXRATE_DLG_LUX_SCROLLBAR,
  ID_CHANGE_TAXRATE_DLG_SCI_SCROLLBAR,
  ID_CHANGE_TAXRATE_DLG_TAX_LABEL,
  ID_CHANGE_TAXRATE_DLG_LUX_LABEL,
  ID_CHANGE_TAXRATE_DLG_SCI_LABEL,
  ID_CHANGE_TAXRATE_DLG_TAX_BLOCK_CHECKBOX,
  ID_CHANGE_TAXRATE_DLG_LUX_BLOCK_CHECKBOX,
  ID_CHANGE_TAXRATE_DLG_SCI_BLOCK_CHECKBOX,
  ID_CHANGE_TAXRATE_DLG_GOVERMENT_LABEL,
  ID_CHANGE_TAXRATE_DLG_OK_BUTTON,
  ID_CHANGE_TAXRATE_DLG_CANCEL_BUTTON,
  ID_SCIENCE_DLG_WINDOW,
  ID_SCIENCE_CANCEL_DLG_BUTTON,
  ID_SCIENCE_DLG_CHANGE_REASARCH_BUTTON,
  ID_SCIENCE_DLG_CHANGE_REASARCH_WINDOW,
  ID_SCIENCE_DLG_CHANGE_GOAL_BUTTON,
  ID_SCIENCE_DLG_CHANGE_GOAL_WINDOW,
  ID_SCIENCE_DLG_CHANGE_GOAL_UP_BUTTON,
  ID_SCIENCE_DLG_CHANGE_GOAL_DOWN_BUTTON,
  ID_SCIENCE_DLG_CHANGE_GOAL_CANCEL_BUTTON,
  ID_UNIT_SELLECT_DLG_WINDOW,
  ID_UNIT_SELLECT_DLG_UP_BUTTON,
  ID_UNIT_SELLECT_DLG_DOWN_BUTTON,
  ID_UNIT_SELLECT_DLG_EXIT_BUTTON,
  ID_UNIT_SELLECT_DLG_VSCROLLBAR,
  ID_TERRAIN_INFO_DLG_WINDOW,
  ID_TERRAIN_INFO_DLG_EXIT_BUTTON,
  ID_TERRAIN_ADV_DLG_WINDOW,
  ID_TERRAIN_ADV_DLG_EXIT_BUTTON,
  ID_PILLAGE_DLG_WINDOW,
  ID_PILLAGE_DLG_EXIT_BUTTON,
  ID_CARAVAN_DLG_WINDOW,
  ID_CARAVAN_DLG_EXIT_BUTTON,
  ID_INCITE_DLG_WINDOW,
  ID_INCITE_DLG_EXIT_BUTTON,
  ID_CONNECT_DLG_WINDOW,
  ID_CONNECT_DLG_EXIT_BUTTON,
  ID_BRIBE_DLG_WINDOW,
  ID_BRIBE_DLG_EXIT_BUTTON,
  ID_ADD_NEW_WORKLIST
};

#endif
