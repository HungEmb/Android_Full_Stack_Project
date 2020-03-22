#define LOG_TAG "HvuledsService"

#include "jni.h"
#include <nativehelper/JNIHelp.h>
#include "android_runtime/AndroidRuntime.h"

#include <android/hardware/hvuleds/2.0/IHvuleds.h>
#include <android/hardware/hvuleds/2.0/types.h>
#include <android-base/chrono_utils.h>
#include <utils/misc.h>
#include <utils/Log.h>
#include <map>
#include <stdio.h>


namespace android {

using IHvuleds   = ::android::hardware::hvuleds::V2_0::IHvuleds;
using Status     = ::android::hardware::hvuleds::V2_0::Status;
using Led        = ::android::hardware::hvuleds::V2_0::Led;
template<typename T>
using Return     = ::android::hardware::Return<T>;

class HvuledsHal {
private:
	static sp<IHvuleds> sHvuleds;
	static bool sHvuledsInit;

	HvuledsHal() {}

public:
	static void disassociate() {
		sHvuledsInit = false;
		sHvuleds = nullptr;
	}

	static sp<IHvuleds> associate() {
		if ((sHvuleds == nullptr && !sHvuledsInit) || 
			(sHvuleds != nullptr && !sHvuleds->ping().isOk())) {
			// will return the hal if it exists the first time.
			sHvuleds = IHvuleds::getService();
			sHvuledsInit = true;

			if (sHvuleds == nullptr) {
				ALOGE("Unable to get IHvuleds interface.");
			}
		}

		return sHvuleds;
	}
};

sp<IHvuleds> HvuledsHal::sHvuleds = nullptr;
bool HvuledsHal::sHvuledsInit = false;

static bool validate(jint led) {
    bool valid = true;

    if (led < 0 || led >= static_cast<jint>(Led::UNKNOWN)) {
        ALOGE("Invalid led parameter %d.", led);
        valid = false;
    }
    return valid;
}

static void processReturn(
        const Return<Status> &ret,
        Led led,
        const int intensity) {
    if (!ret.isOk()) {
        ALOGE("Failed to issue set led command.");
        HvuledsHal::disassociate();
        return;
    }

    switch (static_cast<Status>(ret)) {
        case Status::SUCCESS:
            break;
        case Status::LED_NOT_SUPPORTED:
            ALOGE("Led requested not available on this device. %d", led);
            break;
        case Status::BRIGHTNESS_NOT_SUPPORTED:
            ALOGE("intensity parameter of Led not supported on this device: %d",
                intensity);
            break;
        case Status::UNKNOWN:
        default:
            ALOGE("Unknown error setting led.");
    }
}

static void setLed_native(
	JNIEnv* /* env */,
	jobject /* clazz */,
	jint whichLed,
	jint intensity) {

	if (!validate(whichLed))
		return;

	sp<IHvuleds> hal = HvuledsHal::associate();

	if (hal == nullptr)
		return;

	Led led = static_cast<Led>(whichLed);

	{
        android::base::Timer t;
        Return<Status> ret = hal->setLeds(led, intensity);
        processReturn(ret, led, intensity);
        if (t.duration() > 50ms) ALOGD("Excessive delay setting led");
    }
};

static const JNINativeMethod method_table[] = {
    { "setLed_native", "(II)V", (void*)setLed_native },
};

int register_android_server_HvuledsService(JNIEnv *env) {
    return jniRegisterNativeMethods(env, "com/android/server/HvuledsService",
            method_table, NELEM(method_table));
}

};
