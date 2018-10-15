#include <WiFiClient.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSockets.h>
#include <WebSocketsServer.h>
#include <driver/adc.h>
#include <FS.h>
#include "SPIFFS.h"

// WiFi configuration - can be changed via AP/EEPROM
const char* ssid = "EspDRO";
const char* password = "EspDRO";

// Note: this sketch requires the webserver files to be uploaded to ESP32 SPIFFS, see: https://github.com/me-no-dev/arduino-esp32fs-plugin

// Pin Configuration - !!! Update This !!! 
// Example pinout: https://raw.githubusercontent.com/gojimmypi/ESP32/master/images/myESP32%20DevKitC%20pinout.png
int dataPin = 36; // yellow
int clockPin = 39; // green

// ADC threshold for 1.5V SPCsignals (at 6dB/11-bit, high comes to around 1570 in analogRead() )
#define ADC_TRESHOLD 800

// define this to display the raw signal (useful to troubleshoot cable or signal level issues)
#define DEBUG_SIGNAL 0

// timeout in milliseconds for a bit read ($TODO - change to micros() )
#define BIT_READ_TIMEOUT 100

// timeout for a packet read 
#define PACKET_READ_TIMEOUT 250

// Packet format: [0 .. 19]=data, 20=sign, [21..22]=unused?, 23=inch
#define PACKET_BITS       24

// minimum reading
#define MIN_RANGE -(1<<20)

// DRO circular buffer entries (4096 entries is roughly the equivalent of 8 minutes of data)
#define DRO_BUFFER_SIZE  0x1000


// DRO 1-Reading class
struct Reading
{  
    uint32_t timestamp;
    int32_t microns;

    Reading(): timestamp(millis()), microns(MIN_RANGE)  {}

    Reading& operator=(const Reading& obj) {
      timestamp = obj.timestamp;
      microns = obj.microns;
      return *this;
    }    
};


// DRO circular buffer
Reading dro_buffer[DRO_BUFFER_SIZE] = {};
size_t dro_index = 0;

unsigned char eepromSignature = 0x5A;

bool client_mode = 1;
bool stream_mode = 1;

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void log(char *fmt, ... )
{  
  char buf[128];
  va_list args;
  va_start (args, fmt);
  vsnprintf(buf, 128, fmt, args);
  va_end (args);
  Serial.println(buf);
}

// capped read: -1 (timeout), 0, 1
int getBit() {
    int data;

#if DEBUG_SIGNAL
    // debug code to sample the data reads
    int clk = analogRead(clockPin);
    int d = analogRead(dataPin);
    log("%6d %6d\n", clk, d);
#endif
        
    int readTimeout = millis() + BIT_READ_TIMEOUT;
    while (analogRead(clockPin) > ADC_TRESHOLD) {
      if (millis() > readTimeout)
        return -1;
    }
    
    while (analogRead(clockPin) < ADC_TRESHOLD) {
      if (millis() > readTimeout)
        return -1;
    }
    
    data = (analogRead(dataPin) > ADC_TRESHOLD)?1:0;
    return data;
}

// read one full packet
long getPacket() 
{
    long packet = 0;
    int readTimeout = millis() + PACKET_READ_TIMEOUT;

    int bitIndex = 0;
    while (bitIndex < PACKET_BITS) {
      int bit = getBit();
      if (bit < 0 ) {
        // bit read timeout: reset packet or bail out
        if (millis() > readTimeout) {
          // packet timeout
          return -1;
        }
        
        bitIndex = 0;
        packet = 0;
        continue;
      }

      packet |= (bit & 1)<<bitIndex;
      
      bitIndex++;
    }
    
    return packet;
}

// convert a packet to signed microns
long getMicrons(long packet)
{
  if (packet < 0)
    return MIN_RANGE;
    
  long data = (packet & 0xFFFFF)*( (packet & 0x100000)?-1:1);

  if (packet & 0x800000) {
        // inch value (this comes sub-sampled) 
        data = data*254/200;
  }

  return data;
}


