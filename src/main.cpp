#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <PubSubClient.h>

const char* ssid = "Navn på nettverk";
const char* password = "passord på nettverk";
const char* mqttServer = "IP_ADRESSE_TIL_BROKER"; // Legg til IP-adressen til MQTT-brokeren
const int mqttPort = 1883; // MQTT-brokerens portnummer
const char* mqttUser = "BRUKERNAVN"; // Legg til brukernavnet for MQTT-brokeren
const char* mqttPassword = "PASSORD"; // Legg til passordet for MQTT-brokeren

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

int port = 80;
WebServer server(port);

const int led = LED_BUILTIN;

#define DHTPIN 22
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

float asoilmoist=analogRead(32);//global variable to store exponential smoothed soil moisture reading

// Function to calculate moisture percentage
float calculateMoisturePercentage(float moisture) {
  float dryValue = 3437.87;
  float wetValue = 1367.7;
  
  float moisturePercentage = ((moisture - dryValue) / (wetValue - dryValue)) * 100.0;
  return moisturePercentage;
}

void handleRoot() {
  digitalWrite(led, HIGH);
  
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  float hum = dht.readHumidity();
  float temp = dht.readTemperature(); // Read temperature in Celsius (the default)
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(hum) || isnan(temp)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

   // Update soil moisture value
  asoilmoist = 0.95 * asoilmoist + 0.05 * analogRead(32); // Exponential smoothing of soil moisture

  float moisturePercentage = calculateMoisturePercentage(asoilmoist);
  String webtext = "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP32 WIFI SOIL MOISTURE SENSOR</title>\
    <style>\
      body { background-color: #f2f2f2; font-family: Arial, Helvetica, sans-serif; color: #333; }\
      h1 { text-align: center; }\
      p { margin-bottom: 10px; }\
      .reading { font-size: 18px; font-weight: bold; }\
      .datetime { font-size: 14px; color: #666; }\
    </style>\
  </head>\
  <body>\
    <h1>ESP32 WIFI SOIL MOISTURE SENSOR</h1>\
    <br>\
    <p>Date/Time: <span id='datetime'></span></p>\
    <script>\
      var dt = new Date();\
      document.getElementById('datetime').innerHTML = (('0' + dt.getDate()).slice(-2)) + '.' + (('0' + (dt.getMonth() + 1)).slice(-2)) + '.' + dt.getFullYear() + ' ' + (('0' + dt.getHours()).slice(-2)) + ':' + (('0' + dt.getMinutes()).slice(-2));\
    </script>\
    <br>\
    <p class='reading'>Soil Moisture: " + String(asoilmoist) + "</p>\
    <p class='reading'>Moisture Percentage: " + String(moisturePercentage) + "%</p>\
    <p class='reading'>Temperature: " + String(temp) + " &#176;C</p>\
    <p class='reading'>Humidity: " + String(hum) + " %</p>\
  </body>\
</html>";

  server.send(200, "text/html", webtext);

  // Publish data to MQTT broker
  String mqttPayload = String(moisturePercentage);
  mqttClient.publish("soil-moisture", mqttPayload.c_str());
}

void handleNotFound() {
  digitalWrite(led, HIGH);
  
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
  digitalWrite(led, LOW);
  delay(1000);
  digitalWrite(led, HIGH);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectToMQTTBroker() {
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
    // Handle MQTT message callback, if needed
  });

  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void reconnectToMQTTBroker() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("Reconnected to MQTT broker");
    } else {
      Serial.print("Failed to reconnect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  Serial.begin(115200);

  setupWiFi();
  connectToMQTTBroker();

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  dht.begin();
  delay(2000);
}

void loop() {
 asoilmoist = 0.9 * asoilmoist + 0.1 * analogRead(32); // Exponential smoothing of soil moisture
  server.handleClient();
}