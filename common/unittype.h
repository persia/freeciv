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
#ifndef FC__UNITTYPE_H
#define FC__UNITTYPE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* utility */
#include "bitvector.h"
#include "shared.h"

/* common */
#include "fc_types.h"
#include "name_translation.h"

struct astring;         /* Actually defined in "utility/astring.h". */
struct strvec;          /* Actually defined in "utility/string_vector.h". */

struct ai_type;

/* U_LAST is a value which is guaranteed to be larger than all
 * actual Unit_type_id values. It is used as a flag value;
 * it can also be used for fixed allocations to ensure able
 * to hold full number of unit types.
 * Used in the network protocol. */
#define U_LAST MAX_NUM_ITEMS

/* Used in the network protocol. */
#define SPECENUM_NAME unit_class_flag_id
#define SPECENUM_VALUE0 UCF_TERRAIN_SPEED
#define SPECENUM_VALUE0NAME "TerrainSpeed"
#define SPECENUM_VALUE1 UCF_TERRAIN_DEFENSE
#define SPECENUM_VALUE1NAME "TerrainDefense"
#define SPECENUM_VALUE2 UCF_DAMAGE_SLOWS
#define SPECENUM_VALUE2NAME "DamageSlows"
/* Can occupy enemy cities */
#define SPECENUM_VALUE3 UCF_CAN_OCCUPY_CITY
#define SPECENUM_VALUE3NAME "CanOccupyCity"
#define SPECENUM_VALUE4 UCF_MISSILE
#define SPECENUM_VALUE4NAME "Missile"
#define SPECENUM_VALUE5 UCF_BUILD_ANYWHERE
#define SPECENUM_VALUE5NAME "BuildAnywhere"
#define SPECENUM_VALUE6 UCF_UNREACHABLE
#define SPECENUM_VALUE6NAME "Unreachable"
/* Can collect ransom from barbarian leader */
#define SPECENUM_VALUE7 UCF_COLLECT_RANSOM
#define SPECENUM_VALUE7NAME "CollectRansom"
/* Is subject to ZOC */
#define SPECENUM_VALUE8 UCF_ZOC
#define SPECENUM_VALUE8NAME "ZOC"
/* Can fortify on land squares */
#define SPECENUM_VALUE9 UCF_CAN_FORTIFY
#define SPECENUM_VALUE9NAME "CanFortify"
#define SPECENUM_VALUE10 UCF_CAN_PILLAGE
#define SPECENUM_VALUE10NAME "CanPillage"
/* Cities can still work tile when enemy unit on it */
#define SPECENUM_VALUE11 UCF_DOESNT_OCCUPY_TILE
#define SPECENUM_VALUE11NAME "DoesntOccupyTile"
/* Can attack against units on non-native tiles */
#define SPECENUM_VALUE12 UCF_ATTACK_NON_NATIVE
#define SPECENUM_VALUE12NAME "AttackNonNative"
/* Can launch attack from non-native tile (to native tile) */
#define SPECENUM_VALUE13 UCF_ATT_FROM_NON_NATIVE
#define SPECENUM_VALUE13NAME "AttFromNonNative"
/* Kills citizens upon successful attack against a city */
#define SPECENUM_VALUE14 UCF_KILLCITIZEN
#define SPECENUM_VALUE14NAME "KillCitizen"
/* Can be airlifted */
#define SPECENUM_VALUE15 UCF_AIRLIFTABLE
#define SPECENUM_VALUE15NAME "Airliftable"
/* keep this last */
#define SPECENUM_COUNT UCF_COUNT
#define SPECENUM_BITVECTOR bv_unit_class_flags
#include "specenum_gen.h"

/* Used in savegame processing and clients. */
#define SPECENUM_NAME unit_move_type
#define SPECENUM_VALUE0 UMT_LAND
#define SPECENUM_VALUE0NAME "Land"
#define SPECENUM_VALUE1 UMT_SEA
#define SPECENUM_VALUE1NAME "Sea"
#define SPECENUM_VALUE2 UMT_BOTH
#define SPECENUM_VALUE2NAME "Both"
#include "specenum_gen.h"

