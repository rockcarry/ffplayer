package com.rockcarry.ffplayer;

public final class player {
    public static final int RENDER_LETTERBOX = 0;
    public static final int RENDER_STRETCHED = 1;

    public static final int PARAM_VIDEO_WIDTH    = 0;
    public static final int PARAM_VIDEO_HEIGHT   = 1;
    public static final int PARAM_VIDEO_DURATION = 2;
    public static final int PARAM_VIDEO_POSITION = 3;
    public static final int PARAM_RENDER_MODE    = 4;

    public native Object open (String url, Object surface);
    public native void   close(Object player);
    public native void   play (Object player);
    public native void   pause(Object player);
    public native void   seek (Object player, long sec);
    public native void   setparam(Object player, int id, int value);
    public native int    getparam(Object player, int id);
};
