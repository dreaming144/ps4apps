/* Twin Decks - original two-deck DJ player for PS4 (OpenOrbis + SDL2). */
#include <SDL2/SDL.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "dr_mp3.h"
#include "dr_wav.h"
#include "stb_truetype.h"
int stb_vorbis_decode_filename(const char *filename, int *channels, int *sample_rate, short **output);

#define W 1920
#define H 1080
#define OUTSR 48000
#define PI 3.14159265f
#define ASSETS "/app0/assets/"

/* ---------- palette ---------- */
#define C_BG     0x0F1113
#define C_PANEL  0x171A1E
#define C_PANEL2 0x1F2328
#define C_LINE   0x2B3037
#define C_TEXT   0xE6E8EB
#define C_DIM    0x8A919A
#define C_MUTE   0x565D66
#define C_CAP    0xD9DCE0
static const uint32_t ACC[2]={0x3CC3D8,0xEFA23C};

static uint32_t *fb,*bgb,*T;
static inline float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
static inline void px(int x,int y,uint32_t c,float a){
  if((unsigned)x>=W||(unsigned)y>=H||a<=0.004f) return; uint32_t*p=&T[y*W+x];
  if(a>=0.996f){*p=c;return;} uint32_t d=*p; int ia=(int)(a*256);
  int r=((d>>16)&255)+((((c>>16)&255)-(int)((d>>16)&255))*ia>>8);
  int g=((d>>8)&255)+((((c>>8)&255)-(int)((d>>8)&255))*ia>>8);
  int b=(d&255)+(((c&255)-(int)(d&255))*ia>>8); *p=(r<<16)|(g<<8)|b; }
static void rect(int x,int y,int w,int h,uint32_t c,float a){
  if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;} if(x+w>W)w=W-x; if(y+h>H)h=H-y; if(w<=0||h<=0)return;
  for(int j=y;j<y+h;j++){ if(a>=0.996f){uint32_t*p=&T[j*W+x]; for(int i=0;i<w;i++)p[i]=c;} else for(int i=x;i<x+w;i++)px(i,j,c,a);} }
static float sd_rr(float X,float Y,float cx,float cy,float hw,float hh,float r){
  float qx=fabsf(X-cx)-(hw-r),qy=fabsf(Y-cy)-(hh-r); float ox=qx>0?qx:0,oy=qy>0?qy:0;
  float in=fmaxf(qx,qy); if(in>0)in=0; return sqrtf(ox*ox+oy*oy)+in-r; }
/* stroke t<=0 => fill */
static void rrect(float x,float y,float w,float h,float r,uint32_t c,float a,float t){
  float cx=x+w/2,cy=y+h/2,hw=w/2,hh=h/2;
  for(int j=(int)y-1;j<=(int)(y+h)+1;j++){ if(j<0||j>=H)continue;
    for(int i=(int)x-1;i<=(int)(x+w)+1;i++){
      float d=sd_rr(i+0.5f,j+0.5f,cx,cy,hw,hh,r);
      float cv= t>0? clampf(0.5f-(fabsf(d+t*0.5f)-t*0.5f),0,1) : clampf(0.5f-d,0,1);
      if(cv>0)px(i,j,c,cv*a);} } }
static void circle(float cx,float cy,float R,uint32_t c,float a,float t){
  for(int j=(int)(cy-R-2);j<=(int)(cy+R+2);j++) for(int i=(int)(cx-R-2);i<=(int)(cx+R+2);i++){
    float dx=i+0.5f-cx,dy=j+0.5f-cy; float d=sqrtf(dx*dx+dy*dy)-R;
    float cv= t>0? clampf(0.5f-(fabsf(d+t*0.5f)-t*0.5f),0,1) : clampf(0.5f-d,0,1); if(cv>0)px(i,j,c,cv*a);} }
/* arc centred on radius R, thickness t, angles in radians clockwise from 12 o'clock */
static void arc(float cx,float cy,float R,float t,float a0,float a1,uint32_t c,float a){
  if(a1<a0){float s=a0;a0=a1;a1=s;} float ro=R+t/2+1;
  for(int j=(int)(cy-ro);j<=(int)(cy+ro);j++) for(int i=(int)(cx-ro);i<=(int)(cx+ro);i++){
    float dx=i+0.5f-cx,dy=j+0.5f-cy; float dist=sqrtf(dx*dx+dy*dy); float cr=clampf(0.5f-(fabsf(dist-R)-t*0.5f),0,1); if(cr<=0)continue;
    float an=atan2f(dx,-dy); float out=0; if(an<a0)out=(a0-an)*dist; else if(an>a1)out=(an-a1)*dist;
    float ca=clampf(0.5f-out+t*0.5f,0,1); if(out>0){ /* round cap */ float ex=(an<a0?a0:a1); float qx=cx+sinf(ex)*R,qy=cy-cosf(ex)*R; float dd=sqrtf((i+0.5f-qx)*(i+0.5f-qx)+(j+0.5f-qy)*(j+0.5f-qy)); ca=clampf(0.5f-(dd-t*0.5f),0,1); cr=1;}
    if(ca*cr>0)px(i,j,c,ca*cr*a);} }
static void line(float x0,float y0,float x1,float y1,float t,uint32_t c,float a){
  float mnx=fminf(x0,x1)-t-1,mxx=fmaxf(x0,x1)+t+1,mny=fminf(y0,y1)-t-1,mxy=fmaxf(y0,y1)+t+1;
  float vx=x1-x0,vy=y1-y0,ll=vx*vx+vy*vy; if(ll<1e-4f)ll=1e-4f;
  for(int j=(int)mny;j<=(int)mxy;j++) for(int i=(int)mnx;i<=(int)mxx;i++){
    float px_=i+0.5f-x0,py_=j+0.5f-y0; float h=clampf((px_*vx+py_*vy)/ll,0,1); float dx=px_-vx*h,dy=py_-vy*h;
    float cv=clampf(0.5f-(sqrtf(dx*dx+dy*dy)-t*0.5f),0,1); if(cv>0)px(i,j,c,cv*a);} }
static uint32_t mix(uint32_t a,uint32_t b,float t){ t=clampf(t,0,1);
  int r=(int)(((a>>16)&255)*(1-t)+((b>>16)&255)*t),g=(int)(((a>>8)&255)*(1-t)+((b>>8)&255)*t),bb=(int)((a&255)*(1-t)+(b&255)*t); return (r<<16)|(g<<8)|bb; }

/* ---------- fonts (Inter, OFL) ---------- */
typedef struct { unsigned char*bmp; int w,h,xo,yo; float adv; } Glyph;
typedef struct { Glyph g[96]; float tabw; } Font;
enum { F_CAP, F_LBL, F_BODY, F_TITLE, F_MID, F_BIG, F_HERO, NF };
static Font fonts[NF];
static unsigned char* readfile(const char*p,long*n){ FILE*f=fopen(p,"rb"); if(!f)return 0; fseek(f,0,SEEK_END); long s=ftell(f); fseek(f,0,SEEK_SET);
  unsigned char*b=malloc(s+1); if(fread(b,1,s,f)!=(size_t)s){} b[s]=0; fclose(f); if(n)*n=s; return b; }
static void font_bake(Font*F,stbtt_fontinfo*fi,float px_){
  float sc=stbtt_ScaleForPixelHeight(fi,px_*1.30f); F->tabw=0;
  for(int c=32;c<127;c++){ Glyph*g=&F->g[c-32]; int adv,lsb; stbtt_GetCodepointHMetrics(fi,c,&adv,&lsb); g->adv=adv*sc;
    g->bmp=stbtt_GetCodepointBitmap(fi,0,sc,c,&g->w,&g->h,&g->xo,&g->yo); if(c>='0'&&c<='9'&&g->adv>F->tabw)F->tabw=g->adv; } }
static void fonts_init(void){
  static stbtt_fontinfo m,s; unsigned char*mb=readfile(ASSETS"Inter-Medium.ttf",0),*sb=readfile(ASSETS"Inter-SemiBold.ttf",0);
  if(!mb||!sb) return; stbtt_InitFont(&m,mb,0); stbtt_InitFont(&s,sb,0);
  font_bake(&fonts[F_CAP],&s,11); font_bake(&fonts[F_LBL],&m,14); font_bake(&fonts[F_BODY],&m,17);
  font_bake(&fonts[F_TITLE],&s,23); font_bake(&fonts[F_MID],&s,30); font_bake(&fonts[F_BIG],&s,50); font_bake(&fonts[F_HERO],&s,72); }
/* tab=1 -> tabular numerals; sp = extra letter spacing */
static float text_w(int f,const char*s,int tab,float sp){ float w=0; Font*F=&fonts[f];
  for(;*s;s++){ int c=(unsigned char)*s; if(c<32||c>126)c='?'; w+=((tab&&c>='0'&&c<='9')?F->tabw:F->g[c-32].adv)+sp; } return w; }
static float text(int f,float x,float y,const char*s,uint32_t col,float a,int tab,float sp){
  Font*F=&fonts[f];
  for(;*s;s++){ int c=(unsigned char)*s; if(c<32||c>126)c='?'; Glyph*g=&F->g[c-32]; float adv=g->adv,off=0;
    if(tab&&c>='0'&&c<='9'){ off=(F->tabw-adv)/2; adv=F->tabw; }
    if(g->bmp){ int gx=(int)(x+off+0.5f)+g->xo, gy=(int)y+g->yo;
      for(int j=0;j<g->h;j++) for(int i=0;i<g->w;i++){ unsigned char v=g->bmp[j*g->w+i]; if(v)px(gx+i,gy+j,col,a*v/255.0f);} }
    x+=adv+sp; } return x; }
static void text_r(int f,float xr,float y,const char*s,uint32_t c,float a,int tab,float sp){ text(f,xr-text_w(f,s,tab,sp),y,s,c,a,tab,sp); }
static void text_c(int f,float xc,float y,const char*s,uint32_t c,float a,int tab,float sp){ text(f,xc-text_w(f,s,tab,sp)/2,y,s,c,a,tab,sp); }
static void text_fit(int f,float x,float y,float maxw,const char*s,uint32_t c,float a){
  char b[256]; snprintf(b,sizeof b,"%s",s); if(text_w(f,b,0,0)<=maxw){text(f,x,y,b,c,a,0,0);return;}
  int n=strlen(b); while(n>1){ b[--n]=0; char t[260]; snprintf(t,sizeof t,"%s...",b); if(text_w(f,t,0,0)<=maxw){text(f,x,y,t,c,a,0,0);return;} } }

