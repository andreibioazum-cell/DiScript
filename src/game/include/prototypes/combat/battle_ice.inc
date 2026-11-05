static void ice_reset(void);
static void ice_tick(void);
static void ice_push_point(double *px, double *py, double half);
static void ice_avoid_vector(double x, double y, double *vx, double *vy);
static double ice_bounce_ball(double *x, double *y, double rad, double *vx, double *vy);
static void draw_ice(void);
double ice_near_fighter(double x, double y, double gap);
