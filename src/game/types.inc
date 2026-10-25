typedef struct Player Player;
typedef struct Enemy Enemy;
typedef struct Punch Punch;
typedef struct Gift Gift;
typedef struct FighterSpritePair FighterSpritePair;

/* Paired idle and attack texture names for a fighter appearance. */
struct FighterSpritePair {
    const char *idle;
    const char *punch;
};

struct Player {
    double x;
    double y;
    double size;
    double angle;
    double hp;
    double max_hp;
};
struct Enemy {
    double x;
    double y;
    double size;
    double hp;
    double max_hp;
    double angle;
    double state;
    double state_time;
    double cooldown;
    double hit;
    double strafe;
    double dodge_cd;
    double think;
    double px;
    double py;
    double pvx;
    double pvy;
    double seen;
    double flank;
    double mvx;
    double mvy;
    double svx;
    double svy;
    double freeze;
    double freeze_slow;
    double poison;
    double poison_tick;
    double poison_damage;
    double stun;
    double react;
    double speed_var;
    double aim_err;
    double hesit;
};
struct Punch {
    double x;
    double y;
    double dx;
    double dy;
    double active;
    double shot;
    double hit;
};
struct Gift {
    double x;
    double y;
    double x0;
    double y0;
    double dx;
    double dy;
    double active;
    double t;
    double shot;
    double pierce;
};