/* controller glyphs, uniform 2px stroke */
enum { G_X, G_O, G_SQ, G_TRI };
static void pad_icon(int k,float cx,float cy,float r,uint32_t c){
  if(k==G_O) circle(cx,cy,r*0.72f,c,1,2);
  else if(k==G_SQ) rrect(cx-r*0.68f,cy-r*0.68f,r*1.36f,r*1.36f,1.5f,c,1,2);
  else if(k==G_X){ float d=r*0.62f; line(cx-d,cy-d,cx+d,cy+d,2,c,1); line(cx-d,cy+d,cx+d,cy-d,2,c,1); }
  else { float d=r*0.8f; line(cx,cy-d,cx+d*0.95f,cy+d*0.65f,2,c,1); line(cx+d*0.95f,cy+d*0.65f,cx-d*0.95f,cy+d*0.65f,2,c,1); line(cx-d*0.95f,cy+d*0.65f,cx,cy-d,2,c,1); } }
static float key_chip(float x,float y,const char*k,const char*label){ /* y = baseline */
  float w=text_w(F_CAP,k,0,1)+16; rrect(x,y-15,w,22,4,C_LINE,1,0); text(F_CAP,x+8,y,k,C_TEXT,1,0,1);
  return text(F_LBL,x+w+8,y,label,C_DIM,1,0,0)+24; }
static float icon_chip(float x,float y,int g,const char*label){
  rrect(x,y-15,22,22,11,C_LINE,1,0); pad_icon(g,x+11,y-4,6.5f,C_TEXT); return text(F_LBL,x+30,y,label,C_DIM,1,0,0)+24; }

/* ---------- DSP ---------- */
typedef struct { float b0,b1,b2,a1,a2,z1[2],z2[2]; } BQ;
enum { BQ_LP, BQ_HP, BQ_PEAK, BQ_LSH, BQ_HSH };
static void bq_set(BQ*q,int type,float f,float Q,float gdb){
  float A=powf(10,gdb/40),w=2*PI*f/OUTSR,cs=cosf(w),sn=sinf(w),al=sn/(2*Q),b0,b1,b2,a0,a1,a2;
  switch(type){
  case BQ_LP: b0=(1-cs)/2;b1=1-cs;b2=b0;a0=1+al;a1=-2*cs;a2=1-al;break;
  case BQ_HP: b0=(1+cs)/2;b1=-(1+cs);b2=b0;a0=1+al;a1=-2*cs;a2=1-al;break;
  case BQ_PEAK: b0=1+al*A;b1=-2*cs;b2=1-al*A;a0=1+al/A;a1=-2*cs;a2=1-al/A;break;
  case BQ_LSH: { float s=2*sqrtf(A)*al; b0=A*((A+1)-(A-1)*cs+s);b1=2*A*((A-1)-(A+1)*cs);b2=A*((A+1)-(A-1)*cs-s);a0=(A+1)+(A-1)*cs+s;a1=-2*((A-1)+(A+1)*cs);a2=(A+1)+(A-1)*cs-s;break; }
  default: { float s=2*sqrtf(A)*al; b0=A*((A+1)+(A-1)*cs+s);b1=-2*A*((A-1)+(A+1)*cs);b2=A*((A+1)+(A-1)*cs-s);a0=(A+1)-(A-1)*cs+s;a1=2*((A-1)-(A+1)*cs);a2=(A+1)-(A-1)*cs-s;break; } }
  q->b0=b0/a0;q->b1=b1/a0;q->b2=b2/a0;q->a1=a1/a0;q->a2=a2/a0; }
static inline float bq(BQ*q,int ch,float x){ float y=q->b0*x+q->z1[ch]; q->z1[ch]=q->b1*x-q->a1*y+q->z2[ch]; q->z2[ch]=q->b2*x-q->a2*y; return y; }

#define ECHO_N 96000
#define PS_N 4096
#define FL_N 2048
static const int CombL[4]={1690,1760,1620,1545}, ApL[2]={605,480};
#define NSTEM 4
enum { STEM_VOC, STEM_DRM, STEM_BAS, STEM_OTH };
static const char*STEM_SUF[NSTEM]={"_vocals","_drums","_bass","_other"};
static const char*STEM_LBL[NSTEM]={"VOC","DRM","BAS","OTH"};
typedef struct {
  /* track */
  int16_t*pcm; long n; int rate; char title[160],artist[120]; float bpm; double first; /* first beat, seconds */
  int16_t*stpcm[NSTEM]; long stn[NSTEM]; int sthave[NSTEM]; float stem[NSTEM];
  uint8_t *wpk,*wlo,*wmi,*whi; int nb; uint8_t ov[3][656];
  /* transport */
  volatile double pos; volatile int playing, preview; double cue; int range; float tempo; int keylock,sync,vinyl;
  double hc[8]; int hcset[8]; int loopOn,linSet; double lin,lout; volatile int scratch; volatile float svel,bend; float rot;
  /* mixer */
  float trim,hi,mid,lo,filt,fxamt,fader; int fxtype;
  /* dsp state */
  float c_trim,c_hi,c_mid,c_lo,c_filt; BQ eq[3],flt,sw; int fltOn;
  float ps[2][PS_N]; int psw; float psd;
  float echo[2][ECHO_N]; int ew; float comb[4][2000]; int ci[4]; float cst[4]; float ap[2][2][700]; int ai[2];
  float fl[2][FL_N]; int fw; double lfo; float vu,vuh;
} Deck;
static Deck D[2];
static float xf=0, master=0.8f; static int curve=0; static float mvu=0;
static SDL_AudioDeviceID adev;
static const float RANGES[3]={0.08f,0.16f,0.50f};
static float tempo_mul(Deck*k){ return 1+k->tempo*RANGES[k->range]; }
static float eff_bpm(Deck*k){ return k->bpm*tempo_mul(k)*(1+k->bend); }
static double spb_frames(Deck*k){ return k->rate*60.0/k->bpm; }
static double beatpos(Deck*k){ return (k->pos/k->rate-k->first)*k->bpm/60.0; }
static double beat_to_pos(Deck*k,double b){ return (b*60.0/k->bpm+k->first)*k->rate; }

static void deck_coefs(Deck*k){
  if(k->c_hi!=k->hi){ k->c_hi=k->hi; bq_set(&k->eq[2],BQ_HSH,4000,0.7f,k->hi<0?k->hi*26:k->hi*6); }
  if(k->c_mid!=k->mid){ k->c_mid=k->mid; bq_set(&k->eq[1],BQ_PEAK,1000,0.7f,k->mid<0?k->mid*26:k->mid*6); }
  if(k->c_lo!=k->lo){ k->c_lo=k->lo; bq_set(&k->eq[0],BQ_LSH,250,0.7f,k->lo<0?k->lo*26:k->lo*6); }
  if(k->c_filt!=k->filt){ k->c_filt=k->filt; float v=k->filt; k->fltOn=fabsf(v)>0.03f;
    if(v<0) bq_set(&k->flt,BQ_LP,20000*powf(0.008f,-v),1.0f,0); else if(v>0) bq_set(&k->flt,BQ_HP,20*powf(400,v),1.0f,0); } }
static inline float rd(float*b,int N,float idx){ while(idx<0)idx+=N; int i=(int)idx; float f=idx-i; return b[i%N]*(1-f)+b[(i+1)%N]*f; }
static int16_t*decode(const char*path,long*frames,int*rate);

static int is_stem_sidecar(const char*name){
  const char*dot=strrchr(name,'.'); if(!dot)return 0; char base[256]; size_t n=(size_t)(dot-name); if(n>=sizeof base)n=sizeof base-1;
  memcpy(base,name,n); base[n]=0;
  for(int i=0;i<NSTEM;i++){ size_t sl=strlen(STEM_SUF[i]); if(n>sl&&!strcasecmp(base+n-sl,STEM_SUF[i]))return 1; }
  if(n>13&&!strcasecmp(base+n-13,"_instrumental"))return 1;
  return 0;
}
static void stem_clear(Deck*k){ for(int i=0;i<NSTEM;i++){ free(k->stpcm[i]); k->stpcm[i]=0; k->stn[i]=0; k->sthave[i]=0; k->stem[i]=0; } }
static void stem_path(char*out,size_t sz,const char*mainpath,const char*suf,const char*ext){
  char base[512]; snprintf(base,sizeof base,"%s",mainpath); char*dot=strrchr(base,'.'); if(dot)*dot=0; snprintf(out,sz,"%s%s%s",base,suf,ext); }
static int stem_load_one(Deck*k,int si,const char*mainpath){
  static const char*exts[]={".ogg",".wav",".mp3",".OGG",".WAV",".MP3"};
  for(unsigned e=0;e<sizeof exts/sizeof*exts;e++){ char p[512]; stem_path(p,sizeof p,mainpath,STEM_SUF[si],exts[e]);
    long fr; int sr; int16_t*pcm=decode(p,&fr,&sr); if(!pcm)continue;
    if(sr!=k->rate){ free(pcm); continue; }
    k->stpcm[si]=pcm; k->stn[si]=fr; k->sthave[si]=1; k->stem[si]=1.0f; return 1; }
  return 0;
}
static void stem_load_all(Deck*k,const char*mainpath){ stem_clear(k); if(!k->pcm)return; int n=0;
  for(int i=0;i<NSTEM;i++) if(stem_load_one(k,i,mainpath)) n++; (void)n; }
static void stem_add_at(Deck*k,double pos,float*l,float*r){
  long p=(long)pos; if(p<0)return; for(int i=0;i<NSTEM;i++){ if(!k->sthave[i]||k->stem[i]<=0.001f)continue;
    if(p>=k->stn[i]-1)continue; float f=(float)(pos-p); int16_t*s=&k->stpcm[i][p*2]; float g=k->stem[i];
    *l+=((s[0]+(s[2]-s[0])*f)*(1/32768.0f))*g; *r+=((s[1]+(s[3]-s[1])*f)*(1/32768.0f))*g; } }

