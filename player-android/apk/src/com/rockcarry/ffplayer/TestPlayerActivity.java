package com.rockcarry.ffplayer;

import android.app.Activity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.Surface;
import android.view.SurfaceView;

public class TestPlayerActivity extends Activity {
    private player      mplayer = null;
    private SurfaceView mview   = null;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        mplayer = new player();
        mplayer.open("/sdcard/test.mp4");
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

