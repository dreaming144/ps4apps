/* San Pedro Grower - real-time Trichocereus bridgesii (PS4 OpenOrbis SDL2) */
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
enum { PAD_CROSS=0, PAD_CIRCLE, PAD_SQUARE, PAD_TRIANGLE, PAD_OPTIONS=9 };
static SDL_Window* win; static SDL_Surface* wsurf; static SDL_Renderer* R; static SDL_Texture* low; static SDL_Joystick* pad;
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
static void txt(int x,int y,const char*s,Uint8 r,Uint8 g,Uint8 b){
    for(int i=0;s[i];i++){ int cx=x+i*4; if(s[i]=='%'){putpx(cx,y,r,g,b,255);putpx(cx+2,y+1,r,g,b,255);putpx(cx+1,y+2,r,g,b,255);putpx(cx,y+3,r,g,b,255);putpx(cx+2,y+4,r,g,b,255);} else draw_ch(cx,y,s[i],r,g,b);} }
static void shadowtxt(int x,int y,const char*s,Uint8 r,Uint8 g,Uint8 b){ txt(x+1,y+1,s,20,20,30); txt(x,y,s,r,g,b); }

struct Save { int magic; int planted; long long t0; unsigned seed; };
static Save G; static const int MAGIC=0x53504452;
static void save_game(){ FILE*f=fopen("/data/sanpedro.sav","wb"); if(f){fwrite(&G,sizeof G,1,f);fclose(f);} }
static void load_game(){ FILE*f=fopen("/data/sanpedro.sav","rb"); memset(&G,0,sizeof G); if(f){ if(fread(&G,sizeof G,1,f)!=1||G.magic!=MAGIC) memset(&G,0,sizeof G); fclose(f);} }

static unsigned hsh(unsigned a,unsigned b){ unsigned h=a*2654435761u^(b+0x9e3779b9u+(a<<6)+(a>>2)); h^=h>>15; h*=0x2c1b3c6du; h^=h>>12; return h; }
/* flower k emergence day (k=0 first ~day 90). month m>=1 has 1-2 buds */
static int buds(float* em, int maxn){
    int n=0; em[n++]=88.0f+(hsh(G.seed,1)%5);
    for(int m=1;n<maxn&&m<400;m++){ int c=1+(hsh(G.seed,100+m)&1); float base=90.0f+m*30.0f-30.0f+16.0f;
        for(int i=0;i<c&&n<maxn;i++) em[n++]=base+(float)(hsh(G.seed,1000+m*7+i)%(c==2?13:28))+(i?14.0f:0.0f); }
    return n;
}
/* state */
static float D; static float growth; static float fage; static int fside; static float nextbud;
static void compute(){
    D = G.planted ? (float)((double)(time(NULL)-G.t0)/86400.0) : 0; if(D<0) D=0;
    growth = D>=30?1.0f:(floorf(D)+1)/31.0f; if(D>=30) growth=1;
    static float em[900]; int n=buds(em,900); fage=-1; nextbud=-1;
    for(int i=0;i<n;i++){ float a=D-em[i]; if(a>=0&&a<16){ if(a>fage){fage=a; fside=hsh(G.seed,5000+i)&1;} } if(a<0&&nextbud<0) nextbud=em[i]; }
}
static const char* stage(float a){ if(a<0) return "NONE"; if(a<2) return "AREOLE BUMP"; if(a<7) return "HAIRY BUD TUBE"; if(a<12) return "SWOLLEN BUD"; if(a<14) return "OPENING"; if(a<15.2f) return "FULL BLOOM"; return "WILTING"; }

