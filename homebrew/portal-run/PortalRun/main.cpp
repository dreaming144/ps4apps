/* Cactus Courier: Portal Run - original platformer (OpenOrbis SDL2) */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define VW 320
#define VH 180
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
#define FW 1920
#define FH 1080
#define TS 8
#define LH 22
#define MAXW 240
enum {B_X=0,B_O,B_SQ,B_TRI,B_L1,B_R1,B_OPT=9,B_UP=13,B_DN,B_LT,B_RT};
static void ftxt(int x,int y,const char*s,int sc,Uint8 r,Uint8 g,Uint8 b){ /* scaled text */
  for(int i=0;s[i];i++){ int ix=glyph_ix(s[i]); if(ix<0) continue; for(int row=0;row<5;row++) for(int col=0;col<3;col++) if(FONT[ix][row]&(4>>col)) fill(x+(i*4+col)*sc,y+row*sc,sc,sc,r,g,b); } }
static void ctxt(int y,const char*s,Uint8 r,Uint8 g,Uint8 b){ shadowtxt(VW/2-(int)strlen(s)*2,y,s,r,g,b); }
static void box(int x,int y,int w,int h){ SDL_SetRenderDrawBlendMode(R,SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(R,15,20,30,190); SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(R,&rc); SDL_SetRenderDrawBlendMode(R,SDL_BLENDMODE_NONE); }

static char T[LH][MAXW]; static int LW, LV;
static unsigned rs; static int rnd(int n){ rs=rs*1103515245u+12345u; return (int)((rs>>16)%(unsigned)n); }
static int solidc(int c){ return c=='G'||c=='X'||c=='B'||c=='?'||c=='q'||c=='u'||c=='W'; }
static int tileAt(float px,float py){ int tx=(int)floorf(px/TS),ty=(int)floorf(py/TS); if(tx<0||tx>=LW) return 'X'; if(ty<0||ty>=LH) return '.'; return T[ty][tx]; }
static int hitTX,hitTY;
static int moveBox(float&x,float&y,float w,float h,float&vx,float&vy){
  int f=0; x+=vx;
  if(vx!=0){ float ex=vx>0?x+w-0.01f:x; for(float yy=y;;yy+=TS){ float sy=yy>y+h-0.01f?y+h-0.01f:yy;
      if(solidc(tileAt(ex,sy))){ int tx=(int)floorf(ex/TS); x=vx>0?tx*TS-w:(tx+1)*TS; vx=0; f|=1; break; } if(sy>=y+h-0.01f) break; } }
  y+=vy;
  if(vy!=0){ float ey=vy>0?y+h-0.01f:y; int found=0; for(float xx=x;;xx+=TS){ float sx=xx>x+w-0.01f?x+w-0.01f:xx;
      if(solidc(tileAt(sx,ey))){ int ty=(int)floorf(ey/TS); if(!found){ hitTX=(int)floorf(sx/TS); hitTY=ty; } found=1;
        if(fabsf(sx-(x+w/2))<TS/2){ hitTX=(int)floorf(sx/TS); } }
      if(sx>=x+w-0.01f) break; }
    if(found){ if(vy>0){ y=hitTY*TS-h; f|=2; } else { y=(hitTY+1)*TS; f|=4; } vy=0; } }
  return f;
}
/* entities: 1 grunt 2 beetle 3 moth 4 boss 10 water 11 sunseed 12 pollen 20 fireball 21 bossfire 30 puff */
struct Ent{ int type,alive,hp,t; float x,y,vx,vy,bx,by,w,h; };
static Ent E[128];
static Ent* spawn(int ty,float x,float y){ for(int i=0;i<128;i++) if(!E[i].alive){ Ent&e=E[i]; memset(&e,0,sizeof e); e.type=ty; e.alive=1; e.x=x; e.y=y; e.bx=x; e.by=y; e.w=8; e.h=8; e.hp=1;
    if(ty==1||ty==2) e.vx=-0.45f; if(ty==2) e.hp=2; if(ty==4){ e.w=20; e.h=20; e.hp=6; e.vx=-0.6f; e.bx=0; } if(ty>=20&&ty<30){ e.w=4; e.h=4; } if(ty==10||ty==11) e.vx=0.7f; return &e; } return 0; }
/* player */
static float px,py,pvx,pvy,ax=1,ay=0; static int big,fire,inv,star,onground,face=1,dead,anim,lives=3,seeds,score,timer,cool,clearT,bossDown;
static float pw=6; static float PH(){ return big?14.0f:8.0f; }
struct Portal{ int on,tx,ty,nx,ny; }; static Portal P[2];
struct Save{ int magic,unlocked,best; }; static Save S; static const int MAGIC=0x50524E31;
static void save_game(){ FILE*f=fopen("/data/portalrun.sav","wb"); if(f){ fwrite(&S,sizeof S,1,f); fclose(f);} }
static void load_game(){ FILE*f=fopen("/data/portalrun.sav","rb"); memset(&S,0,sizeof S); if(f){ if(fread(&S,sizeof S,1,f)!=1||S.magic!=MAGIC) memset(&S,0,sizeof S); fclose(f);} if(!S.magic){ S.magic=MAGIC; S.unlocked=1; } }
static const char* LNAME[7]={"DUNE ROAD","SPINE HILLS","DRY CREEK","MOTH CANYON","ROOT CAVES","BASALT RIDGE","GECKO KEEP"};

static void col(int x,int y0,int y1,char c){ for(int y=y0;y<=y1;y++) if(x>=0&&x<LW&&y>=0&&y<LH) T[y][x]=c; }
static void puzzle(int c,int gy){ for(int x=c;x<c+2;x++) for(int y=0;y<=18;y++) T[y][x]=(y==gy||y==gy+1)?'.':'X'; col(c+8,0,11,'G'); for(int x=c-14;x<c;x++){ col(x,19,21,'G'); col(x,0,18,'.'); } }
static void genLevel(int L){
  memset(T,'.',sizeof T); memset(E,0,sizeof E); memset(P,0,sizeof P); rs=L*7919u+13u; LV=L; bossDown=0;
  if(L==6){ LW=48; for(int x=0;x<LW;x++){ col(x,19,21,'X'); T[0][x]='X'; } col(0,0,21,'X'); col(LW-1,0,21,'X');
    for(int x=10;x<15;x++) T[14][x]='X'; for(int x=30;x<35;x++) T[14][x]='X'; T[14][12]='?';
    spawn(4,36*TS,19*TS-20); timer=300; return; }
  LW=220; for(int x=0;x<LW;x++) col(x,19,21,'G');
  int w2=L>=3, x=14, pz=0, pzA=60, pzB=140;
  while(x<LW-26){
    if((pz==0&&x>=pzA)||(w2&&pz==1&&x>=pzB)||(!w2&&pz==1&&x>=pzB&&L==2)){ puzzle(x+16,8+(pz&1)); x+=28; pz++; continue; }
    int s=rnd(6), w;
    switch(s){
      case 0: w=2+rnd(w2?4:3); for(int i=0;i<w;i++) col(x+1+i,19,21,'.'); if(w2&&rnd(2)) spawn(3,(x+2)*TS,12*TS); x+=w+3; break;
      case 1: T[15][x+1]='B'; T[15][x+2]=rnd(3)?'q':'?'; T[15][x+3]='B'; T[15][x+4]='q'; for(int i=1;i<5;i++) T[12][x+i]='o'; spawn(1,(x+5)*TS,18*TS); x+=8; break;
      case 2: for(int i=0;i<4;i++) col(x+i,18-i,18,'G'); for(int i=0;i<4;i++) col(x+4+i,15+i,18,'G'); T[13][x+3]='o'; T[13][x+4]='o'; x+=10; break;
      case 3: spawn(w2&&rnd(2)?2:1,(x+3)*TS,18*TS); spawn(rnd(3)?1:2,(x+7)*TS,18*TS); for(int i=2;i<8;i+=2) T[17][x+i]='o'; x+=10; break;
      case 4: col(x,16,18,'W'); col(x+1,16,18,'W'); for(int i=5;i<7;i++) col(x+i,9,18,'X'); col(x+9,16,18,'W'); col(x+10,16,18,'W'); T[10][x+3]='o'; x+=13; break;
      default: for(int i=2;i<8;i++) T[14][x+i]='G'; T[11][x+4]='o'; T[11][x+5]='o'; spawn(w2||L==2?3:1,(x+5)*TS,w2||L==2?10*TS:18*TS); x+=10; break; }
  }
  T[15][7]='B'; T[15][8]='?'; T[15][9]='B';
  for(int x2=LW-24;x2<LW;x2++){ col(x2,0,18,'.'); col(x2,19,21,'G'); }
  col(LW-8,15,18,'T'); col(LW-1,0,18,'X'); timer=300;
}
static void startLevel(int L){ genLevel(L); px=3*TS; py=18*TS-PH(); pvx=pvy=0; dead=0; inv=0; star=0; clearT=0; cool=0; }
static void die(){ if(dead) return; dead=1; pvy=-4; P[0].on=P[1].on=0; big=0; fire=0; }
static void hurt(){ if(inv||star||dead) return; if(big){ big=0; fire=0; inv=90; py+=6; } else die(); }
static int overlap(float x1,float y1,float w1,float h1,float x2,float y2,float w2,float h2){ return x1<x2+w2&&x2<x1+w1&&y1<y2+h2&&y2<y1+h1; }
static void faceC(const Portal&p,float&cx,float&cy){ cx=p.tx*TS+4+p.nx*4.0f; cy=p.ty*TS+4+p.ny*4.0f; }
static void castPortal(int which){
  float ph=PH(), ox=px+pw/2, oy=py+ph/2; int ptx=(int)floorf(ox/TS), pty=(int)floorf(oy/TS);
  for(int i=0;i<700;i++){ ox+=ax*0.5f; oy+=ay*0.5f; int tx=(int)floorf(ox/TS), ty=(int)floorf(oy/TS);
    if(ty<0||ty>=LH||tx<0||tx>=LW) return;
    if(tx!=ptx||ty!=pty){ int c=T[ty][tx];
      if(solidc(c)){ if(c!='G') return; int nx=0,ny=0; if(tx!=ptx) nx=ptx>tx?1:-1; else ny=pty>ty?1:-1;
        int ex=tx+nx, ey=ty+ny; if(ey>=0&&ey<LH&&solidc(T[ey][ex])) return;
        Portal&o=P[1-which]; if(o.on&&o.tx==tx&&o.ty==ty&&o.nx==nx&&o.ny==ny) o.on=0;
        P[which].on=1; P[which].tx=tx; P[which].ty=ty; P[which].nx=nx; P[which].ny=ny; return; }
      ptx=tx; pty=ty; } } }
static void portals(){
  if(cool>0){ cool--; return; } if(!P[0].on||!P[1].on) return;
  float ph=PH(), pcx=px+pw/2, pcy=py+ph/2;
  for(int k=0;k<2;k++){ Portal&a=P[k]; Portal&b=P[1-k]; float cx,cy; faceC(a,cx,cy); int in=0;
    if(a.nx==-1) in = px+pw>=cx-0.6f && pcx<cx && fabsf(pcy-cy)<9 && pvx>=-0.05f;
    if(a.nx==1)  in = px<=cx+0.6f && pcx>cx && fabsf(pcy-cy)<9 && pvx<=0.05f;
    if(a.ny==-1) in = py+ph>=cy-0.6f && pcy<cy && fabsf(pcx-cx)<5 && pvy>=-0.05f;
    if(a.ny==1)  in = py<=cy+0.6f && pcy>cy && fabsf(pcx-cx)<5 && pvy<=0.05f;
    if(!in) continue;
    float s=sqrtf(pvx*pvx+pvy*pvy), ang=atan2f(pvy,pvx)+atan2f((float)b.ny,(float)b.nx)-atan2f((float)-a.ny,(float)-a.nx);
    pvx=cosf(ang)*s; pvy=sinf(ang)*s; float d=pvx*b.nx+pvy*b.ny; if(d<2.6f){ pvx+=b.nx*(2.6f-d); pvy+=b.ny*(2.6f-d); }
    if(s>6.5f){ pvx*=6.5f/s; pvy*=6.5f/s; }
    float qx,qy; faceC(b,qx,qy); px=qx+b.nx*(pw/2+1)-pw/2; py=qy+b.ny*(ph/2+1)-ph/2; cool=14; return; }
}
static void bumpTile(int tx,int ty){
  if(tx<0||tx>=LW||ty<0||ty>=LH) return; char c=T[ty][tx];
  if(c=='?'){ T[ty][tx]='u'; int k=!big?10:(!fire?11:(rnd(3)?11:12)); if(rnd(5)==0) k=12; spawn(k,tx*TS,(ty-1)*TS); }
  else if(c=='q'){ T[ty][tx]='u'; seeds++; score+=50; }
  else if(c=='B'&&big){ T[ty][tx]='.'; score+=50; spawn(30,tx*TS,ty*TS); }
  for(int i=0;i<128;i++){ Ent&e=E[i]; if(e.alive&&e.type<=2&&overlap(e.x,e.y,e.w,e.h,tx*TS,(ty-1)*TS,8,8)){ e.alive=0; score+=100; } }
}
static int btn(int b){ return pad&&SDL_JoystickGetButton(pad,b); }
static int levelDone; /* 1 = cleared */
static void updatePlayer(int jumpPress,int throwPress,int downPress){
  float ph=PH();
  if(dead){ pvy+=0.25f; py+=pvy; dead++; return; }
  float mx=0; if(pad){ int a=SDL_JoystickGetAxis(pad,0); if(abs(a)>9000) mx=a/32767.0f; }
  if(btn(B_LT)) mx=-1; if(btn(B_RT)) mx=1;
  int run=btn(B_SQ); float maxs=run?2.3f:1.4f, acc=onground?0.14f:0.09f;
  if(mx!=0){ pvx+=mx*acc; if(pvx>maxs*fabsf(mx)+0.01f&&mx>0) pvx-=acc*1.5f; if(pvx<-maxs*fabsf(mx)-0.01f&&mx<0) pvx+=acc*1.5f; face=mx>0?1:-1; }
  else { pvx*=onground?0.82f:0.97f; if(fabsf(pvx)<0.05f) pvx=0; }
  if(pvx>6.5f) pvx=6.5f; if(pvx<-6.5f) pvx=-6.5f;
  if(jumpPress&&onground){ pvy=-(4.1f+fabsf(pvx)*0.15f); onground=0; }
  pvy+=(pvy<0&&!btn(B_X))?0.5f:0.22f; if(pvy>5) pvy=5;
  if(throwPress&&fire){ int n=0; for(int i=0;i<128;i++) if(E[i].alive&&E[i].type==20) n++; if(n<2){ Ent*f=spawn(20,px+(face>0?6:-4),py+3); if(f){ f->vx=3.2f*face; f->vy=1; } } }
  if(downPress&&onground){ int tx=(int)((px+pw/2)/TS), ty=(int)((py+ph+1)/TS);
    if(ty<LH&&tx>=0&&tx<LW&&T[ty][tx]=='W'){ int x=tx; while(x<LW&&T[ty][x]=='W') x++; while(x<LW&&T[ty][x]!='W') x++;
      if(x<LW){ px=x*TS+1; py=ty*TS-ph; pvx=0; pvy=0; } } }
  int f=moveBox(px,py,pw,ph,pvx,pvy); onground=(f&2)!=0; if(f&4) bumpTile(hitTX,hitTY);
  portals();
  anim++; if(inv) inv--; if(star) star--;
  /* tiles */
  for(int yy=(int)(py/TS);yy<=(int)((py+ph-1)/TS);yy++) for(int xx=(int)(px/TS);xx<=(int)((px+pw-1)/TS);xx++){
    if(yy<0||yy>=LH||xx<0||xx>=LW) continue; if(T[yy][xx]=='o'){ T[yy][xx]='.'; seeds++; score+=10; if(seeds>=100){ seeds-=100; lives++; } }
    if(T[yy][xx]=='T'&&!levelDone){ levelDone=1; score+=timer*10+(19-yy)*200; } }
  if(py>LH*TS+8) die();
}
static void updateEnts(){
  float ph=PH();
  for(int i=0;i<128;i++){ Ent&e=E[i]; if(!e.alive) continue; e.t++;
    if(e.x<px-260||e.x>px+340){ if(e.type>=20) e.alive=0; else if(e.type!=4) continue; }
    switch(e.type){
      case 1: case 2: case 10: case 11: { e.vy+=0.22f; if(e.vy>5) e.vy=5; float ovx=e.vx; int f=moveBox(e.x,e.y,e.w,e.h,e.vx,e.vy); if(f&1) e.vx=-ovx; if(e.y>LH*TS) e.alive=0; } break;
      case 12: { e.vy+=0.2f; float ovx=e.vx?e.vx:1.0f; e.vx=ovx; int f=moveBox(e.x,e.y,e.w,e.h,e.vx,e.vy); if(f&1) e.vx=-ovx; if(f&2) e.vy=-3.5f; if(e.y>LH*TS) e.alive=0; } break;
      case 3: e.x=e.bx+sinf(e.t*0.02f)*30; e.y=e.by+sinf(e.t*0.07f)*10; break;
      case 4: { e.vy+=0.22f; float ovx=e.vx; int f=moveBox(e.x,e.y,e.w,e.h,e.vx,e.vy); if(f&1) e.vx=-ovx; if((f&2)&&e.t%110==0) e.vy=-4.2f;
        if(e.t%80==40){ Ent*b=spawn(21,e.x+10,e.y+6); if(b){ b->vx=px<e.x?-2.0f:2.0f; } } if(e.hp<=0){ e.alive=0; bossDown=1; score+=5000; col(LW-4,16,18,'T'); } if(e.bx>0) e.bx--; } break;
      case 20: { float ovy=e.vy; e.vy+=0.25f; int f=moveBox(e.x,e.y,e.w,e.h,e.vx,e.vy); if(f&2) e.vy=-2.2f; else if(f&4) e.vy=-ovy*0; if((f&1)||e.t>100) e.alive=0;
        for(int j=0;j<128;j++){ Ent&o=E[j]; if(o.alive&&((o.type>=1&&o.type<=3)||o.type==4)&&overlap(e.x,e.y,4,4,o.x,o.y,o.w,o.h)){ e.alive=0; if(o.type==4){ if(!o.bx){ o.hp--; o.bx=40; } } else { o.alive=0; score+=200; spawn(30,o.x,o.y); } break; } } } break;
      case 21: e.x+=e.vx; if(e.t>200) e.alive=0; break;
      case 30: if(e.t>20) e.alive=0; break; }
    if(!e.alive||dead) continue;
    if(overlap(px,py,pw,ph,e.x,e.y,e.w,e.h)){
      if(e.type==10){ e.alive=0; if(!big){ big=1; py-=6; } score+=1000; }
      else if(e.type==11){ e.alive=0; big=1; fire=1; score+=1000; }
      else if(e.type==12){ e.alive=0; star=600; score+=1000; }
      else if(e.type==21) hurt();
      else if(e.type>=1&&e.type<=4){
        if(star&&e.type!=4){ e.alive=0; score+=200; spawn(30,e.x,e.y); }
        else if(pvy>0&&py+ph<e.y+5){ pvy=btn(B_X)?-4.6f:-3.0f; if(e.type==4){ if(!e.bx){ e.hp--; e.bx=60; } } else if(--e.hp<=0){ e.alive=0; score+=100; spawn(30,e.x,e.y); } else { e.vx*=1.8f; } }
        else hurt(); } } }
}
/* ---------- drawing ---------- */
static int cam;
static void drawTiles(){
  int w2=LV>=3, fort=LV==6;
  for(int y=0;y<VH;y++){ float t=y/(float)VH; if(fort) fill(0,y,VW,1,(Uint8)(30+t*20),(Uint8)(20+t*10),(Uint8)(30+t*20)); else if(w2) fill(0,y,VW,1,(Uint8)(70+t*150),(Uint8)(50+t*60),(Uint8)(120-t*20)); else fill(0,y,VW,1,(Uint8)(120+t*110),(Uint8)(190+t*30),(Uint8)(240-t*40)); }
  if(!fort) for(int i=0;i<8;i++){ int mx=((i*97-cam/4)%440+440)%440-60, mh=30+(i*37)%40; fill(mx,150-mh,50,mh+30,w2?110:200,w2?70:130,w2?100:90); fill(mx+6,150-mh-6,38,6,w2?110:200,w2?70:130,w2?100:90); }
  int x0=cam/TS;
  for(int ty=0;ty<LH;ty++) for(int tx=x0;tx<=x0+VW/TS+1&&tx<LW;tx++){ char c=T[ty][tx]; int sx=tx*TS-cam, sy=ty*TS; if(c=='.') continue;
    switch(c){
      case 'G': fill(sx,sy,8,8,w2?150:200,w2?100:150,w2?80:90); putpx(sx+2,sy+5,140,100,60,255); putpx(sx+6,sy+2,140,100,60,255); if(ty==0||!solidc(T[ty-1][tx])) fill(sx,sy,8,2,w2?90:110,w2?150:180,70); break;
      case 'X': fill(sx,sy,8,8,60,60,72); fill(sx,sy,8,1,85,85,100); putpx(sx+3,sy+3,40,40,50,255); putpx(sx+4,sy+4,40,40,50,255); break;
      case 'B': fill(sx,sy,8,8,170,110,50); fill(sx+1,sy+1,6,6,200,140,70); fill(sx+1,sy+3,6,1,140,90,40); break;
      case '?': case 'q': { int p=(SDL_GetTicks()/200)%2; fill(sx,sy,8,8,90,40,120); fill(sx+1,sy+1,6,6,150,70,190); fill(sx+3,sy+2,2,4,p?255:230,p?230:200,120); } break;
      case 'u': fill(sx,sy,8,8,90,80,70); fill(sx+1,sy+1,6,6,110,100,90); break;
      case 'W': fill(sx,sy,8,8,110,70,40); fill(sx+2,sy,2,8,140,90,50); if(ty==0||T[ty-1][tx]!='W') fill(sx,sy,8,2,120,230,170); break;
      case 'T': fill(sx+2,sy,4,8,150,90,50); if(ty==0||T[ty-1][tx]!='T'){ fill(sx,sy,8,4,230,190,60); putpx(sx+2,sy+1,0,0,0,255); putpx(sx+5,sy+1,0,0,0,255); fill(sx+2,sy+3,4,1,120,40,30); } else { fill(sx+2,sy+3,4,1,60,180,200); } break;
      case 'o': fill(sx+3,sy+2,2,4,250,220,90); putpx(sx+3,sy+2,255,255,200,255); break; } }
}
static void drawPlayer(){
  if(inv&&(inv/4)%2) return; float ph=PH(); int sx=(int)px-cam, sy=(int)py, h=(int)ph;
  Uint8 r=60,g=170,b=70; if(star){ int k=(anim/3)%3; r=k==0?255:120; g=k==1?255:200; b=k==2?255:90; }
  fill(sx,sy+1,6,h-3,r,g,b); fill(sx+1,sy,4,1,r,g,b); fill(sx+2,sy+1,1,h-3,(Uint8)(r*0.7f),(Uint8)(g*0.7f),(Uint8)(b*0.7f));
  putpx(sx-1,sy+2,255,255,230,255); putpx(sx+6,sy+3,255,255,230,255); putpx(sx-1,sy+h-5,255,255,230,255); putpx(sx+6,sy+h-6,255,255,230,255);
  int ex=face>0?sx+3:sx+1; putpx(ex,sy+2,0,0,0,255); putpx(ex+1,sy+2,0,0,0,255);
  if(fire) fill(sx+1,sy,4,1,250,140,40);
  int bxp=face>0?sx-1:sx+5; fill(bxp,sy+h/2,2,3,140,85,40); putpx(face>0?sx+5:sx,sy+h/2-2,140,85,40,255);
  int fr=(onground&&fabsf(pvx)>0.2f)?(anim/5)%2:0; fill(sx+(fr?0:1),sy+h-2,2,2,120,80,40); fill(sx+(fr?4:3),sy+h-2,2,2,120,80,40);
  int gx=sx+3+(int)(ax*5), gy=sy+h/2+(int)(ay*5); fill(gx-1,gy-1,3,2,180,180,190); putpx(gx+(int)(ax*2),gy+(int)(ay*2),(anim/8)%2?60:255,(anim/8)%2?140:150,(anim/8)%2?255:40,255);
  for(int i=3;i<12;i++) if(i%2) putpx(sx+3+(int)(ax*i*4),sy+h/2+(int)(ay*i*4),255,255,255,255);
}
static void drawPortal(const Portal&p,int k){ if(!p.on) return; Uint8 r=k?255:60,g=k?150:140,b=k?40:255; float cx,cy; faceC(p,cx,cy); int sx=(int)cx-cam, sy=(int)cy;
  if(p.nx){ fill(sx-1,sy-8,2,16,r,g,b); fill(sx-(p.nx<0?2:0)+(p.nx>0?1:0),sy-6,1,12,255,255,255); } else { fill(sx-6,sy-1,12,2,r,g,b); fill(sx-4,sy+(p.ny<0?-1:1),8,1,255,255,255); } }
static void drawEnt(const Ent&e){ int sx=(int)e.x-cam, sy=(int)e.y, t=e.t;
  switch(e.type){
    case 1: fill(sx+2,sy+4,4,3,70,170,160); fill(sx,sy+1,8,3,190,160,230); fill(sx+1,sy,6,1,190,160,230); putpx(sx+(e.vx<0?2:5),sy+5,0,0,0,255); fill(sx+(e.vx<0?1:4),sy+4,3,1,40,20,40); fill(sx+((t/8)%2?1:2),sy+7,2,1,40,60,60); fill(sx+((t/8)%2?5:4),sy+7,2,1,40,60,60); break;
    case 2: fill(sx,sy+3,8,4,e.hp>1?120:170,e.hp>1?125:110,e.hp>1?135:110); fill(sx+1,sy+2,6,1,150,150,160); putpx(sx+2,sy+4,80,80,90,255); putpx(sx+5,sy+4,80,80,90,255); fill(sx+(e.vx<0?0:7),sy+5,1,1,250,250,250); fill(sx+1,sy+7,6,1,40,40,40); break;
    case 3: { int f=(t/5)%2; fill(sx+3,sy+2,2,5,120,90,60); fill(sx,sy+(f?1:3),3,f?4:2,235,225,200); fill(sx+5,sy+(f?1:3),3,f?4:2,235,225,200); putpx(sx+3,sy+1,0,0,0,255); putpx(sx+4,sy+1,0,0,0,255); } break;
    case 4: { if(e.bx&&(t/3)%2) break; int fl=e.vx<0; fill(sx,sy+6,20,10,50,160,150); fill(sx+(fl?0:12),sy,8,8,60,180,165); fill(sx+(fl?0:12),sy-2,8,3,120,50,150); for(int i=0;i<3;i++) putpx(sx+(fl?1:13)+i*3,sy-3,200,200,220,255);
      putpx(sx+(fl?2:17),sy+3,255,220,0,255); fill(sx+(fl?20:-6),sy+10,6,2,50,160,150); fill(sx+3,sy+16,3,4,40,120,110); fill(sx+14,sy+16,3,4,40,120,110); fill(sx+4,sy+8,12,2,240,150,60); } break;
    case 10: fill(sx+2,sy+3,4,4,70,150,255); fill(sx+3,sy+1,2,2,70,150,255); putpx(sx+3,sy+4,220,240,255,255); break;
    case 11: fill(sx+2,sy+2,4,4,250,140,30); putpx(sx+3,sy,255,220,80,255); putpx(sx+3,sy+7,255,220,80,255); putpx(sx,sy+3,255,220,80,255); putpx(sx+7,sy+3,255,220,80,255); break;
    case 12: { Uint8 v=(Uint8)(180+75*sinf(t*0.3f)); fill(sx+1,sy+1,6,6,v,v,60); fill(sx+3,sy+3,2,2,255,255,255); } break;
    case 20: fill(sx,sy,4,4,255,150,30); fill(sx+1,sy+1,2,2,255,240,120); break;
    case 21: fill(sx,sy,4,4,220,40,60); break;
    case 30: for(int i=0;i<4;i++) putpx(sx+4+(int)(cosf(i*1.57f)*t/3),sy+4+(int)(sinf(i*1.57f)*t/3),255,255,255,255); break; } }
static void hud(){ char b[96]; box(0,0,VW,9); snprintf(b,sizeof b,"%s  SEEDS %02d  SCORE %06d  TIME %03d  LIVES %d",LV==6?"KEEP":(LV<3?(LV==0?"1-1":LV==1?"1-2":"1-3"):(LV==3?"2-1":LV==4?"2-2":"2-3")),seeds,score,timer,lives); shadowtxt(3,2,b,255,255,255); }
static void drawCourier(int x,int y,int s){ /* big title sprite */
  fill(x,y+s,6*s,10*s,60,170,70); fill(x+s,y,4*s,s,60,170,70); fill(x+2*s,y+s,s,9*s,45,130,55); fill(x+4*s,y+2*s,s,s,0,0,0);
  fill(x-s,y+5*s,2*s,3*s,140,85,40); fill(x+6*s,y+5*s,4*s,2*s,180,180,190); fill(x+10*s,y+5*s,s,2*s,60,140,255); }
int main(int argc,char*argv[]){
  (void)argc;(void)argv;
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)!=0){ for(;;){} }
  win=SDL_CreateWindow("Portal Run",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,FW,FH,0); if(!win) for(;;){}
  wsurf=SDL_GetWindowSurface(win); R=SDL_CreateSoftwareRenderer(wsurf); if(!R) for(;;){}
  low=SDL_CreateTexture(R,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,VW,VH);
  if(SDL_NumJoysticks()>=1) pad=SDL_JoystickOpen(0);
  load_game(); int state=0, sel=0, psel=0, fr=0, lvl=0, msgT=0; char b[96];
  for(;;){
    Uint32 t0=SDL_GetTicks(); int jp=0,tp=0,dp=0,up=0,lp=0,rp=0,op=0,cp=0,l1=0,r1=0;
    SDL_Event ev; while(SDL_PollEvent(&ev)){ if(ev.type==SDL_QUIT) goto out;
      if(ev.type==SDL_JOYBUTTONDOWN){ int k=ev.jbutton.button; if(k==B_X) jp=1; if(k==B_SQ) tp=1; if(k==B_DN) dp=1; if(k==B_UP) up=1; if(k==B_LT) lp=1; if(k==B_RT) rp=1; if(k==B_OPT) op=1; if(k==B_O) cp=1; if(k==B_L1) l1=1; if(k==B_R1) r1=1; }
      if(ev.type==SDL_JOYAXISMOTION&&ev.jaxis.axis==1&&ev.jaxis.value>26000&&!dp) dp=2;
      if(ev.type==SDL_JOYAXISMOTION&&ev.jaxis.axis==0){ static int lastx=0; int v=ev.jaxis.value; if(v<-20000&&lastx>=-20000) lp=2; if(v>20000&&lastx<=20000) rp=2; lastx=v; } }
    fr++; SDL_SetRenderTarget(R,low);
    if(state==0){ /* title */
      fill(0,0,VW,VH,120,190,240); for(int y=110;y<VH;y++) fill(0,y,VW,1,210,160,100);
      fill(40,60,6,40,60,140,255); fill(42,62,2,36,200,230,255); fill(274,60,6,40,255,150,40); fill(276,62,2,36,255,230,200);
      drawCourier(130,70+(fr/20)%2,4);
      ftxt(52,16,"CACTUS COURIER",4,30,90,40); ftxt(50,14,"CACTUS COURIER",4,250,240,200); ftxt(108,40,"PORTAL RUN",2,255,150,40);
      if((fr/30)%2) ctxt(130,"PRESS X TO START",255,255,255);
      ctxt(150,"R1 BLUE PORTAL  L1 ORANGE PORTAL  RIGHT STICK AIM",230,240,255); ctxt(160,"X JUMP  SQUARE RUN/THROW  DOWN ON ROOT TO WARP",230,240,255);
      if(jp){ state=1; lives=3; score=0; seeds=0; big=fire=0; sel=S.unlocked-1; } }
    else if(state==1){ /* map */
      fill(0,0,VW,VH,200,160,100); fill(0,100,VW,80,110,70,100); ctxt(8,"WORLD MAP",255,255,255); ctxt(20,sel<3?"WORLD 1 - SUNBAKED FLATS":sel<6?"WORLD 2 - DUSK MESA":"THE GECKO KEEP",255,240,180);
      for(int i=0;i<7;i++){ int x=30+i*43, y=i<3?70:(i<6?120:95); if(i<6){ int x2=30+(i+1)*43, y2=i+1<3?70:(i+1<6?120:95); for(int s2=0;s2<20;s2++) putpx(x+(x2-x)*s2/20,y+(y2-y)*s2/20,80,50,30,255); }
        int on=i<S.unlocked; fill(x-5,y-5,10,10,on?(i==6?150:60):90,on?(i==6?60:150):90,on?(i==6?60:70):90);
        if(i==sel){ drawCourier(x-6,y-26+(fr/15)%2,1); fill(x-6,y-6,12,1,255,255,255); fill(x-6,y+5,12,1,255,255,255); } }
      ctxt(150,LNAME[sel],255,255,255); snprintf(b,sizeof b,"LIVES %d   BEST %d   X PLAY  O TITLE",lives,S.best); ctxt(162,b,220,230,240);
      if(lp&&sel>0) sel--; if(rp&&sel<S.unlocked-1) sel++; if(cp) state=0;
      if(jp){ lvl=sel; levelDone=0; startLevel(lvl); state=2; msgT=120; } }
    else if(state==2||state==3){
      if(state==2){
        if(op){ state=3; psel=0; }
        else {
          if(pad){ int rx=SDL_JoystickGetAxis(pad,2), ry=SDL_JoystickGetAxis(pad,3); if(rx*(float)rx+ry*(float)ry>9000.0f*9000.0f){ float l=sqrtf(rx*(float)rx+ry*(float)ry); ax=rx/l; ay=ry/l; } else { ax=(float)face; ay=0; } }
          if(!dead&&!levelDone){ if(r1) castPortal(0); if(l1) castPortal(1); }
          if(!levelDone) updatePlayer(jp,tp,dp); updateEnts();
          if(fr%60==0&&!dead&&!levelDone&&timer>0){ timer--; if(timer==0) die(); }
          if(levelDone){ clearT++; if(clearT>150){ if(lvl+2>S.unlocked) S.unlocked=lvl+2>7?7:lvl+2; if(score>S.best) S.best=score; save_game(); if(lvl==6){ state=5; } else { state=1; sel=lvl+1>6?6:lvl+1; } } }
          if(dead>100){ lives--; if(lives<0){ if(score>S.best){ S.best=score; save_game(); } state=4; fr=0; } else { startLevel(lvl); msgT=90; } }
        } }
      float ph=PH(); cam=(int)(px+pw/2)-140; if(cam<0) cam=0; if(cam>LW*TS-VW) cam=LW*TS-VW; (void)ph;
      drawTiles(); for(int i=0;i<128;i++) if(E[i].alive) drawEnt(E[i]); drawPortal(P[0],0); drawPortal(P[1],1); drawPlayer(); hud();
      if(msgT>0){ msgT--; box(90,70,140,24); ctxt(74,LNAME[lvl],255,240,180); snprintf(b,sizeof b,"LIVES %d",lives); ctxt(84,b,255,255,255); }
      if(LV==6&&!bossDown){ for(int i=0;i<128;i++) if(E[i].alive&&E[i].type==4){ box(100,12,120,8); fill(102,14,E[i].hp*19,4,220,60,60); } }
      if(levelDone) { box(80,60,160,30); ctxt(66,LV==6?"GECKO WARLORD DEFEATED!":"DELIVERY COMPLETE!",255,240,120); snprintf(b,sizeof b,"SCORE %d",score); ctxt(78,b,255,255,255); }
      if(state==3){ box(100,60,120,50); ctxt(66,"PAUSED",255,255,255); const char* o[3]={"RESUME","RESTART LEVEL","QUIT TO MAP"};
        for(int i=0;i<3;i++) ctxt(78+i*10,o[i],i==psel?255:150,i==psel?240:150,i==psel?120:150);
        if(up&&psel>0) psel--; if(dp&&psel<2) psel++; if(op||cp) state=2;
        if(jp){ if(psel==0) state=2; else if(psel==1){ startLevel(lvl); state=2; } else { state=1; } } } }
    else if(state==4){ fill(0,0,VW,VH,20,10,20); ctxt(70,"GAME OVER",255,80,80); snprintf(b,sizeof b,"SCORE %d",score); ctxt(84,b,255,255,255); if(fr>60&&jp) state=0; }
    else { fill(0,0,VW,VH,40,30,70); drawCourier(140,50,3); ctxt(110,"ALL PARCELS DELIVERED - THE DESERT IS SAFE!",255,240,150); snprintf(b,sizeof b,"FINAL SCORE %d",score); ctxt(124,b,255,255,255); ctxt(140,"PRESS X",200,200,255); if(jp) state=0; }
    SDL_SetRenderTarget(R,NULL); SDL_Rect dst={0,0,FW,FH}; SDL_RenderCopy(R,low,NULL,&dst); SDL_UpdateWindowSurface(win);
    Uint32 el=SDL_GetTicks()-t0; if(el<16) SDL_Delay(16-el);
  }
out: SDL_Quit(); return 0;
}
