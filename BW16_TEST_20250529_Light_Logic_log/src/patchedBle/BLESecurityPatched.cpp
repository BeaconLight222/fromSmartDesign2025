#include "BLESecurityPatched.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "gap.h"
#include "gap_bond_le.h"

#ifdef __cplusplus
}
#endif


void (*BLESecurityPatched::_pKeyDisplayCB)(uint8_t conn_id, uint32_t passkey) = nullptr;
uint32_t (*BLESecurityPatched::_pKeyInputCB)(uint8_t conn_id) = nullptr;
bool (*BLESecurityPatched::_pNumCompareCB)(uint8_t conn_id, uint32_t passkey) = nullptr;

BLESecurityPatched::BLESecurityPatched() {
}

void BLESecurityPatched::setPairable(bool pairMode) {
    if (pairMode) {
    _authPairMode = GAP_PAIRING_MODE_PAIRABLE;
    } else {
    _authPairMode = GAP_PAIRING_MODE_NO_PAIRING;
    }
}

void BLESecurityPatched::setAuthFlags(uint16_t authFlags) {
    _authFlags = authFlags;
}

void BLESecurityPatched::setIOCapability(uint8_t ioCap) {
    _authIoCap = ioCap;
}

void BLESecurityPatched::setSecReqEnable(bool secReq) {
    if (secReq) {
    _authSecReqEnable = true;
    } else {
    _authSecReqEnable = false;
    }
}

void BLESecurityPatched::setSecReqFlags(uint16_t secReqFlags) {
    _authSecReqFlags = secReqFlags;
}

void BLESecurityPatched::setStaticPin(uint32_t pin) {
    // Maximum pin length of 6 digits
    if (pin <= GAP_PASSCODE_MAX) {
        _authPairMode = GAP_PAIRING_MODE_PAIRABLE;
        _authFlags = GAP_AUTHEN_BIT_BONDING_FLAG | GAP_AUTHEN_BIT_MITM_FLAG | GAP_AUTHEN_BIT_SC_FLAG;
        _authIoCap = GAP_IO_CAP_DISPLAY_ONLY;
        _authUseFixPasskey = true;
        _authFixPasskey = pin;
        _authSecReqEnable = true;
        _authSecReqFlags = GAP_AUTHEN_BIT_BONDING_FLAG | GAP_AUTHEN_BIT_MITM_FLAG | GAP_AUTHEN_BIT_SC_FLAG;
    } else {
        printf("BLE Static Pin is limited to numbers up to 999999\r\n");
    }
}

void BLESecurityPatched::setPasskeyDisplayCallback(void (*fCallback) (uint8_t conn_id, uint32_t passkey)) {
    _pKeyDisplayCB = fCallback;
}

void BLESecurityPatched::setPasskeyInputCallback(uint32_t (*fCallback) (uint8_t conn_id)) {
    _pKeyInputCB = fCallback;
}

void BLESecurityPatched::setNumericComparisonCallback(bool (*fCallback) (uint8_t conn_id, uint32_t passkey)) {
    _pNumCompareCB = fCallback;
}

void BLESecurityPatched::setupGAPBondManager() {
    // Setup the GAP Bond Manager
    gap_set_param(GAP_PARAM_BOND_PAIRING_MODE, sizeof(_authPairMode), &_authPairMode);
    gap_set_param(GAP_PARAM_BOND_AUTHEN_REQUIREMENTS_FLAGS, sizeof(_authFlags), &_authFlags);
    gap_set_param(GAP_PARAM_BOND_IO_CAPABILITIES, sizeof(_authIoCap), &_authIoCap);
    gap_set_param(GAP_PARAM_BOND_OOB_ENABLED, sizeof(_authOob), &_authOob);
    le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY, sizeof(_authFixPasskey), &_authFixPasskey);
    le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY_ENABLE, sizeof(_authUseFixPasskey), &_authUseFixPasskey);
    le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_ENABLE, sizeof(_authSecReqEnable), &_authSecReqEnable);
    le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_REQUIREMENT, sizeof(_authSecReqFlags), &_authSecReqFlags);
}

