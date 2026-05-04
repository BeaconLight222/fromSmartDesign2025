#ifndef _BLE_ADDR_PATCHED_H_
#define _BLE_ADDR_PATCHED_H_

#include <Arduino.h>

class BLEAddrPatched {
    public:
        BLEAddrPatched();
        BLEAddrPatched(uint8_t (&addr)[6]);
        BLEAddrPatched(const char * str);      // Build a BLEAddrPatched object from an address string. Use of colons (:) to separate address bytes in the string is acceptable.
        const char* str();
        uint8_t* data();
        bool operator ==(const BLEAddrPatched &addr);

    private:
        friend class BLEDevicePatched;
        uint8_t _btAddr[6] = {0};   // BT address is stored MSB at _btAddr[5]
        char _str[20] = {0};
};

#endif
