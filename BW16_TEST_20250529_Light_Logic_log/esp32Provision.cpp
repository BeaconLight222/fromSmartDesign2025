#include "esp32Provision.h"
#include "NonvolatileDataManager.h"

ESP32Provision *esp32ProvisionObjPtr;



static rtw_result_t wifidrv_scan_result_handler(rtw_scan_handler_result_t *malloced_scan_result) {
  rtw_scan_result_t *record;
  // Serial.print("Scan result ssid ");
  // Serial.println((char *)malloced_scan_result->ap_details.SSID.val);

  esp32ProvisionObjPtr->scanResultTime = millis();

  if (malloced_scan_result->scan_complete != RTW_TRUE) {
    record = &malloced_scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0; /* Ensure the SSID is null terminated */

    if (esp32ProvisionObjPtr->_networkCount < MY_SCAN_LIMIT) {
      // skip the no ssid network
      char *ssidPtr = (char *)record->SSID.val;
      if (strlen(ssidPtr) == 0){
        return RTW_SUCCESS;
      }
      // the same SSID may duplicate
      int copyOfNetworkCount = esp32ProvisionObjPtr->_networkCount;
      strcpy(esp32ProvisionObjPtr->_networkSsid[copyOfNetworkCount], ssidPtr);
      esp32ProvisionObjPtr->_networkRssi[copyOfNetworkCount] = record->signal_strength;
      esp32ProvisionObjPtr->_networkEncr[copyOfNetworkCount] = record->security;
      esp32ProvisionObjPtr->_networkChannel[copyOfNetworkCount] = record->channel;
      esp32ProvisionObjPtr->_networkBand[copyOfNetworkCount] = record->band;
      memcpy(esp32ProvisionObjPtr->_networkMac[copyOfNetworkCount], record->BSSID.octet, 6);
      esp32ProvisionObjPtr->_networkCount++;
    }
  }
  return RTW_SUCCESS;
}


void readCB(BLECharacteristicPatched* chr, uint8_t connID) {
  Serial.print("Characteristic ");
  Serial.print(chr->getUUID().str());
  Serial.print(" read by connection ");
  Serial.println(connID);
  if (chr == &(esp32ProvisionObjPtr->provisioningConfigCharacteristic)) {
    if (esp32ProvisionObjPtr->wifiConnectedMessageSent == 1){
      esp32ProvisionObjPtr->wifiConnectedMessageSent = 2;
      Serial.println("WiFi connected message sent to BLE");
    }
  }
}

