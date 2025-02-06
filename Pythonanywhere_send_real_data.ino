#define TINY_GSM_MODEM_SIM7600
#include <SPI.h>
#include <MFRC522.h>
#include <HttpClient.h>
#include <ArduinoJson.h>
#include <TinyGsmClient.h>
#include <HardwareSerial.h>


#if defined(CONFIG_IDF_TARGET_ESP32)
    #define mySerial Serial2
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    #define mySerial Serial1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define mySerial Serial2
#endif

#define SS_PIN 21  // SDA pin
#define RST_PIN 22  // RST pin

MFRC522 rfid(SS_PIN, RST_PIN);

unsigned long lastReadTime = 0;
const unsigned long debounceDelay = 3000;
unsigned long lastGPSTime = 0;
const unsigned long gpsInterval = 30000;

int device_id = 1;
const char apn[] = "m3-world";
const char gprsUser[] = "mms";
const char gprsPass[] = "mms";

const char server[] = "diennd25.pythonanywhere.com";
const int port = 80;

TinyGsm modem(mySerial);
TinyGsmClient client(modem);
HttpClient http(client, server, port);

void setup() {
  Serial.begin(115200);
  delay(1000);

  #if defined(CONFIG_IDF_TARGET_ESP32)
      mySerial.begin(115200);
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
      mySerial.begin(115200);
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
      mySerial.begin(115200, SERIAL_8N1, 18, 17);
  #endif

  delay(500);
  sendCommand("AT+RST");
  modem.restart();
  delay(30000);
  sendCommand("AT+CGPS=1");
  delay(500);
  sendCommand("AT+CGPSINFO=30");

  delay(1000);
  SPI.begin();
  rfid.PCD_Init();
  delay(500);

  modem.restart();

  Serial.print("Connecting to APN: ");
  Serial.println(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("Failed to connect to GPRS");
    while (true);
  }
  Serial.println("Connected to GPRS");
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastGPSTime > gpsInterval) {
    lastGPSTime = currentTime;
    //Serial.println("Sending GPS data...");
    update_serial();
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (currentTime - lastReadTime > debounceDelay) {
      lastReadTime = currentTime;

      String uidString = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        uidString += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
      }

      String reversedUID = "";
      for (int i = uidString.length() - 1; i >= 0; i -= 2) {
        reversedUID += uidString.substring(i - 1, i + 1);
      }

      unsigned long decimalUID = strtoul(reversedUID.c_str(), nullptr, 16);
      String decimalUIDStr = String(decimalUID);
      //if (decimalUIDStr.length() == 9) {
       // decimalUIDStr = "0" + decimalUIDStr;
      //}
      
      JsonDocument jsonDoc;
      jsonDoc["did"] = device_id;
      jsonDoc["uid"] = decimalUIDStr;

      String jsonData;
      serializeJson(jsonDoc, jsonData);
      Serial.println("RFID JSON Data: " + jsonData);

      sendPostRequest(jsonData, "/api/receive-rfid/");
    }
    rfid.PICC_HaltA();
  }
}

void update_serial() {
  static String response = "";
  while (mySerial.available()) {
    char c = mySerial.read();
    if (c == '\n') {
      if (response.startsWith("+CGPSINFO:")) {
        parseGPSInfo(response);
      }
      response = "";
    } else {
      response += c;
    }
  }
}

void parseGPSInfo(const String &response) {
  int startIndex = response.indexOf(':') + 1;
  String data = response.substring(startIndex);
  data.trim();
  
  if (data.length() > 0) {
    int idx = 0;
    String values[9];
    for (int i = 0; i < 9; i++) {
      int nextComma = data.indexOf(',', idx);
      if (nextComma == -1) {
        values[i] = data.substring(idx);
        break;
      } else {
        values[i] = data.substring(idx, nextComma);
        idx = nextComma + 1;
      }
    }
    
    String latitude = values[0];
    String latitudeDirection = values[1];
    String longitude = values[2];
    String longitudeDirection = values[3];
    String speed = values[7];
    
    double lat = convertToDecimal(latitude.toFloat(), latitudeDirection);
    double lon = convertToDecimal(longitude.toFloat(), longitudeDirection);

    JsonDocument jsonDoc;
    jsonDoc["did"] = device_id;
    jsonDoc["loc"] = String(lat, 6) + "," + String(lon, 6);
    jsonDoc["spd"] = speed;

    String jsonData;
    serializeJson(jsonDoc, jsonData);
    Serial.println("GPS JSON Data: " + jsonData);

    sendPostRequest(jsonData, "/api/receive-gps/");
  }
}

double convertToDecimal(float value, String direction) {
  int degrees = int(value / 100);
  float minutes = value - (degrees * 100);
  float decimal = degrees + minutes / 60.0;
  
  if (direction == "S" || direction == "W") {
    decimal = -decimal;
  }
  
  return decimal;
}

void sendPostRequest(const String &data, const char *path) {
  http.beginRequest();
  http.post(path);
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Content-Length", data.length());
  http.beginBody();
  http.print(data);
  http.endRequest();

  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  if (statusCode != 200) {
    Serial.println("Error: Failed to send data");
    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.println("Data sent successfully");
  }
}

void sendCommand(const char *command) {
  mySerial.println(command);
  delay(500);
}