/* Used in the network protocol. */
BV_DEFINE(bv_unit_classes, UCL_LAST);

enum hut_behavior { HUT_NORMAL, HUT_NOTHING, HUT_FRIGHTEN };

enum move_level { MOVE_NONE, MOVE_PARTIAL, MOVE_FULL };

struct extra_type_list;
struct unit_class_list;

struct unit_class {
  Unit_Class_id item_number;
  struct name_translation name;
  enum unit_move_type move_type;
  int min_speed;           /* Minimum speed after damage and effects */
  int hp_loss_pct;         /* Percentage of hitpoints lost each turn not in city or airbase */
  int non_native_def_pct;
  enum hut_behavior hut_behavior;
  bv_unit_class_flags flags;

  struct {
    enum move_level land_move;
    enum move_level sea_move;
  } adv;

  struct {
    struct extra_type_list *refuel_bases;
    struct extra_type_list *native_tile_extras;
    struct unit_class_list *subset_movers;
  } cache;
};

/* Unit "special effects" flags:
 * Note this is now an enumerated type, and not power-of-two integers
 * for bits, though unit_type.flags is still a bitfield, and code
 * which uses unit_has_type_flag() without twiddling bits is unchanged.
 * (It is easier to go from i to (1<<i) than the reverse.)
 * See data/default/units.ruleset for documentation of their effects.
 * Change the array *flag_names[] in unittype.c accordingly.
 * Used in the network protocol.
 */
