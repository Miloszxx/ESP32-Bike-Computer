/**
 * @file BicycleComputer.ino
 * @brief Main execution file for the ESP32 Bicycle Computer.
 */

#include <TFT_eSPI.h> 
#include <SPI.h>
#include <SD.h>
#include <XPT2046_Touchscreen.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEClient.h>

#include "config.h"
#include "secrets.h"

// =========================================================
// GLOBAL OBJECTS
// =========================================================
TFT_eSPI tft = TFT_eSPI(); 
WebServer server(80);
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;
SPIClass touchSpi = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TaskHandle_t TaskBLE;

// =========================================================
// GLOBAL VARIABLES (State & Memory)
// =========================================================
int currentCadence = 0;
bool bleConnected = false;
uint16_t lastCrankRevs = 0;
uint16_t lastCrankTime = 0;
unsigned long lastCadenceSignalTime = 0;

struct RoutePoint { float lat; float lon; };
RoutePoint routePoints[MAX_ROUTE_POINTS];
int routePointsCount = 0;
bool gpxLoaded = false;

// --- GPS & Metrics ---
float gpsSpeed = 0.0;
float odo = 0.0;            
float tripA = 0.0; 
float maxSpeed = 0.0;             
unsigned long totalTimeSec = 0;
unsigned long movingTimeSec = 0;

// --- DELTA TIME TRACKING (Garmin Fix) ---
unsigned long totalTimeFraction = 0;
unsigned long movingTimeFraction = 0;

bool hasPreviousPosition = false;
double prevLat = 0, prevLon = 0;
float prevAltitude = 0;
double distAccumulator = 0.0; 
int currentGradient = 0;
float lastOdoSave = 0.0; 

#define ALT_BUFFER_SIZE 10
float altBuffer[ALT_BUFFER_SIZE];
int altBufferIndex = 0;
bool altBufferFilled = false;
float smoothedAltitude = 0.0;
float prevSmoothedAltitude = 0.0;
float climbDistance = 0.0;

// Climb tracking variables
float climbStartAltitude = 0.0;
int currentAscent = 0;
bool isClimbing = false;


int currentScreen = 4; // Starts at 4 (Home Server Mode)
unsigned long lastUpdateTime = 0;
unsigned long lastTouchTime = 0;
unsigned long lastTouchCheck = 0;
bool isScreenOn = true; 
bool isTrainingActive = false; 
bool forceScreenRefresh = true; 

bool cmdStartRide = false;
bool cmdStopRide = false;

int prevMinute = -1;
int prevSatellites = -1;
float prevSpeed = -1.0;

String gpxBuffer = "";
int gpxBufferCount = 0;
File uploadFile;


void updateAltitudeFilter(float newAlt) {
  altBuffer[altBufferIndex] = newAlt;
  altBufferIndex++;
  if (altBufferIndex >= ALT_BUFFER_SIZE) {
    altBufferIndex = 0;
    altBufferFilled = true;
  }
  float sum = 0;
  int count = altBufferFilled ? ALT_BUFFER_SIZE : altBufferIndex;
  for (int i = 0; i < count; i++) {
    sum += altBuffer[i];
  }
  smoothedAltitude = count > 0 ? sum / count : newAlt;
}

// =========================================================
// SETUP FUNCTION
// =========================================================
void setup() {
  Serial.begin(115200); 

  gpsSerial.setRxBufferSize(1024);
  gpsSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.init();
  tft.setRotation(0); 
  tft.invertDisplay(false);
  tft.fillScreen(TFT_BLACK); 
  tft.setTextColor(TFT_WHITE, TFT_BLACK); 
  tft.setTextDatum(MC_DATUM); 
  tft.drawString("Booting...", 120, 120, 4);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS)) {
    loadODO(); 
    loadRideState();
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("SD Card Error!", 120, 160, 2);
    delay(2000);
  }

  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSpi);
  ts.setRotation(0);

  tft.fillScreen(TFT_WHITE);
  tft.drawRect(5, 5, 230, 310, TFT_MAGENTA);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("Searching for WIFI...", 120, 120, 2);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Searching HOME...", 120, 160, 4);
  
  WiFi.begin(WIFI_SSID_HOME, WIFI_PASS_HOME);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { 
    delay(500);
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Searching MOBILE...", 120, 160, 4);
    WiFi.disconnect(); 
    WiFi.begin(WIFI_SSID_MOBILE, WIFI_PASS_MOBILE);
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) { 
      delay(500);
      attempts++;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentScreen = 4;
    server.on("/", HTTP_GET, handleRoot);
    server.on("/upload", HTTP_POST, []() { server.send(200, "text/plain", "OK"); }, handleFileUpload);
    server.on("/download", HTTP_GET, handleDownload);
    server.begin();
    drawServerScreen();
  } else {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(80); 
    loadGPX();
    currentScreen = 0;
    drawMainScreen();
  }

  xTaskCreatePinnedToCore(connectionBLETask, "TaskBLE", 10000, NULL, 1, &TaskBLE, 0);
}

