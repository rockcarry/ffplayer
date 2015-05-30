package com.rockcarry.ffplayer;

public final class player
{
    public static final int RENDER_LETTERBOX     = 0;
    public static final int RENDER_STRETCHED     = 1;

    public static final int PARAM_VIDEO_WIDTH    = 0;
    public static final int PARAM_VIDEO_HEIGHT   = 1;
    public static final int PARAM_VIDEO_DURATION = 2;
    public static final int PARAM_VIDEO_POSITION = 3;
    public static final int PARAM_RENDER_MODE    = 4;

    private int m_hPlayer;
    public boolean open(String url, Object surface)
    {
        m_hPlayer = nativeOpen(url, surface);
        return (m_hPlayer != 0);
    }

    public void close()                     { nativeClose(m_hPlayer);      }
    public void play ()                     { nativePlay (m_hPlayer);      }
    public void pause()                     { nativePause(m_hPlayer);      }
    public void seek (int sec)              { nativeSeek (m_hPlayer, sec); }
    public void setParam(int id, int value) { nativeSetParam(m_hPlayer, id, value); }
    public int  getParam(int id)            { return nativeGetParam(m_hPlayer, id); }

    private static native int  nativeOpen (String url, Object surface);
    private static native void nativeClose(int hplayer);
    private static native void nativePlay (int hplayer);
    private static native void nativePause(int hplayer);
    private static native void nativeSeek (int hplayer, int sec);
    private static native void nativeSetParam(int hplayer, int id, int value);
    private static native int  nativeGetParam(int hplayer, int id);

    static
    {
        System.loadLibrary("ffplayer_jni");
    }
};
