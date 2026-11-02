package com.chris.goldhenguide;
import android.app.Activity;import android.os.Bundle;import android.webkit.WebView;
public class MainActivity extends Activity{WebView w;
protected void onCreate(Bundle b){super.onCreate(b);w=new WebView(this);w.setBackgroundColor(0xff111111);w.getSettings().setJavaScriptEnabled(true);w.loadUrl("file:///android_asset/guide.html");setContentView(w);}
public void onBackPressed(){if(w.canGoBack())w.goBack();else super.onBackPressed();}}
