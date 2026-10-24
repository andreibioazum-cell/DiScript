static double ball_mode;
static void ball_reset(void);
static double ball_damage_for(double bounces);
static void ball_fire(void);
static void ball_tick(void);
static void ball_fade_tick(void);
static void draw_ball(void);
static void draw_ball_path_from(double x0, double y0, double vx0, double vy0, double bounces0, double t0,
                                uint32_t rgb, double check_foe);
static void ball_online_tick(void);
static void draw_remote_bricks(void);
static void remote_bricks_reset(void);
static void baseline_remote_slot(double s);
static void ball_clear_after_finish(void);
#ifdef CB_PC
static double remote_bricks_live(void);
#endif
static double ult_mode_hit(double x, double y);
static void ult_mode_toggle(void);
