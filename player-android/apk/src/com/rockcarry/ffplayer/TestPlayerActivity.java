package com.rockcarry.ffplayer;

import android.app.Activity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
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
        mview   = (SurfaceView)findViewById(R.id.video_view);
        SurfaceHolder holder = mview.getHolder();
        holder.addCallback(
            new Callback() {
                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                }

                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    mplayer.open("/sdcard/test.mp4", holder.getSurface(), mview.getWidth(), mview.getHeight());
                    mplayer.play();
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                    mplayer.close();
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
    }

    @Override
    public void onPause() {
        super.onPause();
    }
}

