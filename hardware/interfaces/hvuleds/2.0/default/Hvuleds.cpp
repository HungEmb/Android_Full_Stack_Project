#include "Hvuleds.h"

namespace android {
namespace hardware {
namespace hvuleds {
namespace V2_0 {
namespace implementation {

// Methods from ::android::hardware::hvuleds::V2_0::IHvuleds follow.
Return<::android::hardware::hvuleds::V2_0::Status> Hvuleds::setLeds(::android::hardware::hvuleds::V2_0::Led led, int32_t intensity) {
    // TODO implement
    return ::android::hardware::hvuleds::V2_0::Status {};
}


// Methods from ::android::hidl::base::V1_0::IBase follow.

//IHvuleds* HIDL_FETCH_IHvuleds(const char* /* name */) {
    //return new Hvuleds();
//}
//
}  // namespace implementation
}  // namespace V2_0
}  // namespace hvuleds
}  // namespace hardware
}  // namespace android
