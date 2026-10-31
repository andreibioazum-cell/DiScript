#include "state/globals/core/config.inc"
#include "state/globals/core/config_part2.inc"
#include "state/globals/core/entities.inc"
#include "state/globals/core/ui.inc"
#include "state/globals/ui/progress_classes.inc"
#include "state/globals/ui/promo.inc"
#include "state/globals/ui/layout.inc"
#include "state/globals/ui/chat.inc"
#include "state/globals/ui/admin.inc"
#include "state/globals/combat/battle_turrets.inc"
#include "state/globals/combat/battle_enemy_turrets.inc"
#include "state/globals/combat/battle_fighter_look.inc"
#include "state/globals/combat/battle_event_plates.inc"
#include "state/globals/combat/battle_event_dice.inc"
#include "state/globals/combat/battle_event_columns.inc"
#include "state/globals/fx/weather.inc"
#include "state/globals/fx/christmas.inc"
#include "state/globals/ui/chat_bubbles.inc"

#include "objects.inc"

static void state_create(void) {
#include "state/create/core/config.inc"
#include "state/create/core/config_part2.inc"
#include "state/create/core/entities.inc"
#include "state/create/core/ui.inc"
#include "state/create/ui/progress_classes.inc"
#include "state/create/ui/promo.inc"
#include "state/create/ui/layout.inc"
#include "state/create/ui/chat.inc"
#include "state/create/ui/admin.inc"
#include "state/create/combat/battle_turrets.inc"
#include "state/create/combat/battle_enemy_turrets.inc"
#include "state/create/combat/battle_fighter_look.inc"
#include "state/create/combat/battle_event_plates.inc"
#include "state/create/combat/battle_event_dice.inc"
#include "state/create/combat/battle_event_columns.inc"
#include "state/create/fx/weather.inc"
#include "state/create/fx/christmas.inc"
#include "state/create/ui/chat_bubbles.inc"
}

static void state_destroy(void) {
    if (enemy_turret_x) {
        arr_free(enemy_turret_x);
        enemy_turret_x = NULL;
    }
    if (enemy_turret_y) {
        arr_free(enemy_turret_y);
        enemy_turret_y = NULL;
    }
    if (enemy_turret_hp) {
        arr_free(enemy_turret_hp);
        enemy_turret_hp = NULL;
    }
    if (enemy_turret_maxhp) {
        arr_free(enemy_turret_maxhp);
        enemy_turret_maxhp = NULL;
    }
    if (enemy_turret_spawn_t) {
        arr_free(enemy_turret_spawn_t);
        enemy_turret_spawn_t = NULL;
    }
    if (rpunch_a) {
        arr_free(rpunch_a);
        rpunch_a = NULL;
    }
    if (remote_grab) {
        arr_free(remote_grab);
        remote_grab = NULL;
    }
    if (rgrab_box_a) {
        arr_free(rgrab_box_a);
        rgrab_box_a = NULL;
    }
    if (rgrab_punch_t) {
        arr_free(rgrab_punch_t);
        rgrab_punch_t = NULL;
    }
    if (fighter_looks) {
        arr_free(fighter_looks);
        fighter_looks = NULL;
    }
    if (fighter_swings) {
        arr_free(fighter_swings);
        fighter_swings = NULL;
    }
    if (station_boom_xs) {
        arr_free(station_boom_xs);
        station_boom_xs = NULL;
    }
    if (station_boom_ys) {
        arr_free(station_boom_ys);
        station_boom_ys = NULL;
    }
    if (station_boom_ts) {
        arr_free(station_boom_ts);
        station_boom_ts = NULL;
    }
    if (turret_x) {
        arr_free(turret_x);
        turret_x = NULL;
    }
    if (turret_y) {
        arr_free(turret_y);
        turret_y = NULL;
    }
    if (turret_hp) {
        arr_free(turret_hp);
        turret_hp = NULL;
    }
    if (turret_maxhp) {
        arr_free(turret_maxhp);
        turret_maxhp = NULL;
    }
    if (turret_spawn_t) {
        arr_free(turret_spawn_t);
        turret_spawn_t = NULL;
    }
    if (runiverse_fx_a) {
        arr_free(runiverse_fx_a);
        runiverse_fx_a = NULL;
    }
    if (remote_station) {
        arr_free(remote_station);
        remote_station = NULL;
    }
    if (remote_universe) {
        arr_free(remote_universe);
        remote_universe = NULL;
    }
    if (remotes) {
        arr_free(remotes);
        remotes = NULL;
    }
    if (remote_punches) {
        arr_free(remote_punches);
        remote_punches = NULL;
    }
    if (remote_snow) {
        arr_free(remote_snow);
        remote_snow = NULL;
    }
    if (remote_dash) {
        arr_free(remote_dash);
        remote_dash = NULL;
    }
    if (rdash_box_a) {
        arr_free(rdash_box_a);
        rdash_box_a = NULL;
    }
    if (rsnow_ball_a) {
        arr_free(rsnow_ball_a);
        rsnow_ball_a = NULL;
    }
    if (rsnow_boom_a) {
        arr_free(rsnow_boom_a);
        rsnow_boom_a = NULL;
    }
    if (runiverse_box_a) {
        arr_free(runiverse_box_a);
        runiverse_box_a = NULL;
    }
    if (rstation_box_a) {
        arr_free(rstation_box_a);
        rstation_box_a = NULL;
    }
    if (turret_box_a) {
        arr_free(turret_box_a);
        turret_box_a = NULL;
    }
    if (enemy_turret_box_a) {
        arr_free(enemy_turret_box_a);
        enemy_turret_box_a = NULL;
    }
    if (remote_thuds) {
        arr_free(remote_thuds);
        remote_thuds = NULL;
    }
    if (candy_x) {
        arr_free(candy_x);
        candy_x = NULL;
    }
    if (candy_y) {
        arr_free(candy_y);
        candy_y = NULL;
    }
    if (candy_t) {
        arr_free(candy_t);
        candy_t = NULL;
    }
    if (candy_pick) {
        arr_free(candy_pick);
        candy_pick = NULL;
    }
    if (candy_ang) {
        arr_free(candy_ang);
        candy_ang = NULL;
    }
    if (dust) {
        arr_free(dust);
        dust = NULL;
    }
    if (remote_dust_cd) {
        arr_free(remote_dust_cd);
        remote_dust_cd = NULL;
    }
    if (class_level_tbl) {
        arr_free(class_level_tbl);
        class_level_tbl = NULL;
    }
    if (class_unlocked_tbl) {
        arr_free(class_unlocked_tbl);
        class_unlocked_tbl = NULL;
    }
    if (class_owned_tbl) {
        arr_free(class_owned_tbl);
        class_owned_tbl = NULL;
    }
    if (plates_candies) {
        arr_free(plates_candies);
        plates_candies = NULL;
    }
    if (dice_cubes) {
        arr_free(dice_cubes);
        dice_cubes = NULL;
    }
    if (flake_cache) {
        arr_free(flake_cache);
        flake_cache = NULL;
    }
    if (christmas_flake_cache) {
        arr_free(christmas_flake_cache);
        christmas_flake_cache = NULL;
    }
    if (player) {
        player_free(player);
        player = NULL;
    }
    if (enemy) {
        enemy_free(enemy);
        enemy = NULL;
    }
    if (punch) {
        punch_free(punch);
        punch = NULL;
    }
    if (gift) {
        gift_free(gift);
        gift = NULL;
    }
    if (enemy_gift) {
        gift_free(enemy_gift);
        enemy_gift = NULL;
    }
}
