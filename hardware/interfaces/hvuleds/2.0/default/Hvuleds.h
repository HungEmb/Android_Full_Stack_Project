#ifndef ANDROID_HARDWARE_HVULEDS_V2_0_HVULEDS_H
#define ANDROID_HARDWARE_HVULEDS_V2_0_HVULEDS_H

#include <android/hardware/hvuleds/2.0/IHvuleds.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace hvuleds {
namespace V2_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct Hvuleds : public IHvuleds {
    // Methods from ::android::hardware::hvuleds::V2_0::IHvuleds follow.
    Return<::android::hardware::hvuleds::V2_0::Status> setLeds(::android::hardware::hvuleds::V2_0::Led led, int32_t intensity) override;

    // Methods from ::android::hidl::base::V1_0::IBase follow.
    static IHvuleds* getInstance(void);
};

// FIXME: most likely delete, this is only for passthrough implementations
// extern "C" IHvuleds* HIDL_FETCH_IHvuleds(const char* name);

}  // namespace implementation
}  // namespace V2_0
}  // namespace hvuleds
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_HVULEDS_V2_0_HVULEDS_H
