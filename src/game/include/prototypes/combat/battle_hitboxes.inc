static double fade_up_a(double a, double t);
static double fade_down_a(double a, double t);
static double hb_env(double a);
static double hb_zone_col(double env);
static double hb_zone_bytes(double env);
static void draw_hit_strip(double sx, double sy, double dx, double dy, double off0, double off1, double half_w,
                           double env);
static void draw_hit_circle(double x, double y, double r, double env);
static void draw_hit_snowball(double x, double y, double shot, double env);
static void draw_hit_snow(void);
static void draw_hit_dash_path(double sx, double sy, double dx, double dy, double cx, double cy, double pr, double env);
static void draw_hit_dash(void);
static void draw_hit_universe_zone(double env);
static void draw_hit_universe(void);
static void draw_ability_hitboxes(void);
static void draw_hit_astra(void);
