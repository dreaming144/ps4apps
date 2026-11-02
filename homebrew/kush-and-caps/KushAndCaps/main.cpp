/* Kush & Caps - top-down pixel dealer sim (PS4 OpenOrbis SDL2) */
#include <SDL2/SDL.h>
#include <orbis/libkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define FW 1920
#define FH 1080
#define VW 320
#define VH 180
#define TS 16
#define MW 20
#define MH 11

enum { PAD_CROSS=0, PAD_CIRCLE, PAD_SQUARE, PAD_TRIANGLE, PAD_L1, PAD_R1, PAD_OPTIONS=9, PAD_UP=13, PAD_DOWN, PAD_LEFT, PAD_RIGHT };

static SDL_Window* win;
static SDL_Surface* wsurf;
static SDL_Renderer* R;
static SDL_Texture* low;
static SDL_Joystick* pad;

/* map: 0 grass 1 road 2 wall 3 home 4 grow 5 dealer 6 buyer */
static unsigned char map[MH][MW] = {
 {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},
 {2,3,3,0,0,0,1,1,1,0,0,0,0,0,0,6,0,0,0,2},
 {2,3,4,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,2},
 {2,0,0,0,0,0,1,1,1,0,0,2,2,2,0,0,0,0,0,2},
 {2,0,0,0,0,0,1,1,1,0,0,2,5,2,0,0,6,0,0,2},
 {2,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,2},
 {2,0,6,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,2},
 {2,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,2},
 {2,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,6,0,2},
 {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
 {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},
};

struct Save {
    int magic, cash, weed, mush, seed_w, seed_m, heat, day;
    float px, py;
    int grow_w[4], grow_m[4]; /* stages 0 empty 1-3 growing 4 ready */
};
static Save G;
static int frame, dialog, dialog_choice, raid_t, day_t;
static int effect; /* 0 none 1 weed 2 mush */
static int effect_t;
static float camx, camy;
static int buyer_want[6]; /* which buyers want what */
static char msg[96];
static int msg_t;
static int paused;

/* tiny 3x5 font glyphs A-Z 0-9 space . ! ? $ % : - / */
static const unsigned char FONT[][5] = {
 /*A*/{0x2,0x5,0x7,0x5,0x5},/*B*/{0x6,0x5,0x6,0x5,0x6},/*C*/{0x3,0x4,0x4,0x4,0x3},
 /*D*/{0x6,0x5,0x5,0x5,0x6},/*E*/{0x7,0x4,0x6,0x4,0x7},/*F*/{0x7,0x4,0x6,0x4,0x4},
 /*G*/{0x3,0x4,0x5,0x5,0x3},/*H*/{0x5,0x5,0x7,0x5,0x5},/*I*/{0x7,0x2,0x2,0x2,0x7},
 /*J*/{0x1,0x1,0x1,0x5,0x2},/*K*/{0x5,0x5,0x6,0x5,0x5},/*L*/{0x4,0x4,0x4,0x4,0x7},
 /*M*/{0x5,0x7,0x5,0x5,0x5},/*N*/{0x5,0x7,0x7,0x5,0x5},/*O*/{0x2,0x5,0x5,0x5,0x2},
 /*P*/{0x6,0x5,0x6,0x4,0x4},/*Q*/{0x2,0x5,0x5,0x7,0x3},/*R*/{0x6,0x5,0x6,0x5,0x5},
 /*S*/{0x3,0x4,0x2,0x1,0x6},/*T*/{0x7,0x2,0x2,0x2,0x2},/*U*/{0x5,0x5,0x5,0x5,0x7},
 /*V*/{0x5,0x5,0x5,0x5,0x2},/*W*/{0x5,0x5,0x5,0x7,0x5},/*X*/{0x5,0x5,0x2,0x5,0x5},
 /*Y*/{0x5,0x5,0x2,0x2,0x2},/*Z*/{0x7,0x1,0x2,0x4,0x7},
 /*0*/{0x7,0x5,0x5,0x5,0x7},/*1*/{0x2,0x6,0x2,0x2,0x7},/*2*/{0x7,0x1,0x7,0x4,0x7},
 /*3*/{0x7,0x1,0x3,0x1,0x7},/*4*/{0x5,0x5,0x7,0x1,0x1},/*5*/{0x7,0x4,0x7,0x1,0x7},
 /*6*/{0x7,0x4,0x7,0x5,0x7},/*7*/{0x7,0x1,0x2,0x2,0x2},/*8*/{0x7,0x5,0x7,0x5,0x7},
 /*9*/{0x7,0x5,0x7,0x1,0x7},
};
static int glyph_ix(char c) {
    if (c>='A'&&c<='Z') return c-'A';
    if (c>='a'&&c<='z') return c-'a';
    if (c>='0'&&c<='9') return 26+(c-'0');
    return -1;
}
static void putpx(int x,int y,Uint8 r,Uint8 g,Uint8 b,Uint8 a){
    if(x<0||y<0||x>=VW||y>=VH) return;
    SDL_SetRenderDrawColor(R,r,g,b,a);
    SDL_RenderDrawPoint(R,x,y);
}
static void fill(int x,int y,int w,int h,Uint8 r,Uint8 g,Uint8 b){
    SDL_Rect rc={x,y,w,h}; SDL_SetRenderDrawColor(R,r,g,b,255); SDL_RenderFillRect(R,&rc);
}
static void draw_ch(int x,int y,char c,Uint8 r,Uint8 g,Uint8 b){
    int ix=glyph_ix(c); if(ix<0){ if(c=='$'){fill(x+1,y,1,5,r,g,b);fill(x,y+2,3,1,r,g,b);} else if(c=='!'){fill(x+1,y,1,3,r,g,b);fill(x+1,y+4,1,1,r,g,b);} else if(c=='?'){fill(x,y,3,1,r,g,b);fill(x+2,y+1,1,1,r,g,b);fill(x+1,y+2,1,1,r,g,b);fill(x+1,y+4,1,1,r,g,b);} else if(c=='.') fill(x+1,y+4,1,1,r,g,b); else if(c==':'){fill(x+1,y+1,1,1,r,g,b);fill(x+1,y+3,1,1,r,g,b);} else if(c=='-') fill(x,y+2,3,1,r,g,b); else if(c=='/'){putpx(x+2,y,r,g,b,255);putpx(x+1,y+1,r,g,b,255);putpx(x+1,y+2,r,g,b,255);putpx(x,y+3,r,g,b,255);putpx(x,y+4,r,g,b,255);} return; }
    for(int row=0;row<5;row++){ unsigned char bits=FONT[ix][row]; for(int col=0;col<3;col++) if(bits&(4>>col)) putpx(x+col,y+row,r,g,b,255); }
}
static void draw_text(int x,int y,const char*s,Uint8 r,Uint8 g,Uint8 b){
    for(int i=0;s[i];i++){ draw_ch(x+i*4,y,s[i],r,g,b); }
}
static void say(const char*s){ snprintf(msg,sizeof(msg),"%s",s); msg_t=180; }

static int price_w(){ return 40 + (G.day%7)*5 + (rand()%15); }
static int price_m(){ return 55 + (G.day%5)*8 + (rand()%20); }
static int buy_w(){ return 15 + (G.day%4)*3; }
static int buy_m(){ return 22 + (G.day%3)*4; }

static void save_game(){
    FILE*f=fopen("/data/kushcaps.sav","wb");
    if(!f) f=fopen("/app0/kushcaps.sav","wb");
    if(!f) return;
    G.magic=0x4B435053; fwrite(&G,sizeof(G),1,f); fclose(f); say("SAVED!");
}
static int load_game(){
    FILE*f=fopen("/data/kushcaps.sav","rb");
    if(!f) f=fopen("/app0/kushcaps.sav","rb");
    if(!f) return 0;
    Save t; if(fread(&t,sizeof(t),1,f)!=1){fclose(f);return 0;} fclose(f);
    if(t.magic!=0x4B435053) return 0; G=t; say("LOADED!"); return 1;
}
static void new_game(){
    memset(&G,0,sizeof(G)); G.cash=100; G.px=3.5f; G.py=2.5f; G.day=1;
    G.seed_w=3; G.seed_m=2; for(int i=0;i<4;i++){G.grow_w[i]=0;G.grow_m[i]=0;}
    for(int i=0;i<6;i++) buyer_want[i]=rand()%3; /* 0 weed 1 mush 2 either */
}

static int solid(int tx,int ty){
    if(tx<0||ty<0||tx>=MW||ty>=MH) return 1;
    unsigned char t=map[ty][tx]; return t==2;
}
static void try_move(float dx,float dy){
    float nx=G.px+dx, ny=G.py+dy;
    if(!solid((int)nx,(int)(G.py+0.2f)) && !solid((int)nx,(int)(G.py+0.8f))) G.px=nx;
    if(!solid((int)(G.px+0.2f),(int)ny) && !solid((int)(G.px+0.8f),(int)ny)) G.py=ny;
}

static void draw_tile(int tx,int ty,int sx,int sy){
    unsigned char t=map[ty][tx];
    if(t==0) fill(sx,sy,TS,TS,34,120,40);
    else if(t==1){ fill(sx,sy,TS,TS,60,60,65); fill(sx+TS/2-1,sy,2,TS,180,180,60); }
    else if(t==2) fill(sx,sy,TS,TS,90,70,50);
    else if(t==3){ fill(sx,sy,TS,TS,140,90,50); fill(sx+2,sy+6,TS-4,TS-6,100,60,30); }
    else if(t==4){ fill(sx,sy,TS,TS,50,90,40); fill(sx+2,sy+10,TS-4,4,90,60,30); }
    else if(t==5){ fill(sx,sy,TS,TS,80,40,100); fill(sx+4,sy+4,8,8,200,160,40); }
    else if(t==6){ fill(sx,sy,TS,TS,34,120,40); }
}
static void draw_person(int x,int y,Uint8 r,Uint8 g,Uint8 b,int walk){
    int bob=(walk&&(frame/8)%2)?1:0;
    fill(x+4,y+2+bob,8,6,r,g,b); /* head */
    fill(x+5,y+8+bob,6,6,r*3/4,g*3/4,b*3/4); /* body */
    int leg=(walk)?((frame/6)%2):0;
    fill(x+5+leg,y+14,2,2,20,20,30);
    fill(x+9-leg,y+14,2,2,20,20,30);
}
static void draw_plant(int x,int y,int stage,int kind){
    if(stage<=0) return;
    Uint8 r=kind?180:40, g=kind?60:160, b=kind?200:40;
    if(stage==1) fill(x+7,y+12,2,4,r,g,b);
    else if(stage==2){ fill(x+6,y+8,4,8,r,g,b); fill(x+4,y+10,2,2,r,g,b); fill(x+10,y+10,2,2,r,g,b); }
    else if(stage==3){ fill(x+5,y+4,6,12,r,g,b); fill(x+2,y+8,3,3,r,g,b); fill(x+11,y+8,3,3,r,g,b); }
    else { /* ready */ fill(x+4,y+2,8,12,r,g,b); fill(x+2,y+6,3,4,r,g,b); fill(x+11,y+6,3,4,r,g,b);
        if(kind){ fill(x+6,y,4,3,220,80,220); fill(x+7,y-2,2,2,255,180,255); }
        else fill(x+5,y,6,3,60,200,50);
    }
}
static void draw_cop(int x,int y){
    fill(x+4,y+2,8,6,40,60,160);
    fill(x+3,y+1,10,2,20,20,40); /* hat */
    fill(x+5,y+8,6,6,50,70,180);
    fill(x+5,y+14,2,2,20,20,30); fill(x+9,y+14,2,2,20,20,30);
}

static int near_tile(int want){
    int px=(int)G.px, py=(int)G.py;
    for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){
        int x=px+dx,y=py+dy; if(x>=0&&y>=0&&x<MW&&y<MH&&map[y][x]==want) return 1;
    }
    return 0;
}
static int buyer_slot(){
    int px=(int)G.px, py=(int)G.py;
    int n=0;
    for(int y=0;y<MH;y++) for(int x=0;x<MW;x++) if(map[y][x]==6){
        if(abs(px-x)<=1 && abs(py-y)<=1) return n;
        n++;
    }
    return -1;
}

