package com.rockcarry.ffplayer;

public final class player {
    public static final int RENDER_LETTERBOX = 0;
    public static final int RENDER_STRETCHED = 1;

    public static final int PARAM_VIDEO_WIDTH    = 0;
    public static final int PARAM_VIDEO_HEIGHT   = 1;
    public static final int PARAM_VIDEO_DURATION = 2;
    public static final int PARAM_VIDEO_POSITION = 3;
    public static final int PARAM_RENDER_MODE    = 4;

    public Object context;
    public native boolean open(String url, Object surface);
    public native void close();
    public native void play ();
    public native void pause();
    public native void seek (long sec);
    public native void setParam(int id, int value);
    public native int  getParam(int id);

    static {
        System.loadLibrary("ffplayer_jni");
    }
};