#define SPECENUM_NAME unit_type_flag_id
#define SPECENUM_VALUE0 UTYF_TRADE_ROUTE
#define SPECENUM_VALUE0NAME N_("TradeRoute")
#define SPECENUM_VALUE1 UTYF_HELP_WONDER
#define SPECENUM_VALUE1NAME N_("HelpWonder")
#define SPECENUM_VALUE2 UTYF_IGZOC
#define SPECENUM_VALUE2NAME N_("IgZOC")
#define SPECENUM_VALUE3 UTYF_CIVILIAN
#define SPECENUM_VALUE3NAME N_("NonMil")
#define SPECENUM_VALUE4 UTYF_IGTER
#define SPECENUM_VALUE4NAME N_("IgTer")
#define SPECENUM_VALUE5 UTYF_ONEATTACK
#define SPECENUM_VALUE5NAME N_("OneAttack")
#define SPECENUM_VALUE6 UTYF_FIELDUNIT
#define SPECENUM_VALUE6NAME N_("FieldUnit")
#define SPECENUM_VALUE7 UTYF_MARINES
#define SPECENUM_VALUE7NAME N_("Marines")
/* Invisibile except when adjacent (Submarine) */
#define SPECENUM_VALUE8 UTYF_PARTIAL_INVIS
#define SPECENUM_VALUE8NAME N_("Partial_Invis")
/* Does not include ability to found cities */
#define SPECENUM_VALUE9 UTYF_SETTLERS
#define SPECENUM_VALUE9NAME N_("Settlers")
#define SPECENUM_VALUE10 UTYF_DIPLOMAT
#define SPECENUM_VALUE10NAME N_("Diplomat")
/* Trireme sinking effect */
#define SPECENUM_VALUE11 UTYF_TRIREME
#define SPECENUM_VALUE11NAME N_("Trireme")
/* Nuclear attack effect */
#define SPECENUM_VALUE12 UTYF_NUCLEAR
#define SPECENUM_VALUE12NAME N_("Nuclear")
/* Enhanced spy abilities */
#define SPECENUM_VALUE13 UTYF_SPY
#define SPECENUM_VALUE13NAME N_("Spy")
#define SPECENUM_VALUE14 UTYF_PARATROOPERS
#define SPECENUM_VALUE14NAME N_("Paratroopers")
/* Can build cities */
#define SPECENUM_VALUE15 UTYF_CITIES
#define SPECENUM_VALUE15NAME N_("Cities")
/* Cannot attack vs non-native tiles even if class can */
#define SPECENUM_VALUE16 UTYF_ONLY_NATIVE_ATTACK
#define SPECENUM_VALUE16NAME N_("Only_Native_Attack")
/* unit can add to city population */
#define SPECENUM_VALUE17 UTYF_ADD_TO_CITY
#define SPECENUM_VALUE17NAME N_("AddToCity")
/* Only Fundamentalist government can build these units */
#define SPECENUM_VALUE18 UTYF_FANATIC
#define SPECENUM_VALUE18NAME N_("Fanatic")
/* Losing this unit means losing the game */
#define SPECENUM_VALUE19 UTYF_GAMELOSS
#define SPECENUM_VALUE19NAME N_("GameLoss")
/* A player can only have one unit of this type */
#define SPECENUM_VALUE20 UTYF_UNIQUE
#define SPECENUM_VALUE20NAME N_("Unique")
/* Cannot be disbanded, won't easily go away */
#define SPECENUM_VALUE21 UTYF_UNDISBANDABLE
#define SPECENUM_VALUE21NAME N_("Undisbandable")
/* Always wins diplomatic contests */
#define SPECENUM_VALUE22 UTYF_SUPERSPY
#define SPECENUM_VALUE22NAME N_("SuperSpy")
/* Has no homecity */
#define SPECENUM_VALUE23 UTYF_NOHOME
#define SPECENUM_VALUE23NAME N_("NoHome")
/* Cannot increase veteran level */
#define SPECENUM_VALUE24 UTYF_NO_VETERAN
#define SPECENUM_VALUE24NAME N_("NoVeteran")
/* Has the ability to bombard */
#define SPECENUM_VALUE25 UTYF_BOMBARDER
#define SPECENUM_VALUE25NAME N_("Bombarder")
/* Gets double firepower against cities */
#define SPECENUM_VALUE26 UTYF_CITYBUSTER
#define SPECENUM_VALUE26NAME N_("CityBuster")
/* Unit cannot be built (barb leader etc) */
#define SPECENUM_VALUE27 UTYF_NOBUILD
#define SPECENUM_VALUE27NAME N_("NoBuild")
/* Firepower set to 1 when EFT_DEFEND_BONUS applies
 * (for example, land unit attacking city with walls) */
#define SPECENUM_VALUE28 UTYF_BADWALLATTACKER
#define SPECENUM_VALUE28NAME N_("BadWallAttacker")
/* Firepower set to 1 and attackers x2 when in city */
#define SPECENUM_VALUE29 UTYF_BADCITYDEFENDER
#define SPECENUM_VALUE29NAME N_("BadCityDefender")
/* Only barbarians can build this unit */
#define SPECENUM_VALUE30 UTYF_BARBARIAN_ONLY
#define SPECENUM_VALUE30NAME N_("BarbarianOnly")
/* upkeep can switch from shield to gold */
#define SPECENUM_VALUE31 UTYF_SHIELD2GOLD
#define SPECENUM_VALUE31NAME N_("Shield2Gold")
/* Unit can be captured */
#define SPECENUM_VALUE32 UTYF_CAPTURABLE
#define SPECENUM_VALUE32NAME N_("Capturable")
/* Unit can capture other */
#define SPECENUM_VALUE33 UTYF_CAPTURER
#define SPECENUM_VALUE33NAME N_("Capturer")
/* Unit has no ZOC */
#define SPECENUM_VALUE34 UTYF_NOZOC
#define SPECENUM_VALUE34NAME N_("HasNoZOC")
/* Cannot fortify even if class can */
#define SPECENUM_VALUE35 UTYF_CANT_FORTIFY
#define SPECENUM_VALUE35NAME N_("Cant_Fortify")

