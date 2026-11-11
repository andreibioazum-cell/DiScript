static double fighter_look_of(double slot);
static double fighter_pose_of(double slot, double punching);
static void resolve_fighter_look(double slot, double cls, double skin);
static void resolve_fighter_looks(void);
static void draw_fighter_shadow(double x, double y, double ang, double look, double punching);
static void shadow_tap_circle(double x, double y, double r, double a);
static void shadow_tap_tex(double x, double y, double half_px, double scale, const char *name, double ang, double a);
static void draw_shadow_blur_circle(double cx, double cy, double r, double alpha, double blur);
static void draw_shadow_blur_tex(double cx, double cy, double half_px, double scale, const char *name, double ang,
                                 double alpha, double blur);
static void draw_motion_ghosts(double x, double y, double dx, double dy, double half_px, double scale,
                               const char *name, double ang);
static void draw_fighter_body(double x, double y, double ang, double look, double punching);
static void draw_fighter_hurt(double x, double y, double ang, double look, double punching, double scale, double a);