static void deck_sample(Deck*k,float*L,float*R){
  float l=0,r=0;
  if(k->pcm&&(k->playing||k->scratch||k->preview)){
    double rate=k->scratch? k->svel : tempo_mul(k)*(1+k->bend); double step=rate*k->rate/(double)OUTSR;
    long p=(long)k->pos; if(p>=0&&p<k->n-1){ float f=(float)(k->pos-p); int16_t*s=&k->pcm[p*2];
      l=(s[0]+(s[2]-s[0])*f)*(1/32768.0f); r=(s[1]+(s[3]-s[1])*f)*(1/32768.0f); }
    stem_add_at(k,k->pos,&l,&r);
    k->pos+=step;
    if(k->loopOn&&step>0&&k->pos>=k->lout&&k->lout>k->lin) k->pos-=(k->lout-k->lin);
    if(k->pos>=k->n-1){ k->pos=k->n-1; k->playing=0; k->preview=0; } if(k->pos<0)k->pos=0;
    if(k->keylock&&!k->scratch&&fabs(rate-1)>0.0005){ /* master tempo: dual-tap pitch shifter */
      float fct=(float)(1.0/rate),Wn=1800; k->ps[0][k->psw]=l; k->ps[1][k->psw]=r; k->psd+=1-fct; if(k->psd>=Wn)k->psd-=Wn; if(k->psd<0)k->psd+=Wn;
      float d1=k->psd,d2=fmodf(d1+Wn/2,Wn),g1=1-fabsf(2*d1/Wn-1),g2=1-fabsf(2*d2/Wn-1);
      l=g1*rd(k->ps[0],PS_N,k->psw-d1-1)+g2*rd(k->ps[0],PS_N,k->psw-d2-1); r=g1*rd(k->ps[1],PS_N,k->psw-d1-1)+g2*rd(k->ps[1],PS_N,k->psw-d2-1);
      k->psw=(k->psw+1)&(PS_N-1); }
  }
  float g=powf(10,k->trim*12/20); l*=g; r*=g;
  for(int b=0;b<3;b++){ l=bq(&k->eq[b],0,l); r=bq(&k->eq[b],1,r); }
  if(k->fltOn){ l=bq(&k->flt,0,l); r=bq(&k->flt,1,r); }
  float a=k->fxamt; double bl=k->bpm>0? 60.0/eff_bpm(k):0.5;
  if(k->fxtype==1){ int dl=(int)fmin(0.75*bl*OUTSR,ECHO_N-1); int ri=(k->ew-dl+ECHO_N)%ECHO_N;
    float el=k->echo[0][ri],er=k->echo[1][ri]; k->echo[0][k->ew]=l+el*0.45f; k->echo[1][k->ew]=r+er*0.45f; k->ew=(k->ew+1)%ECHO_N; l+=el*a; r+=er*a; }
  else if(k->fxtype==2){ float in=(l+r)*0.5f,s=0; for(int c=0;c<4;c++){ float*cb=k->comb[c]; float o=cb[k->ci[c]]; k->cst[c]=o*0.75f+k->cst[c]*0.25f; cb[k->ci[c]]=in+k->cst[c]*0.82f; k->ci[c]=(k->ci[c]+1)%CombL[c]; s+=o; }
    float o2[2]; for(int ch=0;ch<2;ch++){ float v=s*0.25f; for(int q=0;q<2;q++){ int len=ApL[q]+ch*23; float*ab=k->ap[q][ch]; int ii=k->ai[q]%len; float bo=ab[ii]; ab[ii]=v+bo*0.5f; v=bo-v*0.5f; } o2[ch]=v; }
    k->ai[0]++; k->ai[1]++; if(k->ai[0]>100000){k->ai[0]=k->ai[1]=0;} l+=o2[0]*a*1.4f; r+=o2[1]*a*1.4f; }
  else if(k->fxtype==3){ k->lfo+=0.25/OUTSR; float d=48+200*(0.5f+0.5f*sinf((float)(k->lfo*2*PI)));
    float fl_=rd(k->fl[0],FL_N,k->fw-d),fr=rd(k->fl[1],FL_N,k->fw-d); k->fl[0][k->fw]=l+fl_*0.55f; k->fl[1][k->fw]=r+fr*0.55f; k->fw=(k->fw+1)&(FL_N-1); l=l*(1-a*0.5f)+fl_*a*0.7f; r=r*(1-a*0.5f)+fr*a*0.7f; }
  else if(k->fxtype==4){ k->lfo+=1.0/(bl*8*OUTSR); if(((int)(k->lfo*OUTSR*8))%64==0){} 
    static int cnt=0; if((cnt++&63)==0) bq_set(&k->sw,BQ_LP,150*powf(100,0.5f+0.5f*sinf((float)(k->lfo*2*PI))),2.2f,0);
    float sl=bq(&k->sw,0,l),sr=bq(&k->sw,1,r); l+=(sl-l)*a; r+=(sr-r)*a; }
  float fd=k->fader*k->fader; l*=fd; r*=fd;
  float pk=fmaxf(fabsf(l),fabsf(r)); if(pk>k->vu)k->vu=pk;
  *L=l; *R=r; }
static void audio_cb(void*u,Uint8*st,int len){ (void)u; int16_t*o=(int16_t*)st; int n=len/4;
  deck_coefs(&D[0]); deck_coefs(&D[1]);
  float t=(xf+1)/2,ga,gb; if(curve==0){ga=cosf(t*PI/2);gb=sinf(t*PI/2);} else if(curve==1){ga=1-t;gb=t;} else { ga=t<0.94f?1:(1-t)/0.06f; gb=t>0.06f?1:t/0.06f; }
  for(int i=0;i<n;i++){ float l0,r0,l1,r1; deck_sample(&D[0],&l0,&r0); deck_sample(&D[1],&l1,&r1);
    float L=(l0*ga+l1*gb)*master,R=(r0*ga+r1*gb)*master; float pk=fmaxf(fabsf(L),fabsf(R)); if(pk>mvu)mvu=pk;
    L=clampf(L,-3,3); R=clampf(R,-3,3); L=L*(27+L*L)/(27+9*L*L); R=R*(27+R*R)/(27+9*R*R);
    o[i*2]=(int16_t)(clampf(L,-1,1)*32000); o[i*2+1]=(int16_t)(clampf(R,-1,1)*32000); } }

/* ---------- library ---------- */
typedef struct { char path[512],title[160],artist[120],lic[48]; float bpm; int src; } Track; /* src 0 bundled 1 internal 2 usb */
#define MAXT 3000
static Track lib[MAXT]; static int nlib=0, order[MAXT], sortmode=0;
static const char* SRCN[3]={"Bundled","Internal","USB"};
static const char* SORTN[4]={"Title","Artist","BPM","Source"};
static int is_audio(const char*n){ const char*e=strrchr(n,'.'); if(!e)return 0; char x[8]={0}; for(int i=0;i<6&&e[i];i++)x[i]=tolower((unsigned char)e[i]);
  return !strcmp(x,".mp3")||!strcmp(x,".wav")||!strcmp(x,".ogg"); }
static void add_user(const char*path,int src){ if(nlib>=MAXT)return; Track*t=&lib[nlib]; memset(t,0,sizeof *t); snprintf(t->path,sizeof t->path,"%s",path);
  const char*b=strrchr(path,'/'); b=b?b+1:path; char nm[200]; snprintf(nm,sizeof nm,"%s",b); char*d=strrchr(nm,'.'); if(d)*d=0;
  for(char*c=nm;*c;c++) if(*c=='_')*c=' ';
  char*sep=strstr(nm," - "); if(sep){ *sep=0; snprintf(t->artist,sizeof t->artist,"%s",nm); snprintf(t->title,sizeof t->title,"%s",sep+3); }
  else { snprintf(t->title,sizeof t->title,"%s",nm); snprintf(t->artist,sizeof t->artist,"Unknown artist"); }
  snprintf(t->lic,sizeof t->lic,"Your file"); t->src=src; nlib++; }
static void scan(const char*dir,int src,int depth){ if(depth>6)return; DIR*d=opendir(dir); if(!d)return; struct dirent*e;
  while((e=readdir(d))){ if(e->d_name[0]=='.')continue; char p[512]; snprintf(p,sizeof p,"%s/%s",dir,e->d_name); struct stat s;
    if(stat(p,&s)!=0)continue; if(S_ISDIR(s.st_mode)) scan(p,src,depth+1); else if(is_audio(e->d_name)&&!is_stem_sidecar(e->d_name)) add_user(p,src); } closedir(d); }
static int cmp_t(const void*a,const void*b){ Track*x=&lib[*(int*)a],*y=&lib[*(int*)b]; int r=0;
  if(sortmode==1) r=strcasecmp(x->artist,y->artist); else if(sortmode==2){ r=(x->bpm>y->bpm)-(x->bpm<y->bpm); } else if(sortmode==3) r=x->src-y->src;
  return r?r:strcasecmp(x->title,y->title); }
static void lib_sort(void){ for(int i=0;i<nlib;i++)order[i]=i; qsort(order,nlib,sizeof(int),cmp_t); }
static void lib_scan(void){ nlib=0;
  long n; char*s=(char*)readfile(ASSETS"audio/tracks.txt",&n);
  if(s){ char*ln=strtok(s,"\n"); while(ln&&nlib<MAXT){ if(ln[0]!='#'){ Track*t=&lib[nlib]; memset(t,0,sizeof *t); char*f[6]={0}; int k=0; char*p=ln; f[k++]=p;
        while(*p&&k<6){ if(*p=='|'){*p=0;f[k++]=p+1;} p++; }
        if(k>=5){ snprintf(t->path,sizeof t->path,ASSETS"audio/%s",f[0]); snprintf(t->title,sizeof t->title,"%s",f[1]); snprintf(t->artist,sizeof t->artist,"%s",f[2]);
          t->bpm=atof(f[3]); snprintf(t->lic,sizeof t->lic,"%s",f[4]); t->src=0; nlib++; } } ln=strtok(NULL,"\n"); } free(s); }
  mkdir("/data/djdecks",0777); mkdir("/data/djdecks/music",0777); scan("/data/djdecks/music",1,0);
  static const char*sub[]={"music","Music","MUSIC","djdecks","DJDecks","DJDECKS","TwinDecks"};
  for(int u=0;u<8;u++){ char root[32]; snprintf(root,sizeof root,"/mnt/usb%d",u); DIR*d=opendir(root); if(!d)continue;
    struct dirent*e; while((e=readdir(d))){ char p[512]; snprintf(p,sizeof p,"%s/%s",root,e->d_name); if(is_audio(e->d_name)&&!is_stem_sidecar(e->d_name)) add_user(p,2); } closedir(d);
    for(unsigned q=0;q<sizeof sub/sizeof*sub;q++){ char p[64]; snprintf(p,sizeof p,"%s/%s",root,sub[q]); scan(p,2,0); } }
  lib_sort(); }

/* ---------- decode + analysis ---------- */
static int16_t* decode(const char*path,long*frames,int*rate){
  const char*e=strrchr(path,'.'); char x[8]={0}; for(int i=0;e&&i<6&&e[i];i++)x[i]=tolower((unsigned char)e[i]);
  int16_t*raw=0; long fr=0; int ch=0,sr=0;
  if(!strcmp(x,".mp3")){ drmp3_config c; drmp3_uint64 f; int16_t*p=drmp3_open_file_and_read_pcm_frames_s16(path,&c,&f,NULL); if(!p)return 0;
    fr=(long)f; ch=c.channels; sr=c.sampleRate; raw=malloc(fr*ch*2); memcpy(raw,p,fr*ch*2); drmp3_free(p,NULL); }
  else if(!strcmp(x,".wav")){ unsigned c,s; drwav_uint64 f; int16_t*p=drwav_open_file_and_read_pcm_frames_s16(path,&c,&s,&f,NULL); if(!p)return 0;
    fr=(long)f; ch=c; sr=s; raw=malloc(fr*ch*2); memcpy(raw,p,fr*ch*2); drwav_free(p,NULL); }
  else { short*p; int n=stb_vorbis_decode_filename(path,&ch,&sr,&p); if(n<=0)return 0; fr=n; raw=(int16_t*)p; }
  if(ch<1||fr<1000){ free(raw); return 0; }
  if(ch!=2){ int16_t*st=malloc(fr*4); for(long i=0;i<fr;i++){ st[i*2]=raw[i*ch]; st[i*2+1]=raw[i*ch+(ch>1?1:0)]; } free(raw); raw=st; }
  *frames=fr; *rate=sr; return raw; }
