static void ball_reset(void);
static double ball_damage_for(double bounces);
static void ball_aim_start(double id);
static void ball_aim_joy(double dx, double dy);
static void ball_aim_point(double x, double y);
static void ball_aim_tick(void);
static void ball_aim_cancel(void);
static void ball_fire(void);
static void ball_tick(void);
static void ball_fade_tick(void);
static void draw_ball(void);
static void draw_ball_count(void);
static void ball_online_tick(void);
static void draw_remote_bricks(void);
static void remote_bricks_reset(void);
static void ball_clear_after_finish(void);
#ifdef CB_PC
static double remote_bricks_live(void);
#endif
static double ult_mode_hit(double x, double y);
static void ult_mode_toggle(void);