static void open_dialog(){
    if(near_tile(5)){ dialog=1; dialog_choice=0; return; } /* dealer */
    int b=buyer_slot(); if(b>=0){ dialog=2+b; dialog_choice=0; return; }
    if(near_tile(4)){ dialog=10; dialog_choice=0; return; } /* grow */
    if(near_tile(3)){ dialog=11; dialog_choice=0; return; } /* home save */
}

static void do_choice(){
    if(dialog==1){ /* dealer buy seeds / product */
        if(dialog_choice==0){ int p=buy_w(); if(G.cash>=p){G.cash-=p;G.seed_w++; say("BOUGHT WEED SEED");} else say("NEED MORE CASH"); }
        else if(dialog_choice==1){ int p=buy_m(); if(G.cash>=p){G.cash-=p;G.seed_m++; say("BOUGHT MUSH SEED");} else say("NEED MORE CASH"); }
        else if(dialog_choice==2){ int p=buy_w()*2; if(G.cash>=p){G.cash-=p;G.weed++; say("BOUGHT 1 WEED");} else say("NEED MORE CASH"); }
        else if(dialog_choice==3){ int p=buy_m()*2; if(G.cash>=p){G.cash-=p;G.mush++; say("BOUGHT 1 MUSH");} else say("NEED MORE CASH"); }
        else dialog=0;
    } else if(dialog>=2 && dialog<=7){ /* sell to buyer */
        int b=dialog-2; int want=buyer_want[b];
        if(dialog_choice==0){ /* sell weed */
            if(G.weed<=0) say("NO WEED!");
            else if(want==1) say("THEY WANT MUSH!");
            else { int p=price_w(); G.weed--; G.cash+=p; G.heat+=8; buyer_want[b]=rand()%3; char buf[48]; snprintf(buf,sizeof(buf),"SOLD WEED +$%d",p); say(buf); }
        } else if(dialog_choice==1){
            if(G.mush<=0) say("NO MUSH!");
            else if(want==0) say("THEY WANT WEED!");
            else { int p=price_m(); G.mush--; G.cash+=p; G.heat+=10; buyer_want[b]=rand()%3; char buf[48]; snprintf(buf,sizeof(buf),"SOLD MUSH +$%d",p); say(buf); }
        } else dialog=0;
    } else if(dialog==10){ /* grow beds */
        if(dialog_choice>=0 && dialog_choice<4){
            int i=dialog_choice;
            if(G.grow_w[i]==0 && G.seed_w>0){ G.seed_w--; G.grow_w[i]=1; say("PLANTED WEED"); }
            else if(G.grow_w[i]>=4){ G.grow_w[i]=0; G.weed++; say("HARVESTED WEED"); }
            else if(G.grow_w[i]>0) say("STILL GROWING...");
            else say("NEED WEED SEED");
        } else if(dialog_choice>=4 && dialog_choice<8){
            int i=dialog_choice-4;
            if(G.grow_m[i]==0 && G.seed_m>0){ G.seed_m--; G.grow_m[i]=1; say("PLANTED MUSH"); }
            else if(G.grow_m[i]>=4){ G.grow_m[i]=0; G.mush++; say("HARVESTED MUSH"); }
            else if(G.grow_m[i]>0) say("STILL GROWING...");
            else say("NEED MUSH SEED");
        } else dialog=0;
    } else if(dialog==11){ /* home */
        if(dialog_choice==0){ if(G.weed>0){G.weed--; effect=1; effect_t=300; say("SMOKED WEED... WHOA");} else say("NO WEED"); }
        else if(dialog_choice==1){ if(G.mush>0){G.mush--; effect=2; effect_t=360; say("ATE MUSHROOMS... WOAH");} else say("NO MUSH"); }
        else if(dialog_choice==2){ save_game(); }
        else if(dialog_choice==3){ load_game(); }
        else dialog=0;
    }
}

