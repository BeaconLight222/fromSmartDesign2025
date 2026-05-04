#ifndef _BLE_REMOTESERVICE_PATCHED_H_
#define _BLE_REMOTESERVICE_PATCHED_H_

#include <Arduino.h>
#include "BLEUUIDPatched.h"
#include "BLERemoteCharacteristicPatched.h"

#define MAX_NUM_CHARS 15

#ifdef __cplusplus
extern "C" {
#endif

#include "profile_client.h"

#ifdef __cplusplus
}
#endif

// Forward declaration to avoid include loops
class BLEClientPatched;

class BLERemoteService {
    public:
        BLEUUIDPatched getUUID();

        BLERemoteCharacteristic* getCharacteristic(const char* uuid);
        BLERemoteCharacteristic* getCharacteristic(BLEUUIDPatched uuid);


    private:
        BLERemoteService(BLEUUIDPatched uuid);
        ~BLERemoteService();

        bool addCharacteristic(BLERemoteCharacteristic* newChar);

        void clientReadResultCallbackDefault(uint8_t conn_id, uint16_t cause, uint16_t handle, uint16_t value_size, uint8_t *p_value);
        void clientWriteResultCallbackDefault(uint8_t conn_id, T_GATT_WRITE_TYPE type, uint16_t handle, uint16_t cause, uint8_t credits);
        T_APP_RESULT clientNotifyIndicateCallbackDefault(uint8_t conn_id, bool notify, uint16_t handle, uint16_t value_size, uint8_t *p_value);

        friend class BLEClientPatched;

        BLEUUIDPatched _uuid;
        uint16_t _handleStart = 0;
        uint16_t _handleEnd = 0;

        BLEClientPatched* _pClient = nullptr;
        BLERemoteCharacteristic* _characteristicPtrList[MAX_NUM_CHARS] = {0};
        uint8_t _characteristicCount = 0;
};

#endif
