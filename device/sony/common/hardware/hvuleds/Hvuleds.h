/*
 * Written by HungVu<hungvu98.hust@gmail.com> in March, 2020
 */

#ifndef ANDROID_HARDWARE_HVULEDS_V2_0_HVULEDS_H
#define ANDROID_HARDWARE_HVULEDS_V2_0_HVULEDS_H

#include <android/hardware/hvuleds/2.0/IHvuleds.h>
#include <hardware/hardware.h>
#include <hidl/MQDescriptor.h>
#include <cutils/properties.h>
#include <hidl/Status.h>
#include <map>


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <linux/msm_mdp.h>

#include <sys/ioctl.h>
#include <sys/types.h>


struct hvuleds_t {
	pthread_mutex_t g_lock;
};

const static std::string RED_LED_BASE
        = "/sys/class/leds/led:rgb_red/";

const static std::string GREEN_LED_BASE
        = "/sys/class/leds/led:rgb_green/";

const static std::string BLUE_LED_BASE
        = "/sys/class/leds/led:rgb_blue/";

const static std::string RED_LED_FILE
        = RED_LED_BASE + "brightness";

const static std::string GREEN_LED_FILE
        = GREEN_LED_BASE + "brightness";

const static std::string BLUE_LED_FILE
        = BLUE_LED_BASE + "brightness";

const static std::string RED_BLINK_FILE
        = RED_LED_BASE + "blink";

const static std::string GREEN_BLINK_FILE
        = GREEN_LED_BASE + "blink";

const static std::string BLUE_BLINK_FILE
        = BLUE_LED_BASE + "blink";

namespace android {
	namespace hardware {
		namespace hvuleds {
			namespace V2_0 {
				namespace implementation {
					using::android::hardware::hvuleds::V2_0::IHvuleds;
                    using::android::hardware::hvuleds::V2_0::Status;
                    using::android::hardware::hvuleds::V2_0::Led;
                    using::android::hardware::Return;
                    using::android::hardware::Void;
                    using::android::hardware::hidl_vec;
                    using::android::hardware::hidl_string;
                    using::android::sp;

					struct Hvuleds : public IHvuleds {
						public:
							Hvuleds();
							static IHvuleds *getInstance();
							// Methods from ::android::hardware::hvuleds::V2_0::IHvuleds follow.
							Return<Status> setLeds(Led led, int32_t intensity) override;

						private:
							static Hvuleds *sInstance;
							hvuleds_t *mDevice;
							int setBrightnessLeds(const std::string &path, int intensity);
							static int writeInt(const std::string &path, int value);
                        	static int readInt(const std::string &path);
                        	void openHal();
					};
				} // namespace implementation
			} // namespace V2_0
		} // namespace hvuleds
	} // namespace hardware
} //namespace android

#endif // ANDROID_HARDWARE_HVULEDS_V2_0_HVULEDS


							
