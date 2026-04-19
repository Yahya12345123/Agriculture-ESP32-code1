#include "BluetoothSerial.h"
#include "DHT.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

#define DHTPIN 4
#define DHTTYPE DHT22
#define SOIL_PIN 34
#define LDR_PIN 35
#define RELAY_FAN 33
#define RELAY_PUMP 25
#define RELAY_VAPOR 26

DHT dht(DHTPIN, DHTTYPE); 

BluetoothSerial SerialBT;

float temp, hum, soilPct;
int lightLux;

void setup() {
  Serial.begin(115200);
  
  dht.begin();

  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_VAPOR, OUTPUT);
  
  digitalWrite(RELAY_FAN, HIGH); 
  digitalWrite(RELAY_PUMP, HIGH);
  digitalWrite(RELAY_VAPOR, HIGH);

  SerialBT.begin("ESP32_Plant_Monitor"); 
  Serial.println("Bluetooth Started. Ready to pair.");
}

void loop() {
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  
  int soilRaw = analogRead(SOIL_PIN);
  soilPct = map(soilRaw, 4095, 1500, 0, 100); 
  soilPct = constrain(soilPct, 0, 100);

  int ldrRaw = analogRead(LDR_PIN);
  lightLux = map(ldrRaw, 0, 4095, 0, 30000); 

  if (temp > 30) digitalWrite(RELAY_FAN, LOW); 
  else if (temp < 28) digitalWrite(RELAY_FAN, HIGH);

  if (soilPct < 70) digitalWrite(RELAY_PUMP, LOW);
  else if (soilPct > 85) digitalWrite(RELAY_PUMP, HIGH);

  if (hum < 60) digitalWrite(RELAY_VAPOR, LOW);
  else if (hum > 75) digitalWrite(RELAY_VAPOR, HIGH);

  if (SerialBT.hasClient()) { 
    SerialBT.printf("Temp: %.1f C | Hum: %.1f %% | Soil: %.1f %% | Light: %d Lux\n", 
                    temp, hum, soilPct, lightLux);
  }

  Serial.printf("Temp: %.1fC | Hum: %.1f%% | Soil: %.1f%% | Light: %d Lux\n", 
                temp, hum, soilPct, lightLux);
  
  delay(2000); 
}