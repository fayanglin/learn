package com.iflytek.halcae;


import android.util.Log;

public class JniHalCae {

    private static final String TAG = "JniHalCae";
    
    static {
        System.loadLibrary("ShareHost");
    }

    public native int shareReadInit();

    public native int shareReadData(byte[] data,int dataLen);

    public native int shareReadClear();

}
