/**
 * @file display_ui.ino
 * @brief Handles all TFT drawing functions and UI screens.
 */

void drawServerScreen() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Connected", 120, 50, 4);
  tft.drawString("IP:", 120, 100, 2);
  tft.drawString(WiFi.localIP().toString(), 120, 130, 4);
  tft.drawRect(5, 5, 230, 310, TFT_MAGENTA);
  
  tft.drawRect(10, 220, 220, 60, TFT_CYAN);
  tft.setTextColor(TFT_CYAN, TFT_WHITE);
  tft.drawString("OFF WIFI & START RIDE", 120, 250, 2);
}

void drawMainScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawLine(0, 30, 240, 30, TFT_WHITE);
  tft.drawRect(0, 275, 120, 45, TFT_WHITE); 
  tft.drawRect(120, 275, 120, 45, TFT_WHITE); 
  tft.setTextDatum(MC_DATUM);
  
  if (isTrainingActive) tft.drawString("END", 60, 297, 4);
  else tft.drawString("START", 60, 297, 4);
  
  tft.drawString("STATS", 180, 297, 4);
  refreshMainScreenWithMap();
}

void refreshMainScreenWithMap() {
  tft.fillRect(0, 31, 240, 243, TFT_BLACK);
  
  if (gps.location.isValid() && gpxLoaded) {
    float currentLat = gps.location.lat();
    float currentLon = gps.location.lng();
    float course = gps.course.isValid() ? gps.course.deg() : 0.0;
    float radCourse = course * PI / 180.0;
    float sinC = sin(radCourse);
    float cosC = cos(radCourse);
    float cosLat = cos(currentLat * PI / 180.0);
    float scale = 3.0;
    int oldX = -1, oldY = -1;

    tft.setClipRect(0, 31, 240, 243);

    for(int i = 0; i < routePointsCount; i++) {
      float dy = (routePoints[i].lat - currentLat) * 111320.0;
      float dx = (routePoints[i].lon - currentLon) * 111320.0 * cosLat;
      
      float forwardDist = dx * sinC + dy * cosC;
      float rightDist = dx * cosC - dy * sinC;
      
      int screenX = 120 + (int)(rightDist / scale);
      int screenY = 220 - (int)(forwardDist / scale); 
      
      int minX = min(oldX, screenX); int maxX = max(oldX, screenX);
      int minY = min(oldY, screenY); int maxY = max(oldY, screenY);
      
      if (oldX != -1 && oldY != -1 && maxX >= -10 && minX <= 250 && maxY >= 20 && minY <= 285) {
        uint16_t routeColor;
        int p1 = routePointsCount / 6; int p2 = (routePointsCount * 2) / 6; int p3 = (routePointsCount * 3) / 6;
        int p4 = (routePointsCount * 4) / 6; int p5 = (routePointsCount * 5) / 6;

        if (i < p1) routeColor = TFT_WHITE; else if (i < p2) routeColor = 0x7FFF; else if (i < p3) routeColor = 0xFBFF;
        else if (i < p4) routeColor = 0xFFEF; else if (i < p5) routeColor = 0x7FEF; else routeColor = 0x7DFF;
        
        tft.drawLine(oldX, oldY, screenX, screenY, routeColor); tft.drawLine(oldX+1, oldY, screenX+1, screenY, routeColor);
        tft.drawLine(oldX-1, oldY, screenX-1, screenY, routeColor); tft.drawLine(oldX, oldY+1, screenX, screenY+1, routeColor);
        tft.drawLine(oldX, oldY-1, screenX, screenY-1, routeColor);
      }
      oldX = screenX; oldY = screenY;
    }

    tft.clearClipRect();
  }

  tft.fillTriangle(120, 210, 110, 230, 130, 230, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM); tft.setTextPadding(150); 
  tft.drawString(String(gpsSpeed, 1), 230, 45, 7);
  tft.setTextPadding(0); tft.drawString("km/h", 230, 100, 4);
  drawCadenceUI();

  // Climb UI overlay
  if (isClimbing && currentGradient >= 3) {
    int cx = 10;
    int cy = 255;
    
    tft.fillTriangle(cx + 8, cy - 12, cx, cy, cx + 16, cy, TFT_DARKGREY);
    tft.fillTriangle(cx + 18, cy - 16, cx + 8, cy, cx + 28, cy, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    String climbText = String(currentGradient) + "%  +" + String(currentAscent) + "m";
    tft.drawString(climbText, cx + 35, cy - 8, 2);
  } else {
    tft.fillRect(10, 235, 100, 25, TFT_BLACK);
  }

}

void drawStatsScreen() {
  tft.fillScreen(TFT_BLACK); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.drawLine(0, 30, 240, 30, TFT_WHITE);
  tft.setTextDatum(MC_DATUM); tft.drawString("STATISTICS", 120, 50, 4); tft.drawLine(60, 65, 180, 65, TFT_WHITE);
  tft.setTextDatum(ML_DATUM); 
  tft.drawString("ODO:", 10, 90, 4); tft.drawString("Trip A:", 10, 125, 4);
  tft.drawString("Time:", 10, 175, 4); tft.drawString("Moving:", 10, 210, 4); tft.drawString("V-Max:", 10, 245, 4); 
  tft.drawRect(0, 275, 120, 45, TFT_WHITE); tft.drawRect(120, 275, 120, 45, TFT_WHITE); 
  tft.setTextDatum(MC_DATUM); tft.drawString("RESET", 60, 297, 4); tft.drawString("BACK", 180, 297, 4);
  refreshStatsNumbers();
}

void refreshStatsNumbers() {
  char bufTotal[15]; char bufMoving[15];
  sprintf(bufTotal, "%02d:%02d:%02d", totalTimeSec / 3600, (totalTimeSec % 3600) / 60, totalTimeSec % 60);
  sprintf(bufMoving, "%02d:%02d:%02d", movingTimeSec / 3600, (movingTimeSec % 3600) / 60, movingTimeSec % 60);

  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextDatum(ML_DATUM); tft.setTextPadding(120);
  tft.drawString(String(odo, 1) + " km", 120, 90, 4); tft.drawString(String(tripA, 1) + " km", 120, 125, 4);
  tft.drawString(String(bufTotal), 120, 175, 4); tft.drawString(String(bufMoving), 120, 210, 4);
  tft.drawString(String(maxSpeed, 1) + " km/h", 120, 245, 4); tft.setTextPadding(0);
}

void drawConfirmScreen1() {
  tft.fillScreen(TFT_WHITE); tft.drawRect(5, 5, 230, 310, TFT_MAGENTA); tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM); tft.drawString("END RIDE? (1/3)", 120, 80, 4);
  tft.drawRect(80, 140, 80, 60, TFT_CYAN); tft.setTextColor(TFT_CYAN, TFT_WHITE); tft.drawString("YES", 120, 170, 2);
  tft.drawRect(20, 240, 200, 60, TFT_MAGENTA); tft.setTextColor(TFT_MAGENTA, TFT_WHITE); tft.drawString("NO (GO BACK)", 120, 270, 4);
}

