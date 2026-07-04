/**
 * @file sensors_gps_ble.ino
 * @brief Handles Bluetooth Low Energy connections and Cadence logic.
 */

static void bleNotificationCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 1) return;
  uint8_t flags = pData[0];
  bool hasCadenceData = (flags & 2) >> 1;
  
  if (hasCadenceData) {
    int index = 1;
    if (flags & 1) index += 6; // Skip wheel data if present

    if (index + 4 <= length) {
      uint16_t crankRevs = pData[index] | (pData[index+1] << 8);
      uint16_t crankTime = pData[index+2] | (pData[index+3] << 8);
      
      // React ONLY on new physical rotation
      if (crankTime != lastCrankTime) {
        lastCadenceSignalTime = millis(); 
        
        if (lastCrankTime != 0) {
          uint16_t timeDiff = crankTime - lastCrankTime;
          uint16_t revDiff = crankRevs - lastCrankRevs;
          
          if (timeDiff > 0) {
            // Safe 32-bit calculation to prevent integer overflow (negative values)
            uint32_t calc = ((uint32_t)revDiff * 1024 * 60) / timeDiff;
            currentCadence = (int)calc;
            
            // Filter out glitch values from sensor resets
            if (currentCadence > 200) currentCadence = 0;
            
            forceScreenRefresh = true; // Wake up screen on cadence change
          }
        }
        lastCrankRevs = crankRevs;
        lastCrankTime = crankTime;
      }
    }
  }
}

class CycplusCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    // Empty
  }
  void onDisconnect(BLEClient* pclient) {
    bleConnected = false; 
    currentCadence = 0;
    Serial.println("[BLE] DISCONNECTED! Searching in background...");
  }
};

void connectToCadenceSensor() {
  static bool isBleInitialized = false;
  static BLEClient* pClient = nullptr;
  
  if (!isBleInitialized) {
    BLEDevice::init("");
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new CycplusCallback());
    isBleInitialized = true;
  }
  
  if (bleConnected) return; 

  BLEAddress address(CADENCE_SENSOR_MAC);
  bool isPhysicallyConnected = pClient->connect(address, BLE_ADDR_TYPE_RANDOM);
  
  if (isPhysicallyConnected) {
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID((uint16_t)0x1816));
    if (pRemoteService != nullptr) {
      BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID((uint16_t)0x2A5B));
      if (pRemoteCharacteristic != nullptr && pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(bleNotificationCallback);
        bleConnected = true;
        Serial.println("[BLE] CADENCE SENSOR CONNECTED!");
      }
    }
  }
}

void connectionBLETask(void * parameter) {
  for(;;) { 
    // Shield against CPU hang: only attempt BLE reconnect if WIFI is OFF
    if (!bleConnected && WiFi.getMode() == WIFI_OFF) {
      connectToCadenceSensor();
    }
    // Sleep Core 0 for 5 seconds
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}