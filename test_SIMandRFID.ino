#include <HardwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 21  // SDA pin number
#define RST_PIN 22  // RST pin number

MFRC522 rfid(SS_PIN, RST_PIN);

#if defined(CONFIG_IDF_TARGET_ESP32)
    #define mySerial Serial2
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    #define mySerial Serial1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define mySerial Serial2
#endif

unsigned long lastReadTime = 0;
const unsigned long debounceDelay = 3000;
unsigned long lastGPSTime = 0;
const unsigned long gpsInterval = 15000;

int device_id = 1;

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
    sendCommand("AT+RST");  // Reset the module
    delay(20000);
    sendCommand("AT+CGPS=1");
    delay(500);
    sendCommand("AT+CGPSINFO=15");

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
            String decimalUIDStr = String(decimalUID);
            if (decimalUIDStr.length() == 9) {
                decimalUIDStr = "0" + decimalUIDStr;
            }
            Serial.println("Card UID: " + decimalUIDStr);
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
        String date = values[4];
        String time = values[5];
        
        double lat = convertToDecimal(latitude.toFloat(), latitudeDirection);
        double lon = convertToDecimal(longitude.toFloat(), longitudeDirection);
        String dateTime = formatDateTime(date, time);

        Serial.print("Location: ");
        Serial.print(lat, 6);
        Serial.print(",");
        Serial.println(lon, 6);
        Serial.print("Speed: ");
        Serial.println(speed);
        Serial.print("Time: ");
        Serial.println(dateTime);
        Serial.print("\n");
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

String formatDateTime(String date, String time) {
    String day = date.substring(0, 2);
    String month = date.substring(2, 4);
    String year = "20" + date.substring(4, 6);
    
    int hour = time.substring(0, 2).toInt();
    int minute = time.substring(2, 4).toInt();
    int second = time.substring(4, 6).toInt();
    
    hour += 7;
    if (hour >= 24) {
        hour -= 24;
        day = String(day.toInt() + 1);
    }

    char dateTime[20];
    sprintf(dateTime, "%s-%s-%s %02d:%02d:%02d", year.c_str(), month.c_str(), day.c_str(), hour, minute, second);
    return String(dateTime);
}

void sendCommand(const char *command) {
    mySerial.println(command);
    delay(500);
}
