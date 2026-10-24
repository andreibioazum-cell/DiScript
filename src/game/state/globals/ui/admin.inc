/* Admin orb + panel state. Session-local: nothing here is saved to storage,
 * every reset starts with the orb in its default corner and cheats off. */
double admin_ball_x;
double admin_ball_y;
double admin_ball_grab;
double admin_ball_id;
double admin_ball_px;
double admin_ball_py;
double admin_ball_grab_dx;
double admin_ball_grab_dy;
double admin_ball_t;
double admin_panel_open;
/* Cheat stain: once any switch is flipped on, the current battle pays no
 * Cups at the end - even if the switch goes straight back off. */
double cheat_stain;
double cheat_god;
double cheat_damage;
double cheat_speed;
double cheat_nocd;
double cheat_cc;
double cheat_estun;
/* Joke mode from this panel: masterpiece hitboxes without the web flag. */
double cheat_chaos;
/* Trajectory preview: all bounce directions of ebuC's (буК) projectile. */
double cheat_ball_path;
/* Mark: the admin panel put this stun on the enemy, not the game. Such a stun
 * draws no orange ring - neither while the cheat is on nor after it is off while
 * the leftover stun still ticks. Cleared by a real stun (stun_enemy) and by the
 * start of a new fight. */
double cheat_estun_mark;
