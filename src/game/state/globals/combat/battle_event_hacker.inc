/* H4c4k0r raid (event 7): mutable state of the scripted hack scene. */
double hack_active;
/* Seconds since the raid script started; drives the whole timeline. */
double hack_t;
/* Intro glitch overlay: seconds left of full-screen interference. */
double hack_glitch;
/* Device the raid detected: 0 unknown, 1 phone (.bin), 2 PC (.so). */
double hack_platform;
/* 1 when the platform payload file was read and its checksum matched. */
double hack_payload_ok;
double hack_payload_bytes;
/* Index of the next payload command the hacker "types" and fires. */
double hack_cmd_idx;
/* Broken event stubs: disco flicker and dead plates leftovers. */
double hack_stub_disco_t;
double hack_stub_plates_t;
/* The one cloud of the hacker's custom storm, hanging in the corner. */
double hack_cloud_on;
double hack_cloud_x;
double hack_cloud_y;
double hack_cloud_t;
/* Menu-button avalanche: spawn timer and count of dropped buttons. */
double hack_btn_cd;
double hack_btn_made;
DSArray *hack_buttons;
/* Seconds since the final ban; 0 while the raid still argues. */
double hack_ban_t;
/* Compensation rain: dropped canes, collected canes, spawn timer. */
double hack_cane_made;
double hack_cane_got;
double hack_cane_cd;
double hack_cane_pop_t;
DSArray *hack_canes;
/* Grace timer before the clean victory ends the round. */
double hack_finish_t;
/* Terminal window positions: compact cmd boxes the player can drag around. */
double hack_term_x[3];
double hack_term_y[3];
/* Drag state: which terminal title bar a pointer holds (-1 for none). */
double hack_drag_win;
double hack_drag_id;
double hack_drag_dx;
double hack_drag_dy;
