package com.rockcarry.ffplayer;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.Surface;
import android.view.SurfaceView;
import android.widget.Toast;

public class TestPlayerActivity extends Activity {
    private player      mplayer = null;
    private SurfaceView mview   = null;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        String url    = "/sdcard/test.mp4";
        Intent intent = getIntent();
        String action = intent.getAction();
        if (intent.ACTION_VIEW.equals(action)) {
            Uri    uri    = (Uri) intent.getData();
            String scheme = uri.getScheme();
            if (scheme.equals("file")) {
                url = uri.getPath();
            } else if (  scheme.equals("http" )
                      || scheme.equals("https")
                      || scheme.equals("rtsp" )
                      || scheme.equals("rtmp" ) ) {
                url = uri.toString();
            } else if (scheme.equals("content")) {
                String[] proj = { MediaStore.Images.Media.DATA };
                Cursor cursor = managedQuery(uri, proj, null, null, null);
                int    colidx = cursor.getColumnIndexOrThrow(MediaStore.Images.Media.DATA);
                cursor.moveToFirst();
                url = cursor.getString(colidx);
            }
        }

        mplayer = new player();
        if (!mplayer.open(url)) {
            String str = String.format(getString(R.string.open_video_failed), url);
            Toast.makeText(this, str, Toast.LENGTH_LONG).show();
//          finish(); return;
        }

        mplayer.setPlayerEventCallback(new player.playerEventCallback() {
            @Override
            public void onPlayerEvent(int event, long param) {
//              android.util.Log.d("===ck===", "event = " + event + ", param = " + param);
            }
        });

        mview = (SurfaceView)findViewById(R.id.video_view);
        mview.getHolder().addCallback(
            new Callback() {
                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                }

                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    mplayer.setDisplayWindow(holder.getSurface());
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                    mplayer.setDisplayWindow(null);
                }
            }
        );
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mplayer.close();
    }

    @Override
    public void onResume() {
        super.onResume();
        mplayer.play();
    }

    @Override
    public void onPause() {
        super.onPause();
        mplayer.pause();
    }
}

