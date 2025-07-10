#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define DHTPIN        5
#define DHTTYPE       DHT11
#define SDA_PIN       21
#define SCL_PIN       22
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
}

void loop() {
  float humidity = dht.readHumidity();
  float tempC    = dht.readTemperature();
  if (!isnan(humidity) && !isnan(tempC)) {
    float tempF = tempC * 9.0 / 5.0 + 32.0;
    Serial.printf("T:%.1f°F H:%.1f%%\n", tempF, humidity);

    display.clearDisplay();
    display.setTextSize(2);

    display.setCursor(0, 0);
    display.print("T:");
    display.print(tempF, 1);
    display.print("F");

    display.setCursor(0, 32);
    display.print("H:");
    display.print(humidity, 0);
    display.print("%");

    display.display();
  }
  delay(2000);
}
