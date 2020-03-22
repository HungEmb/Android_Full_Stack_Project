/*
 * Written by HungVu<hungvu98.hust@gmail.com> in March, 2020
 */

package android.os;

import android.os.IHvuledsService;

public class HvuledsManager
{

	public void setLed(int led, int intensity) {
		try {
			mService.setLed(led, intensity);
		} catch (RemoteException e) {
		}
	}

	public HvuledsManager(IHvuledsService service) {
        mService = service;
    }

    IHvuledsService mService;
}
