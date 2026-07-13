/**
 * @file network_sd.ino
 * @brief Handles SD card storage, GPX formatting, and Web Server logic.
 */

void startGPXRecording() {
  gpxBuffer = "";
  gpxBufferCount = 0;
  
  if (!SD.exists("/training.gpx")) {
    File file = SD.open("/training.gpx", FILE_WRITE);
    if (file) {
      file.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
      file.println("<gpx version=\"1.1\" creator=\"ESP32 Bicycle Computer\" xmlns=\"http://www.topografix.com/GPX/1/1\" xmlns:gpxtpx=\"http://www.garmin.com/xmlschemas/TrackPointExtension/v1\">");
      file.println("<trk><name>Ride ESP32</name><trkseg>");
      file.close();
    }
  }
}

void flushGPXBuffer() {
  if (gpxBuffer.length() > 0) {
    File file = SD.open("/training.gpx", FILE_APPEND);
    if (file) {
      file.print(gpxBuffer);
      file.close();
    }
    gpxBuffer = "";
    gpxBufferCount = 0;
    saveRideState(); 
  }
}

void saveGPXPoint() {
  if (!isTrainingActive || !gps.location.isValid() || !gps.time.isValid() || gps.date.year() < 2024) return;
  char timeBuff[25];
  sprintf(timeBuff, "%04d-%02d-%02dT%02d:%02d:%02dZ", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
          
  String line = "<trkpt lat=\"" + String(gps.location.lat(), 6) + "\" lon=\"" + String(gps.location.lng(), 6) + "\">\n";
  line += "  <ele>" + String(gps.altitude.meters(), 1) + "</ele>\n";
  line += "  <time>" + String(timeBuff) + "</time>\n";
  line += "  <extensions>\n    <gpxtpx:TrackPointExtension>\n";
  line += "      <gpxtpx:cad>" + String(currentCadence) + "</gpxtpx:cad>\n";
  line += "    </gpxtpx:TrackPointExtension>\n  </extensions>\n</trkpt>\n";

  gpxBuffer += line; gpxBufferCount++;
  if (gpxBufferCount >= 10) flushGPXBuffer();
}

void stopGPXRecording() {
  flushGPXBuffer();
}

void loadGPX() {
  routePointsCount = 0; gpxLoaded = false;
  File file = SD.open("/route.gpx");
  if (!file) return;
  
  while (file.available() && routePointsCount < MAX_ROUTE_POINTS) {
    String line = file.readStringUntil('\n');
    int latIdx = line.indexOf("lat=\""); int lonIdx = line.indexOf("lon=\"");
    if (latIdx > 0 && lonIdx > 0) {
      routePoints[routePointsCount].lat = line.substring(latIdx + 5, line.indexOf("\"", latIdx + 5)).toFloat();
      routePoints[routePointsCount].lon = line.substring(lonIdx + 5, line.indexOf("\"", lonIdx + 5)).toFloat();
      routePointsCount++;
    }
  }
  file.close();
  if (routePointsCount > 0) gpxLoaded = true;
}

void loadODO() {
  if (SD.exists("/odo.txt")) {
    File file = SD.open("/odo.txt", FILE_READ);
    if (file) { String val = file.readStringUntil('\n'); odo = val.toFloat(); lastOdoSave = odo; file.close(); }
  }
}

void saveODO() {
  File file = SD.open("/odo.txt", FILE_WRITE);
  if (file) { file.print(odo, 2); file.close(); lastOdoSave = odo; }
}

void saveRideState() {
  File file = SD.open("/state.txt", FILE_WRITE);
  if (file) {
    file.println(tripA); file.println(totalTimeSec); file.println(movingTimeSec); file.println(maxSpeed); file.close();
  }
}

void loadRideState() {
  if (SD.exists("/state.txt")) {
    File file = SD.open("/state.txt", FILE_READ);
    if (file) {
      tripA = file.readStringUntil('\n').toFloat(); totalTimeSec = file.readStringUntil('\n').toInt();
      movingTimeSec = file.readStringUntil('\n').toInt(); maxSpeed = file.readStringUntil('\n').toFloat(); file.close();
    }
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Bike Computer</title><style>body{font-family:sans-serif; background:#222; color:#fff; padding:20px; text-align:center;} ";
  html += "input[type=file]{margin:20px 0;} input[type=submit], .btn{background:#e63946; color:white; border:none; padding:15px 30px; font-size:18px; border-radius:10px; text-decoration:none; display:inline-block; margin-top:15px;} ";
  html += ".btn-strava{background:#fc4c02;}</style></head><body><h1>Bike Computer Base</h1>";
  html += "<div style='background:#333; padding:20px; border-radius:10px; margin-bottom:20px; border: 2px solid #fc4c02;'>";
  html += "<h2>Latest Ride</h2><a href='/download?file=/training.gpx' class='btn btn-strava'>Download GPX (Strava)</a></div>";
  html += "<div style='background:#333; padding:20px; border-radius:10px; margin-bottom:20px;'>";
  html += "<h2>Upload New Route (route.gpx)</h2><form method='POST' action='/upload' enctype='multipart/form-data'>";
  html += "<input type='file' name='f' accept='.gpx'><br><input type='submit' value='Upload to Device'></form></div>";
  html += "<div style='background:#333; padding:20px; border-radius:10px;'><h2>Other Files</h2>";
  html += "<a href='/download?file=/odo.txt' class='btn' style='background:#457b9d;'>Download ODO</a><br>";
  html += "<a href='/download?file=/route.gpx' class='btn' style='background:#457b9d;'>Download current route</a></div></body></html>";
  server.send(200, "text/html", html);
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    if (SD.exists(filename)) SD.remove(filename);
    uploadFile = SD.open(filename, FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    server.sendHeader("Location", "/"); server.send(303);
  }
}

void handleDownload() {
  if (server.hasArg("file")) {
    String path = server.arg("file");
    if (SD.exists(path)) {
      File file = SD.open(path, FILE_READ);
      
      if (path.endsWith(".gpx")) {
        server.setContentLength(file.size() + 22);
        server.sendHeader("Content-Disposition", "attachment; filename=training.gpx");
        server.send(200, "application/octet-stream", "");
        
        WiFiClient client = server.client();
        byte buffer[512];
        while (file.available()) {
          size_t count = file.read(buffer, 512);
          client.write(buffer, count);
        }
        client.write((const uint8_t*)"</trkseg></trk></gpx>\n", 22);
      } else {
        server.streamFile(file, "application/octet-stream");
      }
      file.close();
      return;
    }
  }
  server.send(404, "text/plain", "File Not Found");
}