static void tick_day(){
    day_t++;
    if(day_t>1200){ /* ~20s at 60fps */
        day_t=0; G.day++;
        for(int i=0;i<4;i++){ if(G.grow_w[i]>0&&G.grow_w[i]<4) G.grow_w[i]++; if(G.grow_m[i]>0&&G.grow_m[i]<4) G.grow_m[i]++; }
        if(G.heat>0) G.heat-=3; if(G.heat<0) G.heat=0;
        say("A NEW DAY DAWNS");
        for(int i=0;i<6;i++) buyer_want[i]=rand()%3;
    }
    if(G.heat>=50 && raid_t<=0 && (rand()%400)==0){ raid_t=240; say("POLICE RAID!! RUN!"); }
    if(raid_t>0){
        raid_t--;
        /* cops chase toward player roughly */
        if(raid_t==1){
            /* if still high heat and near road, get busted */
            if(G.heat>=40){ int lost=G.weed+G.mush; G.weed=0; G.mush=0; G.heat=10; G.cash = G.cash*3/4; char buf[64]; snprintf(buf,sizeof(buf),"BUSTED! LOST STASH (-%d)",lost); say(buf); }
        }
    }
    if(effect_t>0){ effect_t--; if(effect_t==0) effect=0; }
    if(msg_t>0) msg_t--;
}

