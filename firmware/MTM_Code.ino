#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <MHZ19.h>
#include <HardwareSerial.h>

// WiFi credentials
const char* ssid = “SSID”;
const char* password = “password”;

// ThingSpeak settings
const char* thingspeak_server = "http://api.thingspeak.com/update";
const char* apiKey = "";  // Write API key
const char* readAPIKey = "";  // Read API key
const char* channelID = "";

// Hardware pins
#define RX_PIN 16
#define TX_PIN 17
#define FAN_PIN 25

// Thresholds for Lions Mane
#define CO2_THRESHOLD 800      // ppm
#define HUMIDITY_THRESHOLD 93  // % - Lions Mane likes 85-95%, fan on if >88%
#define TEMP_THRESHOLD 75      // °F - Lions Mane likes 65-75°F

// Sensors
MHZ19 myMHZ19;
HardwareSerial mySerial(2);
Adafruit_SHT31 sht31 = Adafruit_SHT31();  // Using one sensor for now
WebServer server(80);  // Local web server

// Global variables
int co2 = 0;
float temperature = 0, humidity = 0;
bool fanOn = false;
unsigned long lastReading = 0;
unsigned long lastUpload = 0;
unsigned long lastFanOn = 0;
String lastFanOnTime = "Never";

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Mushroom Monitor with ThingSpeak...");
  
  // Initialize I2C
  Wire.begin(21, 22);  // Default I2C pins
  
  // Scan for I2C devices
  Serial.println("\nScanning for I2C devices...");
  byte error, address;
  int nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address<16) Serial.print("0");
      Serial.println(address,HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) Serial.println("No I2C devices found\n");
  else Serial.println("I2C scan done\n");
  
  // Initialize SHT31 sensor
  if (!sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31 at 0x44");
    // Try alternate address
    if (!sht31.begin(0x45)) {
      Serial.println("Couldn't find SHT31 at 0x45 either");
    }
  }
  
  // Initialize CO2 sensor
  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  myMHZ19.begin(mySerial);
  myMHZ19.autoCalibration(false);
  
  // Initialize fan
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, HIGH);  // Fan OFF (reversed)
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("\nView data at: https://thingspeak.com/channels/" + String(channelID));
  Serial.println("Or create channel at: https://thingspeak.com/channels/new");
  
  // Setup web server
  server.on("/", handleRoot);
  server.on("/json", handleJSON);
  server.on("/fan/on", handleFanOn);
  server.on("/fan/off", handleFanOff);
  server.on("/fan/auto", handleFanAuto);
  server.begin();
  
  Serial.println("\nLocal web server at: http://" + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();  // Handle web requests
  
  // Read sensors every 5 seconds
  if (millis() - lastReading > 5000) {
    readSensors();
    controlFan();
    lastReading = millis();
  }
  
  // Upload to ThingSpeak every 30 seconds (free account limit: 15 sec)
  if (millis() - lastUpload > 30000) {
    uploadToThingSpeak();
    lastUpload = millis();
  }
}

void readSensors() {
  // Read CO2
  co2 = myMHZ19.getCO2();
  
  // Validate CO2 reading
  if (co2 < 400 || co2 > 5000) {
    Serial.print("Invalid CO2 reading: ");
    Serial.println(co2);
    co2 = 400;  // Use minimum atmospheric CO2
  }
  
  // Read temperature and humidity only if sensor is connected
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
  
  if (!isnan(t) && !isnan(h)) {
    temperature = t * 9/5 + 32;  // Convert to °F
    humidity = h;
  } else {
    // Use default values if sensor not working
    temperature = 72.0;  // Default temp
    humidity = 85.0;     // Default humidity
    Serial.println("SHT31 not responding - using defaults");
  }
  
  // Print readings
  Serial.println("\n--- Sensor Readings ---");
  Serial.print("CO2: "); Serial.print(co2); Serial.println(" ppm");
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println("°F");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.println("%");
  Serial.print("Fan: "); Serial.println(fanOn ? "ON" : "OFF");
}

void controlFan() {
  bool shouldFanBeOn = false;
  String reason = "";
  
  // Check all conditions
  if (co2 > CO2_THRESHOLD) {
    shouldFanBeOn = true;
    reason = "High CO2 (" + String(co2) + " ppm)";
  }
  else if (humidity > HUMIDITY_THRESHOLD) {
    shouldFanBeOn = true;
    reason = "High humidity (" + String(humidity,1) + "%)";
  }
  else if (temperature > TEMP_THRESHOLD) {
    shouldFanBeOn = true;
    reason = "High temp (" + String(temperature,1) + "°F)";
  }
  
  // Control fan
  if (shouldFanBeOn && !fanOn) {
    digitalWrite(FAN_PIN, LOW);  // Turn ON (reversed)
    fanOn = true;
    lastFanOn = millis();
    lastFanOnTime = getTimeString();
    Serial.println("Fan ON - " + reason);
  }
  else if (!shouldFanBeOn && fanOn) {
    digitalWrite(FAN_PIN, HIGH);  // Turn OFF (reversed)
    fanOn = false;
    Serial.println("Fan OFF - All conditions normal");
  }
}

void uploadToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // ThingSpeak fields - SWAPPED TO MATCH YOUR SETUP:
    // Field 1: Temperature (showing as "CO2" on ThingSpeak)
    // Field 2: Humidity (showing as "Temperature" on ThingSpeak)  
    // Field 3: CO2 (showing as "Humidity" on ThingSpeak)
    // Field 4: Fan Status
    // Field 5: Time since fan on (minutes)
    
    int minutesSinceFan = lastFanOn > 0 ? (millis() - lastFanOn) / 60000 : -1;
    
    String url = String(thingspeak_server) + "?api_key=" + apiKey +
                 "&field1=" + String(temperature, 1) +
                 "&field2=" + String(humidity, 1) +
                 "&field3=" + String(co2) +
                 "&field4=" + String(fanOn ? 1 : 0) +
                 "&field5=" + String(minutesSinceFan);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      Serial.println("Data sent to ThingSpeak! Response: " + String(httpCode));
      
      // Also create a simple web view
      Serial.println("\nPublic view URL:");
      Serial.println("https://thingspeak.com/channels/" + String(channelID) + "/private_show");
    } else {
      Serial.println("Error sending to ThingSpeak: " + String(httpCode));
    }
    
    http.end();
  }
}

String getTimeString() {
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  if (days > 0) {
    return String(days) + "d " + String(hours % 24) + "h ago";
  } else if (hours > 0) {
    return String(hours) + "h " + String(minutes % 60) + "m ago";
  } else if (minutes > 0) {
    return String(minutes) + "m ago";
  } else {
    return String(seconds) + "s ago";
  }
}

// Web server handlers
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Mushroom Monitor</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='10'>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;margin:20px;background:#f0f0f0;}";
  html += ".container{max-width:600px;margin:auto;background:white;padding:30px;border-radius:15px;box-shadow:0 4px 15px rgba(0,0,0,0.1);}";
  html += "h1{color:#333;margin-bottom:30px;}";
  html += ".sensor-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:20px;margin:20px 0;}";
  html += ".sensor-card{background:#f8f9fa;padding:20px;border-radius:10px;border-left:4px solid #2196F3;}";
  html += ".sensor-value{font-size:36px;font-weight:bold;margin:10px 0;}";
  html += ".sensor-label{color:#666;font-size:14px;}";
  html += ".co2{border-left-color:" + String(co2 > 1000 ? "#f44336" : co2 > 800 ? "#ff9800" : "#4CAF50") + ";}";
  html += ".temp{border-left-color:" + String(temperature > TEMP_THRESHOLD ? "#f44336" : "#2196F3") + ";}";
  html += ".humidity{border-left-color:" + String(humidity > HUMIDITY_THRESHOLD ? "#f44336" : "#03a9f4") + ";}";
  html += ".status{font-size:20px;margin:20px 0;padding:15px;border-radius:10px;}";
  html += ".fan-on{background:#c8e6c9;color:#2e7d32;}";
  html += ".fan-off{background:#ffcdd2;color:#c62828;}";
  html += "button{font-size:18px;padding:12px 24px;margin:5px;border:none;border-radius:8px;cursor:pointer;}";
  html += ".btn-on{background:#4CAF50;color:white;}";
  html += ".btn-off{background:#f44336;color:white;}";
  html += ".btn-auto{background:#2196F3;color:white;}";
  html += ".info{margin-top:20px;padding:15px;background:#e3f2fd;border-radius:8px;font-size:14px;}";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h1>🍄 Mushroom Monitor</h1>";
  
  html += "<div class='sensor-grid'>";
  html += "<div class='sensor-card co2'>";
  html += "<div class='sensor-label'>CO2</div>";
  html += "<div class='sensor-value'>" + String(co2) + "</div>";
  html += "<div class='sensor-label'>ppm</div>";
  html += "</div>";
  
  html += "<div class='sensor-card temp'>";
  html += "<div class='sensor-label'>Temperature</div>";
  html += "<div class='sensor-value'>" + String(temperature, 1) + "°</div>";
  html += "<div class='sensor-label'>Fahrenheit</div>";
  html += "</div>";
  
  html += "<div class='sensor-card humidity'>";
  html += "<div class='sensor-label'>Humidity</div>";
  html += "<div class='sensor-value'>" + String(humidity, 1) + "%</div>";
  html += "<div class='sensor-label'>RH</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='status " + String(fanOn ? "fan-on" : "fan-off") + "'>";
  html += "Fan: " + String(fanOn ? "ON" : "OFF");
  html += " | Last ON: " + lastFanOnTime;
  html += "</div>";
  
  html += "<div style='margin:20px 0;'>";
  html += "<button class='btn-on' onclick=\"fetch('/fan/on').then(()=>location.reload())\">Turn ON</button>";
  html += "<button class='btn-off' onclick=\"fetch('/fan/off').then(()=>location.reload())\">Turn OFF</button>";
  html += "<button class='btn-auto' onclick=\"fetch('/fan/auto').then(()=>location.reload())\">Auto Mode</button>";
  html += "</div>";
  
  html += "<div class='info'>";
  html += "<strong>Thresholds:</strong> CO2 > " + String(CO2_THRESHOLD) + " ppm | ";
  html += "Temp > " + String(TEMP_THRESHOLD) + "°F | ";
  html += "Humidity > " + String(HUMIDITY_THRESHOLD) + "%<br>";
  html += "<strong>ThingSpeak:</strong> <a href='https://thingspeak.com/channels/" + String(channelID) + "' target='_blank'>View Charts</a>";
  html += "</div>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleJSON() {
  String json = "{";
  json += "\"co2\":" + String(co2) + ",";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"fan\":" + String(fanOn ? "true" : "false") + ",";
  json += "\"lastFanOn\":\"" + lastFanOnTime + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleFanOn() {
  digitalWrite(FAN_PIN, LOW);  // Turn ON (reversed)
  fanOn = true;
  lastFanOn = millis();
  lastFanOnTime = "Manual ON";
  server.send(200, "text/plain", "Fan turned ON");
}

void handleFanOff() {
  digitalWrite(FAN_PIN, HIGH);  // Turn OFF (reversed)
  fanOn = false;
  server.send(200, "text/plain", "Fan turned OFF");
}

void handleFanAuto() {
  server.send(200, "text/plain", "Auto mode active");
}