#define SPECENUM_VALUE36 UTYF_USER_FLAG_1
#define SPECENUM_VALUE37 UTYF_USER_FLAG_2
#define SPECENUM_VALUE38 UTYF_USER_FLAG_3
#define SPECENUM_VALUE39 UTYF_USER_FLAG_4
#define SPECENUM_VALUE40 UTYF_USER_FLAG_5
#define SPECENUM_VALUE41 UTYF_USER_FLAG_6
#define SPECENUM_VALUE42 UTYF_USER_FLAG_7
#define SPECENUM_VALUE43 UTYF_USER_FLAG_8
#define SPECENUM_VALUE44 UTYF_USER_FLAG_9
#define SPECENUM_VALUE45 UTYF_USER_FLAG_10
#define SPECENUM_VALUE46 UTYF_USER_FLAG_11
#define SPECENUM_VALUE47 UTYF_USER_FLAG_12
#define SPECENUM_VALUE48 UTYF_USER_FLAG_13
#define SPECENUM_VALUE49 UTYF_USER_FLAG_14
#define SPECENUM_VALUE50 UTYF_USER_FLAG_15
#define SPECENUM_VALUE51 UTYF_USER_FLAG_16
#define SPECENUM_VALUE52 UTYF_USER_FLAG_17
#define SPECENUM_VALUE53 UTYF_USER_FLAG_18
#define SPECENUM_VALUE54 UTYF_USER_FLAG_19
#define SPECENUM_VALUE55 UTYF_USER_FLAG_20
#define SPECENUM_VALUE56 UTYF_USER_FLAG_21
#define SPECENUM_VALUE57 UTYF_USER_FLAG_22
#define SPECENUM_VALUE58 UTYF_USER_FLAG_23
#define SPECENUM_VALUE59 UTYF_USER_FLAG_24
#define SPECENUM_VALUE60 UTYF_USER_FLAG_25
#define SPECENUM_VALUE61 UTYF_USER_FLAG_26
#define SPECENUM_VALUE62 UTYF_USER_FLAG_27
#define SPECENUM_VALUE63 UTYF_USER_FLAG_28
#define SPECENUM_VALUE64 UTYF_USER_FLAG_29
#define SPECENUM_VALUE65 UTYF_USER_FLAG_30
#define SPECENUM_VALUE66 UTYF_USER_FLAG_31
#define SPECENUM_VALUE67 UTYF_USER_FLAG_32
/* Note that first role must have value next to last flag */

#define UTYF_LAST_USER_FLAG UTYF_USER_FLAG_32
#define MAX_NUM_USER_UNIT_FLAGS (UTYF_LAST_USER_FLAG - UTYF_USER_FLAG_1 + 1)
#define SPECENUM_NAMEOVERRIDE
#define SPECENUM_BITVECTOR bv_unit_type_flags
#include "specenum_gen.h"


/* Unit "roles": these are similar to unit flags but differ in that
   they don't represent intrinsic properties or abilities of units,
   but determine which units are used (mainly by the server or AI)
   in various circumstances, or "roles".
   Note that in some cases flags can act as roles, eg, we don't need
   a role for "settlers", because we can just use UTYF_SETTLERS.
   (Now have to consider UTYF_CITIES too)
   So we make sure flag values and role values are distinct,
   so some functions can use them interchangably.
   See data/classic/units.ruleset for documentation of their effects.
*/
#define L_FIRST (UTYF_LAST_USER_FLAG + 1)