void writeCB(BLECharacteristicPatched* chr, uint8_t connID) {
  if (chr == &(esp32ProvisionObjPtr->provisioningProtoVerCharacteristic)) {
    if (chr->getDataLen() > 0) {
      // does not care what the data is, just send back the result
      Serial.print("proto ver written: ");
      Serial.println(chr->readString());
      //use security 0 to bypass signature handshaking
      String versionInfo = "{\"prov\": {\"ver\": \"netprov-v1.2\",\"sec_ver\": 0,\"cap\": [\"wifi_prov\", \"wifi_scan\"]}}";
      chr->writeString(versionInfo);
    }
  } else if (chr == &(esp32ProvisionObjPtr->provisioningSessionCharacteristic)) {
    int dataLen = chr->getDataLen();
    if (dataLen > 0) {
      uint8_t dataBuffer[dataLen];
      chr->getData(dataBuffer, dataLen);
      //check if we get 52 03 A2 01 00, Sec0Payload, S0SessionCmd
      if (dataLen == 5 && dataBuffer[0] == 0x52 && dataBuffer[1] == 0x03 && dataBuffer[2] == 0xA2 && dataBuffer[3] == 0x01 && dataBuffer[4] == 0x00) {
        Serial.println("Received 5203A20100 for session");
        // Send back a response with the 52 05 08 01 AA 01 00, Sec0Payload, S0_Session_Response 1
        uint8_t responseData[] = { 0x52, 0x05, 0x08, 0x01, 0xAA, 0x01, 0x00 };
        chr->setData(responseData, sizeof(responseData));
      } else {
        Serial.print("session data written: ");
        Serial.print("Data length: ");
        Serial.println(dataLen);
        Serial.print("Data: ");
        for (int i = 0; i < dataLen; i++) {
          if (dataBuffer[i] < 0x10) {
            Serial.print("0");  // Print leading zero for single digit hex values
          }
          Serial.print(dataBuffer[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
      }
    }
  } else if (chr == &(esp32ProvisionObjPtr->provisioningScanCharacteristic)) {
    int dataLen = chr->getDataLen();
    uint8_t dataBuffer[dataLen];
    chr->getData(dataBuffer, dataLen);
    if (dataLen > 0) {
      uint8_t firstFieldNumber = dataBuffer[0] >> 3;
      if (firstFieldNumber == 10) {  // CmdScanWifiStart
        Serial.println("Received scan start cmd");
        // Here you can process the scan data as needed
        // For now, just send 08 01 5A 00

        {
          // scan for existing networks:
          uint32_t startScanTime = millis();
          esp32ProvisionObjPtr->_networkCount = 0;
          esp32ProvisionObjPtr->scanResultTime = 0;
          wifi_scan_networks(wifidrv_scan_result_handler, NULL);
          //seems not blocking properly, so wait for a while
          while (((millis() - startScanTime) < 15000) && ((esp32ProvisionObjPtr->scanResultTime == 0) || ((int)(millis() - esp32ProvisionObjPtr->scanResultTime) < 1000))) {
            delay(100);
          }
          Serial.println("Scanned networks count: ");
          Serial.println(esp32ProvisionObjPtr->_networkCount);
        }

        uint8_t responseData[] = { 0x08, 0x01, 0x5A, 0x00 };  // TypeRespScanWifiStart RespScanWifiStart
        chr->setData(responseData, sizeof(responseData));
      } else if (dataBuffer[0] == 0x08 && dataBuffer[1] == 0x02) {  // TypeCmdScanWifiStatus
        Serial.println("Received scan status data");
        uint8_t scanResultCount = esp32ProvisionObjPtr->_networkCount;
        uint8_t responseData[] = { 0x08, 0x03, 0x6A, 0x04, 0x08, 0x01, 0x10, scanResultCount };  // TypeRespScanWifiStatus(3), RespScanWifiStatus(13): scan_finished(1):1, result_count(2):scanResultCount
        chr->setData(responseData, sizeof(responseData));
      } else if (dataBuffer[0] == 0x08 && dataBuffer[1] == 0x04 && dataBuffer[2] == 0x72) {  // TypeCmdScanWifiResult
        int index = 0;
        int readEntryLen = 0;
        int itemsBytes = dataBuffer[3];
        for (int i = 0; i < itemsBytes; i += 2) {
          uint8_t fieldNumber = dataBuffer[4 + i] >> 3;
          if (fieldNumber == 1) {  // start_index
            index = dataBuffer[4 + i + 1];
          } else if (fieldNumber == 2) {  // count
            readEntryLen = dataBuffer[4 + i + 1];
          }
        }
        Serial.print("Received scan result for index: ");
        Serial.print(index);
        Serial.print(", count: ");
        Serial.println(readEntryLen);

        std::vector<unsigned char> arraryOfRespScanWifiResult;
        for (int i = index; i < (index + readEntryLen); i++) {
          char* ssidPtr = esp32ProvisionObjPtr->_networkSsid[i];
          int channel = esp32ProvisionObjPtr->_networkChannel[i];
          int rssi = esp32ProvisionObjPtr->_networkRssi[i];
          uint8_t* bssidPtr = esp32ProvisionObjPtr->_networkMac[i];
          int auth = esp32ProvisionObjPtr->convertFromRealtekAuthModeToEsp32AuthMode(esp32ProvisionObjPtr->_networkEncr[i]);
          std::vector<unsigned char> encodedWifiScanResult = esp32ProvisionObjPtr->encodeWifiScanResult(ssidPtr, channel, rssi, bssidPtr, auth);
          // pack WiFiScanResult into a RespScanWifiResult
          std::vector<unsigned char> WiFiScanResult;
          WiFiScanResult.push_back(WIFI_SCAN_RESULT__ENTRY__FIELD_NUMBER << 3 | 2);  // Field number and wire type
          std::vector<unsigned char> scanResultSize = esp32ProvisionObjPtr->encodeLEB128(encodedWifiScanResult.size());
          WiFiScanResult.insert(WiFiScanResult.end(), scanResultSize.begin(), scanResultSize.end());
          WiFiScanResult.insert(WiFiScanResult.end(), encodedWifiScanResult.begin(), encodedWifiScanResult.end());
          arraryOfRespScanWifiResult.insert(arraryOfRespScanWifiResult.end(), WiFiScanResult.begin(), WiFiScanResult.end());
        }
        // pack RespScanWifiResult list into a response
        std::vector<unsigned char> response;
        response.push_back(0x08);  // NetworkScanMsgType (1)
        response.push_back(0x05);  // TypeRespScanWifiResult (5)
        response.push_back(0x7A);  // RespScanWifiResult (15)
        std::vector<unsigned char> respScanWifiResultSize = esp32ProvisionObjPtr->encodeLEB128(arraryOfRespScanWifiResult.size());
        response.insert(response.end(), respScanWifiResultSize.begin(), respScanWifiResultSize.end());
        response.insert(response.end(), arraryOfRespScanWifiResult.begin(), arraryOfRespScanWifiResult.end());

        chr->setData(response.data(), response.size());
      } else {
        Serial.print("scan data written: ");
        Serial.print("Data length: ");
        Serial.println(dataLen);
        Serial.print("Data: ");
        for (int i = 0; i < dataLen; i++) {
          if (dataBuffer[i] < 0x10) {
            Serial.print("0");  // Print leading zero for single digit hex values
          }
          Serial.print(dataBuffer[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
      }
    }
  } else if (chr == &(esp32ProvisionObjPtr->provisioningConfigCharacteristic)) {
    int dataLen = chr->getDataLen();
    if (dataLen > 0) {
      uint8_t dataBuffer[dataLen];
      chr->getData(dataBuffer, dataLen);
      if (dataBuffer[0] == 0x08 && dataBuffer[1] == 0x02 && dataBuffer[2] == 0x62) { 
        //NetworkConfigMsgType(1) TypeCmdSetWifiConfig(2)
        //CmdSetWifiConfig(12)
        int cmdSetWifiConfigLength = dataBuffer[3];
        Serial.println("CmdSetWifiConfig length: " + String(cmdSetWifiConfigLength));
        int readPtr = 4;
        esp32ProvisionObjPtr->toConnectSSID = "";
        esp32ProvisionObjPtr->toConnectPassword = "";
        while (readPtr < dataLen){
          if (dataBuffer[readPtr] == 0x0A) {
            // Found SSID
            readPtr++;
            int ssidLen = dataBuffer[readPtr];
            readPtr++;
            char ssidConvertBuff[ssidLen + 1];
            memcpy(ssidConvertBuff, &dataBuffer[readPtr], ssidLen);
            ssidConvertBuff[ssidLen] = '\0';
            esp32ProvisionObjPtr->toConnectSSID = String(ssidConvertBuff);
            readPtr += ssidLen;
          } else if (dataBuffer[readPtr] == 0x12) {
            // Found Password
            readPtr++;
            int passwordLen = dataBuffer[readPtr];
            readPtr++;
            char passwordConvert[passwordLen+1];
            memcpy(passwordConvert, &dataBuffer[readPtr], passwordLen);
            passwordConvert[passwordLen] = '\0';
            esp32ProvisionObjPtr->toConnectPassword = String(passwordConvert);
            readPtr += passwordLen;
          }
        }

        Serial.println(esp32ProvisionObjPtr->toConnectSSID);
        Serial.println(esp32ProvisionObjPtr->toConnectPassword);

        uint8_t responseData[] = { 0x08, 0x03, 0x6A, 0x00 };  // NetworkConfigMsgType(1) TypeRespSetWifiConfig(3) RespSetWifiConfig(13)
        chr->setData(responseData, sizeof(responseData));
      } else if (dataBuffer[0] == 0x08 && dataBuffer[1] == 0x04 && dataBuffer[2] == 0x72 && dataBuffer[3] == 0x00 ) {
        //NetworkConfigMsgType(1) TypeCmdApplyWifiConfig(4) CmdApplyWifiConfig(14)
        //apply wifi config
        Serial.print("Try connecting to WiFi with SSID: ");
        Serial.println(esp32ProvisionObjPtr->toConnectSSID);
        //can not start directly, or we get stack overflow. Let main loop handle it
        esp32ProvisionObjPtr->triggerWifiConnect = 1;
        esp32ProvisionObjPtr->wifiConnectFailed = 0;
        uint8_t responseData[] = { 0x08, 0x05, 0x7a, 0x00}; //NetworkConfigMsgType(1) TypeRespApplyWifiConfig(5) RespApplyWifiConfig(15)
        chr->setData(responseData, sizeof(responseData));
      } else if (dataBuffer[0] == 0x52 && dataBuffer[1] == 0x00) {
        //NetworkConfigMsgType(1) TypeCmdApplyThreadConfig(10)?
        uint8_t wifiStatus = WiFi.status();
        if (esp32ProvisionObjPtr->wifiConnectFailed){
          //the main loop tried to connect, but failed
          Serial.println("WiFi connect failed");
          uint8_t responseDataAuthError[] = { 0x08, 0x01, 0x5A, 0x04, 0x10, 0x03, 0x50, 0x00 }; //NetworkConfigMsgType(1) TypeRespGetWifiStatus(1) RespGetWifiStatus(11) len 4 WifiStationState(2) ConnectionFailed(3) WifiConnectFailedReason(10) AuthError(0)
          chr->setData(responseDataAuthError, sizeof(responseDataAuthError));
        }else if (wifiStatus == WL_DISCONNECTED) {
          //just say we are connecting for now
          Serial.println("WiFi still connecting ");
          uint8_t responseDataConnecting[] = { 0x08, 0x01, 0x5A, 0x02, 0x10, 0x01 };
          chr->setData(responseDataConnecting, sizeof(responseDataConnecting));
        }else if (wifiStatus == WL_CONNECTED) {
          //just say we are connecting for now
          Serial.println("WiFi connected ");
          uint8_t responseDataConnected[] = { 0x08, 0x01, 0x5A, 0x02, 0x10, 0x00 }; //Connected
          chr->setData(responseDataConnected, sizeof(responseDataConnected));
          esp32ProvisionObjPtr->wifiConnectedMessageSent = 1;
        } else {
          //WL_CONNECTED success
          Serial.print("unhandled WiFi status: ");
          Serial.println(wifiStatus);
        }
      } else {
        Serial.print("config data written: ");
        Serial.print("Data length: ");
        Serial.println(dataLen);
        Serial.print("Data: ");
        for (int i = 0; i < dataLen; i++) {
          if (dataBuffer[i] < 0x10) {
            Serial.print("0");  // Print leading zero for single digit hex values
          }
          Serial.print(dataBuffer[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
      }
    }
  } else {
    Serial.print("unhandled Characteristic ");
    Serial.print(chr->getUUID().str());
    Serial.print(" write by connection ");
    Serial.println(connID);
    if (chr->getDataLen() > 0) {
      Serial.print("Received string: ");
      Serial.print(chr->readString());
      Serial.println();
    }
  }
}

void ESP32Provision::begin() {
  esp32ProvisionObjPtr = this;
  char* thingName = NonvolatileDataManager::getThingName();
  char thingNameBufferUpperCase[32];
  memset(thingNameBufferUpperCase, 0, sizeof(thingNameBufferUpperCase));

  // Convert thingName to uppercase
  for (int i = 0; thingName[i] != '\0' && i < 31; i++) {
    thingNameBufferUpperCase[i] = toupper(thingName[i]);
  }
  thingNameBufferUpperCase[31] = '\0';  // Ensure null termination

  advdata.addFlags();
  advdata.addCompleteServices(BLEUUIDPatched(ESP32_PROVISIONING_SERVICE_UUID));
  scandata.addCompleteName(thingNameBufferUpperCase);

  provisioningControlCharacteristic.setProperties(GATT_CHAR_PROP_READ | GATT_CHAR_PROP_WRITE);
  provisioningControlCharacteristic.setPermissions(GATT_PERM_READ | GATT_PERM_WRITE);
  provisioningControlCharacteristic.setWriteCallback(writeCB);
  provisioningControlCharacteristic.setReadCallback(readCB);
  provisioningControlCharacteristic.setUserDescriptor("prov-ctrl");

  provisioningScanCharacteristic.setReadProperty(true);
  provisioningScanCharacteristic.setWriteProperty(true);
  provisioningScanCharacteristic.setWritePermissions(GATT_PERM_WRITE);
  provisioningScanCharacteristic.setReadPermissions(GATT_PERM_READ);
  provisioningScanCharacteristic.setUserDescriptor("prov-scan");
  provisioningScanCharacteristic.setBufferLen(DESC_VALUE_MAX_LEN);  //230
  provisioningScanCharacteristic.setWriteCallback(writeCB);
  provisioningScanCharacteristic.setReadCallback(readCB);

  provisioningSessionCharacteristic.setReadProperty(true);
  provisioningSessionCharacteristic.setWriteProperty(true);
  provisioningSessionCharacteristic.setWritePermissions(GATT_PERM_WRITE);
  provisioningSessionCharacteristic.setReadPermissions(GATT_PERM_READ);
  provisioningSessionCharacteristic.setUserDescriptor("prov-session");
  provisioningSessionCharacteristic.setBufferLen(64);
  provisioningSessionCharacteristic.setWriteCallback(writeCB);
  provisioningSessionCharacteristic.setReadCallback(readCB);

  provisioningConfigCharacteristic.setReadProperty(true);
  provisioningConfigCharacteristic.setWriteProperty(true);
  provisioningConfigCharacteristic.setWritePermissions(GATT_PERM_WRITE);
  provisioningConfigCharacteristic.setReadPermissions(GATT_PERM_READ);
  provisioningConfigCharacteristic.setUserDescriptor("prov-config");
  provisioningConfigCharacteristic.setBufferLen(DESC_VALUE_MAX_LEN);  //230
  provisioningConfigCharacteristic.setWriteCallback(writeCB);
  provisioningConfigCharacteristic.setReadCallback(readCB);

  provisioningProtoVerCharacteristic.setReadProperty(true);
  provisioningProtoVerCharacteristic.setWriteProperty(true);
  provisioningProtoVerCharacteristic.setWritePermissions(GATT_PERM_WRITE);
  provisioningProtoVerCharacteristic.setReadPermissions(GATT_PERM_READ);
  provisioningProtoVerCharacteristic.setUserDescriptor("proto-ver");
  //provisioningProtoVerCharacteristic.setBufferLen(1024);
  provisioningProtoVerCharacteristic.setWriteCallback(writeCB);
  provisioningProtoVerCharacteristic.setReadCallback(readCB);
  provisioningProtoVerCharacteristic.setBufferLen(128);


  provisioningService.addCharacteristic(provisioningControlCharacteristic);
  provisioningService.addCharacteristic(provisioningScanCharacteristic);
  provisioningService.addCharacteristic(provisioningSessionCharacteristic);
  provisioningService.addCharacteristic(provisioningConfigCharacteristic);
  provisioningService.addCharacteristic(provisioningProtoVerCharacteristic);

  BLEPatched.init();
  BLEPatched.setDeviceName(thingNameBufferUpperCase);
  BLEPatched.configAdvert()->setAdvData(advdata);
  BLEPatched.configAdvert()->setScanRspData(scandata);
  BLEPatched.configServer(1);
  BLEPatched.addService(provisioningService);

  BLEPatched.beginPeripheral();

  WiFi.status();

  wifiConnectedMessageSent = 0;
}

void ESP32Provision::stop() {
  BLEPatched.end();
  BLEPatched.deinit();
  esp32ProvisionObjPtr = nullptr;
}

void ESP32Provision::loop() {
  if (triggerWifiConnect){
    triggerWifiConnect = 0;
    int wifiRet;
    if (toConnectPassword.length() == 0) {
      // No password, use open network
      wifiRet = WiFi.begin((char *)toConnectSSID.c_str());
    } else {
      // Use password to connect
      wifiRet = WiFi.begin((char *)toConnectSSID.c_str(), (char *)toConnectPassword.c_str());
    }
    if (wifiRet != WL_CONNECTED) {
      wifiConnectFailed = 1;
    }
    Serial.print("WiFi.begin ret: ");
    Serial.println(wifiRet);
  }
}

std::vector<unsigned char> ESP32Provision::encodeLEB128(int64_t value) {
    std::vector<unsigned char> result;
    uint64_t convertedValue = value;  // deal with negative values correctly
    while (convertedValue > 0x7F) {
        result.push_back((convertedValue & 0x7F) | 0x80);
        convertedValue >>= 7;
    }
    result.push_back(convertedValue & 0x7F);
    return result;
}

std::vector<unsigned char> ESP32Provision::encodeWifiScanResult(char *ssid, uint8_t channel, int32_t rssi, uint8_t *bssid, uint8_t auth){
    std::vector<unsigned char> result;
    // Encode the fields as per the specification

    result.push_back(WIFI_SCAN_RESULT__SSID__FIELD_NUMBER << 3 | 2);  // Field number 1 SSID and wire type LEN
    int ssidLength = strlen(ssid);
    if (ssidLength > 255) {
        ssidLength = 255;  // Limit SSID length to 255 bytes
    }
    result.push_back(ssidLength);  // Length of SSID
    result.insert(result.end(), ssid, ssid + ssidLength);  // SSID data

    result.push_back(WIFI_SCAN_RESULT__CHANNEL__FIELD_NUMBER << 3 | 0);  // Field number 2 Channel and wire type VARINT
    std::vector<unsigned char> channelData = encodeLEB128(channel);
    result.insert(result.end(), channelData.begin(), channelData.end());  // Channel data

    result.push_back(WIFI_SCAN_RESULT__RSSI__FIELD_NUMBER << 3 | 0);  // Field number 3 RSSI and wire type VARINT
    std::vector<unsigned char> rssiData = encodeLEB128(rssi);
    result.insert(result.end(), rssiData.begin(), rssiData.end());  // RSSI data

    result.push_back(WIFI_SCAN_RESULT__BSSID__FIELD_NUMBER << 3 | 2);  // Field number 4 BSSID and wire type LEN
    result.push_back(6);  // Length of BSSID (6 bytes)
    result.insert(result.end(), bssid, bssid + 6);  // BSSID data

    result.push_back(WIFI_SCAN_RESULT__AUTH__FIELD_NUMBER << 3 | 0);  // Field number 5 Auth and wire type VARINT
    result.push_back(auth);  // Authentication mode 

    return result;
}

int ESP32Provision::convertFromRealtekAuthModeToEsp32AuthMode(uint32_t realtekAuthMode){
switch (realtekAuthMode) {
    case RTW_SECURITY_OPEN:
      return (int)WifiAuthMode::Open;
      break;
    case RTW_SECURITY_WEP_PSK:
      return (int)WifiAuthMode::WEP;
      break;
    case RTW_SECURITY_WPA_TKIP_PSK:
      return (int)WifiAuthMode::WPA_PSK;
      break;
    case RTW_SECURITY_WPA_AES_PSK:
      return (int)WifiAuthMode::WPA_PSK;
      break;
    case RTW_SECURITY_WPA2_AES_PSK:
      return (int)WifiAuthMode::WPA2_PSK;
      break;
    case RTW_SECURITY_WPA2_TKIP_PSK:
      return (int)WifiAuthMode::WPA2_PSK;
      break;
    case RTW_SECURITY_WPA2_MIXED_PSK:
      return (int)WifiAuthMode::WPA2_PSK;
      break;
    case RTW_SECURITY_WPA_WPA2_MIXED_PSK:
      return (int)WifiAuthMode::WPA_WPA2_PSK;
      break;
    case RTW_SECURITY_WPA3_AES_PSK:
      return (int)WifiAuthMode::WPA3_PSK;
      break;
    case RTW_SECURITY_WPA2_WPA3_MIXED:
      return (int)WifiAuthMode::WPA2_WPA3_PSK;
      break;
  }
  return (int)WifiAuthMode::Open;  // Default to Open if no match found
}