static void render_world(){
    SDL_SetRenderTarget(R, low);
    /* effect tint clear */
    if(effect==1) fill(0,0,VW,VH,20,40,20);
    else if(effect==2) fill(0,0,VW,VH,40,10,50);
    else fill(0,0,VW,VH,20,30,20);

    float wobx=0,woby=0;
    if(effect==1){ wobx=sinf(frame*0.15f)*3; woby=cosf(frame*0.11f)*2; }
    if(effect==2){ wobx=sinf(frame*0.08f)*6; woby=cosf(frame*0.13f)*5; }

    camx = G.px*TS - VW/2 + wobx;
    camy = G.py*TS - VH/2 + woby;
    if(camx<0) camx=0; if(camy<0) camy=0;
    if(camx>MW*TS-VW) camx=MW*TS-VW; if(camy>MH*TS-VH) camy=MH*TS-VH;

    for(int ty=0;ty<MH;ty++) for(int tx=0;tx<MW;tx++){
        int sx=(int)(tx*TS - camx), sy=(int)(ty*TS - camy);
        if(sx>-TS&&sy>-TS&&sx<VW&&sy<VH) draw_tile(tx,ty,sx,sy);
    }
    /* grow beds visual at home grow tile (2,2) area */
    int gx=(int)(2*TS - camx), gy=(int)(2*TS - camy);
    for(int i=0;i<4;i++) draw_plant(gx-8+i*8, gy, G.grow_w[i], 0);
    for(int i=0;i<4;i++) draw_plant(gx-8+i*8, gy+8, G.grow_m[i], 1);

    /* buyers */
    int bn=0;
    for(int y=0;y<MH;y++) for(int x=0;x<MW;x++) if(map[y][x]==6){
        int sx=(int)(x*TS - camx), sy=(int)(y*TS - camy);
        Uint8 cr=180,cg=140,cb=100;
        if(buyer_want[bn]==0){cr=80;cg=200;cb=80;} else if(buyer_want[bn]==1){cr=200;cg=80;cb=200;}
        draw_person(sx,sy,cr,cg,cb,0); bn++;
    }
    /* dealer */
    for(int y=0;y<MH;y++) for(int x=0;x<MW;x++) if(map[y][x]==5){
        int sx=(int)(x*TS - camx), sy=(int)(y*TS - camy);
        draw_person(sx,sy,160,80,200,0);
    }
    /* player */
    {
        int sx=(int)(G.px*TS - camx), sy=(int)(G.py*TS - camy);
        int moving=1;
        draw_person(sx,sy,80,200,120,moving);
        if(effect==2 && (frame%3)==0){ /* trails */
            fill(sx+2,sy+2,4,4,255,100,255);
        }
    }
    /* raid cops */
    if(raid_t>0){
        float t=(240-raid_t)/240.f;
        int cx=(int)((5+t*10)*TS - camx);
        int cy=(int)((5)*TS - camy);
        draw_cop(cx,cy); draw_cop(cx+20,cy+8);
        fill(0,0,VW,8,180,20,20);
        draw_text(VW/2-40,1,"RAID IN PROGRESS",255,255,255);
    }

    /* HUD */
    fill(0,0,VW,14,10,10,20);
    char hud[80];
    snprintf(hud,sizeof(hud),"$%d W:%d M:%d DAY%d",G.cash,G.weed,G.mush,G.day);
    draw_text(2,3,hud,220,220,100);
    /* heat bar */
    fill(VW-60,3,50,6,40,40,40);
    int hw = G.heat>100?50:(G.heat/2);
    fill(VW-60,3,hw,6, G.heat>60?220:180, G.heat>60?40:120, 40);
    draw_text(VW-78,3,"HEAT",200,100,100);

    if(msg_t>0){ fill(20,VH-20,VW-40,12,0,0,0); draw_text(24,VH-17,msg,255,255,180); }

    /* dialogs */
    if(dialog){
        fill(20,40,VW-40,90,15,15,30);
        fill(22,42,VW-44,86,30,30,55);
        if(dialog==1){
            draw_text(28,48,"DEALER - BUY",255,220,100);
            const char*opts[]={"WEED SEED","MUSH SEED","1 WEED","1 MUSH","BYE"};
            for(int i=0;i<5;i++){ char line[40]; snprintf(line,sizeof(line),"%s%s", dialog_choice==i?"> ":"  ", opts[i]); draw_text(28,58+i*8,line, dialog_choice==i?255:180,255,180); }
        } else if(dialog>=2 && dialog<=7){
            int b=dialog-2; const char*w = buyer_want[b]==0?"WANTS WEED": buyer_want[b]==1?"WANTS MUSH":"WANTS ANY";
            draw_text(28,48,"BUYER",255,220,100); draw_text(60,48,w,180,255,180);
            const char*opts[]={"SELL WEED","SELL MUSH","BYE"};
            for(int i=0;i<3;i++){ char line[40]; snprintf(line,sizeof(line),"%s%s", dialog_choice==i?"> ":"  ", opts[i]); draw_text(28,60+i*10,line, dialog_choice==i?255:180,255,180); }
        } else if(dialog==10){
            draw_text(28,48,"GROW BEDS  X=DO",255,220,100);
            for(int i=0;i<4;i++){ char line[48]; snprintf(line,sizeof(line),"%sW%d:%s", dialog_choice==i?"> ":"  ", i+1, G.grow_w[i]==0?"EMPTY":G.grow_w[i]>=4?"READY":"GROW"); draw_text(28,56+i*6,line,200,255,200); }
            for(int i=0;i<4;i++){ char line[48]; snprintf(line,sizeof(line),"%sM%d:%s", dialog_choice==i+4?"> ":"  ", i+1, G.grow_m[i]==0?"EMPTY":G.grow_m[i]>=4?"READY":"GROW"); draw_text(140,56+i*6,line,255,180,255); }
            draw_text(28,110, dialog_choice==8?"> BYE":"  BYE",200,200,200);
        } else if(dialog==11){
            draw_text(28,48,"HOME",255,220,100);
            const char*opts[]={"SMOKE WEED","EAT MUSH","SAVE","LOAD","BYE"};
            for(int i=0;i<5;i++){ char line[40]; snprintf(line,sizeof(line),"%s%s", dialog_choice==i?"> ":"  ", opts[i]); draw_text(28,58+i*8,line, dialog_choice==i?255:180,255,180); }
        }
    }

    /* title flash first 3s */
    if(frame<180){
        fill(60,70,200,30,0,0,0);
        draw_text(90,78,"KUSH AND CAPS",100,255,120);
        draw_text(70,90,"X TALK  O USE/CONFIRM",200,200,200);
    }

    SDL_SetRenderTarget(R, NULL);
}

