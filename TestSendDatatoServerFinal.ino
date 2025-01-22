#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define SS_PIN 21  // SDA pin
#define RST_PIN 22  // RST pin

MFRC522 rfid(SS_PIN, RST_PIN);

#if defined(CONFIG_IDF_TARGET_ESP32)
    #define mySerial Serial2
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    #define mySerial Serial1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define mySerial Serial2
#endif

const char apn[] = "m3-world";
const char user[] = "mms";
const char pass[] = "mms";
const char* serverUrl = "https://test-bus-api.onrender.com/api/";

TinyGsm modem(mySerial);
TinyGsmClient client(modem); // Using TinyGsmClientSecure for HTTPS

unsigned long lastReadTime = 0;
const unsigned long debounceDelay = 3000;
unsigned long lastGPSTime = 0;
const unsigned long gpsInterval = 15000;

int device_id = 1;

void setup() {
    Serial.begin(115200);
    delay(1000);

    mySerial.begin(115200);
    delay(4000);

    Serial.println("Initializing modem...");
    modem.restart();
    String modemInfo = modem.getModemInfo();
    Serial.println("Modem: " + modemInfo);

    Serial.print("Connecting to APN: ");
    Serial.println(apn);
    if (!modem.gprsConnect(apn, user, pass)) {
        Serial.println("Failed to connect to GPRS");
        while (true);
    }
    Serial.println("GPRS connected");

    delay(15000);
    sendCommand("AT+CGPS=1"); // Enable GPS
    delay(500);
    sendCommand("AT+CGPSINFO=15");

    delay(500);
    SPI.begin();
    rfid.PCD_Init();
}

void loop() {
    unsigned long currentTime = millis();

    if (currentTime - lastGPSTime > gpsInterval) {
        lastGPSTime = currentTime;
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
            // String decimalUIDStr = String(decimalUID);
            // if (decimalUIDStr.length() == 9) {
            //     decimalUIDStr = "0" + decimalUIDStr;
            // }

            JsonDocument jsonDoc;
            jsonDoc["id"] = device_id;
            jsonDoc["UID"] = decimalUID;

            String jsonData;
            serializeJson(jsonDoc, jsonData);
            Serial.println("RFID JSON Data: " + jsonData);

            sendToServer(jsonData, "receive-rfid/");
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

        if (!latitude.isEmpty() && !longitude.isEmpty()) {
            double lat = convertToDecimal(latitude.toFloat(), latitudeDirection);
            double lon = convertToDecimal(longitude.toFloat(), longitudeDirection);

            // Pack GPS data into minimized JSON and print
            JsonDocument jsonDoc;
            jsonDoc["id"] = device_id;
            jsonDoc["loc"] = String(lat, 6) + "," + String(lon, 6);
            jsonDoc["spd"] = speed;

            String jsonData;
            serializeJson(jsonDoc, jsonData);
            Serial.println("GPS JSON Data: " + jsonData);

            sendToServer(jsonData, "receive-gps/");
        }
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

void sendCommand(const char *command) {
    mySerial.println(command);
    delay(500);
}

void sendToServer(const String &jsonData, const String &endpoint) {
    if (modem.isGprsConnected()) {
        HTTPClient http;
        String fullUrl = String(serverUrl) + endpoint;
        http.begin(fullUrl); // HTTP

        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(jsonData);
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Server Response: " + response);
        } else {
            Serial.println("Error on sending POST: " + String(httpResponseCode));
        }
        http.end();
    } else {
        Serial.println("GPRS not connected");
        modem.gprsConnect(apn, user, pass); // Try to reconnect
    }
}
