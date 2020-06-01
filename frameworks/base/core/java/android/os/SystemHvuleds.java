/**
 * Written by HungVu<hungvu98.hust@gmail.com> in May, 2020
 */

package android.os;

import android.content.Context;
import android.util.Log;


public class SystemHvuleds extends HvuledsManager {
	private static final String TAG = "hvuleds";
	private IHvuledsService mService;

	private static final boolean ON = true;
	private static final boolean OFF = false;

	private static int[] rgbIntensity = {0, 0, 0};
	private static boolean[] rgbStatus = {OFF, OFF, OFF};

	public SystemHvuleds() {
		mService = IHvuledsService.Stub.asInterface(ServiceManager.getService("hvuleds"));
	}

	public SystemHvuleds(Context context) {
		mService = IHvuledsService.Stub.asInterface(ServiceManager.getService("hvuleds"));
	}

	@Override
	public void setLedBrightness(int ledID, int intensity) {
		if(mService == null) {
			Log.w(TAG, "Failed to set brightness of led, no hvuleds service");
			return;
		}
		try {
			rgbIntensity[ledID] = intensity;
			if(intensity == 0) {
				rgbStatus[ledID] = OFF;
			} else {
				rgbStatus[ledID] = ON;
			}
			mService.setLed(ledID, intensity);
			Log.d(TAG, "Adjusting brightness of led");
		} catch (RemoteException e) {
			Log.w(TAG, "Failed to set brightness of led. ", e);
		}
	}

	@Override
	public void turnLedStatus(int ledID) {
		if(mService == null) {
			Log.w(TAG, "Failed to turn status of led, no hvuleds service");
			return;
		}
		try {
			if(rgbStatus[ledID] == ON) {
				mService.setLed(ledID, 0);
				rgbStatus[ledID] = OFF;
			} else if(rgbStatus[ledID] == OFF) {
				mService.setLed(ledID, rgbIntensity[ledID]);
				rgbStatus[ledID] = OFF;
			}
			Log.d(TAG, "Switching status of led");
		} catch (RemoteException e) {
			Log.w(TAG, "Failed to turn status of led. ", e);
		}
	}
}