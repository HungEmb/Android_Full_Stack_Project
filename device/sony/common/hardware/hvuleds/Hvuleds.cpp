/*
 * Copyright (C) 2016 The Android Open Source Project
 * Copyright (C) 2018 Shane Francis
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Hvuleds.sony"

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