void spcTask( void * parameter )
{
    uint32_t lastReadTime = millis();

    for(;;) { 
      long packet = getPacket();
      
      if (packet < 0) {
        // read timeout, display?
        if (millis() > lastReadTime + PACKET_READ_TIMEOUT) {
          // advance last read to time-out
          lastReadTime = millis();
          log("* %d: no SPC data", lastReadTime);
        }
      } else {

        // add to local queue
        //log("* %d: microns=%d raw=0x%08X", millis(), getMicrons(packet), packet);
        size_t new_dro_index = (dro_index+1) % DRO_BUFFER_SIZE;
        dro_buffer[new_dro_index].timestamp = millis();
        dro_buffer[new_dro_index].microns = getMicrons(packet);
        dro_index = new_dro_index;

        // broadcast to all websocket clients
        char buf[128];
        sprintf(buf, "{\"axis0\":%d,\"ts\":%d}", dro_buffer[new_dro_index].microns, dro_buffer[new_dro_index].timestamp);
        webSocket.broadcastTXT(buf);
      }
    } 
    //vTaskDelete( NULL ); 
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            log("[%u] Disconnected!\n", num);
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                log("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

                // send message to client
                webSocket.sendTXT(num, "Connected");
            }
            break;
            
        case WStype_TEXT:
            log("[%u] get Text: %s\n", num, payload);

            // send message to client
            // webSocket.sendTXT(num, "message here");

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
            
        case WStype_BIN:
            log("[%u] get binary length: %u\n", num, length);

            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
            
    case WStype_ERROR:      
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
    }

}

// root HTTP request
void handle_root() {
  String content = "<!DOCTYPE HTML>\r\n<html><body>";
  content += "<h1>#EspDRO</h1><br>";
  content += "<br><br><h2><a href=\"/EspDRO.html\">Start #EspDRO</a></h2><br><br>";
  content += "Quick links: <ul>";
  content += "<li><a href=\"/EspDRO.html\">#EspDRO</a> - Webserver DRO</li>";
  content += "<li><a href=\"/setup\">/setup</a> - Setup WiFi & EEPROM</li>";
  content += "<li><a href=\"/peek\">/peek</a> - return the last DRO reading as text</li>";
  content += "<li><a href=\"/raw\">/raw</a> - raw dump of readings buffer as JSON</li>";
  content += "</ul><br>";  
  content += "<br>SPC Connector -> GPIO reference:<br><ul><li>Data pin:" + String(dataPin) + "</li><li>Clock pin:" + String(clockPin) + "</li></ul><br>";
  content += "</body></html>";
  
  server.send(200, "text/html", content);
  delay(50);
}

// SPIFFS content type autodetect
String getContentType(String filename)
{
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  
  return "text/plain";
}

// serve SPIFFS requests
bool handleFS(String path)
{
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, getContentType(path));
    file.close();
    return true;    
  }
  log("Error in SPIFFS request: '%s", path);
  return false;
}