static void sky(struct tm* t){
    float h=t->tm_hour+t->tm_min/60.0f; float day = h<5||h>20?0: h<7?(h-5)/2: h>18?(20-h)/2:1;
    for(int y=0;y<140;y++){ float k=y/140.0f;
        Uint8 r=(Uint8)(10+day*(90+k*80)+ (1-fabsf(day-0.5f)*2)*k*120), g=(Uint8)(12+day*(150+k*60)), b=(Uint8)(35+day*(200-k*20));
        fill(0,y,VW,1,r,g,b); }
    if(day<0.6f){ for(int i=0;i<60;i++){ unsigned s=hsh(i,77); int sx=s%VW, sy=(s>>9)%120; if(((s>>20)+SDL_GetTicks()/400)%9) putpx(sx,sy,230,230,255,255);} }
    float a=(h-6)/12.0f; if(a>=0&&a<=1){ int sx=(int)(20+a*280), sy=(int)(110-sinf(a*3.14159f)*90); for(int dy=-5;dy<=5;dy++)for(int dx=-5;dx<=5;dx++) if(dx*dx+dy*dy<=25) putpx(sx+dx,sy+dy,255,230,120,255);} 
    else { float b2=(h<6?h+24-18:h-18)/12.0f; int sx=(int)(20+b2*280), sy=(int)(110-sinf(b2*3.14159f)*90); for(int dy=-4;dy<=4;dy++)for(int dx=-4;dx<=4;dx++) if(dx*dx+dy*dy<=16&&!((dx-2)*(dx-2)+dy*dy<=10)) putpx(sx+dx,sy+dy,240,240,220,255);} 
    float sh=0.35f+0.65f*day; fill(0,140,VW,40,(Uint8)(120*sh),(Uint8)(90*sh),(Uint8)(60*sh));
    for(int x=0;x<VW;x+=3) putpx(x,140+(x*7)%5,(Uint8)(90*sh),(Uint8)(130*sh),(Uint8)(60*sh),255);
}
static void areole(int x,int y,int dir){ putpx(x,y,235,225,200,255); putpx(x+dir,y,235,225,200,255);
    for(int i=1;i<=4;i++){ putpx(x+dir*(1+i),y-i/2,210,180,90,255);} for(int i=1;i<=3;i++) putpx(x+dir*i,y+i,180,140,70,255); putpx(x,y-2,200,170,90,255); putpx(x,y-3,200,170,90,255); }
