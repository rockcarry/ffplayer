package com.rockcarry.ffplayer;

import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.widget.RelativeLayout;

public final class player
{
    public static final int MSG_OPEN_DONE           = (('O' << 24) | ('P' << 16) | ('E' << 8) | ('N' << 0));
    public static final int MSG_OPEN_FAILED         = (('F' << 24) | ('A' << 16) | ('I' << 8) | ('L' << 0));
    public static final int MSG_PLAY_PROGRESS       = (('R' << 24) | ('U' << 16) | ('N' << 8) | (' ' << 0));
    public static final int MSG_PLAY_COMPLETED      = (('E' << 24) | ('N' << 16) | ('D' << 8) | (' ' << 0));

    public static final int PARAM_MEDIA_DURATION    = 0x1000 + 0;
    public static final int PARAM_MEDIA_POSITION    = 0x1000 + 1;
    public static final int PARAM_VIDEO_WIDTH       = 0x1000 + 2;
    public static final int PARAM_VIDEO_HEIGHT      = 0x1000 + 3;
//  public static final int PARAM_VIDEO_MODE        = 0x1000 + 4;
    public static final int PARAM_AUDIO_VOLUME      = 0x1000 + 5;
    public static final int PARAM_PLAY_SPEED        = 0x1000 + 6;
//  public static final int PARAM_VISUAL_EFFECT     = 0x1000 + 7;
    public static final int PARAM_AVSYNC_TIME_DIFF  = 0x1000 + 8;
//  public static final int PARAM_PLAYER_CALLBACK   = 0x1000 + 9;
    public static final int PARAM_AFILTER_ENABLE    = 0x1000 +10;
    public static final int PARAM_VFILTER_ENABLE    = 0x1000 +11;

    public player() {
    }

    public player(String url, Handler h) {
        mPlayerMsgHandler = h;
        open(url, false);
    }

    protected void finalize() {
        close();
    }

    public boolean open(String url, boolean sync) {
        if (sync) {
            nativeClose(m_hPlayer);
            m_hPlayer = 0;
            m_hPlayer = nativeOpen(m_strUrl, null, 0, 0);
            nativeInitJniObject (m_hPlayer);
            nativeEnableCallback(m_hPlayer, mPlayerMsgHandler != null ? 1 : 0);
            if (mPlayerMsgHandler != null) {
                mPlayerMsgHandler.sendEmptyMessage(m_hPlayer != 0 ? MSG_OPEN_DONE : MSG_OPEN_FAILED);
            }
            return m_hPlayer != 0 ? true : false;
        } else {
            m_strUrl = url;
            m_bClose = false;
            if (mPlayerInitThread == null) {
                mPlayerInitThread = new PlayerInitThread();
                mPlayerInitThread.start();
            }
            synchronized (mPlayerInitEvent) {
                mPlayerInitEvent.notify();
            }
            return true;
        }
    }

    public void close() {
        m_bClose = true;
        synchronized (mPlayerInitEvent) {
            mPlayerInitEvent.notify();
        }
    }

    public void play ()                      { nativePlay (m_hPlayer);     }
    public void pause()                      { nativePause(m_hPlayer);     }
    public void seek (long ms)               { nativeSeek (m_hPlayer, ms); }
    public void setParam(int id, long value) { nativeSetParam(m_hPlayer, id, value); }
    public long getParam(int id)             { return nativeGetParam(m_hPlayer, id); }

    public void setDisplaySurface(Object surface) {
        mSurface = surface;
        mTexture = null;
        nativeSetDisplaySurface(m_hPlayer, surface);
    }

    public void setDisplayTexture(Object texture) {
        mTexture = texture;
        mSurface = null;
        nativeSetDisplayTexture(m_hPlayer, texture);
    }

    public void setPlayerMsgHandler(Handler h) {
        mPlayerMsgHandler = h;
        nativeEnableCallback(m_hPlayer, mPlayerMsgHandler != null ? 1 : 0);
    }

    public boolean initVideoSize(int rw, int rh, View v) {
        int vw = (int)getParam(player.PARAM_VIDEO_WIDTH ); // video width
        int vh = (int)getParam(player.PARAM_VIDEO_HEIGHT); // video height
        if (rw <= 0 || rh <= 0 || vw <= 0 || vh <= 0 || v == null) return false;

        int sw, sh; // scale width & height
        if (rw * vh < vw * rh) {
            sw = rw; sh = sw * vh / vw;
        } else {
            sh = rh; sw = sh * vw / vh;
        }

        RelativeLayout.LayoutParams lp = (RelativeLayout.LayoutParams)v.getLayoutParams();
        lp.width  = sw;
        lp.height = sh;
        v.setLayoutParams(lp);
        return true;
    }

    //++ for player event callback
    private Handler mPlayerMsgHandler = null;
    private void internalPlayerEventCallback(int event, long param) {
        if (mPlayerMsgHandler != null) {
            Message msg = new Message();
            msg.what = event;
            msg.obj  = new Long(param);
            mPlayerMsgHandler.sendMessage(msg);
        }
    }
    private native void nativeInitJniObject(long hplayer);
    //-- for player event callback

    private static native long nativeOpen (String url, Object surface, int w, int h);
    private static native void nativeClose(long hplayer);
    private static native void nativePlay (long hplayer);
    private static native void nativePause(long hplayer);
    private static native void nativeSeek (long hplayer, long ms);
    private static native void nativeSetParam(long hplayer, int id, long value);
    private static native long nativeGetParam(long hplayer, int id);
    private static native void nativeSetDisplaySurface(long hplayer, Object surf);
    private static native void nativeSetDisplayTexture(long hplayer, Object text);
    private static native void nativeEnableCallback   (long hplayer, int enable );

    static {
        System.loadLibrary("ffplayer_jni");
    }

    private volatile boolean m_bClose          = false;
    private volatile long    m_hPlayer         = 0;
    private String           m_strUrl          = "";
    private Object           mPlayerInitEvent  = new Object();
    private PlayerInitThread mPlayerInitThread = null;
    private Object           mSurface          = null;
    private Object           mTexture          = null;
    class PlayerInitThread extends Thread {
        @Override
        public void run() {
            while (true) {
                nativeClose(m_hPlayer);
                m_hPlayer = 0;
                if (m_bClose) break;

                m_hPlayer = nativeOpen(m_strUrl, null, 0, 0);
                nativeInitJniObject (m_hPlayer);
                nativeEnableCallback(m_hPlayer, mPlayerMsgHandler != null ? 1 : 0);
                if (mSurface != null) nativeSetDisplaySurface(m_hPlayer, mSurface);
                if (mTexture != null) nativeSetDisplayTexture(m_hPlayer, mTexture);
                if (mPlayerMsgHandler != null) {
                    mPlayerMsgHandler.sendEmptyMessage(m_hPlayer != 0 ? MSG_OPEN_DONE : MSG_OPEN_FAILED);
                }

                synchronized (mPlayerInitEvent) {
                    try {
                        mPlayerInitEvent.wait();
                    } catch (InterruptedException e) {}
                }
            }
            mPlayerInitThread = null;
        }
    }
};

