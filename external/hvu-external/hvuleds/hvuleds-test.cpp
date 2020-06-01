#include <android/hardware/hvuleds/2.0/IHvuleds.h>
#include <hidl/Status.h>
#include <hidl/LegacySupport.h>
#include <utils/misc.h>
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hidl/HidlSupport.h>

#include<stdio.h>

using android::hardware::hvuleds::V2_0::IHvuleds;
using Led       = ::android::hardware::hvuleds::V2_0::Led;
using Status    = ::android::hardware::hvuleds::V2_0::Status;
using android::sp;  


int main(int argc, char *argv[]) {
      Status res;
      Led led;
      //int32_t brightness;
      android::sp<IHvuleds> ser = IHvuleds::getService();
      
      if(strcmp(argv[1], "RED") == 0) 
            led = Led::RED;
      else if (strcmp(argv[1], "GREEN") == 0)
            led = Led::GREEN;
      else if (strcmp(argv[1], "BLUE") == 0)
            led = Led::BLUE;
      else {
            led = Led::UNKNOWN;
            printf("Led is invaled\n");
      }
      
      // brightness = ser->getLed(led);
      // printf("Brightness: %d\n", brightness);

      res = ser->setLeds(led, 0);
      printf("Status = %d\n",res);
      return 0;
}
