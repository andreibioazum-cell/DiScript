static double fighter_half(double scale);
static double player_half(void);
static double enemy_half(void);
static double turret_half(void);
static double sat_axis_ok(double nx, double ny, double tx, double ty, double aux, double auy, double ae0, double ae1,
                          double bux, double buy, double be0, double be1);
static double rects_overlap(double ax, double ay, double aux, double auy, double ae0, double ae1, double bx, double by,
                            double bux, double buy, double be0, double be1);
static double strip_hits_box(double sx, double sy, double dx, double dy, double off0, double off1, double half_w,
                             double bx, double by, double bang, double bh);
static double circle_hits_rect(double cx, double cy, double r, double bx, double by, double bang, double hu,
                               double hv);
static double circle_hits_box(double cx, double cy, double r, double bx, double by, double bang, double bh);
static double ball_hits_fighter_r(double cx, double cy, double r, double fx, double fy, double fang, double scale);
static double ball_seg_hits_fighter(double ox, double oy, double nx, double ny, double r, double fx, double fy,
                                    double fang, double scale);
static double strip_hits_player(double sx, double sy, double dx, double dy, double off0, double off1, double half_w);
static double strip_hits_enemy(double sx, double sy, double dx, double dy, double off0, double off1, double half_w);
static double strip_hits_turret(double sx, double sy, double dx, double dy, double off0, double off1, double half_w,
                                double tx, double ty);
static double circle_hits_player(double cx, double cy, double r);
static double circle_hits_enemy(double cx, double cy, double r);
static double circle_hits_turret(double cx, double cy, double r, double tx, double ty);
static double punch_hits_player(double px, double py, double pdx, double pdy, double reach, double width);
static double punch_hits_enemy(double px, double py, double pdx, double pdy, double reach, double width);
static double punch_hits_turret(double px, double py, double pdx, double pdy, double reach, double width, double tx,
                                double ty);
static double along(double px, double py, double pdx, double pdy, double tx, double ty);