static void plant(){
    int cx=160, base=150; fill(cx-18,base,36,20,160,75,45); fill(cx-20,base,40,4,185,95,55); fill(cx-16,base+4,32,2,120,60,35); fill(cx-17,base-1,34,2,70,50,35);
    if(!G.planted){ fill(cx-3,base-4,6,4,110,150,150); areole(cx-2,base-3,-1); return; }
    int h=(int)(8+growth*110), w=growth<0.3f?8:14; int top=base-h;
    for(int y=top;y<base;y++){ int ww=w; if(y<top+4) ww=w-(top+4-y)*2; if(ww<2) ww=2; int x0=cx-ww/2;
        for(int x=x0;x<x0+ww;x++){ int rib=((x-x0)*6/ww); int edge=(x-x0)%(ww/6>0?ww/6:1)==0; Uint8 r=70,g=(Uint8)(140-rib*4),b=(Uint8)(140+rib*2); if(edge){r-=25;g-=30;b-=25;} if(x==x0||x==x0+ww-1){r=40;g=85;b=95;} putpx(x,y,r,g,b,255);} }
    for(int y=top+3;y<base-2;y+=9){ areole(cx-w/2,y,-1); areole(cx+w/2-1,y,1); putpx(cx,y+4,235,225,200,255); putpx(cx,y+3,200,170,90,255); }
    if(fage>=0){ int dir=fside?1:-1; int ax=cx+dir*(w/2), ay=top+12; float a=fage;
        if(a<2){ int s=1+(int)a; for(int i=0;i<s;i++) putpx(ax+dir*i,ay,200,170,160,255); putpx(ax+dir*s,ay-1,90,70,60,255);} 
        else { int L = a<7 ? (int)(2+(a-2)*3.2f) : 18; float ang=-0.9f; float dx=dir*cosf(ang), dy=sinf(ang);
            for(int i=0;i<L;i++){ int px=ax+(int)(dx*i), py=ay+(int)(dy*i); putpx(px,py,95,120,80,255); putpx(px+1,py,80,100,70,255); if(i%2==0){ putpx(px-1,py,60,45,40,255); putpx(px+2,py-1,60,45,40,255);} }
            int ex=ax+(int)(dx*L), ey=ay+(int)(dy*L);
            if(a<12){ int r=a<7?2:(int)(2+(a-7)*0.8f); for(int y=-r-1;y<=r;y++)for(int x=-r;x<=r;x++) if(x*x+y*y<=r*r) putpx(ex+x,ey+y-r,(Uint8)(150+(a>7?(a-7)*15:0)),140,120,255); }
            else if(a<15.2f){ float o=a<14?(a-12)/2:1; int r=(int)(4+o*12); for(int k=0;k<14;k++){ float t=k*6.2832f/14; for(int i=2;i<r;i++) putpx(ex+(int)(cosf(t)*i),ey-r/2+(int)(sinf(t)*i*0.7f),255,252,(Uint8)(240-i),255);} 
                for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++) putpx(ex+x,ey-r/2+y,250,230,120,255); }
            else { for(int i=0;i<7;i++) putpx(ex+(i%3)-1,ey+i/2,190,170,130,255); }
        } }
}
static void box(int x,int y,int w,int h){ SDL_SetRenderDrawBlendMode(R,SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(R,15,20,30,190); SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(R,&rc); SDL_SetRenderDrawBlendMode(R,SDL_BLENDMODE_NONE); }

int main(int argc, char* argv[]){
    (void)argc;(void)argv;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)!=0){ for(;;){} }
    win=SDL_CreateWindow("San Pedro Grower",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,FW,FH,0); if(!win) for(;;){}
    wsurf=SDL_GetWindowSurface(win); R=SDL_CreateSoftwareRenderer(wsurf); if(!R) for(;;){}
    low=SDL_CreateTexture(R,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,VW,VH);
    if(SDL_NumJoysticks()>=1) pad=SDL_JoystickOpen(0);
    load_game(); int info=0, confirm=0; char b[96];
    for(;;){
        SDL_Event ev; while(SDL_PollEvent(&ev)){ if(ev.type==SDL_QUIT) goto out;
            if(ev.type==SDL_JOYBUTTONDOWN){ int k=ev.jbutton.button;
                if(confirm){ if(k==PAD_CROSS){ G.planted=0; save_game(); } confirm=0; }
                else if(k==PAD_CROSS&&!G.planted){ G.magic=MAGIC; G.planted=1; G.t0=time(NULL); G.seed=(unsigned)G.t0*2654435761u^SDL_GetTicks(); save_game(); }
                else if(k==PAD_OPTIONS) info=!info;
                else if(k==PAD_TRIANGLE&&G.planted) confirm=1; } }
        compute(); time_t now=time(NULL); struct tm* t=localtime(&now);
        SDL_SetRenderTarget(R,low); sky(t); plant();
        box(0,0,VW,16); shadowtxt(4,2,"SAN PEDRO GROWER - TRICHOCEREUS BRIDGESII",200,230,210);
        if(!G.planted){ box(70,60,180,22); shadowtxt(80,64,"PRESS X TO PLANT A CUTTING",255,255,255); shadowtxt(84,73,"IT GROWS IN REAL TIME",180,200,200); }
        else { snprintf(b,sizeof b,"DAY %d   GROWTH %d%%",(int)D+1,(int)(growth*100)); shadowtxt(4,9,b,255,240,180);
            box(0,164,VW,16);
            if(fage>=0){ float tb=14-fage; if(tb>0) snprintf(b,sizeof b,"FLOWER: %s - %d DAYS TO BLOOM",stage(fage),(int)ceilf(tb)); else snprintf(b,sizeof b,"FLOWER: %s",stage(fage)); }
            else if(nextbud>=0) snprintf(b,sizeof b,"NO FLOWER - NEXT BUD IN ABOUT %d DAYS",(int)ceilf(nextbud-D)); else snprintf(b,sizeof b,"NO FLOWER");
            shadowtxt(4,169,b,255,255,255); }
        if(info){ box(40,30,240,96); const char* L[]={"BOLIVIAN TORCH CACTUS","GROWS A LITTLE EACH REAL DAY","MATURE AT DAY 30","FIRST BUD AROUND DAY 90","THEN 1-2 FLOWERS A MONTH","BUD TO BLOOM: 14 DAYS","NIGHT BLOOMING - WILTS IN 1-2 DAYS","X PLANT  TRIANGLE REPLANT  OPTIONS INFO"};
            for(int i=0;i<8;i++) shadowtxt(48,36+i*11,L[i],i?220:255,i?230:240,i?220:160);
            snprintf(b,sizeof b,"%02d:%02d",t->tm_hour,t->tm_min); shadowtxt(240,36,b,200,200,255); }
        if(confirm){ box(60,70,200,24); shadowtxt(70,74,"REPLANT? THIS RESETS YOUR CACTUS",255,200,160); shadowtxt(90,84,"X YES   ANY OTHER NO",255,255,255); }
        SDL_SetRenderTarget(R,NULL); SDL_Rect dst={0,0,FW,FH}; SDL_RenderCopy(R,low,NULL,&dst); SDL_UpdateWindowSurface(win);
        SDL_Delay(30);
    }
out: SDL_Quit(); return 0;
}
