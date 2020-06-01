/**
 * Written by HungVu<hungvu98.hust@gmail.com> in May, 2020
 */

package android.os;

import android.annotation.IntDef;
import android.annotation.RequiresPermission;
import android.annotation.SystemService;
import android.app.ActivityThread;
import android.content.Context;
import android.media.AudioAttributes;
import android.util.Log;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import android.hardware.hvuleds.V2_0.Led;


@SystemService(Context.HVULEDS_SERVICE)
public abstract class HvuledsManager {
	private static final String TAG = "hvuleds";

	public static final int RGB_INDICATOR_LED_RED = Led.RED;
	public static final int RGB_INDICATOR_LED_GREEN = Led.GREEN;
	public static final int RGB_INDICATOR_LED_BLUE = Led.BLUE;

	public HvuledsManager() {
	}

	public abstract void setLedBrightness(int ledID, int intensity);
	public abstract void turnLedStatus(int ledID);


}
