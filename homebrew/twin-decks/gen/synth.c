/* Original procedural dance loops for Twin Decks. Released CC0. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#define SR 44100
static float frand(unsigned*s){*s=*s*1664525u+1013904223u;return ((*s>>8)/8388608.0f)-1.0f;}
typedef struct{const char*f;float bpm;int style;int root;unsigned seed;} T;
static float note(int n){return 440.0f*powf(2.0f,(n-69)/12.0f);}
int main(){
 T t[]={{"neon_avenue",124,0,45,11},{"midnight_circuit",130,1,40,22},{"chrome_breaks",174,2,43,33},{"dusty_crates",90,3,41,44},
        {"basement_pulse",126,0,48,55},{"iron_lung_techno",134,1,38,66},{"liquid_fireflies",172,2,46,77},{"sunset_garage",132,4,44,88},
        {"low_end_theory_lab",95,3,39,99},{"afterhours_acid",128,5,36,111}};
 int prog[4][4]={{0,0,5,3},{0,3,7,5},{0,-2,-4,-5},{0,5,0,7}};
 for(int k=0;k<10;k++){T*x=&t[k]; unsigned s=x->seed; float spb=60.0f/x->bpm; int bars=x->bpm>160?96:64; if(x->bpm<100)bars=48;
  long N=(long)(bars*4*spb*SR); int16_t*o=malloc(N*4); char fn[64]; sprintf(fn,"%s.wav",x->f);
  float lp=0,ph=0,bph=0,acid=0; int pr=s%4; int pat=s%7;
  for(long i=0;i<N;i++){ float tt=(float)i/SR; float beat=tt/spb; int b=(int)beat; float fb=beat-b; int bar=b/4; int sec=bar/8;
   float L=0,Rr=0; float st=fb*spb; /* seconds since beat */
   int drop=(sec%4)!=0, intro=sec==0;
   /* kick */
   int kick; if(x->style==2){int p=(b%2==0)&&fb<0.5f; kick=p; } else if(x->style==3){kick=(b%4==0)||(b%4==2&&(bar&1));} else if(x->style==4){kick=(b%2==0);} else kick=1;
   float kv=0; if(kick){ float ff=50+120*expf(-st*30); float kp=2*M_PI*ff*st; kv=sinf(kp)*expf(-st*(x->style==3?6:8)); }
   if(x->style==2){ float sb=fmodf(beat,2.0f); if(sb>1.5f){float s2=(sb-1.5f)*spb; kv+=sinf(2*M_PI*(55+90*expf(-s2*30))*s2)*expf(-s2*9);} }
   /* snare/clap on 2&4 */
   float sn=0; if(b%2==1){ sn=frand(&s)*expf(-st*(x->style==3?14:20))*0.5f + sinf(2*M_PI*190*st)*expf(-st*25)*0.3f; }
   /* hats */
   float h8=fmodf(beat*2,1.0f)*spb/2; float hat=frand(&s)*expf(-h8*(x->style==1?60:90))*((x->style==0||x->style==4)&&((int)(beat*2)%2==1)?0.35f:0.15f);
   if(x->style==2){float h16=fmodf(beat*4,1)*spb/4; hat=frand(&s)*expf(-h16*120)*0.15f;}
   /* bass */
   int chord=prog[pr][bar%4]; float bf=note(x->root+chord+((pat>>(b%3))&1?12:0));
   float sub=fmodf(beat*2,1.0f); float benv= x->style==2? 1.0f : (x->style==0||x->style==4? (sub>0.5f?1:0.2f):1.0f);
   bph+=bf/SR; if(bph>1)bph-=1; float saw=2*bph-1;
   float cut= x->style==5? 0.02f+0.25f*(0.5f+0.5f*sinf(tt*0.3f))*expf(-fmodf(beat*4,1)*3): 0.04f;
   if(x->style==5){ int st16=(int)(beat*4)%16; int nn=x->root+12+((s>>3)%3==0?12:0)+((st16*7)%5==0?3:0); bph+=note(nn)/SR-bf/SR; acid=1;}
   lp+=cut*(saw-lp); float bass=(x->style==3? sinf(2*M_PI*bph): lp)*benv*0.5f;
   /* pad / stab */
   float pad=0; if(!intro){ int nts[3]={0,3,7}; if((x->style==0||x->style==4))nts[1]=4; for(int q=0;q<3;q++){ float f=note(x->root+24+chord+nts[q]); pad+=sinf(2*M_PI*f*tt+0.3f*sinf(2*M_PI*f*0.5f*tt)); }
     float stab= (x->style==1||x->style==5)? expf(-fmodf(beat,1.0f)*spb*6)*((b%4==3)?1:0) : (x->style==0||x->style==4)? expf(-fmodf(beat+0.5f,1.0f)*spb*5): 0.6f;
     pad*=0.07f*stab*(drop?1:0.4f); }
   float mel=0; if(drop&&x->style!=1){ int st8=(int)(beat*2); int sc[5]={0,3,5,7,10}; int n=x->root+36+sc[(st8*3+bar+ (x->seed>>2))%5]; float e=expf(-fmodf(beat*2,1)*spb/2*8); mel=sinf(2*M_PI*note(n)*tt)*e*0.08f; ph+=1; }
   L=kv*0.8f+sn*0.7f+hat*0.9f+bass*(intro&&bar<4?0:1)+pad+mel; Rr=kv*0.8f+sn*0.7f+hat*0.7f+bass*(intro&&bar<4?0:1)+pad*0.9f+mel*1.1f;
   if(intro&&bar<2){L*=0.9f;Rr*=0.9f;}
   float fo=1; long rem=N-i; if(rem<SR*2)fo=rem/(SR*2.0f);
   L=tanhf(L*1.1f)*fo; Rr=tanhf(Rr*1.1f)*fo; o[i*2]=(int16_t)(L*30000); o[i*2+1]=(int16_t)(Rr*30000);}
  (void)acid;
  FILE*f=fopen(fn,"wb"); uint32_t ds=N*4,v; fwrite("RIFF",1,4,f); v=36+ds; fwrite(&v,4,1,f); fwrite("WAVEfmt ",1,8,f); v=16; fwrite(&v,4,1,f);
  uint16_t a=1,c=2,bb=16,ba=4; fwrite(&a,2,1,f); fwrite(&c,2,1,f); v=SR; fwrite(&v,4,1,f); v=SR*4; fwrite(&v,4,1,f); fwrite(&ba,2,1,f); fwrite(&bb,2,1,f);
  fwrite("data",1,4,f); fwrite(&ds,4,1,f); fwrite(o,4,N,f); fclose(f); free(o); printf("%s %.0f\n",fn,x->bpm);}
}
