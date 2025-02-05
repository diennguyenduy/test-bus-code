#define TINY_GSM_MODEM_SIM7600

#include <TinyGsmClient.h>
#include <HttpClient.h>
#include <HardwareSerial.h>

#if defined(CONFIG_IDF_TARGET_ESP32)
    #define mySerial Serial2
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    #define mySerial Serial1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define mySerial Serial2
#endif

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
  delay(100);

  mySerial.begin(115200);
  delay(5000);

  modem.restart();
  Serial.println("Modem restarted");

  Serial.print("Connecting to APN: ");
  Serial.println(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("Failed to connect to GPRS");
    while (true);
  }
  Serial.println("Connected to GPRS");
}

void loop() {
  // Example GPS data
  int device_id = 1;
  String location = "21.0278,105.8342";
  String speed = "50";

  String postData = "{\"device_id\":\"" + String(device_id) + "\",\"location\":\"" + location + "\",\"speed\":\"" + speed + "\"}";

  http.beginRequest();
  http.post("/api/receive-gps/");
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Content-Length", postData.length());
  http.beginBody();
  http.print(postData);
  http.endRequest();

  // Get the response from the server
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  // Log the status code and response
  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  // Check if the request was successful
  if (statusCode != 200) {
    Serial.println("Error: Failed to send GPS data");
    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.println("GPS data sent successfully");
  }

  // Wait for a few seconds before sending the next data
  delay(10000);
}