static void present(){
    /* scale low -> window */
    SDL_Rect dst={0,0,FW,FH};
    SDL_RenderCopy(R, low, NULL, &dst);
    /* colour shift overlay for mush */
    if(effect==2){
        Uint8 a = (Uint8)(40 + 30*sinf(frame*0.2f));
        SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(R, 180, 40, 220, a);
        SDL_Rect full={0,0,FW,FH}; SDL_RenderFillRect(R,&full);
        SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
    }
    if(effect==1){
        SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(R, 40, 180, 40, 30);
        SDL_Rect full={0,0,FW,FH}; SDL_RenderFillRect(R,&full);
        SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
    }
    SDL_UpdateWindowSurface(win);
}

static int max_choice(){
    if(dialog==1||dialog==11) return 4;
    if(dialog>=2&&dialog<=7) return 2;
    if(dialog==10) return 8;
    return 0;
}

int main(int argc, char* argv[]){
    (void)argc;(void)argv;
    srand((unsigned)time(NULL));
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)!=0){ for(;;){} }
    win = SDL_CreateWindow("Kush & Caps", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, FW, FH, 0);
    if(!win) for(;;){}
    wsurf = SDL_GetWindowSurface(win);
    R = SDL_CreateSoftwareRenderer(wsurf);
    if(!R) for(;;){}
    low = SDL_CreateTexture(R, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, VW, VH);
    if(SDL_NumJoysticks()>=1) pad = SDL_JoystickOpen(0);

    new_game();
    load_game(); /* try */

    int running=1;
    Uint32 last=SDL_GetTicks();
    float acc=0;
    while(running){
        Uint32 now=SDL_GetTicks();
        float dt=(now-last)/1000.f; last=now;
        /* slow-mo when high */
        float scale = (effect==1)?0.55f:(effect==2)?0.4f:1.f;
        acc += dt*scale;
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) running=0;
            if(ev.type==SDL_JOYBUTTONDOWN){
                int b=ev.jbutton.button;
                if(dialog){
                    if(b==PAD_UP||b==PAD_LEFT){ dialog_choice--; if(dialog_choice<0) dialog_choice=max_choice(); }
                    if(b==PAD_DOWN||b==PAD_RIGHT){ dialog_choice++; if(dialog_choice>max_choice()) dialog_choice=0; }
                    if(b==PAD_CROSS||b==PAD_CIRCLE) do_choice();
                    if(b==PAD_OPTIONS) dialog=0;
                } else {
                    if(b==PAD_CROSS) open_dialog();
                    if(b==PAD_OPTIONS) save_game();
                    if(b==PAD_TRIANGLE){ if(G.weed>0){G.weed--;effect=1;effect_t=300;say("SMOKED WEED");} }
                    if(b==PAD_SQUARE){ if(G.mush>0){G.mush--;effect=2;effect_t=360;say("ATE MUSH");} }
                }
            }
        }
        /* stick move */
        if(!dialog && pad){
            Sint16 ax=SDL_JoystickGetAxis(pad,0), ay=SDL_JoystickGetAxis(pad,1);
            float mx=0,my=0;
            if(ax>8000||ax<-8000) mx = ax/32767.f;
            if(ay>8000||ay<-8000) my = ay/32767.f;
            /* also dpad held */
            if(SDL_JoystickGetButton(pad,PAD_LEFT)) mx=-1;
            if(SDL_JoystickGetButton(pad,PAD_RIGHT)) mx=1;
            if(SDL_JoystickGetButton(pad,PAD_UP)) my=-1;
            if(SDL_JoystickGetButton(pad,PAD_DOWN)) my=1;
            float sp=2.2f*(effect?0.7f:1.f)*0.016f*60; /* approx */
            /* use fixed step when frame ticks */
        }

        while(acc>=1.f/60.f){
            acc-=1.f/60.f;
            frame++;
            if(!dialog && pad){
                Sint16 ax=SDL_JoystickGetAxis(pad,0), ay=SDL_JoystickGetAxis(pad,1);
                float mx=0,my=0;
                if(ax>8000||ax<-8000) mx=ax/32767.f;
                if(ay>8000||ay<-8000) my=ay/32767.f;
                if(SDL_JoystickGetButton(pad,PAD_LEFT)) mx=-1;
                if(SDL_JoystickGetButton(pad,PAD_RIGHT)) mx=1;
                if(SDL_JoystickGetButton(pad,PAD_UP)) my=-1;
                if(SDL_JoystickGetButton(pad,PAD_DOWN)) my=1;
                float sp=0.08f*(effect?0.65f:1.f);
                if(mx||my){ float l=sqrtf(mx*mx+my*my); try_move(mx/l*sp, my/l*sp); }
            }
            tick_day();
        }
        render_world();
        present();
        /* cap */
        SDL_Delay(1);
    }
    SDL_Quit();
    return 0;
}
