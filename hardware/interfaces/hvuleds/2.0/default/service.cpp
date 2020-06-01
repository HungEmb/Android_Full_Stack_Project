/*
 * Written by HungVu<hungvu98.hust@gmail.com> in May, 2020
 */

#define LOG_TAG "android.hardware.hvuleds@2.0-service"

#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <android-base/logging.h>
#include <android/hardware/hvuleds/2.0/IHvuleds.h>
#include "Hvuleds.h"

using android::hardware::hvuleds::V2_0::IHvuleds;
using android::hardware::hvuleds::V2_0::implementation::Hvuleds;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::sp;
using android::NO_ERROR;

int main() {
    LOG(INFO) << __func__ << " : Start HAL";
    android::sp<IHvuleds> hvuleds = Hvuleds::getInstance();

    configureRpcThreadpool(1, true /*callerWillJoin*/);

    if (hvuleds != nullptr) {
        auto rc = hvuleds->registerAsService();
        if (rc != NO_ERROR) {
            LOG(ERROR) << "Cannot start Hvuleds service: " << rc;
            return rc;
        }
    } else {
        LOG(ERROR) << "Can't create instance of Hvuleds, nullptr";
    }

    joinRpcThreadpool();

    return 0; // should never get here
}