#define SPECENUM_NAME unit_role_id
/* is built first when city established */
#define SPECENUM_VALUE68 L_FIRSTBUILD
#define SPECENUM_VALUE68NAME N_("FirstBuild")
/* initial explorer unit */
#define SPECENUM_VALUE69 L_EXPLORER
#define SPECENUM_VALUE69NAME N_("Explorer")
/* can be found in hut */
#define SPECENUM_VALUE70 L_HUT
#define SPECENUM_VALUE70NAME N_("Hut")
/* can be found in hut, global tech required */
#define SPECENUM_VALUE71 L_HUT_TECH
#define SPECENUM_VALUE71NAME N_("HutTech")
/* is created in Partisan circumstances */
#define SPECENUM_VALUE72 L_PARTISAN
#define SPECENUM_VALUE72NAME N_("Partisan")
/* ok on defense (AI) */
#define SPECENUM_VALUE73 L_DEFEND_OK
#define SPECENUM_VALUE73NAME N_("DefendOk")
/* primary purpose is defense (AI) */
#define SPECENUM_VALUE74 L_DEFEND_GOOD
#define SPECENUM_VALUE74NAME N_("DefendGood")
/* quick attacking unit (Horse..Armor) (unused)*/
#define SPECENUM_VALUE75 L_ATTACK_FAST
#define SPECENUM_VALUE75NAME N_("AttackFast")
/* powerful attacking unit (Catapult..) (unused) */
#define SPECENUM_VALUE76 L_ATTACK_STRONG
#define SPECENUM_VALUE76NAME N_("AttackStrong")
/* is useful for ferrying (AI) */
#define SPECENUM_VALUE77 L_FERRYBOAT
#define SPECENUM_VALUE77NAME N_("FerryBoat")
/* barbarians unit, land only */
#define SPECENUM_VALUE78 L_BARBARIAN
#define SPECENUM_VALUE78NAME N_("Barbarian")
/* barbarians unit, global tech required */
#define SPECENUM_VALUE79 L_BARBARIAN_TECH
#define SPECENUM_VALUE79NAME N_("BarbarianTech")
/* barbarian boat */
#define SPECENUM_VALUE80 L_BARBARIAN_BOAT
#define SPECENUM_VALUE80NAME N_("BarbarianBoat")
/* what barbarians should build */
#define SPECENUM_VALUE81 L_BARBARIAN_BUILD
#define SPECENUM_VALUE81NAME N_("BarbarianBuild")
/* barbarians build when global tech */
#define SPECENUM_VALUE82 L_BARBARIAN_BUILD_TECH
#define SPECENUM_VALUE82NAME N_("BarbarianBuildTech")
/* barbarian leader */
#define SPECENUM_VALUE83 L_BARBARIAN_LEADER
#define SPECENUM_VALUE83NAME N_("BarbarianLeader")
/* sea raider unit */
#define SPECENUM_VALUE84 L_BARBARIAN_SEA
#define SPECENUM_VALUE84NAME N_("BarbarianSea")
/* sea raider unit, global tech required */
#define SPECENUM_VALUE85 L_BARBARIAN_SEA_TECH
#define SPECENUM_VALUE85NAME N_("BarbarianSeaTech")
/* can found cities */
#define SPECENUM_VALUE86 L_CITIES
#define SPECENUM_VALUE86NAME N_("Cities")
/* can improve terrain */
#define SPECENUM_VALUE87 L_SETTLERS
#define SPECENUM_VALUE87NAME N_("Settlers")
/* loss results in loss of game */
#define SPECENUM_VALUE88 L_GAMELOSS
#define SPECENUM_VALUE88NAME N_("GameLoss")
/* can do diplomat actions */
#define SPECENUM_VALUE89 L_DIPLOMAT
#define SPECENUM_VALUE89NAME N_("Diplomat")
/* AI hunter type unit */
#define SPECENUM_VALUE90 L_HUNTER
#define SPECENUM_VALUE90NAME N_("Hunter")
#define L_LAST (L_HUNTER+1)

#include "specenum_gen.h"

#define L_MAX 64 /* Used in the network protocol. */

FC_STATIC_ASSERT(L_LAST - L_FIRST <= L_MAX, too_many_unit_roles);

/* Used in the network protocol. */
BV_DEFINE(bv_unit_type_roles, L_MAX);