/* 100 buckets/s: peak + low/mid/high energy; optional tempo + first beat estimate */
static void analyse(Deck*k,float known){
  int bs=k->rate/100; k->nb=(int)(k->n/bs); free(k->wpk);free(k->wlo);free(k->wmi);free(k->whi);
  k->wpk=calloc(k->nb+1,1);k->wlo=calloc(k->nb+1,1);k->wmi=calloc(k->nb+1,1);k->whi=calloc(k->nb+1,1);
  float*env=calloc(k->nb+1,sizeof(float)); float lp=0,lp2=0,al=1-expf(-2*PI*220/k->rate),ah=1-expf(-2*PI*2200/k->rate);
  float mx[4]={1e-6f,1e-6f,1e-6f,1e-6f}; float*tmp=malloc(sizeof(float)*4*(k->nb+1));
  for(int b=0;b<k->nb;b++){ float pk=0,el=0,em=0,eh=0; for(int i=0;i<bs;i++){ long s=(long)b*bs+i; float x=(k->pcm[s*2]+k->pcm[s*2+1])*(0.5f/32768);
      lp+=al*(x-lp); lp2+=ah*(x-lp2); float hi=x-lp2,mi=lp2-lp; float ax=fabsf(x); if(ax>pk)pk=ax; el+=lp*lp; em+=mi*mi; eh+=hi*hi; }
    tmp[b*4]=pk; tmp[b*4+1]=sqrtf(el/bs); tmp[b*4+2]=sqrtf(em/bs); tmp[b*4+3]=sqrtf(eh/bs); for(int q=0;q<4;q++) if(tmp[b*4+q]>mx[q])mx[q]=tmp[b*4+q]; }
  float prev=0; for(int b=0;b<k->nb;b++){ k->wpk[b]=(uint8_t)(255*tmp[b*4]/mx[0]); k->wlo[b]=(uint8_t)(255*tmp[b*4+1]/mx[1]); k->wmi[b]=(uint8_t)(255*tmp[b*4+2]/mx[2]); k->whi[b]=(uint8_t)(255*tmp[b*4+3]/mx[3]);
    float e=tmp[b*4+1]*2+tmp[b*4+2]; float d=e-prev; env[b]=d>0?d:0; prev=e; }
  for(int c=0;c<656;c++){ int b0=(int)((long)c*k->nb/656),b1=(int)((long)(c+1)*k->nb/656); uint8_t a=0,l=0,h=0; for(int b=b0;b<b1&&b<k->nb;b++){ if(k->wpk[b]>a)a=k->wpk[b]; if(k->wlo[b]>l)l=k->wlo[b]; if(k->whi[b]>h)h=k->whi[b]; } k->ov[0][c]=a; k->ov[1][c]=l; k->ov[2][c]=h; }
  free(tmp);
  int lim=k->nb<9000?k->nb:9000; /* first 90 s */
  if(known>0) k->bpm=known; else {
    float best=0,bb=120; for(float bp=70;bp<=180;bp+=0.05f){ float lag=6000/bp,s=0; for(int i=0;i+2*lag+1<lim;i+=1){ float j=i+lag; int ji=(int)j; float f=j-ji; s+=env[i]*(env[ji]*(1-f)+env[ji+1]*f); } if(s>best){best=s;bb=bp;} }
    if(bb<88) bb*=2; else if(bb>178) bb/=2; k->bpm=roundf(bb*10)/10; }
  float lag=6000/k->bpm,bs2=-1; double ph=0; for(float o=0;o<lag;o+=0.25f){ float s=0; for(float t=o;t<lim-1;t+=lag){ int ti=(int)t; s+=env[ti]; } if(s>bs2){bs2=s;ph=o;} }
  k->first=(known>0)?0:ph/100.0; if(known>0){ /* bundled tracks start on the beat */ }
  free(env); }
static char toast[160]; static Uint32 toastT=0;
static void load_track(int d,int ti);

/* ---------- transport ---------- */
static void present(void);
static void draw_frame(void);
static void set_toast(const char*s){ snprintf(toast,sizeof toast,"%s",s); toastT=SDL_GetTicks()+2600; }
static void load_track(int d,int ti){
  Deck*k=&D[d]; Track*t=&lib[ti];
  if(k->playing){ set_toast("Pause the deck before loading a new track"); return; }
  char m[200]; snprintf(m,sizeof m,"Loading %s ...",t->title); set_toast(m); draw_frame(); present();
  long fr; int sr; int16_t*p=decode(t->path,&fr,&sr); if(!p){ set_toast("Could not decode that file"); return; }
  SDL_LockAudioDevice(adev); int16_t*old=k->pcm; k->pcm=0; k->playing=0; k->preview=0; stem_clear(k); SDL_UnlockAudioDevice(adev); free(old);
  k->n=fr; k->rate=sr; k->pcm=p; /* analyse while deck still silent */
  Deck tmp; (void)tmp; k->playing=0;
  int16_t*keep=k->pcm; SDL_LockAudioDevice(adev); k->pcm=0; SDL_UnlockAudioDevice(adev); k->pcm=keep;
  analyse(k,t->bpm); if(t->bpm<=0) t->bpm=k->bpm;
  snprintf(k->title,sizeof k->title,"%s",t->title); snprintf(k->artist,sizeof k->artist,"%s",t->artist);
  stem_load_all(k,t->path);
  SDL_LockAudioDevice(adev); k->pos=k->first*k->rate; k->cue=k->pos; k->loopOn=0; k->linSet=0; memset(k->hcset,0,sizeof k->hcset); k->sync=0; SDL_UnlockAudioDevice(adev);
  { int sc=0; for(int i=0;i<NSTEM;i++) if(k->sthave[i]) sc++;
    if(sc) snprintf(m,sizeof m,"Deck %c  %s  %.1f BPM  %d stems",'A'+d,t->title,k->bpm,sc);
    else snprintf(m,sizeof m,"Deck %c  %s  %.1f BPM",'A'+d,t->title,k->bpm);
    set_toast(m); } }
static void stem_vocal_off(Deck*k){ if(k->sthave[STEM_VOC]) k->stem[STEM_VOC]=0;
  else { k->mid=-0.85f; k->hi=-0.35f; deck_coefs(k); } }
static void stem_instrumental(Deck*k){ if(k->sthave[STEM_VOC]) k->stem[STEM_VOC]=0;
  else { k->mid=-0.9f; k->hi=-0.4f; k->lo=0.15f; deck_coefs(k); } }
static void phase_align(int d){ Deck*k=&D[d],*o=&D[1-d]; if(!k->pcm||!o->pcm||k->bpm<=0||o->bpm<=0)return;
  double a=beatpos(o),b=beatpos(k); double df=(a-floor(a))-(b-floor(b)); if(df>0.5)df-=1; if(df<-0.5)df+=1;
  SDL_LockAudioDevice(adev); k->pos+=df*spb_frames(k); if(k->pos<0)k->pos=0; SDL_UnlockAudioDevice(adev); }
static void match_tempo(int d){ Deck*k=&D[d],*o=&D[1-d]; if(k->bpm<=0||o->bpm<=0)return;
  float mul=eff_bpm(o)/(1+o->bend)/k->bpm; float v=(mul-1)/RANGES[k->range]; while(fabsf(v)>1&&k->range<2){ k->range++; v=(mul-1)/RANGES[k->range]; } k->tempo=clampf(v,-1,1); }
static void do_sync(int d){ Deck*k=&D[d],*o=&D[1-d]; if(k->sync){ k->sync=0; return; }
  if(!k->pcm||!o->pcm){ set_toast("Load a track on both decks to sync"); return; }
  k->sync=1; o->sync=0; match_tempo(d); phase_align(d); }
static void play_toggle(int d){ Deck*k=&D[d]; if(!k->pcm)return;
  if(k->preview){ k->preview=0; k->playing=1; return; }
  k->playing=!k->playing; if(k->playing&&k->sync) phase_align(d); }
static void cue_down(int d){ Deck*k=&D[d]; if(!k->pcm)return;
  SDL_LockAudioDevice(adev);
  if(k->playing){ k->playing=0; k->pos=k->cue; }
  else if(fabs(k->pos-k->cue)<k->rate*0.02) k->preview=1;
  else { double q=k->pos; if(k->bpm>0){ double b=floor(beatpos(k)+0.5); double qp=beat_to_pos(k,b); if(fabs(qp-q)<k->rate*0.12)q=qp; } k->cue=q; k->pos=q; }
  SDL_UnlockAudioDevice(adev); }
static void cue_up(int d){ Deck*k=&D[d]; if(k->preview){ SDL_LockAudioDevice(adev); k->preview=0; k->pos=k->cue; SDL_UnlockAudioDevice(adev);} }
static void hotcue(int d,int i,int clear){ Deck*k=&D[d]; if(!k->pcm)return; if(clear){ k->hcset[i]=0; return; }
  if(k->hcset[i]){ SDL_LockAudioDevice(adev); k->pos=k->hc[i]; SDL_UnlockAudioDevice(adev); if(!k->playing){ k->cue=k->hc[i]; } }
  else { double q=k->pos; if(k->bpm>0){ double qp=beat_to_pos(k,floor(beatpos(k)+0.5)); if(fabs(qp-q)<k->rate*0.08)q=qp; } k->hc[i]=q; k->hcset[i]=1; } }
static double qbeat(Deck*k,double p){ if(k->bpm<=0)return p; k->pos=k->pos; double b=(p/k->rate-k->first)*k->bpm/60.0; return beat_to_pos(k,floor(b+0.5)); }
enum { L_IN, L_OUT, L_EXIT, L_HALF, L_DBL, L_4, L_8, L_16 };
static void loop_act(int d,int a){ Deck*k=&D[d]; if(!k->pcm)return; SDL_LockAudioDevice(adev);
  double len=k->lout-k->lin;
  switch(a){
  case L_IN: k->lin=qbeat(k,k->pos); k->linSet=1; k->loopOn=0; break;
  case L_OUT: if(k->linSet){ double q=qbeat(k,k->pos); if(q<=k->lin) q=k->pos; if(q>k->lin+64){ k->lout=q; k->loopOn=1; } } break;
  case L_EXIT: if(k->loopOn) k->loopOn=0; else if(len>0){ k->loopOn=1; if(k->pos>k->lout||k->pos<k->lin) k->pos=k->lin; } break;
  case L_HALF: case L_DBL: if(len>0){ len*= a==L_HALF?0.5:2; if(len>64){ k->lout=k->lin+len; if(k->loopOn&&k->pos>=k->lout) k->pos=k->lin+fmod(k->pos-k->lin,len);} } break;
  default: { if(k->bpm<=0)break; int nb=a==L_4?4:a==L_8?8:16; double sp=spb_frames(k);
    if(k->loopOn&&fabs(len-nb*sp)<2){ k->loopOn=0; break; }
    k->lin=beat_to_pos(k,floor(beatpos(k)+1e-6)); k->lout=k->lin+nb*sp; k->linSet=1; k->loopOn=1; } }
  SDL_UnlockAudioDevice(adev); }

