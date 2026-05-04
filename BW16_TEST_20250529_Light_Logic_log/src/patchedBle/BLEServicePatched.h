#ifndef _BLE_SERVICE_PATCHED_H_
#define _BLE_SERVICE_PATCHED_H_

#include <Arduino.h>
#include "BLEUUIDPatched.h"
#include "BLECharacteristicPatched.h"

#define MAX_NUM_CHARS 15

#ifdef __cplusplus
extern "C" {
#endif

#include "gatt.h"

#ifdef __cplusplus
}
#endif

// Bluetooth SIG defined services and profiles can be found at:
// https://www.bluetooth.com/specifications/gatt/
// https://www.bluetooth.com/specifications/gatt/services/

class BLEServicePatched {
    public:
        BLEServicePatched(BLEUUIDPatched uuid);
        BLEServicePatched(const char* uuid);
        ~BLEServicePatched();

        void setUUID(BLEUUIDPatched uuid);
        BLEUUIDPatched getUUID();

        void addCharacteristic(BLECharacteristicPatched& newChar);
        BLECharacteristicPatched* getCharacteristic(uint8_t charIndex);


    private:
        void setServiceID(T_SERVER_ID service_id);
        T_SERVER_ID getServiceID();

        uint8_t getAttrCount();
        T_ATTRIB_APPL* generateServiceAttrTable();
        uint8_t generateAttrServiceDeclaration(T_ATTRIB_APPL* attr, uint8_t index);
        void printAttr();

        T_APP_RESULT serviceAttrReadCallbackDefault(uint8_t conn_id, T_SERVER_ID service_id, uint16_t attrib_index,
                                                                    uint16_t offset, uint16_t *p_length, uint8_t **pp_value);
        T_APP_RESULT serviceAttrWriteCallbackDefault(uint8_t conn_id, T_SERVER_ID service_id, uint16_t attrib_index,
                                                                    T_WRITE_TYPE write_type, uint16_t length, uint8_t *p_value,
                                                                    P_FUN_WRITE_IND_POST_PROC *p_write_ind_post_proc);
        void serviceCccdUpdateCallbackDefault(uint8_t conn_id, T_SERVER_ID service_id, uint16_t attrib_index, uint16_t ccc_bits);


        friend class BLEDevicePatched;
        friend class BLECharacteristicPatched;

        BLEUUIDPatched _uuid;
        T_SERVER_ID _service_id = 0xff;

        BLECharacteristicPatched* _characteristicPtrList[MAX_NUM_CHARS] = {0};
        uint8_t _characteristicCount = 0;

        T_ATTRIB_APPL* _service_attr_tbl = NULL;
        uint8_t _total_attr_count = 0;
};

#endif
