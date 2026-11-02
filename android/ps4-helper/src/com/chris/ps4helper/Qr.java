package com.chris.ps4helper;
/** Minimal offline QR encoder: version 3, EC level L, byte mode, mask 0. Max 53 bytes. */
public class Qr {
  static final int N=29; boolean[][] m=new boolean[N][N], f=new boolean[N][N];
  void set(int x,int y,boolean d){m[y][x]=d;f[y][x]=true;}
  static int mul(int a,int b){int z=0;for(int i=7;i>=0;i--){z=(z<<1)^((z>>>7)*0x11D);z^=((b>>>i)&1)*a;}return z;}
  public static boolean[][] encode(String s){Qr q=new Qr();q.build(s.getBytes());return q.m;}
  void build(byte[] b){
    for(int i=0;i<N;i++){set(6,i,i%2==0);set(i,6,i%2==0);}
    finder(3,3);finder(N-4,3);finder(3,N-4);
    for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)set(22+dx,22+dy,Math.max(Math.abs(dx),Math.abs(dy))!=1);
    format(0);
    int dataLen=55; int[] d=new int[dataLen]; int bit=0;
    long[] parts={4,4,b.length,8}; // mode 0100, count
    java.util.ArrayList<Integer> bits=new java.util.ArrayList<>();
    for(int i=3;i>=0;i--)bits.add((4>>i)&1);
    for(int i=7;i>=0;i--)bits.add((b.length>>i)&1);
    for(byte x:b)for(int i=7;i>=0;i--)bits.add((x>>i)&1);
    for(int i=0;i<4&&bits.size()<dataLen*8;i++)bits.add(0);
    while(bits.size()%8!=0)bits.add(0);
    for(int i=0;i<bits.size()/8;i++){int v=0;for(int j=0;j<8;j++)v=(v<<1)|bits.get(i*8+j);d[i]=v;}
    for(int i=bits.size()/8,p=0;i<dataLen;i++,p^=1)d[i]=p==0?0xEC:0x11;
    int ec=15; int[] div=new int[ec]; div[ec-1]=1; int root=1;
    for(int i=0;i<ec;i++){for(int j=0;j<ec;j++){div[j]=mul(div[j],root);if(j+1<ec)div[j]^=div[j+1];}root=mul(root,2);}
    int[] r=new int[ec];
    for(int x:d){int fct=x^r[0];System.arraycopy(r,1,r,0,ec-1);r[ec-1]=0;for(int i=0;i<ec;i++)r[i]^=mul(div[i],fct);}
    int[] all=new int[70];System.arraycopy(d,0,all,0,55);System.arraycopy(r,0,all,55,15);
    int i=0;
    for(int right=N-1;right>=1;right-=2){if(right==6)right=5;
      for(int v=0;v<N;v++)for(int j=0;j<2;j++){int x=right-j;boolean up=((right+1)&2)==0;int y=up?N-1-v:v;
        if(!f[y][x]&&i<all.length*8){m[y][x]=((all[i>>>3]>>>(7-(i&7)))&1)!=0;i++;}}}
    for(int y=0;y<N;y++)for(int x=0;x<N;x++)if(!f[y][x]&&(x+y)%2==0)m[y][x]=!m[y][x];
    format(0);
  }
  void finder(int cx,int cy){for(int dy=-4;dy<=4;dy++)for(int dx=-4;dx<=4;dx++){int x=cx+dx,y=cy+dy;if(x<0||y<0||x>=N||y>=N)continue;int dist=Math.max(Math.abs(dx),Math.abs(dy));set(x,y,dist!=2&&dist!=4);}}
  void format(int mask){int data=(1<<3)|mask;int rem=data;for(int i=0;i<10;i++)rem=(rem<<1)^((rem>>>9)*0x537);int bits=((data<<10)|rem)^0x5412;
    for(int i=0;i<=5;i++)set(8,i,g(bits,i));set(8,7,g(bits,6));set(8,8,g(bits,7));set(7,8,g(bits,8));
    for(int i=9;i<15;i++)set(14-i,8,g(bits,i));
    for(int i=0;i<8;i++)set(N-1-i,8,g(bits,i));for(int i=8;i<15;i++)set(8,N-15+i,g(bits,i));set(8,N-8,true);}
  static boolean g(int x,int i){return((x>>>i)&1)!=0;}
}
