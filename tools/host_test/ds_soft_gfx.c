/* Host no-op graphics surface: records nothing, satisfies the love.* renderer
 * bindings when running headless VM tests. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

void rect(float x,float y,float w,float h,uint32_t c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void rect_line(float x,float y,float w,float h,float t,uint32_t c){(void)x;(void)y;(void)w;(void)h;(void)t;(void)c;}
void roundrect(float x,float y,float w,float h,float r,uint32_t c){(void)x;(void)y;(void)w;(void)h;(void)r;(void)c;}
void rect_rot(float x,float y,float w,float h,float a,uint32_t c){(void)x;(void)y;(void)w;(void)h;(void)a;(void)c;}
void circle(float x,float y,float r,uint32_t c){(void)x;(void)y;(void)r;(void)c;}
void ring(float x,float y,float r,float t,uint32_t c){(void)x;(void)y;(void)r;(void)t;(void)c;}
void ellipse_fill(float x,float y,float rx,float ry,uint32_t c){(void)x;(void)y;(void)rx;(void)ry;(void)c;}
void ellipse_line(float x,float y,float rx,float ry,float t,uint32_t c){(void)x;(void)y;(void)rx;(void)ry;(void)t;(void)c;}
void sector(float x,float y,float r,float a,float s,uint32_t c){(void)x;(void)y;(void)r;(void)a;(void)s;(void)c;}
void arc_line(float x,float y,float r,float a,float s,float t,int p,uint32_t c){(void)x;(void)y;(void)r;(void)a;(void)s;(void)t;(void)p;(void)c;}
void line(float x1,float y1,float x2,float y2,float t,uint32_t c){(void)x1;(void)y1;(void)x2;(void)y2;(void)t;(void)c;}
void poly_fill(const float *xy,int n,uint32_t c){(void)xy;(void)n;(void)c;}
void poly_line(const float *xy,int n,float t,int cl,uint32_t c){(void)xy;(void)n;(void)t;(void)cl;(void)c;}
void tex(float x,float y,const char *n,float a,float s){(void)x;(void)y;(void)n;(void)a;(void)s;}
void tex_tint(float x,float y,const char *n,float a,float s,uint32_t c){(void)x;(void)y;(void)n;(void)a;(void)s;(void)c;}
void tex_alpha(float x,float y,const char *n,float a,float s,double al){(void)x;(void)y;(void)n;(void)a;(void)s;(void)al;}
void tex_round(float x,float y,const char *n,float s,float r){(void)x;(void)y;(void)n;(void)s;(void)r;}
void tex_ex(const char *n,float x,float y,float a,float sx,float sy,float ox,float oy,uint32_t t){(void)n;(void)x;(void)y;(void)a;(void)sx;(void)sy;(void)ox;(void)oy;(void)t;}
void text(const char *s,float x,float y,uint32_t c){(void)s;(void)x;(void)y;(void)c;}
void text_scaled(const char *s,float x,float y,uint32_t c,float sc){(void)s;(void)x;(void)y;(void)c;(void)sc;}
void text_n(const char *s,size_t n,float x,float y,uint32_t c,float sc){(void)s;(void)n;(void)x;(void)y;(void)c;(void)sc;}
void text_rainbow(const char *s,float x,float y,float sc,float h){(void)s;(void)x;(void)y;(void)sc;(void)h;}
void text_shimmer(const char *s,float x,float y,uint32_t c,float sc,float p){(void)s;(void)x;(void)y;(void)c;(void)sc;(void)p;}
float text_width(const char *s,float sc){int n=0;while(s&&s[n])n++;return (float)n*8.0f*(sc>0?sc:1.0f);}
float text_width_n(const char *s,size_t n,float sc){(void)s;return (float)n*8.0f*(sc>0?sc:1.0f);}
float text_line_height(float sc){return 16.0f*(sc>0?sc:1.0f);}
float text_mono_advance(float sc){return 8.0f*(sc>0?sc:1.0f);}
void ds_gfx_gray(double on){(void)on;}
void ds_gfx_shake(double a,double s){(void)a;(void)s;}
void ds_gfx_scissor(double x,double y,double w,double h){(void)x;(void)y;(void)w;(void)h;}
int png_load(const char *n){(void)n;return 0;}
int tex_size(const char *n,int *w,int *h){(void)n;if(w)*w=0;if(h)*h=0;return 0;}
int snd_play(const char *n){(void)n;return 0;}
int snd_loop(const char *n){(void)n;return 0;}
void snd_stop(const char *n){(void)n;}
void snd_stop_all(void){}
void snd_volume(const char *n,double v){(void)n;(void)v;}
int snd_synth_tone(double f,double d,const char *w,double v){(void)f;(void)d;(void)w;(void)v;return 1;}
int snd_synth_sfx(const char *p,double v){(void)p;(void)v;return 1;}