/* ---------- controls & focus ---------- */
enum { K_BTN, K_KNOB, K_VFAD, K_HFAD };
enum { A_PLAY, A_CUE, A_SYNC, A_KEY, A_RANGE, A_VINYL, A_HC, A_LOOP, A_FXT, A_CURVE, A_VAL, A_VOCUT, A_INST };
typedef struct { float x,y,w,h; int kind,deck,act,sub; float*v,lo,hi,def; const char*label; int bip; } Ctl;
static Ctl ctl[96]; static int nctl=0, focus=0;
static int addc(float x,float y,float w,float h,int kind,int deck,int act,int sub,float*v,float lo,float hi,float def,const char*lbl,int bip){
  Ctl*c=&ctl[nctl]; c->x=x;c->y=y;c->w=w;c->h=h;c->kind=kind;c->deck=deck;c->act=act;c->sub=sub;c->v=v;c->lo=lo;c->hi=hi;c->def=def;c->label=lbl;c->bip=bip; return nctl++; }
#define PY 320
static const int DPX[2]={32,1192};
#define MX 744
static const float STRIPX[2]={852,1068};
static const char*LOOPN[8]={"IN","OUT","EXIT","1/2","X2","4","8","16"};
static const char*HCN[8]={"1","2","3","4","5","6","7","8"};
static void build_ctl(void){ nctl=0;
  for(int d=0;d<2;d++){ Deck*k=&D[d]; int px_=DPX[d];
    addc(px_+336,PY+224,216,48,K_BTN,d,A_SYNC,0,0,0,0,0,"SYNC",0);
    addc(px_+336,PY+280,216,48,K_BTN,d,A_KEY,0,0,0,0,0,"MASTER TEMPO",0);
    addc(px_+336,PY+336,216,48,K_BTN,d,A_RANGE,0,0,0,0,0,"RANGE",0);
    addc(px_+336,PY+392,216,48,K_BTN,d,A_VINYL,0,0,0,0,0,"VINYL",0);
    addc(px_+592,PY+224,80,272,K_VFAD,d,A_VAL,1,&k->tempo,-1,1,0,"TEMPO",1);
    addc(px_+24,PY+512,152,56,K_BTN,d,A_CUE,0,0,0,0,0,"CUE",0);
    addc(px_+184,PY+512,152,56,K_BTN,d,A_PLAY,0,0,0,0,0,"PLAY",0);
    for(int i=0;i<8;i++) addc(px_+24+i*82,PY+584,74,56,K_BTN,d,A_HC,i,0,0,0,0,HCN[i],0);
    for(int i=0;i<8;i++) addc(px_+24+i*82,PY+656,74,48,K_BTN,d,A_LOOP,i,0,0,0,0,LOOPN[i],0);
    float sx=STRIPX[d];
    addc(sx-26,PY+64-26,52,52,K_KNOB,d,A_VAL,0,&k->trim,-1,1,0,"TRIM",1);
    addc(sx-26,PY+128-26,52,52,K_KNOB,d,A_VAL,0,&k->hi,-1,1,0,"HI",1);
    addc(sx-26,PY+192-26,52,52,K_KNOB,d,A_VAL,0,&k->mid,-1,1,0,"MID",1);
    addc(sx-26,PY+256-26,52,52,K_KNOB,d,A_VAL,0,&k->lo,-1,1,0,"LOW",1);
    addc(sx-26,PY+320-26,52,52,K_KNOB,d,A_VAL,0,&k->filt,-1,1,0,"FILTER",1);
    for(int i=0;i<NSTEM;i++) addc(px_+24+i*82,PY+520,74,40,K_VFAD,d,A_VAL,10+i,&k->stem[i],0,1,0,STEM_LBL[i],0);
    addc(sx-72,PY+368,68,40,K_BTN,d,A_FXT,0,0,0,0,0,"FX",0);
    addc(sx+4,PY+368,68,40,K_BTN,d,A_VOCUT,0,0,0,0,0,"VOC CUT",0);
    addc(sx+76,PY+368,68,40,K_BTN,d,A_INST,0,0,0,0,0,"INST",0);
    addc(sx-26,PY+448-26,52,52,K_KNOB,d,A_VAL,0,&k->fxamt,0,1,0,"FX AMT",0);
    addc(sx-24,PY+496,48,136,K_VFAD,d,A_VAL,0,&k->fader,0,1,1,"",0); }
  addc(800,PY+648,320,40,K_HFAD,-1,A_VAL,0,&xf,-1,1,0,"",1);
  addc(900,PY+696,120,28,K_BTN,-1,A_CURVE,0,0,0,0,0,"CURVE",0);
  addc(1816,8,40,40,K_KNOB,-1,A_VAL,0,&master,0,1,0.8f,"",0);
  for(int i=0;i<nctl;i++) if(ctl[i].act==A_PLAY&&ctl[i].deck==0) focus=i; }
static void move_focus(int dx,int dy){ Ctl*c=&ctl[focus]; float cx=c->x+c->w/2,cy=c->y+c->h/2; int best=-1; float bs=1e9;
  for(int i=0;i<nctl;i++){ if(i==focus)continue; float ox=ctl[i].x+ctl[i].w/2-cx,oy=ctl[i].y+ctl[i].h/2-cy; float along=ox*dx+oy*dy; if(along<4)continue;
    float perp=fabsf(dx?oy:ox); float s=along+perp*2.5f; if(s<bs){bs=s;best=i;} } if(best>=0)focus=best; }

/* ---------- drawing ---------- */
static void panel(float x,float y,float w,float h){ for(int i=3;i>=1;i--) rrect(x-i+2,y-i+4,w+2*i-4,h+2*i-4,10+i,0x000000,0.10f,0); rrect(x,y,w,h,10,C_PANEL,1,0); rrect(x,y,w,h,10,0x262A30,1,1); }
static void caplabel(float x,float y,const char*s,uint32_t c){ text(F_CAP,x,y,s,c,1,0,1.2f); }
static void logo_mark(float cx,float cy,float r){ circle(cx-r*0.55f,cy,r,ACC[0],1,r*0.22f); circle(cx+r*0.55f,cy,r,ACC[1],1,r*0.22f); }
static void build_bg(void){ T=bgb; rect(0,0,W,H,C_BG,1);
  rect(0,0,W,56,0x131619,1); rect(0,56,W,1,0x22262B,1);
  logo_mark(52,28,10); text(F_TITLE,80,37,"Twin Decks",C_TEXT,1,0,0.2f);
  for(int d=0;d<2;d++){ float y=64+d*120; panel(32,y,1856,112); }
  for(int d=0;d<2;d++){ int x=DPX[d]; panel(x,PY,696,728);
    /* jog base */ float cx=x+160,cy=PY+360; circle(cx,cy,136,0x121417,1,0); circle(cx,cy,136,0x2C3138,1,2);
    for(int r=0;r<5;r++) circle(cx,cy,118-r*14,0x1A1D21,1,1);
    circle(cx,cy,46,C_PANEL2,1,0); circle(cx,cy,46,0x30353C,1,1.5f);
    caplabel(x+592,PY+216-8+0,"",C_DIM); }
  panel(MX,PY,432,728);
  caplabel(STRIPX[0]-6,PY+28,"A",ACC[0]); caplabel(STRIPX[1]-6,PY+28,"B",ACC[1]);
  static const char*rows[]={"TRIM","HI","MID","LOW","FILTER"}; for(int i=0;i<5;i++) text_c(F_CAP,960,PY+64+i*64+4,rows[i],C_DIM,1,0,1.2f);
  text_c(F_CAP,960,PY+536,"STEMS",C_DIM,1,0,1.2f);
  text_c(F_CAP,960,PY+392,"FX",C_DIM,1,0,1.2f); text_c(F_CAP,960,PY+452,"AMOUNT",C_DIM,1,0,1.2f);
  T=fb; }
static void draw_btn(Ctl*c,int on,uint32_t acc,const char*lbl,int fill){
  if(fill&&on){ rrect(c->x,c->y,c->w,c->h,6,acc,1,0); text_c(F_CAP,c->x+c->w/2,c->y+c->h/2+5,lbl,0x0E1012,1,0,1.2f); return; }
  rrect(c->x,c->y,c->w,c->h,6,on?mix(C_PANEL2,acc,0.16f):C_PANEL2,1,0); rrect(c->x,c->y,c->w,c->h,6,on?acc:C_LINE,1,1);
  text_c(F_CAP,c->x+c->w/2,c->y+c->h/2+5,lbl,on?acc:C_TEXT,1,0,1.2f); }
static void draw_knob(Ctl*c,uint32_t acc){ float cx=c->x+c->w/2,cy=c->y+c->h/2,R=c->w/2-4; float t=(*c->v-c->lo)/(c->hi-c->lo);
  float a0=-2.36f,a1=2.36f,av=a0+(a1-a0)*t,az=c->bip?0:a0;
  arc(cx,cy,R,3,a0,a1,C_LINE,1); if(fabsf(av-az)>0.02f) arc(cx,cy,R,3,az,av,acc,1);
  circle(cx,cy,R-7,C_PANEL2,1,0); circle(cx,cy,R-7,0x343A42,1,1);
  line(cx+sinf(av)*(R-17),cy-cosf(av)*(R-17),cx+sinf(av)*(R-9),cy-cosf(av)*(R-9),2.5f,C_TEXT,1); }
static void draw_vfad(Ctl*c,uint32_t acc,int tempo){ float cx=c->x+c->w/2; rrect(cx-2,c->y,4,c->h,2,0x0B0C0E,1,0);
  for(int i=0;i<=8;i++){ float y=c->y+c->h*i/8.0f; rect((int)(cx-16),(int)y,i==4&&tempo?32:10,1,C_LINE,1); if(!(i==4&&tempo)) rect((int)(cx+6),(int)y,10,1,C_LINE,1); }
  float t=(*c->v-c->lo)/(c->hi-c->lo); float y=c->y+c->h*(1-t); (void)acc;
  rrect(cx-20,y-11,40,22,4,0x000000,0.35f,0); rrect(cx-20,y-12,40,22,4,C_CAP,1,0); rect((int)(cx-14),(int)y-1,28,2,tempo&&fabsf(*c->v)<0.002f?acc:0x2A2E33,1); }
