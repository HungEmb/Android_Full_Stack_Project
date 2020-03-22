/*
 * Written by HungVu<hungvu98.hust@gmail.com> in March, 2020
 */

package com.android.server;

import android.os.IHvuledsService;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Process;
import android.util.Log;

public class HvuledsService extends IHvuledsService.Stub {
	private static final String TAG = "HvuledsService";
	private Context mContext;

	public HvuledsService(Context context) {
		super();
		mContext = context;
		Log.i(TAG, "Hvuleds Service started");
	}

	@Override
    public void setLed(int led, int intensity) {
    	setLed_native(led, intensity);
    }

	static native void setLed_native(int led, int intensity);
}