/* Used in the network protocol. */
#define SPECENUM_NAME combat_bonus_type
#define SPECENUM_VALUE0 CBONUS_DEFENSE_MULTIPLIER
#define SPECENUM_VALUE0NAME "DefenseMultiplier"
#define SPECENUM_VALUE1 CBONUS_DEFENSE_DIVIDER
#define SPECENUM_VALUE1NAME "DefenseDivider"
#define SPECENUM_VALUE2 CBONUS_FIREPOWER1
#define SPECENUM_VALUE2NAME "Firepower1"
#include "specenum_gen.h"

struct combat_bonus {
  enum unit_type_flag_id  flag;
  enum combat_bonus_type  type;
  int                     value;
};

/* get 'struct combat_bonus_list' and related functions: */
#define SPECLIST_TAG combat_bonus
#define SPECLIST_TYPE struct combat_bonus
#include "speclist.h"

#define combat_bonus_list_iterate(bonuslist, pbonus) \
    TYPED_LIST_ITERATE(struct combat_bonus, bonuslist, pbonus)
#define combat_bonus_list_iterate_end LIST_ITERATE_END

BV_DEFINE(bv_unit_types, U_LAST);

struct veteran_level {
  struct name_translation name; /* level/rank name */
  int power_fact; /* combat/work speed/diplomatic power factor (in %) */
  int move_bonus;
  int raise_chance; /* server only */
  int work_raise_chance; /* server only */
};

struct veteran_system {
  int levels;

  struct veteran_level *definitions;
};

struct unit_type {
  Unit_type_id item_number;
  struct name_translation name;
  bool disabled;                        /* Does not really exist - hole in improvments array */
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  char sound_move[MAX_LEN_NAME];
  char sound_move_alt[MAX_LEN_NAME];
  char sound_fight[MAX_LEN_NAME];
  char sound_fight_alt[MAX_LEN_NAME];
  int build_cost;			/* Use wrappers to access this. */
  int pop_cost;  /* number of workers the unit contains (e.g., settlers, engineers)*/
  int attack_strength;
  int defense_strength;
  int move_rate;
  int unknown_move_cost; /* See utype_unknown_move_cost(). */

  struct advance *require_advance;	/* may be NULL */
  struct impr_type *need_improvement;	/* may be NULL */
  struct government *need_government;	/* may be NULL */

  int vision_radius_sq;
  int transport_capacity;
  int hp;
  int firepower;
  struct combat_bonus_list *bonuses;

#define U_NOT_OBSOLETED (NULL)
  struct unit_type *obsoleted_by;
  struct unit_type *converted_to;
  int convert_time;
  int fuel;

  bv_unit_type_flags flags;
  bv_unit_type_roles roles;

  int happy_cost;  /* unhappy people in home city */
  int upkeep[O_LAST];

  int paratroopers_range; /* only valid for F_PARATROOPERS */
  int paratroopers_mr_req;
  int paratroopers_mr_sub;

  /* Additional values for the expanded veteran system */
  struct veteran_system *veteran;

  /* Values for bombardment */
  int bombard_rate;

  /* Values for founding cities */
  int city_size;

  struct unit_class *uclass;

  bv_unit_classes cargo;

  /* Can attack these classes even if they are otherwise "Unreachable" */
  bv_unit_classes targets;
  /* Can load into these class transports at any location,
   * even if they are otherwise "Unreachable". */
  bv_unit_classes embarks;
  /* Can unload from these class transports at any location,
   * even if they are otherwise "Unreachable". */
  bv_unit_classes disembarks;

  struct strvec *helptext;

  struct {
    bool igwall;
  } adv;

  void *ais[FC_AI_LAST];
};

/* General unit and unit type (matched) routines */
Unit_type_id utype_count(void);
Unit_type_id utype_index(const struct unit_type *punittype);
Unit_type_id utype_number(const struct unit_type *punittype);

