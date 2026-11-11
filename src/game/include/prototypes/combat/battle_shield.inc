static double pierce_has(double mask, double bit);
static double turret_pierce_bit(double i);
static double nearest_turret(DSArray *xs, DSArray *ys, DSArray *hps, double x, double y);
static double shield_absorb_turrets(DSArray *xs, DSArray *ys, DSArray *hps, double x, double y, double mine,
                                    double dmg);
static double player_shield_absorb(double dmg);
static double enemy_shield_absorb(double dmg);
static void snow_chip_turrets(DSArray *xs, DSArray *ys, DSArray *hps, double mine, double x, double y);
static void snow_chip_enemy_turrets(void);
static void snow_chip_player_turrets(void);