// =========================================================
// MAIN LOOP
// =========================================================
void loop() {
  if (millis() - lastCadenceSignalTime > 3000) {
    if (currentCadence != 0) {
      currentCadence = 0;
      forceScreenRefresh = true; 
    }
    lastCrankTime = 0;
  }

  unsigned long currentMillis = millis();

  if (cmdStartRide) {
    cmdStartRide = false;
    tft.fillRect(0, 275, 120, 45, TFT_BLACK);
    tft.drawRect(0, 275, 120, 45, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Wait...", 60, 297, 2); 
    isTrainingActive = true;
    startGPXRecording(); 
    currentScreen = 0;
    forceScreenRefresh = true; 
    drawMainScreen();
  }

  if (cmdStopRide) {
    cmdStopRide = false;
    tft.fillRect(10, 180, 100, 80, TFT_BLACK);
    tft.drawRect(10, 180, 100, 80, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Wait...", 60, 220, 2);
    isTrainingActive = false;
    stopGPXRecording(); 
    currentScreen = 0;
    forceScreenRefresh = true;
    drawMainScreen();
  }

if (currentScreen == 4) {
    server.handleClient();
    if (currentMillis - lastTouchCheck > 100) {
      lastTouchCheck = currentMillis;
      if (gps.altitude.isValid()) {
      updateAltitudeFilter(gps.altitude.meters());
    }

    if (gpsSpeed > 3.5) {
      climbDistance += (gpsSpeed / 3.6);
    }

if (climbDistance >= 15.0) {
      if (prevSmoothedAltitude != 0.0) { 
        float altDifference = smoothedAltitude - prevSmoothedAltitude;
        currentGradient = (int)((altDifference / climbDistance) * 100.0);
        if (currentGradient > 25) currentGradient = 25;
        if (currentGradient < -25) currentGradient = -25;
        
        // Climb state logic
        if (currentGradient >= 3) {
          if (!isClimbing) {
            isClimbing = true;
            climbStartAltitude = prevSmoothedAltitude;
          }
          currentAscent = (int)(smoothedAltitude - climbStartAltitude);
          if (currentAscent < 0) currentAscent = 0;
        } else {
          isClimbing = false;
          climbStartAltitude = 0.0;
          currentAscent = 0;
        }
      }
      prevSmoothedAltitude = smoothedAltitude;
      climbDistance = 0.0; 
      forceScreenRefresh = true;
    }
      if (ts.touched()) {
        TS_Point p = ts.getPoint();
        int y = map(p.y, 300, 3800, 0, 320);
        
        if (y > 210) {
          tft.fillScreen(TFT_BLACK);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("Turning off WIFI...", 120, 160, 4);
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF); 
          setCpuFrequencyMhz(80); 
          loadGPX(); 
          currentScreen = 0;
          forceScreenRefresh = true;
          drawMainScreen();
          delay(500);
        }
      }
    }
    return;
  }

  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  if (currentMillis - lastTouchCheck > 50) {
    lastTouchCheck = currentMillis;
    if (ts.touched()) {
      TS_Point p = ts.getPoint();
      if (currentMillis - lastTouchTime > 250) { 
        lastTouchTime = currentMillis;
        
        if (!isScreenOn) {
          digitalWrite(21, HIGH); 
          isScreenOn = true;
          forceScreenRefresh = true;
          if (currentScreen == 0) drawMainScreen();
          else if (currentScreen == 1) drawStatsScreen();
          else if (currentScreen == 2) drawConfirmScreen1();
          else if (currentScreen == 3) drawResetScreen1(); 
          else if (currentScreen == 5) drawConfirmScreen2(); 
          else if (currentScreen == 6) drawConfirmScreen3();
        } else {
          int x = map(p.x, 300, 3800, 0, 240);
          int y = map(p.y, 300, 3800, 0, 320);

          if (currentScreen == 0) {
            if (y > 270) {
              if (x < 120) { 
                if (isTrainingActive) {
                  currentScreen = 2;
                  forceScreenRefresh = true;
                  drawConfirmScreen1();
                } else {
                  cmdStartRide = true;
                }
              } else { 
                currentScreen = 1;
                forceScreenRefresh = true;
                drawStatsScreen();
              }
            } else if (y < 120 && x < 120) { 
              digitalWrite(21, LOW);
              isScreenOn = false;
            }
          } 
          else if (currentScreen == 1) {
            if (y > 270) { 
              if (x < 120) { 
                currentScreen = 3;
                forceScreenRefresh = true;
                drawResetScreen1();
              } else { 
                currentScreen = 0;
                forceScreenRefresh = true;
                drawMainScreen();
              }
            }
          }
          else if (currentScreen == 2) {
            if (x >= 80 && x <= 160 && y >= 140 && y <= 200) { 
              currentScreen = 5;
              forceScreenRefresh = true;
              drawConfirmScreen2();
            } else if (x >= 20 && x <= 220 && y >= 240 && y <= 310) { 
              currentScreen = 0;
              forceScreenRefresh = true;
              drawMainScreen();
            }
          }
          else if (currentScreen == 5) {
            if (x >= 10 && x <= 90 && y >= 40 && y <= 100) { 
              currentScreen = 6;
              forceScreenRefresh = true;
              drawConfirmScreen3();
            } else if (x >= 20 && x <= 220 && y >= 240 && y <= 310) { 
              currentScreen = 0;
              forceScreenRefresh = true;
              drawMainScreen();
            }
          }
          else if (currentScreen == 6) {
            if (x >= 150 && x <= 230 && y >= 40 && y <= 100) { 
              cmdStopRide = true;
            } else if (x >= 20 && x <= 220 && y >= 240 && y <= 310) { 
              currentScreen = 0;
              forceScreenRefresh = true;
              drawMainScreen();
            }
          }
          else if (currentScreen == 3) {
            if (x >= 80 && x <= 160 && y >= 140 && y <= 200) { 
              currentScreen = 9;
              forceScreenRefresh = true;
              drawResetScreen2();
            } else if (x >= 20 && x <= 220 && y >= 240 && y <= 310) { 
              currentScreen = 1;
              forceScreenRefresh = true;
              drawStatsScreen();
            }
          }
          else if (currentScreen == 9) {
            if (x >= 10 && x <= 90 && y >= 40 && y <= 100) { 
              currentScreen = 10;
              forceScreenRefresh = true;
              drawResetScreen3();
            } else if (x >= 20 && x <= 220 && y >= 240 && y <= 310) { 
              currentScreen = 1;
              forceScreenRefresh = true;
              drawStatsScreen();
            }
          }
          else if (currentScreen == 10) {
            if (x >= 150 && x <= 230 && y >= 40 && y <= 100) { 
              tripA = 0; totalTimeSec = 0; movingTimeSec = 0; maxSpeed = 0.0;
              if (SD.exists("/state.txt")) SD.remove("/state.txt"); 
              if (SD.exists("/training.gpx")) SD.remove("/training.gpx"); 
              currentScreen = 1; 
              forceScreenRefresh = true;
              drawStatsScreen();
            } else if (x >= 20 && x <= 220 && y >= 240 && y <= 310) { 
              currentScreen = 1;
              forceScreenRefresh = true;
              drawStatsScreen();
            }
          }
        }
      }
    }
  }

  // --- DELTA TIME & 1-SECOND REFRESH ---
  if (currentMillis - lastUpdateTime >= 1000) {
    unsigned long timeDelta = currentMillis - lastUpdateTime;
    lastUpdateTime = currentMillis;
    
    if (gps.speed.isValid()) {
      float v = gps.speed.kmph();
      gpsSpeed = (v > 2.0) ? v : 0.0; 
      if (gpsSpeed > maxSpeed) { maxSpeed = gpsSpeed; if (currentScreen == 1) forceScreenRefresh = true; }
    } else { gpsSpeed = 0.0; }

    bool needsRefresh = forceScreenRefresh; 
    forceScreenRefresh = false;

    if (isTrainingActive) {
      totalTimeFraction += timeDelta;
      if (totalTimeFraction >= 1000) {
        totalTimeSec += (totalTimeFraction / 1000);
        totalTimeFraction %= 1000;
      }
      if (gpsSpeed > 3.5) {
        movingTimeFraction += timeDelta;
        if (movingTimeFraction >= 1000) {
          movingTimeSec += (movingTimeFraction / 1000);
          movingTimeFraction %= 1000;
        }
      }
      if (currentScreen == 1) needsRefresh = true;
    }

    if (gps.time.isValid() && gps.time.minute() != prevMinute) { prevMinute = gps.time.minute(); needsRefresh = true; }
    if (gps.satellites.isValid() && gps.satellites.value() != prevSatellites) { prevSatellites = gps.satellites.value(); needsRefresh = true; }
    if (gpsSpeed != prevSpeed) { prevSpeed = gpsSpeed; needsRefresh = true; }
    if (gpsSpeed > 0.0) needsRefresh = true;

    if (gps.location.isValid() && gps.altitude.isValid() && gps.hdop.isValid() && gps.hdop.hdop() < 3.0) {
      if (!hasPreviousPosition) {
        prevLat = gps.location.lat(); prevLon = gps.location.lng(); prevAltitude = gps.altitude.meters(); 
        hasPreviousPosition = true;
      } else {
        double d = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), prevLat, prevLon);
        if (d > 1.0 && gpsSpeed > 2.0) { 
          odo += (d / 1000.0);
          if (isTrainingActive) { 
            tripA += (d / 1000.0); 
            saveGPXPoint(); 
          }
          prevLat = gps.location.lat();
          prevLon = gps.location.lng();
        }
      }
    }

    if (odo - lastOdoSave >= 0.1) saveODO();
    
    if (isScreenOn && needsRefresh) {
      if (currentScreen != 2 && currentScreen != 3 && currentScreen != 5 && currentScreen != 6) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        char statsBuff[20]; sprintf(statsBuff, "Sat:%d", gps.satellites.value());
        tft.setTextDatum(TR_DATUM); tft.setTextPadding(80); tft.drawString(statsBuff, 230, 8, 2);

        tft.setTextDatum(MC_DATUM); tft.setTextPadding(100);
        if (gps.altitude.isValid()) {
          String altGradStr = String((int)gps.altitude.meters()) + "m  " + String(currentGradient) + "%";
          tft.drawString(altGradStr, 120, 8, 2);
        } else { tft.drawString("--m", 120, 8, 2); }

        int legX = 90; int legY = 26; int sq = 6; int sp = 4;
        tft.fillRect(legX, legY, sq, sq, TFT_WHITE); tft.fillRect(legX + (sq+sp), legY, sq, sq, 0x7FFF);
        tft.fillRect(legX + 2*(sq+sp), legY, sq, sq, 0xFBFF); tft.fillRect(legX + 3*(sq+sp), legY, sq, sq, 0xFFEF);
        tft.fillRect(legX + 4*(sq+sp), legY, sq, sq, 0x7FEF); tft.fillRect(legX + 5*(sq+sp), legY, sq, sq, 0x7DFF);

        tft.setTextDatum(TL_DATUM); tft.setTextPadding(60);
        if (gps.time.isValid()) {
          int h = (gps.time.hour() + 2) % 24; char timeStringBuff[10];
          sprintf(timeStringBuff, "%02d:%02d", h, gps.time.minute());
          tft.drawString(timeStringBuff, 10, 8, 2); 
        } else { tft.drawString("--:--", 10, 8, 2); }
        tft.setTextPadding(0);
      }
      if (currentScreen == 0) refreshMainScreenWithMap();
      else if (currentScreen == 1) refreshStatsNumbers(); 
    }
  }
}