struct unit_type *unit_type(const struct unit *punit);
struct unit_type *utype_by_number(const Unit_type_id id);

struct unit_type *unit_type_by_rule_name(const char *name);
struct unit_type *unit_type_by_translated_name(const char *name);

const char *unit_rule_name(const struct unit *punit);
const char *utype_rule_name(const struct unit_type *punittype);

const char *unit_name_translation(const struct unit *punit);
const char *utype_name_translation(const struct unit_type *punittype);

const char *utype_values_string(const struct unit_type *punittype);
const char *utype_values_translation(const struct unit_type *punittype);

/* General unit type flag and role routines */
bool unit_has_type_flag(const struct unit *punit, enum unit_type_flag_id flag);
bool utype_has_flag(const struct unit_type *punittype, int flag);

bool unit_has_type_role(const struct unit *punit, enum unit_role_id role);
bool utype_has_role(const struct unit_type *punittype, int role);

void user_unit_type_flags_init(void);
void set_user_unit_type_flag_name(enum unit_type_flag_id id, const char *name,
                                  const char *helptxt);
const char *unit_type_flag_helptxt(enum unit_type_flag_id id);

bool unit_can_take_over(const struct unit *punit);
bool utype_can_take_over(const struct unit_type *punittype);

bool utype_can_freely_load(const struct unit_type *pcargotype,
                           const struct unit_type *ptranstype);
bool utype_can_freely_unload(const struct unit_type *pcargotype,
                             const struct unit_type *ptranstype);

bool is_actor_unit_type(const struct unit_type *putype);
bool utype_can_do_action(const struct unit_type *putype,
                         const int action_id);
bool utype_acts_hostile(const struct unit_type *putype);

bool can_unit_act_when_ustate_is(const struct unit_type *punit_type,
                                 const enum ustate_prop prop,
                                 const bool is_there);
bool can_utype_do_act_if_tgt_diplrel(const struct unit_type *punit_type,
                                     const int action_id,
                                     const int prop,
                                     const bool is_there);

/* Functions to operate on various flag and roles. */
typedef bool (*role_unit_callback)(struct unit_type *ptype, void *data);

void role_unit_precalcs(void);
void role_unit_precalcs_free(void);
int num_role_units(int role);
struct unit_type *role_units_iterate(int role, role_unit_callback cb, void *data);
struct unit_type *role_units_iterate_backwards(int role, role_unit_callback cb, void *data);
struct unit_type *get_role_unit(int role, int index);
struct unit_type *best_role_unit(const struct city *pcity, int role);
struct unit_type *best_role_unit_for_player(const struct player *pplayer,
					    int role);
struct unit_type *first_role_unit_for_player(const struct player *pplayer,
					     int role);
bool role_units_translations(struct astring *astr, int flag, bool alts);

/* General unit class routines */
Unit_Class_id uclass_count(void);
Unit_Class_id uclass_number(const struct unit_class *pclass);
/* Optimised to be identical to uclass_number: the implementation
 * unittype.c is also semantically correct. */
#define uclass_index(_c_) (_c_)->item_number
#ifndef uclass_index
Unit_Class_id uclass_index(const struct unit_class *pclass);
#endif /* uclass_index */

struct unit_class *unit_class(const struct unit *punit);
struct unit_class *uclass_by_number(const Unit_Class_id id);
#define utype_class(_t_) (_t_)->uclass
#ifndef utype_class
struct unit_class *utype_class(const struct unit_type *punittype);
#endif /* utype_class */

struct unit_class *unit_class_by_rule_name(const char *s);

const char *uclass_rule_name(const struct unit_class *pclass);
const char *uclass_name_translation(const struct unit_class *pclass);

bool uclass_has_flag(const struct unit_class *punitclass,
                     enum unit_class_flag_id flag);

/* Ancillary routines */
int unit_build_shield_cost(const struct unit *punit);
int utype_build_shield_cost(const struct unit_type *punittype);

