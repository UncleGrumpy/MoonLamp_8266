#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include <WebSocketsServer.h>
#include <Hash.h>


ESP8266WiFiMulti wifiMulti;       // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

ESP8266WebServer server(80);       // create a web server on port 80
WebSocketsServer webSocket(81);    // create a websocket server on port 81

File fsUploadFile;                // a File variable to temporarily store the received file

const char *ssid = "Moonlight"; // The name of the Wi-Fi network that will be created
const char *password = "";   // The password required to connect to it, leave blank for an open network

const char *OTAName = "moon";           // A hostname and a password for the OTA service
const char *OTAPassword = "3df077555fc40990d412237186c637a8";     // md5 hash of password

// specify the pins with an RGB LED connected
#define LED_RED     2           // 100r resistor
#define LED_GREEN   0           // 470r resistor
#define LED_BLUE    3           // 220r resistor

const char* mdnsName = "moon"; // Domain name for the mDNS responder

/*__________________________________________________________SETUP__________________________________________________________*/

void setup() {
  pinMode(LED_RED, OUTPUT);    // the pins with LEDs connected are outputs
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  
  analogWrite(LED_RED, 1023);    // Turn on moon.
  analogWrite(LED_GREEN, 1023);
  analogWrite(LED_BLUE, 1023);
    
  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection

  startMDNS();                 // Start the mDNS responder

  startLittleFS();               // Start the FS and list all contents

  startWebSocket();            // Start a WebSocket server

  startServer();               // Start a HTTP server with a file read handler and an upload handler
  
  startOTA();                  // Start the OTA service

  analogWrite(LED_RED, 1023);    // Normal moon.
  analogWrite(LED_GREEN, 1023);
  analogWrite(LED_BLUE, 1023);
    
}

/*__________________________________________________________LOOP__________________________________________________________*/

bool rainbow = false;             // The rainbow effect is turned off on startup

unsigned long prevMillis = millis();
int hue = 0;

void loop() {
  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // run the server
  ArduinoOTA.handle();                        // listen for OTA events

  if (rainbow) {                              // if the rainbow effect is turned on
    if (millis() > prevMillis + 32) {
      if (++hue == 360)                       // Cycle through the color wheel (increment by one degree every 32 ms)
        hue = 0;
      setHue(hue);                            // Set the RGB LED to the right color
      prevMillis = millis();
      }
    }
}

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startWiFi() { // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  //wifiMulti.addAP("ssid_from_AP_1", "your_password_for_AP_1");   // add Wi-Fi networks you want to connect to
  //wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  //wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED && WiFi.softAPgetStationNum() < 1) {  // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  if (WiFi.softAPgetStationNum() == 0) {     // If the ESP is connected to an AP
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());             // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  } else {                                   // If a station is connected to the ESP SoftAP
    Serial.print("Station connected to ESP8266 AP");
  }
  Serial.println("\r\n");
}


void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPasswordHash(OTAPassword);
  ArduinoOTA.setPort(8266);
  
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
    analogWrite(LED_RED, 0);    // Blue moon.
    analogWrite(LED_GREEN, 0);
    analogWrite(LED_BLUE, 512);
    delay(350);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
    analogWrite(LED_RED, 0);
    analogWrite(LED_GREEN, 1023);
    analogWrite(LED_BLUE, 0);
    delay(5000);
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    analogWrite(LED_RED, 0);
    analogWrite(LED_GREEN, 0);
    analogWrite(LED_BLUE, 1023 - progress*4);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    analogWrite(LED_BLUE, 0);
    analogWrite(LED_GREEN, 0);
    analogWrite(LED_RED, 512);
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    delay(2000);
  });
  ArduinoOTA.begin();
  analogWrite(LED_RED, 0);    // Blue moon.
  analogWrite(LED_GREEN, 0);
  analogWrite(LED_BLUE, 512);
  Serial.println("OTA ready\r\n");
}

void startLittleFS() {    // Start LittleFS and list all contents
  LittleFS.begin();       // Start the File System
  Serial.println("LittleFS started. Contents:");
  {
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {               // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str() );
    }
    Serial.printf("\n");
  }
}

void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                  // start the websocket server
  webSocket.onEvent(webSocketEvent);  // if there's an incomming websocket message, go to function 'webSocketEvent'
  MDNS.addService("ws", "tcp", 81);
  Serial.println("WebSocket server started.");
}

