#include "vegg_game.h"

#include <math.h>
#include <stdio.h>

#include "nvs.h"
#include "esp_heap_caps.h"
#include "vegg_sprites.h"

namespace {

enum VeggState {
    VEGG_RUNNING,
    VEGG_JUMPING,
    VEGG_GAME_OVER
};

static lv_obj_t *screen = NULL;
static lv_obj_t *runner = NULL;
static lv_obj_t *score_label = NULL;
static lv_obj_t *best_label = NULL;
static lv_obj_t *game_over_panel = NULL;
static lv_timer_t *timer = NULL;
static lv_obj_t *obstacle_obj[4] = {NULL, NULL, NULL, NULL};

struct Obstacle { bool active; bool branch; float x; };
static Obstacle obstacles[4];

static bool active = false;
static VeggState state = VEGG_RUNNING;
static float runner_x = 140.0f;
static float jump_h = 0.0f;
static float jump_v = 0.0f;
static float speed = 240.0f;
static float distance_run = 0.0f;
static float run_time = 0.0f;
static float next_gap = 260.0f;
static uint32_t score = 0;
static uint32_t best = 0;
static uint32_t last_tick = 0;
static uint32_t game_over_at = 0;
static uint32_t rng_state = 2463534242u;
static void (*exit_callback)(void) = NULL;

static lv_obj_t *world = NULL;
static uint16_t *world_pixels = NULL;
static lv_image_dsc_t world_dsc = {};
static constexpr int WORLD_W=466, WORLD_H=466;
static constexpr float WORLD_CX=233.0f, WORLD_R=699.0f, WORLD_G=298.0f;

static inline uint16_t rgb(uint8_t r,uint8_t g,uint8_t b){return (uint16_t)(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));}
static inline void px(int x,int y,uint16_t c){if(world_pixels&&x>=0&&x<WORLD_W&&y>=0&&y<WORLD_H)world_pixels[y*WORLD_W+x]=c;}
static void fill_rect(int x,int y,int w,int h,uint16_t c){
    if(!world_pixels)return; int x0=x<0?0:x,y0=y<0?0:y,x1=x+w>WORLD_W?WORLD_W:x+w,y1=y+h>WORLD_H?WORLD_H:y+h;
    for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx)world_pixels[yy*WORLD_W+xx]=c;
}
static void fill_circle(int cx,int cy,int r,uint16_t c){
    if(r<1)r=1; for(int y=-r;y<=r;++y){int xx=(int)sqrtf((float)(r*r-y*y));fill_rect(cx-xx,cy+y,xx*2+1,1,c);}
}
static void draw_line(int x0,int y0,int x1,int y1,uint16_t c){
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;){px(x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}
}
static float edgef(float ax,float ay,float bx,float by,float x,float y){return (x-ax)*(by-ay)-(y-ay)*(bx-ax);}
static void fill_triangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t c){
    int minx=(int)fmaxf(0,floorf(fminf((float)x0,fminf((float)x1,(float)x2)))),maxx=(int)fminf(WORLD_W-1,ceilf(fmaxf((float)x0,fmaxf((float)x1,(float)x2))));
    int miny=(int)fmaxf(0,floorf(fminf((float)y0,fminf((float)y1,(float)y2)))),maxy=(int)fminf(WORLD_H-1,ceilf(fmaxf((float)y0,fmaxf((float)y1,(float)y2))));
    float area=edgef(x0,y0,x1,y1,x2,y2); if(fabsf(area)<.01f)return;
    for(int y=miny;y<=maxy;++y)for(int x=minx;x<=maxx;++x){
        float a=edgef(x1,y1,x2,y2,x,y),b=edgef(x2,y2,x0,y0,x,y),d=edgef(x0,y0,x1,y1,x,y);
        if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0))px(x,y,c);
    }
}
struct WorldFrame{float bx,by,c,s,k;};
static float ground_y(float x){float d=x-WORLD_CX;return WORLD_G+d*d/(2*WORLD_R);}
static WorldFrame frame_at(float x,float yoff,float k){float d=x-WORLD_CX,n=sqrtf(WORLD_R*WORLD_R+d*d);return {x,ground_y(x)+yoff,WORLD_R/n,d/n,k};}
static void world_point(const WorldFrame&f,float lx,float ly,int&ox,int&oy){ox=(int)lroundf(f.bx+(lx*f.c+ly*f.s)*f.k);oy=(int)lroundf(f.by+(lx*f.s-ly*f.c)*f.k);}
static void world_tri(const WorldFrame&f,float x0,float y0,float x1,float y1,float x2,float y2,uint16_t c){int ax,ay,bx,by,cx,cy;world_point(f,x0,y0,ax,ay);world_point(f,x1,y1,bx,by);world_point(f,x2,y2,cx,cy);fill_triangle(ax,ay,bx,by,cx,cy,c);}
static void world_quad(const WorldFrame&f,float a,float b,float c,float d,float e,float g,float h,float i,uint16_t col){world_tri(f,a,b,c,d,e,g,col);world_tri(f,a,b,e,g,h,i,col);}
static void world_circle(const WorldFrame&f,float x,float y,float r,uint16_t c){int a,b;world_point(f,x,y,a,b);fill_circle(a,b,(int)lroundf(r*f.k),c);}
static uint32_t hash32(uint32_t x){x^=x>>16;x*=0x7feb352d;x^=x>>15;x*=0x846ca68b;x^=x>>16;return x;}
static void world_tree(float x,float base,float t,uint8_t kind,uint16_t tr,uint16_t c1,uint16_t c2){
    WorldFrame f=frame_at(x,-base,t);
    if(kind==0){float th=60,w=13,r=34;world_quad(f,-w/2,-6,w/2,-6,w*.35f,th,-w*.35f,th,tr);world_circle(f,-r*.8f,th+r*.1f,r*.75f,c1);world_circle(f,r*.8f,th+r*.1f,r*.75f,c1);world_circle(f,0,th+r*.6f,r,c1);world_circle(f,-r*.25f,th+r*.95f,r*.55f,c2);}
    else{float w=10;world_quad(f,-w/2,-6,w/2,-6,w*.4f,45,-w*.4f,45,tr);world_tri(f,-36,28,36,28,0,82,c1);world_tri(f,-29,58,29,58,0,112,c1);world_tri(f,-21,88,21,88,0,138,c2);}
}
static void world_layer(float factor,float cell,float base,float tmin,float tmax,int skip,uint16_t tr,uint16_t c1,uint16_t c2,uint32_t seed){
    float off=distance_run*factor;int i0=(int)floorf((off-120)/cell),i1=(int)floorf((off+WORLD_W+120)/cell);
    for(int i=i0;i<=i1;++i){uint32_t h=hash32((uint32_t)i*0x9E3779B1u+seed);if((int)(h%100)<skip)continue;float wx=i*cell+((h>>8)&255)/255.0f*cell*.9f;float t=tmin+((h>>16)&255)/255.0f*(tmax-tmin);world_tree(wx-off,base,t,((h>>24)&3)==0?1:0,tr,c1,c2);}
}
static void world_band(float up,uint16_t col,uint16_t grass,uint16_t soil){
    for(int x=0;x<WORLD_W;x+=4){int y=(int)lroundf(ground_y(x+2)-up);if(y<0)y=0;if(y>=WORLD_H)continue;fill_rect(x,y,4,WORLD_H-y,col);fill_rect(x,y,4,5,grass);if(soil){int sy=y+26;if(sy<WORLD_H)fill_rect(x,sy,4,WORLD_H-sy,soil);}}
}
static void world_bush(float x){
    WorldFrame f=frame_at(x,2,1);uint16_t dk=rgb(35,105,50),md=rgb(55,145,65),red=rgb(225,40,65),br=rgb(105,66,38);
    world_circle(f,-14,10,13,dk);world_circle(f,14,10,13,dk);world_circle(f,0,15,15,md);world_circle(f,-8,20,9,md);world_circle(f,9,20,9,md);world_quad(f,-18,4,18,4,17,-4,-17,-4,br);
    world_circle(f,-10,14,3,red);world_circle(f,3,23,3,red);world_circle(f,14,13,3,red);world_circle(f,-1,9,3,red);world_circle(f,9,20,2,red);
}
static void world_branch(float x){
    WorldFrame f=frame_at(x,4,1);uint16_t tr=rgb(105,66,38),c1=rgb(40,120,55),c2=rgb(75,165,75);
    world_quad(f,-9,-6,9,-6,7,150,-7,150,tr);world_quad(f,-4,78,-4,62,-92,66,-92,74,tr);
    world_circle(f,0,160,38,c1);world_circle(f,-26,148,28,c1);world_circle(f,26,148,28,c1);world_circle(f,-86,70,12,c1);world_circle(f,-80,80,18,c1);world_circle(f,-55,92,30,c1);world_circle(f,-25,100,34,c1);world_circle(f,-40,108,20,c2);
}
static void world_eevee(){
    uint8_t fr=jump_h>.5f?0:(uint8_t)((uint32_t)(run_time*10)%VEGG_EEVEE_RUN_FRAMES);
    const uint16_t ws[]={VEGG_EEVEE_RUN0_W,VEGG_EEVEE_RUN1_W,VEGG_EEVEE_RUN2_W},hs[]={VEGG_EEVEE_RUN0_H,VEGG_EEVEE_RUN1_H,VEGG_EEVEE_RUN2_H};
    const uint8_t*srcs[]={VEGG_EEVEE_RUN0,VEGG_EEVEE_RUN1,VEGG_EEVEE_RUN2};uint16_t w=ws[fr],h=hs[fr];WorldFrame f=frame_at(runner_x,-jump_h,3);float ox=(w-1)*.5f;
    for(uint16_t sy=0;sy<h;++sy)for(uint16_t sx=0;sx<w;++sx){uint16_t p=sy*w+sx;uint8_t q=srcs[fr][p>>1],pi=(p&1)?(q&15):(q>>4);if(!pi)continue;int x,y;world_point(f,(w-1-sx)-ox,(h-1-sy),x,y);fill_rect(x,y,3,3,VEGG_EEVEE_PALETTE[pi]);}
}
static void render_world(){
    const uint8_t a[3]={110,185,235},b[3]={210,238,225};int hh=(int)(WORLD_G+40);
    for(int i=0;i<10;++i){float u=(float)i/9;fill_rect(0,hh*i/10,WORLD_W,hh/10+1,rgb((uint8_t)(a[0]+(b[0]-a[0])*u),(uint8_t)(a[1]+(b[1]-a[1])*u),(uint8_t)(a[2]+(b[2]-a[2])*u)));}
    world_band(26,rgb(78,138,112),rgb(95,155,128),0);world_layer(.35f,46,26,.45f,.70f,20,rgb(80,100,95),rgb(70,130,110),rgb(95,158,132),0x1111);
    world_band(13,rgb(58,122,82),rgb(78,145,95),0);world_layer(.65f,62,13,.65f,.95f,22,rgb(92,64,44),rgb(40,108,68),rgb(62,138,88),0x2222);
    world_band(0,rgb(70,150,60),rgb(105,190,72),rgb(112,82,52));world_layer(1,120,0,.95f,1.30f,40,rgb(108,68,40),rgb(40,125,55),rgb(72,162,72),0x3333);
    uint16_t gr=rgb(70,150,70);for(int x=5;x<WORLD_W;x+=38){int y=(int)lroundf(ground_y(x));draw_line(x,y,x-4,y-10,gr);draw_line(x+4,y,x+7,y-13,gr);draw_line(x+8,y,x+12,y-8,gr);}
    int cloud=(int)((lv_tick_get()/45)%560)-60;uint16_t cc=rgb(250,252,246);fill_circle(cloud,82,11,cc);fill_circle(cloud+16,85,9,cc);fill_circle(cloud-13,87,8,cc);
    for(int i=0;i<4;++i)if(obstacles[i].active)(obstacles[i].branch?world_branch:world_bush)(obstacles[i].x);
    world_eevee(); if(world)lv_obj_invalidate(world);
}
static void update_world_image(){if(!world)return;world_dsc.header.magic=LV_IMAGE_HEADER_MAGIC;world_dsc.header.cf=LV_COLOR_FORMAT_RGB565;world_dsc.header.w=WORLD_W;world_dsc.header.h=WORLD_H;world_dsc.header.stride=WORLD_W*2;world_dsc.data=(const uint8_t*)world_pixels;world_dsc.data_size=WORLD_W*WORLD_H*2;lv_image_set_src(world,&world_dsc);}
static void build_scene(void)
{
    world_pixels=(uint16_t*)heap_caps_malloc(WORLD_W*WORLD_H*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!world_pixels) world_pixels=(uint16_t*)heap_caps_malloc(WORLD_W*WORLD_H*2,MALLOC_CAP_8BIT);
    if(!world_pixels){ active=false; return; }

    world=lv_image_create(screen);
    lv_obj_set_size(world,WORLD_W,WORLD_H);
    lv_obj_set_pos(world,0,0);
    lv_obj_clear_flag(world,LV_OBJ_FLAG_CLICKABLE);
    update_world_image();
    render_world();

    score_label=lv_label_create(screen);
    lv_obj_set_style_text_font(score_label,&lv_font_montserrat_22,0);
    lv_obj_set_style_text_color(score_label,lv_color_white(),0);
    lv_label_set_text(score_label,"SCORE  0000");
    lv_obj_align(score_label,LV_ALIGN_TOP_MID,0,58);

    best_label=lv_label_create(screen);
    lv_obj_set_style_text_font(best_label,&lv_font_montserrat_22,0);
    lv_obj_set_style_text_color(best_label,lv_color_white(),0);
    lv_label_set_text(best_label,"BEST  0000");
    lv_obj_align(best_label,LV_ALIGN_TOP_MID,0,88);

    lv_obj_t *back=lv_button_create(screen);
    lv_obj_set_size(back,64,46);
    lv_obj_align(back,LV_ALIGN_TOP_MID,0,8);
    lv_obj_t *back_label=lv_label_create(back);
    lv_label_set_text(back_label,LV_SYMBOL_LEFT);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back,exit_cb,LV_EVENT_CLICKED,NULL);

    game_over_panel=lv_obj_create(screen);
    lv_obj_remove_style_all(game_over_panel);
    lv_obj_set_style_bg_color(game_over_panel,lv_color_hex(0x193021),0);
    lv_obj_set_style_bg_opa(game_over_panel,LV_OPA_COVER,0);
    lv_obj_set_style_radius(game_over_panel,22,0);
    lv_obj_set_style_border_width(game_over_panel,2,0);
    lv_obj_set_style_border_color(game_over_panel,lv_color_hex(0xFFBE5A),0);
    lv_obj_set_size(game_over_panel,320,190);
    lv_obj_align(game_over_panel,LV_ALIGN_CENTER,0,-35);
    lv_obj_add_flag(game_over_panel,LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *over=lv_label_create(game_over_panel);
    lv_label_set_text(over,"GAME OVER");
    lv_obj_set_style_text_font(over,&lv_font_montserrat_24,0);
    lv_obj_set_style_text_color(over,lv_color_hex(0xFF963C),0);
    lv_obj_align(over,LV_ALIGN_TOP_MID,0,18);

    lv_obj_t *hint=lv_label_create(game_over_panel);
    lv_label_set_text(hint,"Tap = retry");
    lv_obj_set_style_text_font(hint,&lv_font_montserrat_22,0);
    lv_obj_align(hint,LV_ALIGN_BOTTOM_MID,0,-20);

    lv_obj_t *touch=lv_obj_create(screen);
    lv_obj_remove_style_all(touch);
    lv_obj_set_size(touch,466,370);
    lv_obj_set_pos(touch,0,90);
    lv_obj_set_style_bg_opa(touch,LV_OPA_TRANSP,0);
    lv_obj_clear_flag(touch,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(touch,tap_cb,LV_EVENT_CLICKED,NULL);
}
include "vegg_game.h"

#include <math.h>
#include <stdio.h>

#include "nvs.h"
#include "esp_heap_caps.h"
#include "vegg_sprites.h"

namespace {

enum VeggState {
    VEGG_RUNNING,
    VEGG_JUMPING,
    VEGG_GAME_OVER
};

static lv_obj_t *screen = NULL;
static lv_obj_t *runner = NULL;
static lv_obj_t *score_label = NULL;
static lv_obj_t *best_label = NULL;
static lv_obj_t *game_over_panel = NULL;
static lv_timer_t *timer = NULL;
static lv_obj_t *obstacle_obj[4] = {NULL, NULL, NULL, NULL};

struct Obstacle { bool active; bool branch; float x; };
static Obstacle obstacles[4];

static bool active = false;
static VeggState state = VEGG_RUNNING;
static float runner_x = 140.0f;
static float jump_h = 0.0f;
static float jump_v = 0.0f;
static float speed = 240.0f;
static float distance_run = 0.0f;
static float run_time = 0.0f;
static float next_gap = 260.0f;
static uint32_t score = 0;
static uint32_t best = 0;
static uint32_t last_tick = 0;
static uint32_t game_over_at = 0;
static uint32_t rng_state = 2463534242u;
static void (*exit_callback)(void) = NULL;

static lv_obj_t *world = NULL;
static uint16_t *world_pixels = NULL;
static lv_image_dsc_t world_dsc = {};
static constexpr int WORLD_W=466, WORLD_H=466;
static constexpr float WORLD_CX=233.0f, WORLD_R=699.0f, WORLD_G=298.0f;

static inline uint16_t rgb(uint8_t r,uint8_t g,uint8_t b){return (uint16_t)(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));}
static inline void px(int x,int y,uint16_t c){if(world_pixels&&x>=0&&x<WORLD_W&&y>=0&&y<WORLD_H)world_pixels[y*WORLD_W+x]=c;}
static void fill_rect(int x,int y,int w,int h,uint16_t c){
    if(!world_pixels)return; int x0=x<0?0:x,y0=y<0?0:y,x1=x+w>WORLD_W?WORLD_W:x+w,y1=y+h>WORLD_H?WORLD_H:y+h;
    for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx)world_pixels[yy*WORLD_W+xx]=c;
}
static void fill_circle(int cx,int cy,int r,uint16_t c){
    if(r<1)r=1; for(int y=-r;y<=r;++y){int xx=(int)sqrtf((float)(r*r-y*y));fill_rect(cx-xx,cy+y,xx*2+1,1,c);}
}
static void draw_line(int x0,int y0,int x1,int y1,uint16_t c){
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;){px(x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}
}
static float edgef(float ax,float ay,float bx,float by,float x,float y){return (x-ax)*(by-ay)-(y-ay)*(bx-ax);}
static void fill_triangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t c){
    int minx=(int)fmaxf(0,floorf(fminf((float)x0,fminf((float)x1,(float)x2)))),maxx=(int)fminf(WORLD_W-1,ceilf(fmaxf((float)x0,fmaxf((float)x1,(float)x2))));
    int miny=(int)fmaxf(0,floorf(fminf((float)y0,fminf((float)y1,(float)y2)))),maxy=(int)fminf(WORLD_H-1,ceilf(fmaxf((float)y0,fmaxf((float)y1,(float)y2))));
    float area=edgef(x0,y0,x1,y1,x2,y2); if(fabsf(area)<.01f)return;
    for(int y=miny;y<=maxy;++y)for(int x=minx;x<=maxx;++x){
        float a=edgef(x1,y1,x2,y2,x,y),b=edgef(x2,y2,x0,y0,x,y),d=edgef(x0,y0,x1,y1,x,y);
        if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0))px(x,y,c);
    }
}
struct WorldFrame{float bx,by,c,s,k;};
static float ground_y(float x){float d=x-WORLD_CX;return WORLD_G+d*d/(2*WORLD_R);}
static WorldFrame frame_at(float x,float yoff,float k){float d=x-WORLD_CX,n=sqrtf(WORLD_R*WORLD_R+d*d);return {x,ground_y(x)+yoff,WORLD_R/n,d/n,k};}
static void world_point(const WorldFrame&f,float lx,float ly,int&ox,int&oy){ox=(int)lroundf(f.bx+(lx*f.c+ly*f.s)*f.k);oy=(int)lroundf(f.by+(lx*f.s-ly*f.c)*f.k);}
static void world_tri(const WorldFrame&f,float x0,float y0,float x1,float y1,float x2,float y2,uint16_t c){int ax,ay,bx,by,cx,cy;world_point(f,x0,y0,ax,ay);world_point(f,x1,y1,bx,by);world_point(f,x2,y2,cx,cy);fill_triangle(ax,ay,bx,by,cx,cy,c);}
static void world_quad(const WorldFrame&f,float a,float b,float c,float d,float e,float g,float h,float i,uint16_t col){world_tri(f,a,b,c,d,e,g,col);world_tri(f,a,b,e,g,h,i,col);}
static void world_circle(const WorldFrame&f,float x,float y,float r,uint16_t c){int a,b;world_point(f,x,y,a,b);fill_circle(a,b,(int)lroundf(r*f.k),c);}
static uint32_t hash32(uint32_t x){x^=x>>16;x*=0x7feb352d;x^=x>>15;x*=0x846ca68b;x^=x>>16;return x;}
static void world_tree(float x,float base,float t,uint8_t kind,uint16_t tr,uint16_t c1,uint16_t c2){
    WorldFrame f=frame_at(x,-base,t);
    if(kind==0){float th=60,w=13,r=34;world_quad(f,-w/2,-6,w/2,-6,w*.35f,th,-w*.35f,th,tr);world_circle(f,-r*.8f,th+r*.1f,r*.75f,c1);world_circle(f,r*.8f,th+r*.1f,r*.75f,c1);world_circle(f,0,th+r*.6f,r,c1);world_circle(f,-r*.25f,th+r*.95f,r*.55f,c2);}
    else{float w=10;world_quad(f,-w/2,-6,w/2,-6,w*.4f,45,-w*.4f,45,tr);world_tri(f,-36,28,36,28,0,82,c1);world_tri(f,-29,58,29,58,0,112,c1);world_tri(f,-21,88,21,88,0,138,c2);}
}
static void world_layer(float factor,float cell,float base,float tmin,float tmax,int skip,uint16_t tr,uint16_t c1,uint16_t c2,uint32_t seed){
    float off=distance_run*factor;int i0=(int)floorf((off-120)/cell),i1=(int)floorf((off+WORLD_W+120)/cell);
    for(int i=i0;i<=i1;++i){uint32_t h=hash32((uint32_t)i*0x9E3779B1u+seed);if((int)(h%100)<skip)continue;float wx=i*cell+((h>>8)&255)/255.0f*cell*.9f;float t=tmin+((h>>16)&255)/255.0f*(tmax-tmin);world_tree(wx-off,base,t,((h>>24)&3)==0?1:0,tr,c1,c2);}
}
static void world_band(float up,uint16_t col,uint16_t grass,uint16_t soil){
    for(int x=0;x<WORLD_W;x+=4){int y=(int)lroundf(ground_y(x+2)-up);if(y<0)y=0;if(y>=WORLD_H)continue;fill_rect(x,y,4,WORLD_H-y,col);fill_rect(x,y,4,5,grass);if(soil){int sy=y+26;if(sy<WORLD_H)fill_rect(x,sy,4,WORLD_H-sy,soil);}}
}
static void world_bush(float x){
    WorldFrame f=frame_at(x,2,1);uint16_t dk=rgb(35,105,50),md=rgb(55,145,65),red=rgb(225,40,65),br=rgb(105,66,38);
    world_circle(f,-14,10,13,dk);world_circle(f,14,10,13,dk);world_circle(f,0,15,15,md);world_circle(f,-8,20,9,md);world_circle(f,9,20,9,md);world_quad(f,-18,4,18,4,17,-4,-17,-4,br);
    world_circle(f,-10,14,3,red);world_circle(f,3,23,3,red);world_circle(f,14,13,3,red);world_circle(f,-1,9,3,red);world_circle(f,9,20,2,red);
}
static void world_branch(float x){
    WorldFrame f=frame_at(x,4,1);uint16_t tr=rgb(105,66,38),c1=rgb(40,120,55),c2=rgb(75,165,75);
    world_quad(f,-9,-6,9,-6,7,150,-7,150,tr);world_quad(f,-4,78,-4,62,-92,66,-92,74,tr);
    world_circle(f,0,160,38,c1);world_circle(f,-26,148,28,c1);world_circle(f,26,148,28,c1);world_circle(f,-86,70,12,c1);world_circle(f,-80,80,18,c1);world_circle(f,-55,92,30,c1);world_circle(f,-25,100,34,c1);world_circle(f,-40,108,20,c2);
}
static void world_eevee(){
    uint8_t fr=jump_h>.5f?0:(uint8_t)((uint32_t)(run_time*10)%VEGG_EEVEE_RUN_FRAMES);
    const uint16_t ws[]={VEGG_EEVEE_RUN0_W,VEGG_EEVEE_RUN1_W,VEGG_EEVEE_RUN2_W},hs[]={VEGG_EEVEE_RUN0_H,VEGG_EEVEE_RUN1_H,VEGG_EEVEE_RUN2_H};
    const uint8_t*srcs[]={VEGG_EEVEE_RUN0,VEGG_EEVEE_RUN1,VEGG_EEVEE_RUN2};uint16_t w=ws[fr],h=hs[fr];WorldFrame f=frame_at(runner_x,-jump_h,3);float ox=(w-1)*.5f;
    for(uint16_t sy=0;sy<h;++sy)for(uint16_t sx=0;sx<w;++sx){uint16_t p=sy*w+sx;uint8_t q=srcs[fr][p>>1],pi=(p&1)?(q&15):(q>>4);if(!pi)continue;int x,y;world_point(f,(w-1-sx)-ox,(h-1-sy),x,y);fill_rect(x,y,3,3,VEGG_EEVEE_PALETTE[pi]);}
}
static void render_world(){
    const uint8_t a[3]={110,185,235},b[3]={210,238,225};int hh=(int)(WORLD_G+40);
    for(int i=0;i<10;++i){float u=(float)i/9;fill_rect(0,hh*i/10,WORLD_W,hh/10+1,rgb((uint8_t)(a[0]+(b[0]-a[0])*u),(uint8_t)(a[1]+(b[1]-a[1])*u),(uint8_t)(a[2]+(b[2]-a[2])*u)));}
    world_band(26,rgb(78,138,112),rgb(95,155,128),0);world_layer(.35f,46,26,.45f,.70f,20,rgb(80,100,95),rgb(70,130,110),rgb(95,158,132),0x1111);
    world_band(13,rgb(58,122,82),rgb(78,145,95),0);world_layer(.65f,62,13,.65f,.95f,22,rgb(92,64,44),rgb(40,108,68),rgb(62,138,88),0x2222);
    world_band(0,rgb(70,150,60),rgb(105,190,72),rgb(112,82,52));world_layer(1,120,0,.95f,1.30f,40,rgb(108,68,40),rgb(40,125,55),rgb(72,162,72),0x3333);
    uint16_t gr=rgb(70,150,70);for(int x=5;x<WORLD_W;x+=38){int y=(int)lroundf(ground_y(x));draw_line(x,y,x-4,y-10,gr);draw_line(x+4,y,x+7,y-13,gr);draw_line(x+8,y,x+12,y-8,gr);}
    int cloud=(int)((lv_tick_get()/45)%560)-60;uint16_t cc=rgb(250,252,246);fill_circle(cloud,82,11,cc);fill_circle(cloud+16,85,9,cc);fill_circle(cloud-13,87,8,cc);
    for(int i=0;i<4;++i)if(obstacles[i].active)(obstacles[i].branch?world_branch:world_bush)(obstacles[i].x);
    world_eevee(); if(world)lv_obj_invalidate(world);
}
static void update_world_image(){if(!world)return;world_dsc.header.magic=LV_IMAGE_HEADER_MAGIC;world_dsc.header.cf=LV_COLOR_FORMAT_RGB565;world_dsc.header.w=WORLD_W;world_dsc.header.h=WORLD_H;world_dsc.header.stride=WORLD_W*2;world_dsc.data=(const uint8_t*)world_pixels;world_dsc.data_size=WORLD_W*WORLD_H*2;lv_image_set_src(world,&world_dsc);}
static void build_scene(void)
{
    lv_obj_set_style_bg_color(screen, SKY, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *horizon = lv_obj_create(screen);
    lv_obj_remove_style_all(horizon);
    lv_obj_set_style_bg_color(horizon, SKY2, 0);
    lv_obj_set_size(horizon, 466, 300);
    lv_obj_set_pos(horizon, 0, 0);
    lv_obj_clear_flag(horizon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *planet = lv_obj_create(screen);
    lv_obj_remove_style_all(planet);
    lv_obj_set_style_bg_color(planet, GROUND, 0);
    lv_obj_set_style_radius(planet, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(planet, 700, 300);
    lv_obj_set_pos(planet, -117, 285);
    lv_obj_clear_flag(planet, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *grass = lv_obj_create(screen);
    lv_obj_remove_style_all(grass);
    lv_obj_set_style_bg_color(grass, GROUND_DARK, 0);
    lv_obj_set_style_radius(grass, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(grass, 650, 240);
    lv_obj_set_pos(grass, -92, 300);
    lv_obj_clear_flag(grass, LV_OBJ_FLAG_CLICKABLE);

    score_label = lv_label_create(screen);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(score_label, lv_color_white(), 0);
    lv_label_set_text(score_label, "SCORE  0000");
    lv_obj_align(score_label, LV_ALIGN_TOP_MID, 0, 58);

    best_label = lv_label_create(screen);
    lv_obj_set_style_text_font(best_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(best_label, lv_color_white(), 0);
    lv_label_set_text(best_label, "BEST  0000");
    lv_obj_align(best_label, LV_ALIGN_TOP_MID, 0, 88);

    lv_obj_t *back = lv_button_create(screen);
    lv_obj_set_size(back, 64, 46);
    lv_obj_align(back, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back, exit_cb, LV_EVENT_CLICKED, NULL);

    game_over_panel = lv_obj_create(screen);
    lv_obj_remove_style_all(game_over_panel);
    lv_obj_set_style_bg_color(game_over_panel, lv_color_hex(0x193021), 0);
    lv_obj_set_style_bg_opa(game_over_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(game_over_panel, 22, 0);
    lv_obj_set_style_border_width(game_over_panel, 2, 0);
    lv_obj_set_style_border_color(game_over_panel, lv_color_hex(0xFFBE5A), 0);
    lv_obj_set_size(game_over_panel, 320, 190);
    lv_obj_align(game_over_panel, LV_ALIGN_CENTER, 0, -35);
    lv_obj_add_flag(game_over_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *over = lv_label_create(game_over_panel);
    lv_label_set_text(over, "GAME OVER");
    lv_obj_set_style_text_font(over, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(over, lv_color_hex(0xFF963C), 0);
    lv_obj_align(over, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *hint = lv_label_create(game_over_panel);
    lv_label_set_text(hint, "Tap = retry");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_22, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);

    runner = lv_image_create(screen);
    lv_obj_add_flag(runner, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(runner, LV_OBJ_FLAG_CLICKABLE);
    create_runner_image();

    lv_obj_t *touch = lv_obj_create(screen);
    lv_obj_remove_style_all(touch);
    lv_obj_set_size(touch, 466, 370);
    lv_obj_set_pos(touch, 0, 90);
    lv_obj_set_style_bg_opa(touch, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(touch, tap_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(touch, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

} // namespace

void vegg_open(lv_obj_t *new_screen)
{
    if (!new_screen || active) return;

    screen = new_screen;
    nvs_load_best();
    rng_state ^= lv_tick_get();

    for (int i = 0; i < 4; ++i) {
        obstacles[i].active = false;
        obstacle_obj[i] = NULL;
    }

    runner_x = 140;
    jump_h = 0;
    jump_v = 0;
    speed = 240;
    distance_run = 0;
    run_time = 0;
    score = 0;
    next_gap = 260;
    state = VEGG_RUNNING;
    active = true;
    last_tick = lv_tick_get();

    build_scene();
    update_labels();
    timer = lv_timer_create(tick, 40, NULL);
}

void vegg_stop(void)
{
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }

    for (int i = 0; i < 4; ++i) {
        obstacle_obj[i] = NULL;
    }

    runner = NULL;
    if(world_pixels){ heap_caps_free(world_pixels); world_pixels=NULL; }
    world=NULL;
    score_label = NULL;
    best_label = NULL;
    game_over_panel = NULL;
    screen = NULL;
    active = false;
}

void vegg_set_exit_callback(void (*callback)(void))
{
    exit_callback = callback;
}

bool vegg_is_active(void)
{
    return active;
}

uint32_t vegg_best_score(void)
{
    nvs_load_best();
    return best;
}

void vegg_set_best_score(uint32_t value)
{
    best = value;
    nvs_save_best();
}static void draw_obstacle(int i){ LV_UNUSED(i); }