static void draw_hfad(Ctl*c){ float cy=c->y+c->h/2; rrect(c->x,cy-2,c->w,4,2,0x0B0C0E,1,0); for(int i=0;i<=8;i++) rect((int)(c->x+c->w*i/8.0f),(int)cy+10,1,i==4?8:5,C_LINE,1);
  float t=(*c->v+1)/2,x=c->x+c->w*t; rrect(x-12,cy-17,24,34,4,0x000000,0.35f,0); rrect(x-12,cy-18,24,34,4,C_CAP,1,0); rect((int)x-1,(int)cy-12,2,22,0x2A2E33,1); }
static void vu(float x,float y,float h,float lv,float hold,uint32_t acc){ int seg=17; float sh=(h-(seg-1)*2)/seg;
  for(int i=0;i<seg;i++){ float yy=y+h-(i+1)*sh-i*2; float th=(float)(i+1)/seg; uint32_t c=i>=15?0xE0554A:i>=12?0xE2C14A:acc;
    int lit=lv>=th*0.999f-1.0f/seg*0.5f, hl=fabsf(hold-th)<0.5f/seg; rect((int)x,(int)yy,8,(int)sh,lit||hl?c:0x23272C,lit||hl?1:1); } }
static float lvl_db(float pk){ if(pk<1e-4f)return 0; float db=20*log10f(pk); return clampf((db+36)/39,0,1); }
static void wave_col(uint8_t lo,uint8_t mi,uint8_t hi,uint32_t acc,uint32_t*c){ float s=lo+mi+hi+1; float l=lo/s,h=hi/s;
  uint32_t base=mix(mix(acc,0x1E5BD8,0.0f),acc,0); *c=mix(mix(mix(acc,0x2A6CF0,l*0.9f),0xF2F4F6,h*1.2f),base,0); }
static void zoom_wave(int d){ Deck*k=&D[d]; uint32_t acc=ACC[d]; float y0=64+d*120+8,hh=96,cyy=y0+hh/2; int x0=40,x1=1880,xc=960;
  if(!k->pcm){ text_c(F_BODY,960,cyy+6,"Press  Triangle  to open the library and load a track",C_MUTE,1,0,0); return; }
  double sec=k->pos/k->rate; float spp=7.0f/(x1-x0); 
  if(k->loopOn||k->linSet){ double ls=k->lin/k->rate,le=k->loopOn?k->lout/k->rate:sec; int a=(int)(xc+(ls-sec)/spp),b=(int)(xc+(le-sec)/spp); if(a<x0)a=x0; if(b>x1)b=x1; if(b>a) rect(a,(int)y0,b-a,(int)hh,acc,0.10f); }
  for(int x=x0;x<x1;x++){ double t=sec+(x-xc)*spp; int b=(int)(t*100); if(b<0||b>=k->nb)continue; float a=k->wpk[b]/255.0f*(hh/2-4); uint32_t c; wave_col(k->wlo[b],k->wmi[b],k->whi[b],acc,&c);
    float al=x<xc?0.55f:1; int yt=(int)(cyy-a),yb=(int)(cyy+a); for(int y=yt;y<=yb;y++) px(x,y,c,al); }
  if(k->bpm>0){ double bl=60.0/k->bpm; double b0=ceil((sec-(xc-x0)*spp-k->first)/bl); for(double b=b0;;b++){ double t=k->first+b*bl; int x=(int)(xc+(t-sec)/spp); if(x>=x1)break; if(x<x0)continue;
      int down=((long)b%4+4)%4==0; rect(x,(int)y0,1,down?14:7,down?C_TEXT:C_DIM,down?0.9f:0.6f); rect(x,(int)(y0+hh)-(down?14:7),1,down?14:7,down?C_TEXT:C_DIM,down?0.9f:0.6f); } }
  for(int i=0;i<8;i++) if(k->hcset[i]){ int x=(int)(xc+(k->hc[i]/k->rate-sec)/spp); if(x>=x0&&x<x1){ rect(x,(int)y0,2,(int)hh,acc,0.8f); rrect(x,y0,16,16,3,acc,1,0); text_c(F_CAP,x+8,y0+12,HCN[i],0x0E1012,1,0,0);} }
  { int x=(int)(xc+(k->cue/k->rate-sec)/spp); if(x>=x0&&x<x1){ for(int j=0;j<8;j++) rect(x-j,(int)(y0+hh)-8+j,2*j+1,1,0xE2C14A,1); } }
  rect(xc-1,(int)y0-2,2,(int)hh+4,0xFFFFFF,1);
  rrect(40,y0+4,24,20,4,acc,1,0); text_c(F_CAP,52,y0+18,d?"B":"A",0x0E1012,1,0,0); }
static void fmt_time(double s,char*b,int neg){ if(s<0)s=0; int m=(int)(s/60); double r=s-m*60; sprintf(b,"%s%02d:%04.1f",neg?"-":"",m,r); }
static void draw_deck(int d){ Deck*k=&D[d]; uint32_t acc=ACC[d]; int x=DPX[d],y=PY; char b[96];
  rrect(x+24,y+22,28,22,4,acc,1,0); text_c(F_CAP,x+38,y+37,d?"B":"A",0x0E1012,1,0,0);
  text_fit(F_TITLE,x+64,y+42,380,k->pcm?k->title:"No track loaded",k->pcm?C_TEXT:C_MUTE,1);
  text_fit(F_BODY,x+64,y+74,380,k->pcm?k->artist:"Open the library to load",C_DIM,1);
  /* BPM block */
  float eb=k->bpm>0?eff_bpm(k):0; if(k->pcm&&k->bpm>0) sprintf(b,"%.2f",eb); else strcpy(b,"--.--");
  text_r(F_BIG,x+672,y+64,b,k->sync?acc:C_TEXT,1,1,0); caplabel(x+672-text_w(F_BIG,b,1,0)-48,y+40,"BPM",C_DIM);
  float pct=(tempo_mul(k)-1)*100; sprintf(b,"%+.2f%%",pct); text_r(F_BODY,x+672,y+98,b,fabsf(pct)<0.005f?C_DIM:C_TEXT,1,1,0);
  float wv=text_w(F_BODY,b,1,0); sprintf(b,"RANGE %d%%",(int)(RANGES[k->range]*100)); text_r(F_CAP,x+672-wv-20,y+96,b,C_DIM,1,0,1.2f);
  if(k->bpm>0){ sprintf(b,"TRACK %.1f",k->bpm); text(F_CAP,x+456,y+96,b,C_MUTE,1,0,1.2f); }
  /* overview */
  int ox=x+24,oy=y+120,ow=648,oh=48; rrect(ox,oy,ow,oh,4,0x111316,1,0);
  if(k->pcm){ float prog=(float)(k->pos/k->n); for(int c=0;c<ow&&c<656;c++){ float a=k->ov[0][c]/255.0f*(oh/2-3); uint32_t col=mix(mix(acc,0x2A6CF0,k->ov[1][c]/400.0f),0xF2F4F6,k->ov[2][c]/600.0f);
      float al=(c<prog*ow)?0.35f:0.95f; rect(ox+c,(int)(oy+oh/2-a),1,(int)(2*a+1),col,al); }
    for(int i=0;i<8;i++) if(k->hcset[i]) rect(ox+(int)(k->hc[i]/k->n*ow),oy,1,oh,acc,0.9f);
    if(k->loopOn) rect(ox+(int)(k->lin/k->n*ow),oy,(int)fmax(2,(k->lout-k->lin)/k->n*ow),oh,acc,0.25f);
    rect(ox+(int)(prog*ow)-1,oy-3,2,oh+6,0xFFFFFF,1);
    double s=k->pos/k->rate,tot=(double)k->n/k->rate; fmt_time(s,b,0); text(F_TITLE,x+24,y+204,b,C_TEXT,1,1,0); fmt_time(tot-s,b,1); text_r(F_TITLE,x+672,y+204,b,(tot-s<30&&k->playing&&((SDL_GetTicks()/400)&1))?0xE0554A:C_DIM,1,1,0); }
  /* beat phase meter */
  float pmx=x+252; caplabel(pmx,y+184,"BEAT",C_MUTE);
  int bi=-1; double bp=0; if(k->pcm&&k->bpm>0){ bp=beatpos(k); bi=(int)(((long)floor(bp))%4+4)%4; }
  for(int i=0;i<4;i++) rrect(pmx+48+i*44,y+176,36,10,2,i==bi?acc:0x262A30,1,0);
  Deck*o=&D[1-d]; float off=0; int have=k->pcm&&o->pcm&&k->bpm>0&&o->bpm>0&&o->playing;
  if(have){ double a=beatpos(o); off=(float)((bp-floor(bp))-(a-floor(a))); if(off>0.5f)off-=1; if(off<-0.5f)off+=1; }
  rrect(pmx+48,y+194,168,4,2,0x262A30,1,0); rect((int)(pmx+48+84),y+190,1,12,C_DIM,1);
  if(have){ float mx_=pmx+48+84+off*168; rrect(mx_-3,y+190,6,12,2,fabsf(off)<0.03f?acc:0xE2C14A,1,0); }
  /* jog */
  float cx=x+160,cy=PY+360; float rot=k->rot; int touch=k->scratch||fabsf(k->bend)>0.001f;
  if(k->pcm){ float pr=(float)(k->pos/k->n); arc(cx,cy,128,4,0,pr*2*PI*0.999f+0.001f,acc,0.85f); }
  circle(cx,cy,136,touch?acc:0x2C3138,1,2);
  line(cx+sinf(rot)*58,cy-cosf(rot)*58,cx+sinf(rot)*112,cy-cosf(rot)*112,4,touch?acc:C_TEXT,1);
  text_c(F_CAP,cx,cy+4,k->vinyl?"VINYL":"NUDGE",k->vinyl?acc:C_DIM,1,0,1.2f);
  sprintf(b,"%+.2f",pct); (void)b;
  caplabel(x+604,y+212,"TEMPO",C_DIM);
  sprintf(b,"%.2f%%",fabsf(pct)); }