void drawConfirmScreen2() {
  tft.fillScreen(TFT_WHITE); tft.drawRect(5, 5, 230, 310, TFT_MAGENTA); tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM); tft.drawString("END RIDE? (2/3)", 120, 170, 4);
  tft.drawRect(10, 40, 80, 60, TFT_CYAN); tft.setTextColor(TFT_CYAN, TFT_WHITE); tft.drawString("YES", 50, 70, 2);
  tft.drawRect(20, 240, 200, 60, TFT_MAGENTA); tft.setTextColor(TFT_MAGENTA, TFT_WHITE); tft.drawString("NO (GO BACK)", 120, 270, 4);
}

void drawConfirmScreen3() {
  tft.fillScreen(TFT_WHITE); tft.drawRect(5, 5, 230, 310, TFT_MAGENTA); tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM); tft.drawString("END", 120, 150, 4); tft.drawString("RIDE? (3/3)", 120, 180, 4);
  tft.drawRect(150, 40, 80, 60, TFT_CYAN); tft.setTextColor(TFT_CYAN, TFT_WHITE); tft.drawString("YES", 190, 70, 2);
  tft.drawRect(20, 240, 200, 60, TFT_MAGENTA); tft.setTextColor(TFT_MAGENTA, TFT_WHITE); tft.drawString("NO (GO BACK)", 120, 270, 4);
}

void drawResetScreen1() {
  tft.fillScreen(TFT_WHITE); tft.drawRect(5, 5, 230, 310, TFT_MAGENTA); tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM); tft.drawString("RESET? (1/3)", 120, 80, 4);
  tft.drawRect(80, 140, 80, 60, TFT_CYAN); tft.setTextColor(TFT_CYAN, TFT_WHITE); tft.drawString("YES", 120, 170, 2);
  tft.drawRect(20, 240, 200, 60, TFT_MAGENTA); tft.setTextColor(TFT_MAGENTA, TFT_WHITE); tft.drawString("NO (GO BACK)", 120, 270, 4);
}

void drawResetScreen2() {
  tft.fillScreen(TFT_WHITE); tft.drawRect(5, 5, 230, 310, TFT_MAGENTA); tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM); tft.drawString("RESET? (2/3)", 120, 170, 4);
  tft.drawRect(10, 40, 80, 60, TFT_CYAN); tft.setTextColor(TFT_CYAN, TFT_WHITE); tft.drawString("YES", 50, 70, 2);
  tft.drawRect(20, 240, 200, 60, TFT_MAGENTA); tft.setTextColor(TFT_MAGENTA, TFT_WHITE); tft.drawString("NO (GO BACK)", 120, 270, 4);
}

void drawResetScreen3() {
  tft.fillScreen(TFT_WHITE); tft.drawRect(5, 5, 230, 310, TFT_MAGENTA); tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(MC_DATUM); tft.drawString("SURE?", 120, 150, 4); tft.drawString("RESET (3/3)", 120, 180, 4);
  tft.drawRect(150, 40, 80, 60, TFT_CYAN); tft.setTextColor(TFT_CYAN, TFT_WHITE); tft.drawString("YES", 190, 70, 2);
  tft.drawRect(20, 240, 200, 60, TFT_MAGENTA); tft.setTextColor(TFT_MAGENTA, TFT_WHITE); tft.drawString("NO (GO BACK)", 120, 270, 4);
}

void drawCadenceUI() {
  int xPos = 230; int yPos = 270; tft.setTextDatum(BR_DATUM); tft.setTextPadding(85);
  if (bleConnected) { tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.drawString(String(currentCadence), xPos, yPos, 6); } 
  else { tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.drawString("--", xPos, yPos, 6); }
  tft.setTextPadding(0); 
}