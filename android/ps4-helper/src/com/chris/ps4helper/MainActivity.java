package com.chris.ps4helper;
import android.app.*;import android.content.*;import android.graphics.*;import android.graphics.drawable.GradientDrawable;
import android.os.*;import android.view.*;import android.widget.*;import java.net.*;import java.util.*;
public class MainActivity extends Activity{
  static final String PERM="com.termux.permission.RUN_COMMAND",SCRIPT="/data/data/com.termux/files/home/ps4/start.sh";
  static final int BG=0xff111827,CARD=0xff1f2937,GOLD=0xffFFE066,GOLD2=0xffD4A017,TXT=0xffe6e6e6,BLUE=0xff2563eb,BLUE2=0xff1d4ed8,EDGE=0xff374151,MUTED=0xff9ca3af,GREEN=0xff22c55e;
  int step=0;
  protected void onCreate(Bundle b){super.onCreate(b);if(Build.VERSION.SDK_INT>=23&&checkSelfPermission(PERM)!=0)requestPermissions(new String[]{PERM},1);if(Build.VERSION.SDK_INT>=33&&checkSelfPermission("android.permission.POST_NOTIFICATIONS")!=0)requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"},2);render();handle(getIntent());}
  protected void onNewIntent(Intent i){super.onNewIntent(i);handle(i);}
  void handle(Intent i){if(i!=null&&i.hasExtra("page")){step=Math.max(0,Math.min(5,i.getIntExtra("page",0)));render();}String a=i==null?null:i.getStringExtra("debug_cmd");if(a==null)return;if("start".equals(a))run("8081");else if("stop".equals(a))run("stop");}
  String ip(){try{for(NetworkInterface n:Collections.list(NetworkInterface.getNetworkInterfaces())){if(!n.getName().startsWith("wlan"))continue;for(InetAddress a:Collections.list(n.getInetAddresses()))if(a instanceof Inet4Address&&!a.isLoopbackAddress())return a.getHostAddress();}}catch(Exception e){}
    try{android.net.wifi.WifiManager w=(android.net.wifi.WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);int v=w.getConnectionInfo().getIpAddress();if(v!=0)return(v&255)+"."+(v>>8&255)+"."+(v>>16&255)+"."+(v>>24&255);}catch(Exception e){}return null;}
  String url(){String i=ip();return i==null?null:"http://"+i+":8081/";}
  void run(final String arg){
    // Some phones (nubia/ZTE) refuse to start Termux's service unless Termux is already running, so wake Termux first.
    try{Intent t=new Intent();t.setClassName("com.termux","com.termux.app.TermuxActivity");t.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);startActivity(t);}catch(Exception e){}
    say("Waking Termux…",false);
    new Handler(Looper.getMainLooper()).postDelayed(()->{send(arg);Intent me=new Intent(this,MainActivity.class);me.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT|Intent.FLAG_ACTIVITY_NEW_TASK);startActivity(me);},1800);}
  boolean send(String arg){
    try{if(Build.VERSION.SDK_INT>=23&&checkSelfPermission(PERM)!=0)throw new SecurityException("no permission");
      Intent i=new Intent("com.termux.RUN_COMMAND");i.setClassName("com.termux","com.termux.app.RunCommandService");
      i.putExtra("com.termux.RUN_COMMAND_PATH","/data/data/com.termux/files/usr/bin/sh");
      i.putExtra("com.termux.RUN_COMMAND_ARGUMENTS",arg.equals("stop")?new String[]{"-c","sh '"+SCRIPT+"' stop; sleep 1; X='startservice -n com.termux/.app.TermuxService -a com.termux.service_stop'; am $X || termux-am $X || /system/bin/am $X"}:new String[]{SCRIPT,arg});
      i.putExtra("com.termux.RUN_COMMAND_WORKDIR","/data/data/com.termux/files/home/ps4");
      i.putExtra("com.termux.RUN_COMMAND_BACKGROUND",true);
      if((Build.VERSION.SDK_INT>=26?startForegroundService(i):startService(i))==null)throw new Exception("Termux not found");if(arg.equals("stop"))((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).cancel(7);else notifyOff();
      say(arg.equals("stop")?"Stop command sent to Termux.":"Server starting in Termux… give it a few seconds.",false);return true;
    }catch(Exception e){say("Couldn't start Termux from here ("+e.getMessage()+"). Open Termux once, then try again — or tap the 'PS4 Servers' widget on your home screen.",true);return false;}}
  void say(String s,boolean err){final String js="say("+org.json.JSONObject.quote(s)+","+err+")";runOnUiThread(()->{if(wv!=null)wv.evaluateJavascript(js,null);});}
  android.webkit.WebView wv;
  class Bridge{
    @android.webkit.JavascriptInterface public void start(){runOnUiThread(()->run("8081"));}
    @android.webkit.JavascriptInterface public void stop(){runOnUiThread(()->{run("stop");((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).cancel(7);});}
    @android.webkit.JavascriptInterface public void copy(final String v){runOnUiThread(()->{((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("DNS",v));Toast.makeText(MainActivity.this,"Copied "+v,Toast.LENGTH_SHORT).show();});}
    @android.webkit.JavascriptInterface public String url(){return MainActivity.this.url();}
    @android.webkit.JavascriptInterface public int initial(){return step;}
    @android.webkit.JavascriptInterface public void page(int n){step=n;}
    @android.webkit.JavascriptInterface public String qr(String u){boolean[][] q=Qr.encode(u);int n=q.length,s=10,qz=4;Bitmap bm=Bitmap.createBitmap((n+2*qz)*s,(n+2*qz)*s,Bitmap.Config.ARGB_8888);bm.eraseColor(Color.WHITE);
      for(int y=0;y<n;y++)for(int x=0;x<n;x++)if(q[y][x])for(int a=0;a<s;a++)for(int b=0;b<s;b++)bm.setPixel((x+qz)*s+a,(y+qz)*s+b,Color.BLACK);
      java.io.ByteArrayOutputStream o=new java.io.ByteArrayOutputStream();bm.compress(Bitmap.CompressFormat.PNG,100,o);return "data:image/png;base64,"+android.util.Base64.encodeToString(o.toByteArray(),android.util.Base64.NO_WRAP);}
  }
  void render(){if(wv==null){getWindow().setStatusBarColor(BG);getWindow().setNavigationBarColor(BG);
      wv=new android.webkit.WebView(this);wv.setBackgroundColor(BG);wv.getSettings().setJavaScriptEnabled(true);wv.addJavascriptInterface(new Bridge(),"A");wv.loadUrl("file:///android_asset/index.html");setContentView(wv);}
    else wv.evaluateJavascript("go("+step+")",null);}
  protected void onResume(){super.onResume();if(wv!=null)wv.evaluateJavascript("window.addr&&addr()",null);}
  void notifyOff(){try{NotificationManager nm=(NotificationManager)getSystemService(NOTIFICATION_SERVICE);
    if(Build.VERSION.SDK_INT>=26)nm.createNotificationChannel(new NotificationChannel("srv","PS4 server",NotificationManager.IMPORTANCE_DEFAULT));
    Intent o=new Intent(this,MainActivity.class).putExtra("page",5).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
    PendingIntent pi=PendingIntent.getActivity(this,6,o,PendingIntent.FLAG_UPDATE_CURRENT|(Build.VERSION.SDK_INT>=23?PendingIntent.FLAG_IMMUTABLE:0));
    Notification.Builder nb=Build.VERSION.SDK_INT>=26?new Notification.Builder(this,"srv"):new Notification.Builder(this);
    nb.setSmallIcon(android.R.drawable.ic_lock_power_off).setContentTitle("Turn off the server").setContentText("PS4 servers are running. Tap to turn them off.").setContentIntent(pi).setOngoing(true).setColor(GOLD2);
    nm.notify(7,nb.build());}catch(Exception e){}}
  public void onBackPressed(){if(step>0){step--;render();}else super.onBackPressed();}
}