static void draw_controls(void){
  for(int i=0;i<nctl;i++){ Ctl*c=&ctl[i]; int d=c->deck; uint32_t acc=d>=0?ACC[d]:C_TEXT; Deck*k=d>=0?&D[d]:0; char b[48];
    switch(c->act){
    case A_SYNC: draw_btn(c,k->sync,acc,"SYNC",1); break;
    case A_KEY: draw_btn(c,k->keylock,acc,"MASTER TEMPO",0); break;
    case A_RANGE: sprintf(b,"RANGE  %d%%",(int)(RANGES[k->range]*100)); draw_btn(c,0,acc,b,0); break;
    case A_VINYL: draw_btn(c,k->vinyl,acc,"VINYL MODE",0); break;
    case A_PLAY: { int blink=!k->playing&&k->pcm&&((SDL_GetTicks()/500)&1); draw_btn(c,k->playing||k->preview,acc,"",1);
        if(!(k->playing||k->preview)) rrect(c->x,c->y,c->w,c->h,6,blink?acc:C_LINE,1,1);
        uint32_t ic=(k->playing||k->preview)?0x0E1012:C_TEXT; float ix=c->x+c->w/2-14,iy=c->y+c->h/2;
        { float tx=ix,ty=iy; for(int r=-9;r<=9;r++){ float w_=(9-abs(r))*1.1f; rect((int)tx,(int)(ty+r),(int)w_,1,ic,1);} }
        rect((int)ix+16,(int)iy-9,4,18,ic,1); rect((int)ix+24,(int)iy-9,4,18,ic,1); } break;
    case A_CUE: { int on=k->pcm&&!k->playing&&fabs(k->pos-k->cue)<k->rate*0.02; draw_btn(c,on||k->preview,0xE2C14A,"CUE",0); } break;
    case A_HC: { int on=k->hcset[c->sub]; if(on){ rrect(c->x,c->y,c->w,c->h,6,mix(C_PANEL2,acc,0.22f),1,0); rrect(c->x,c->y,c->w,4,2,acc,1,0); text_c(F_BODY,c->x+c->w/2,c->y+c->h/2+7,c->label,acc,1,0,0);} 
        else { rrect(c->x,c->y,c->w,c->h,6,C_PANEL2,1,0); rrect(c->x,c->y,c->w,c->h,6,C_LINE,1,1); text_c(F_BODY,c->x+c->w/2,c->y+c->h/2+7,c->label,C_MUTE,1,0,0);} } break;
    case A_LOOP: { int on=0; double len=k->lout-k->lin; if(c->sub==L_IN) on=k->linSet&&!k->loopOn; if(c->sub==L_OUT||c->sub==L_EXIT) on=k->loopOn;
        if(c->sub>=L_4&&k->loopOn&&k->bpm>0){ int nb=c->sub==L_4?4:c->sub==L_8?8:16; on=fabs(len-nb*spb_frames(k))<2; }
        draw_btn(c,on,acc,c->sub==L_EXIT&&!k->loopOn?"RELOOP":c->label,0); } break;
    case A_FXT: { static const char*fx[]={"OFF","ECHO","REVERB","FLANGER","SWEEP"}; draw_btn(c,k->fxtype!=0,acc,fx[k->fxtype],0); } break;
    case A_VOCUT: { int on=k->sthave[STEM_VOC]?k->stem[STEM_VOC]<0.02f:(k->mid<-0.5f); draw_btn(c,on,acc,"VOC CUT",0); } break;
    case A_INST: { int on=k->sthave[STEM_VOC]?k->stem[STEM_VOC]<0.02f:(k->mid<-0.6f); draw_btn(c,on,0x6BCF7A,"INST",0); } break;
    case A_CURVE: { static const char*cv[]={"SMOOTH","LINEAR","CUT"}; rrect(c->x,c->y,c->w,c->h,6,C_PANEL2,1,0); text_c(F_CAP,c->x+c->w/2,c->y+19,cv[curve],C_DIM,1,0,1.2f); } break;
    case A_VAL: { uint32_t col=d>=0?acc:C_TEXT; if(c->sub>=10&&c->sub<10+NSTEM){ int si=c->sub-10; if(!k||!k->sthave[si]) col=C_MUTE; }
      if(c->kind==K_KNOB) draw_knob(c,col); else if(c->kind==K_VFAD) draw_vfad(c,col,c->sub==1); else draw_hfad(c); } break; }
    if(i==focus){ float m=4; rrect(c->x-m,c->y-m,c->w+2*m,c->h+2*m,c->kind==K_KNOB?c->w/2+m:8,0xFFFFFF,0.95f,2); } } }

/* ---------- overlays ---------- */
enum { M_MAIN, M_LIB, M_HELP, M_CRED };
static int mode=M_MAIN, lsel=0, ltop=0; static char*credits=0;
static void dim_all(float a){ rect(0,0,W,H,0x06080A,a); }
static void draw_lib(void){ dim_all(0.72f); float x=240,y=120,w=1440,h=840; panel(x,y,w,h);
  text(F_MID,x+32,y+56,"Library",C_TEXT,1,0,0); char b[160]; sprintf(b,"%d tracks   Sorted by %s",nlib,SORTN[sortmode]); text(F_BODY,x+180,y+54,b,C_DIM,1,0,0);
  float cx[6]={x+32,x+96,x+620,x+1000,x+1120,x+1250};
  static const char*hd[]={"#","TITLE","ARTIST","BPM","SOURCE","LICENCE"};
  for(int i=0;i<6;i++){ if(i==3) text_r(F_CAP,cx[3]+60,y+104,hd[i],C_MUTE,1,0,1.2f); else caplabel(cx[i],y+104,hd[i],C_MUTE); }
  rect((int)x+24,(int)y+116,(int)w-48,1,C_LINE,1);
  int rows=15; if(lsel<ltop)ltop=lsel; if(lsel>=ltop+rows)ltop=lsel-rows+1;
  for(int r=0;r<rows&&ltop+r<nlib;r++){ int i=ltop+r; Track*t=&lib[order[i]]; float ry=y+128+r*44; int sel=i==lsel;
    if(sel){ rrect(x+24,ry,w-48,40,6,C_PANEL2,1,0); rrect(x+24,ry,w-48,40,6,0xFFFFFF,0.9f,2); } else if(r&1) rect((int)x+24,(int)ry,(int)w-48,40,0xFFFFFF,0.015f);
    sprintf(b,"%d",i+1); text(F_LBL,cx[0],ry+26,b,C_MUTE,1,1,0);
    text_fit(F_BODY,cx[1],ry+27,500,t->title,C_TEXT,1); text_fit(F_BODY,cx[2],ry+27,360,t->artist,C_DIM,1);
    if(t->bpm>0)sprintf(b,"%.1f",t->bpm); else strcpy(b,"--"); text_r(F_BODY,cx[3]+60,ry+27,b,C_TEXT,1,1,0);
    text(F_LBL,cx[4],ry+26,SRCN[t->src],t->src?ACC[1]:C_DIM,1,0,0); text_fit(F_LBL,cx[5],ry+26,150,t->lic,C_DIM,1); }
  if(!nlib) text_c(F_BODY,960,y+300,"No tracks found",C_DIM,1,0,0);
  float fy=y+h-28; float fx=x+32; fx=key_chip(fx,fy,"L1","Load to A"); fx=key_chip(fx,fy,"R1","Load to B"); fx=icon_chip(fx,fy,G_X,"Load to free deck");
  fx=icon_chip(fx,fy,G_SQ,"Sort"); fx=key_chip(fx,fy,"OPTIONS","Rescan USB + folder"); fx=icon_chip(fx,fy,G_O,"Close");
  text_r(F_LBL,x+w-32,fy,"Add music: /data/djdecks/music  or  USB:/music",C_MUTE,1,0,0); }
static void draw_help(void){ dim_all(0.75f); float x=320,y=110,w=1280,h=860; panel(x,y,w,h); text(F_MID,x+32,y+56,"Controls",C_TEXT,1,0,0);
  static const char*L[][2]={{"L2  /  R2","Play / pause deck A / B"},{"L1  /  R1","Cue deck A / B (hold to preview)"},{"Left stick  left/right","Jog deck A: nudge, or scratch in vinyl mode"},
    {"Right stick  left/right","Jog deck B: nudge, or scratch in vinyl mode"},{"Touchpad  (slide finger)","Crossfader: left edge A, right edge B"},{"D-pad","Move the highlight between controls"},
    {"Left stick  up/down","Adjust the highlighted knob or fader"},{"Cross","Press the highlighted button / set or jump to hot cue"},{"Circle","Reset highlighted control, clear hot cue"},
    {"Square","Sync the deck under the highlight"},{"Triangle","Library"},{"L3  /  R3","Toggle vinyl mode A / B"},{"Options","This screen  (Square here: credits)"},
    {"VOC CUT / INST","Mute vocals (stem fader or EQ karaoke cut)"},{"VOC DRM BAS OTH","Stem layer faders when sidecar files are present"}};
  for(unsigned i=0;i<sizeof L/sizeof*L;i++){ float ry=y+120+i*48; text(F_BODY,x+40,ry,L[i][0],C_TEXT,1,0,0); text(F_BODY,x+440,ry,L[i][1],C_DIM,1,0,0); if(i) rect((int)x+40,(int)ry-30,(int)w-80,1,0x23272C,1); }
  float fx=x+40,fy=y+h-28; fx=icon_chip(fx,fy,G_SQ,"Credits"); icon_chip(fx,fy,G_O,"Close"); }
static void draw_cred(void){ dim_all(0.75f); float x=240,y=90,w=1440,h=900; panel(x,y,w,h); text(F_MID,x+32,y+56,"Credits",C_TEXT,1,0,0);
  if(credits){ char*s=credits; float ry=y+100; while(*s&&ry<y+h-50){ char ln[256]; int n=0; while(s[n]&&s[n]!='\n'&&n<255){ln[n]=s[n];n++;} ln[n]=0; s+=n; if(*s)s++;
      int head=ln[0]=='#'; text(head?F_CAP:F_LBL,x+40,ry,head?ln+1:ln,head?ACC[0]:C_DIM,1,0,head?1.2f:0); ry+=head?30:24; } }
  icon_chip(x+40,y+h-28,G_O,"Close"); }
static void draw_mixer(void){ for(int d=0;d<2;d++){ Deck*k=&D[d]; float l=lvl_db(k->vu); if(l>k->vuh)k->vuh=l; else k->vuh-=0.006f; float cur=l;
    static float sm[2]; sm[d]=fmaxf(cur,sm[d]-0.03f); vu(d?968:944,PY+496,136,sm[d],k->vuh,ACC[d]); k->vu=0; }
  static float ms=0,mh=0; float l=lvl_db(mvu); mvu=0; ms=fmaxf(l,ms-0.03f); if(l>mh)mh=l; else mh-=0.006f;
  for(int i=0;i<24;i++){ float th=(i+1)/24.0f; uint32_t c=i>=21?0xE0554A:i>=17?0xE2C14A:0x8FD39B; int lit=ms>=th-0.02f||fabsf(mh-th)<0.02f; rect(1560+i*10,24,8,10,lit?c:0x23272C,1); }
  caplabel(1560,18,"MASTER",C_MUTE); caplabel(1756,18,"",C_MUTE);
  caplabel(800,PY+640,"A",ACC[0]); text_r(F_CAP,1120,PY+640,"B",ACC[1],1,0,0); }
static void draw_topbar(void){ float x=520,y=36; x=key_chip(x,y,"L2 R2","Play"); x=key_chip(x,y,"L1 R1","Cue"); x=icon_chip(x,y,G_TRI,"Library"); x=icon_chip(x,y,G_SQ,"Sync"); x=key_chip(x,y,"TOUCHPAD","Crossfader");
  key_chip(x,y,"OPTIONS","Controls"); }