void startMDNS() { // Start the mDNS responder
  MDNS.begin(mdnsName);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  }, handleFileUpload);                       // go to 'handleFileUpload'

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
  // and check if the file exists

  server.begin();                             // start the HTTP server
  MDNS.addService("http", "tcp", 80);
  Serial.println("HTTP server started.");
}

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri())) {   // check if the file exists in the flash memory (LittleFS), if so, send it
    server.send(404, "text/plain", "404: File not found" );
  }
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (LittleFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed version
    File file = LittleFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload() { // upload a new file to the LittleFS
  HTTPUpload& upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz";                  // So if an uploaded file is not compressed, the existing compressed
      if (LittleFS.exists(pathWithGz))                     // version of that file must be deleted (if it exists)
        LittleFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = LittleFS.open(path, "w");            // Open the file for writing in LittleFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        rainbow = false;                  // Turn rainbow off when a new connection is established
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.printf("[%u] get Text: %s\n", num, payload);
      if (payload[0] == '#') {            // we get RGB data                   
        // Split RGB HEX String into individual color values
        char redX[5] = {0};
        char grnX[5] = {0};
        char bluX[5] = {0};        
        redX[0] = grnX[0] = bluX[0] = '0';
        redX[1] = grnX[1] = bluX[1] = 'X';
        redX[2] = payload[1];
        redX[3] = payload[2];
        grnX[2] = payload[3];
        grnX[3] = payload[4];
        bluX[2] = payload[5];
        bluX[3] = payload[6];
        // Convert HEX String to integer
        int redW = strtol(redX, NULL, 16);
        int grnW = strtol(grnX, NULL, 16);
        int bluW = strtol(bluX, NULL, 16);
        // convert 4-bit web (0-255) to 10-bit analog (0-1023) range.
        int red = redW * 4.012;
        int grn = grnW * 4.012;
        int blu = bluW * 4.012;
        // convert linear (0..512..1023) to geometric (0..256..1023) scale.
        int r = red * red / 1023;
        int g = grn * grn / 1023;
        int b = blu * blu / 1023;
        
        analogWrite(LED_RED,   r);                         // write it to the LED output pins
        analogWrite(LED_GREEN, g);
        analogWrite(LED_BLUE,  b);
      } else if (payload[0] == 'R') {                      // the browser sends an R when the rainbow effect is enabled
        rainbow = true;
      } else if (payload[0] == 'N') {                      // the browser sends an N when the rainbow effect is disabled
        rainbow = false;
      }
      break;
  }
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

void savePrefs(String lastColor) {
  File prefs = LittleFS.open(F("/cfg/rgb"), "w+");
  String saved = prefs.readString();
  if ( lastColor != saved ) {
    prefs.print(lastColor);
    prefs.close();
  }
}

String loadPrefs() {
  File prefs = LittleFS.open(F("/cfg/rgb"), "r");
  String saved = prefs.readString();
  return String( saved );
}

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String("ERROR");
  }
}

String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void setHue(int hue) { // Set the RGB LED to a given hue (color) (0° = Red, 120° = Green, 240° = Blue)
  hue %= 360;                   // hue is an angle between 0 and 359°
  float radH = hue * 3.142 / 180; // Convert degrees to radians
  float rf, gf, bf;

  if (hue >= 0 && hue < 120) {  // Convert from HSI color space to RGB
    rf = cos(radH * 3 / 4);
    gf = sin(radH * 3 / 4);
    bf = 0;
  } else if (hue >= 120 && hue < 240) {
    radH -= 2.09439;
    gf = cos(radH * 3 / 4);
    bf = sin(radH * 3 / 4);
    rf = 0;
  } else if (hue >= 240 && hue < 360) {
    radH -= 4.188787;
    bf = cos(radH * 3 / 4);
    rf = sin(radH * 3 / 4);
    gf = 0;
  }
  int r = rf * rf * 1023;
  int g = gf * gf * 1023;
  int b = bf * bf * 1023;

  analogWrite(LED_RED,  r);    // Write the right color to the LED output pins
  analogWrite(LED_GREEN, g);
  analogWrite(LED_BLUE,  b);
}
