/*
 * Written by HungVu<hungvu98.hust@gmail.com> in May, 2020
 */

#define LOG_TAG "Hvuleds"

#include <android-base/logging.h>
#include <fstream>
#include "Hvuleds.h"

#ifdef UCOMMSVR_BACKLIGHT
extern "C" {
#include <comm_server/ucomm_ext.h>
}
#endif

namespace android {
	namespace hardware {
		namespace hvuleds {
			namespace V2_0 {
				namespace implementation {

					Hvuleds *Hvuleds::sInstance = nullptr;

					Hvuleds::Hvuleds() {
						LOG(INFO) << "Hvuleds";
						openHal();
						sInstance = this;
					}

					void Hvuleds::openHal() {
						LOG(INFO) << __func__ << ": Setup HAL";
                        mDevice = static_cast<hvuleds_t *>(malloc(sizeof(hvuleds_t)));
                        memset(mDevice, 0, sizeof(hvuleds_t));

                        mDevice->g_lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
					}

					Return<Status> Hvuleds::setLeds(Led led, int32_t intensity) {
                        const static std::string *CONTROLLED_LED_FILE;
                        switch (led) {
                            case Led::RED:
                                LOG(DEBUG) << __func__ << " : Led::RED"; 
                                CONTROLLED_LED_FILE = &RED_LED_FILE;
                                break;
                            case Led::GREEN:
                                LOG(DEBUG) << __func__ << " : Led::GREEN";
                                CONTROLLED_LED_FILE = &GREEN_LED_FILE;
                                break;
                            case Led::BLUE:
                                LOG(DEBUG) << __func__ << " : Led::BLUE";
                                CONTROLLED_LED_FILE = &BLUE_LED_FILE;
                                break;
                            default:
                                LOG(DEBUG) << __func__ << " : Unknown led";
                                return Status::LED_NOT_SUPPORTED;
                        }
                        setBrightnessLeds(*CONTROLLED_LED_FILE, intensity);
                        return Status::SUCCESS;
					}

					IHvuleds *Hvuleds::getInstance(void) {
                        if (!sInstance) {
                            sInstance = new Hvuleds();
                        }
                        return sInstance;
                    }

                    int Hvuleds::writeInt(const std::string &path, int value) {
                        std::ofstream stream(path);

                        if (!stream) {
                            LOG(ERROR) << "Failed to open " << path << ", error=" << errno
                                       << "(" << strerror(errno) << ")";
                            return -errno;
                        }

                        stream << value << std::endl;

                        return 0;
                    }

                    int Hvuleds::readInt(const std::string &path) {
                        std::ifstream stream(path);
                        int value = 0;

                        if (!stream) {
                            LOG(ERROR) << "Failed to open " << path << ", error=" << errno
                                       << "(" << strerror(errno) << ")";
                            return -errno;
                        }

                        stream >> value;

                        return value;
                    }

                    int Hvuleds::setBrightnessLeds(const std::string &path, int intensity) {
                        if(!mDevice) {
                            return -1;
                        }

                        pthread_mutex_lock(&mDevice->g_lock);
                        writeInt(path, intensity);
                        pthread_mutex_unlock(&mDevice->g_lock);
                        return 0;
                    }
                } //namespace implementation
            } // namespace V2_0
        } // namespace hvuleds
    } // namespace hardware
} //namespace android