int utype_buy_gold_cost(const struct unit_type *punittype,
			int shields_in_stock);

const struct veteran_system *
  utype_veteran_system(const struct unit_type *punittype);
int utype_veteran_levels(const struct unit_type *punittype);
const struct veteran_level *
  utype_veteran_level(const struct unit_type *punittype, int level);
const char *utype_veteran_name_translation(const struct unit_type *punittype,
                                           int level);
bool utype_veteran_has_power_bonus(const struct unit_type *punittype);

struct veteran_system *veteran_system_new(int count);
void veteran_system_destroy(struct veteran_system *vsystem);
void veteran_system_definition(struct veteran_system *vsystem, int level,
                               const char *vlist_name, int vlist_power,
                               int vlist_move, int vlist_raise,
                               int vlist_wraise);

int unit_disband_shields(const struct unit *punit);
int utype_disband_shields(const struct unit_type *punittype);

int unit_pop_value(const struct unit *punit);
int utype_pop_value(const struct unit_type *punittype);

enum unit_move_type utype_move_type(const struct unit_type *punittype);
void set_unit_move_type(struct unit_class *puclass);

/* player related unit functions */
int utype_upkeep_cost(const struct unit_type *ut, struct player *pplayer,
                      Output_type_id otype);
int utype_happy_cost(const struct unit_type *ut, const struct player *pplayer);

struct unit_type *can_upgrade_unittype(const struct player *pplayer,
				       struct unit_type *punittype);
int unit_upgrade_price(const struct player *pplayer,
		       const struct unit_type *from,
		       const struct unit_type *to);

bool can_player_build_unit_direct(const struct player *p,
				  const struct unit_type *punittype);
bool can_player_build_unit_later(const struct player *p,
				 const struct unit_type *punittype);
bool can_player_build_unit_now(const struct player *p,
			       const struct unit_type *punittype);

#define utype_fuel(ptype) (ptype)->fuel

/* Initialization and iteration */
void unit_types_init(void);
void unit_types_free(void);
void unit_type_flags_free(void);

struct unit_type *unit_type_array_first(void);
const struct unit_type *unit_type_array_last(void);

#define unit_type_iterate(_p)						\
{									\
  struct unit_type *_p = unit_type_array_first();			\
  if (NULL != _p) {							\
    for (; _p <= unit_type_array_last(); _p++) {

#define unit_type_iterate_end						\
    }									\
  }									\
}

void *utype_ai_data(const struct unit_type *ptype, const struct ai_type *ai);
void utype_set_ai_data(struct unit_type *ptype, const struct ai_type *ai,
                       void *data);

void unit_type_action_cache_set(struct unit_type *ptype);

/* Initialization and iteration */
void unit_classes_init(void);
void unit_classes_free(void);

void set_unit_class_caches(struct unit_class *pclass);

struct unit_class *unit_class_array_first(void);
const struct unit_class *unit_class_array_last(void);

#define unit_class_iterate(_p)						\
{									\
  struct unit_class *_p = unit_class_array_first();			\
  if (NULL != _p) {							\
    for (; _p <= unit_class_array_last(); _p++) {

#define unit_class_iterate_end						\
    }									\
  }									\
}

#define SPECLIST_TAG unit_class
#define SPECLIST_TYPE struct unit_class
#include "speclist.h"

#define unit_class_list_iterate(uclass_list, pclass) \
  TYPED_LIST_ITERATE(struct unit_class, uclass_list, pclass)
#define unit_class_list_iterate_end LIST_ITERATE_END

#define SPECLIST_TAG unit_type
#define SPECLIST_TYPE struct unit_type
#include "speclist.h"

#define unit_type_list_iterate(utype_list, ptype) \
  TYPED_LIST_ITERATE(struct unit_type, utype_list, ptype)
#define unit_type_list_iterate_end LIST_ITERATE_END

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FC__UNITTYPE_H */
