package com.rockcarry.ffplayer;

public final class player
{
    public static final int VIDEO_MODE_LETTERBOX        = 0;
    public static final int VIDEO_MODE_STRETCHED        = 1;

    public static final int EVENT_PLAY_PROGRESS         = 0;
    public static final int EVENT_PLAY_COMPLETED        = 1;

    public static final int PARAM_MEDIA_DURATION        = 0x1000 + 0;
    public static final int PARAM_MEDIA_POSITION        = 0x1000 + 1;
    public static final int PARAM_VIDEO_WIDTH           = 0x1000 + 2;
    public static final int PARAM_VIDEO_HEIGHT          = 0x1000 + 3;
    public static final int PARAM_VIDEO_MODE            = 0x1000 + 4;
    public static final int PARAM_AUDIO_VOLUME          = 0x1000 + 5;
    public static final int PARAM_PLAY_SPEED            = 0x1000 + 6;
    public static final int PARAM_DECODE_THREAD_COUNT   = 0x1000 + 7;
//  public static final int PARAM_VISUAL_EFFECT         = 0x1000 + 8;
    public static final int PARAM_AVSYNC_TIME_DIFF      = 0x1000 + 9;
//  public static final int PARAM_PLAYER_CALLBACK       = 0x1000 +10;
    public static final int PARAM_AUDIO_STREAM_TOTAL    = 0x1000 +11;
    public static final int PARAM_VIDEO_STREAM_TOTAL    = 0x1000 +12;
    public static final int PARAM_SUBTITLE_STREAM_TOTAL = 0x1000 +13;
    public static final int PARAM_AUDIO_STREAM_CUR      = 0x1000 +14;
    public static final int PARAM_VIDEO_STREAM_CUR      = 0x1000 +15;
    public static final int PARAM_SUBTITLE_STREAM_CUR   = 0x1000 +16;

    private int m_hPlayer = 0;
    public boolean open(String url, Object surface, int w, int h) {
        m_hPlayer = nativeOpen(url, surface, w, h);
        return (m_hPlayer != 0);
    }

    public void close()                     { nativeClose(m_hPlayer);      }
    public void play ()                     { nativePlay (m_hPlayer);      }
    public void pause()                     { nativePause(m_hPlayer);      }
    public void seek (int sec)              { nativeSeek (m_hPlayer, sec); }
    public void setParam(int id, int value) { nativeSetParam(m_hPlayer, id, value); }
    public int  getParam(int id)            { return nativeGetParam(m_hPlayer, id); }

    private static native int  nativeOpen (String url, Object surface, int w, int h);
    private static native void nativeClose(int hplayer);
    private static native void nativePlay (int hplayer);
    private static native void nativePause(int hplayer);
    private static native void nativeSeek (int hplayer, int sec);
    private static native void nativeSetParam(int hplayer, int id, int value);
    private static native int  nativeGetParam(int hplayer, int id);

    static {
        System.loadLibrary("ffplayer_jni");
    }
};