void setup() {

  pinMode(dataPin, INPUT);     
  pinMode(clockPin, INPUT);

  EEPROM.begin(512);
  delay(20);

  Reading start_reading;
  dro_buffer[dro_index] = start_reading;

  Serial.begin(115200);
  Serial.println("EspDRO 0.99.0 Initialized.");

  analogReadResolution(11);
  
  analogSetAttenuation(ADC_6db);
  adc1_config_width(ADC_WIDTH_BIT_10);

  String essid = ssid;
  String epassword = password;
  
  if (client_mode) 
  {
    // attempt WiFi client mode
    essid = "";
    epassword = "";

    if (EEPROM.read(128) != eepromSignature) {
      log("EEPROM not initialized");
    } else {
      for (int i = 0; i < 32; ++i)
      {
        essid += char(EEPROM.read(i));
      }
      
      Serial.print("EEPROM SSID: ");
      Serial.println(essid);
      for (int i = 32; i < 96; ++i)
      {
        epassword += char(EEPROM.read(i));
      }
    }

    if (essid.length() > 0) {
      // connect to EEPROM settings
    } else {
      // connect to hardcoded wifi
      essid = ssid;
      epassword = password;
    }
    
    Serial.print("\n\rConnecting to ");
    Serial.print(essid);
    WiFi.begin(essid.c_str(), epassword.c_str());
  
    int retries=50;
    while ((WiFi.status() != WL_CONNECTED) && (retries-- > 0)) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(essid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (!MDNS.begin("EspDRO")) {
      log("Error setting up MDNS responder.");
    } else {
      log("MDNS responder started");
    }
    
  } else {
      // fall-back to AP mode for setup
      Serial.println("WiFi connection failed, running in AP mode");
      WiFi.softAP(ssid, password);
  
      IPAddress myIP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(myIP);
  }
   
  server.on("/", handle_root);
    
  server.on("/peek", [](){    
    String webString = String(dro_buffer[dro_index].microns);
    server.send(200, "text/plain", webString); 
    delay(50);
  });
  
  
  server.on("/raw", [](){      
    unsigned int ts = 0;

    // ?ts=<micros> - 
    if(server.hasArg("ts")) {
      ts = server.arg("ts").toInt();
    }
    
    String rawJSON;
    for(size_t offset = 1; offset <= DRO_BUFFER_SIZE; offset++) {
      if (dro_buffer[(dro_index + offset) % DRO_BUFFER_SIZE].microns != MIN_RANGE) {
        rawJSON += "{\"axis0\":";
        rawJSON +=  dro_buffer[(dro_index + offset) % DRO_BUFFER_SIZE].microns;
        rawJSON += ",\"ts\":";
        rawJSON += dro_buffer[(dro_index + offset) % DRO_BUFFER_SIZE].timestamp;
        rawJSON += "}";
        yield();
      }
      //snprintf(buf, sizeof(buf), "{%d, %d}", dro_buffer[(dro_index + offset) % DRO_BUFFER_SIZE].timestamp, dro_buffer[(dro_index + offset) % DRO_BUFFER_SIZE].microns);
      //log(buf);
    }
    server.send(200, "text/plain", rawJSON);

  });
    
  server.on("/setup", [](){
    String content = "<!DOCTYPE HTML>\r\n<html>";
    content += "WiFi Setup<br></p><form method='get' action='settings'><label>SSID:</label><input name='ssid' length=32><br><label>Password:</label><input name='password' type=\"password\" length=64><br><input type=\"submit\" value=\"Save credentials\"></form><br><br><a href=\"/settings&reset=1\">Reset EEPROM</a></html>";
    server.send(200, "text/html", content);
  });
  
  server.on("/settings", [](){
    String lssid = server.arg("ssid");
    String lpassword = server.arg("password");
    String reset = server.arg("reset");
    if (lssid.length() > 0 && lpassword.length() > 0) {
      Serial.println("clearing eeprom");
      for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
      }
      
      Serial.print("Saving SSID=");
      for (int i = 0; i < lssid.length(); ++i)
      {
        EEPROM.write(i, lssid[i]);
        Serial.print(lssid[i]);
      }
                  
      Serial.println(" password=");
      for (int i = 0; i < lpassword.length(); ++i)
      {
        EEPROM.write(32+i, lpassword[i]);
        Serial.print(lpassword[i]); 
      }

      EEPROM.write(128, eepromSignature);
      EEPROM.commit();
      Serial.println("");
      Serial.println("EEPROM saved.");
      
      server.send(200, "application/json", "{\"Success\":\"WiFi settings saved to EEPROM. Reset to apply.\"}");
    }
    else if (reset.length() > 0) {
      String content = "<!DOCTYPE HTML>\r\n<html>";
      content += "<p>EEPROM settings cleared</p></html>";
      Serial.println("Clearing EEPROM.");
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }

      EEPROM.write(128, ~eepromSignature);
      
      EEPROM.commit();
      server.send(200, "text/html", content);
    } else {
      server.send(404, "application/json", "{\"Error\":\"404 not found\"}");
    }
  });
              
  server.begin();
  log("HTTP server started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);  
  log("WebSockets server started");

  xTaskCreate(spcTask, "spcTask", 10000, NULL, 1, NULL); 
  Serial.println("SPC Thread started");

  SPIFFS.begin();
  Serial.println("SPI Flash File System initialized");

  server.onNotFound([]() {
    if (!handleFS(server.uri()))
      server.send(404, "text/plain", "404: Not Found (forgot to upload SPIFFS data?)");
  });
   
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());

  Serial.println("Serial commands:\n    stop - stop the streaming of data\n    start - start the streaming of data");
}


size_t last_dro_index = 0;
void loop(){

  server.handleClient();
  webSocket.loop();

  if(Serial.available() > 0)
  {
      String serial_cmd = Serial.readStringUntil('\n');
      Serial.println("Received: cmd='" + serial_cmd + "'");

      if (serial_cmd.startsWith("stop")) {
        // serial_cmd.substring(14).toInt();
        stream_mode = 0;
      } else if (serial_cmd.startsWith("start")) {
        stream_mode = 1;
      }
  }

  // async dump the DRO stream over serial
  if (stream_mode && (last_dro_index != dro_index)) {
      log("{\"axis0\":%d,\"ts\":%d}", dro_buffer[last_dro_index].microns, dro_buffer[last_dro_index].timestamp);
      last_dro_index = (last_dro_index+1) % DRO_BUFFER_SIZE;
  }

}