static void draw_frame(void){ memcpy(fb,bgb,W*H*4); T=fb;
  draw_topbar(); zoom_wave(0); zoom_wave(1); draw_deck(0); draw_deck(1); draw_mixer(); draw_controls();
  if(mode==M_LIB)draw_lib(); else if(mode==M_HELP)draw_help(); else if(mode==M_CRED)draw_cred();
  if(toastT>SDL_GetTicks()){ float w=text_w(F_BODY,toast,0,0)+48; rrect(960-w/2,1000,w,44,8,C_PANEL2,1,0); rrect(960-w/2,1000,w,44,8,C_LINE,1,1); text_c(F_BODY,960,1028,toast,C_TEXT,1,0,0); } }
static SDL_Window*win; static SDL_Surface*wsurf,*fbs;
static void present(void){ SDL_BlitSurface(fbs,NULL,wsurf,NULL); SDL_UpdateWindowSurface(win); }
static void splash(void){ for(int f=0;f<70;f++){ T=fb; rect(0,0,W,H,C_BG,1); float a=f<20?f/20.0f:f>55?(70-f)/15.0f:1;
    logo_mark(960,440,44); rect(0,0,W,H,C_BG,1-a); /* fade */
    text_c(F_HERO,960,600,"Twin Decks",C_TEXT,a,0,0.5f); text_c(F_BODY,960,644,"Two-deck performance player",C_DIM,a,0,0);
    present(); SDL_Delay(16); } }

/* ---------- main ---------- */
enum { B_X=0,B_O,B_SQ,B_TRI,B_L1,B_R1,B_L2,B_R2,B_SHARE,B_OPT,B_L3,B_R3,B_UP=13,B_DN,B_LT,B_RT };
static float axis(SDL_Joystick*j,int a){ if(!j)return 0; float v=SDL_JoystickGetAxis(j,a)/32767.0f; float dz=0.14f; if(fabsf(v)<dz)return 0; return (v>0?v-dz:v+dz)/(1-dz); }
static int free_deck(void){ if(!D[0].pcm)return 0; if(!D[1].pcm)return 1; if(!D[0].playing)return 0; return 1; }
static void press_ctl(int down){ Ctl*c=&ctl[focus]; int d=c->deck; Deck*k=d>=0?&D[d]:0;
  if(c->act==A_CUE){ if(down)cue_down(d); else cue_up(d); return; } if(!down)return;
  switch(c->act){ case A_PLAY: play_toggle(d); break; case A_SYNC: do_sync(d); break; case A_KEY: k->keylock=!k->keylock; break;
  case A_RANGE: { float m=tempo_mul(k); k->range=(k->range+1)%3; k->tempo=clampf((m-1)/RANGES[k->range],-1,1); } break;
  case A_VINYL: k->vinyl=!k->vinyl; break; case A_HC: hotcue(d,c->sub,0); break; case A_LOOP: loop_act(d,c->sub); break;
  case A_FXT: k->fxtype=(k->fxtype+1)%5; if(k->fxtype&&k->fxamt<0.05f)k->fxamt=0.5f; break; case A_CURVE: curve=(curve+1)%3; break;
  case A_VOCUT: stem_vocal_off(k); break; case A_INST: stem_instrumental(k); break;
  case A_VAL: if(c->sub>=10&&c->sub<10+NSTEM){ int si=c->sub-10; *c->v=k->sthave[si]?1.0f:0; } else *c->v=c->def; break; } }
int main(int argc,char*argv[]){ (void)argc;(void)argv;
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_AUDIO)!=0){ for(;;){} }
  win=SDL_CreateWindow("Twin Decks",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,W,H,0); if(!win)for(;;){}
  wsurf=SDL_GetWindowSurface(win); fb=malloc(W*H*4); bgb=malloc(W*H*4); T=fb;
  fbs=SDL_CreateRGBSurfaceWithFormatFrom(fb,W,H,32,W*4,SDL_PIXELFORMAT_RGB888);
  fonts_init(); splash();
  SDL_Joystick*pad=SDL_NumJoysticks()>0?SDL_JoystickOpen(0):0;
  int tph=-1,uid=0; if(sceUserServiceGetInitialUser(&uid)==0){ tph=scePadGetHandle(uid,0,0); if(tph<0) tph=scePadOpen(uid,0,0,NULL); }
  for(int d=0;d<2;d++){ Deck*k=&D[d]; k->fader=1; k->range=0; k->c_hi=k->c_mid=k->c_lo=k->c_filt=99; }
  SDL_AudioSpec want,got; memset(&want,0,sizeof want); want.freq=OUTSR; want.format=AUDIO_S16; want.channels=2; want.samples=1024; want.callback=audio_cb;
  adev=SDL_OpenAudioDevice(NULL,0,&want,&got,0); SDL_PauseAudioDevice(adev,0);
  credits=(char*)readfile(ASSETS"CREDITS.txt",0);
  build_bg(); build_ctl(); lib_scan(); set_toast("Press Triangle to open the library");
  Uint32 last=SDL_GetTicks(); int held[17]={0}; Uint32 rep=0; int trigA=0,trigB=0;
  for(;;){ Uint32 now=SDL_GetTicks(); float dt=(now-last)/1000.0f; if(dt>0.1f)dt=0.1f; last=now;
    int pr[17]={0},rl[17]={0};
    SDL_Event ev; while(SDL_PollEvent(&ev)){ if(ev.type==SDL_QUIT) goto out;
      if(ev.type==SDL_JOYBUTTONDOWN&&ev.jbutton.button<17){ pr[ev.jbutton.button]=1; held[ev.jbutton.button]=1; rep=now+320; }
      if(ev.type==SDL_JOYBUTTONUP&&ev.jbutton.button<17){ rl[ev.jbutton.button]=1; held[ev.jbutton.button]=0; } }
    /* triggers: button or analogue axis */
    int ta=held[B_L2]||(pad&&SDL_JoystickGetAxis(pad,4)>16000), tb=held[B_R2]||(pad&&SDL_JoystickGetAxis(pad,5)>16000);
    int pA=ta&&!trigA, pB=tb&&!trigB; trigA=ta; trigB=tb;
    int dx=0,dy=0; if(pr[B_LT])dx=-1; if(pr[B_RT])dx=1; if(pr[B_UP])dy=-1; if(pr[B_DN])dy=1;
    if(!dx&&!dy&&now>rep&&(held[B_LT]||held[B_RT]||held[B_UP]||held[B_DN])){ rep=now+70; dx=held[B_LT]?-1:held[B_RT]?1:0; dy=held[B_UP]?-1:held[B_DN]?1:0; }
    float lx=axis(pad,0),ly=axis(pad,1),rx=axis(pad,2),ry=axis(pad,3);
    if(mode==M_LIB){ static float acc_=0; acc_+=ly*dt*14; while(acc_>1){acc_-=1;dy=1;} while(acc_<-1){acc_+=1;dy=-1;}
      if(dy) lsel=lsel+dy; if(dx) lsel+=dx*15; if(lsel<0)lsel=0; if(lsel>=nlib)lsel=nlib-1; if(lsel<0)lsel=0;
      if(nlib&&(pr[B_L1]||pA)){ load_track(0,order[lsel]); mode=M_MAIN; }
      else if(nlib&&(pr[B_R1]||pB)){ load_track(1,order[lsel]); mode=M_MAIN; }
      else if(nlib&&pr[B_X]){ load_track(free_deck(),order[lsel]); mode=M_MAIN; }
      if(pr[B_SQ]){ int cur=nlib?order[lsel]:0; sortmode=(sortmode+1)%4; lib_sort(); for(int i=0;i<nlib;i++) if(order[i]==cur)lsel=i; }
      if(pr[B_OPT]){ lib_scan(); lsel=0; char m[64]; sprintf(m,"Found %d tracks",nlib); set_toast(m); }
      if(pr[B_O]||pr[B_TRI]) mode=M_MAIN; }
    else if(mode==M_HELP){ if(pr[B_SQ])mode=M_CRED; else if(pr[B_O]||pr[B_OPT]||pr[B_X])mode=M_MAIN; }
    else if(mode==M_CRED){ if(pr[B_O]||pr[B_OPT]||pr[B_X]||pr[B_SQ])mode=M_HELP; }
    else {
      if(dx||dy) move_focus(dx,dy);
      if(pr[B_X]) press_ctl(1); if(rl[B_X]) press_ctl(0);
      if(pr[B_O]){ Ctl*c=&ctl[focus]; if(c->act==A_HC) hotcue(c->deck,c->sub,1); else if(c->act==A_VAL){ *c->v=c->def; if(c->sub==1)D[c->deck].sync=0; }
        else if(c->act==A_LOOP&&c->deck>=0){ D[c->deck].loopOn=0; } }
      if(pr[B_SQ]){ int d=ctl[focus].deck; do_sync(d<0?0:d); }
      if(pr[B_TRI]){ mode=M_LIB; } if(pr[B_OPT]) mode=M_HELP;
      if(pA) play_toggle(0); if(pB) play_toggle(1);
      if(pr[B_L1]) cue_down(0); if(rl[B_L1]) cue_up(0); if(pr[B_R1]) cue_down(1); if(rl[B_R1]) cue_up(1);
      if(pr[B_L3]) D[0].vinyl=!D[0].vinyl; if(pr[B_R3]) D[1].vinyl=!D[1].vinyl;
      Ctl*c=&ctl[focus]; if(c->act==A_VAL&&ly!=0){ float sp=(c->hi-c->lo)*(c->sub==1?0.12f:0.7f); *c->v=clampf(*c->v-ly*fabsf(ly)*sp*dt,c->lo,c->hi); if(c->sub==1&&c->deck>=0)D[c->deck].sync=0; }
      (void)ry; }
    /* touchpad = crossfader, absolute while touching, holds on release */
    if(tph>=0){ OrbisPadData pd; memset(&pd,0,sizeof pd); if(scePadReadState(tph,&pd)==0&&pd.touch.fingers>0&&mode==M_MAIN){ float t=(pd.touch.touch[0].x-40)/1840.0f; xf=clampf(t*2-1,-1,1); } }
    /* jogs (active in every mode so a held stick never sticks) */
    float js[2]={mode==M_MAIN?lx:0,mode==M_MAIN?rx:0};
    for(int d=0;d<2;d++){ Deck*k=&D[d]; float j=js[d];
      if(k->vinyl&&fabsf(j)>0.05f){ k->svel=j*2.5f; k->scratch=1; k->bend=0; } else { k->scratch=0; k->bend=k->vinyl?0:j*0.10f; }
      if(k->sync&&D[1-d].pcm) match_tempo(d);
      double sp=k->scratch?k->svel:(k->playing||k->preview?tempo_mul(k)*(1+k->bend):0); k->rot+=(float)(sp*dt*2*PI*0.5556); if(k->rot>2*PI)k->rot-=2*PI; if(k->rot<0)k->rot+=2*PI; }
    draw_frame(); present();
    Uint32 el=SDL_GetTicks()-now; if(el<16) SDL_Delay(16-el); }
out: SDL_Quit(); return 0; }
