// Library Installation
#include "BluetoothSerial.h"
#include "DHT.h"

// Bluetooth Configuration
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

// Defining ESP32 Pins for sensors, relays of actuators
#define DHTPIN 4
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define LDR_PIN 35
#define RELAY_FAN 33
#define RELAY_PUMP 25
#define RELAY_HEATER 27
#define LED_R 13
#define LED_B 14

// PWM (Pulse Width Modulation) for smoother signals 
const int freq = 5000; // 5 kHz
const int resolution = 8; // Brighteness level to control

DHT dht(DHTPIN, DHTTYPE);  // DHT11 Settings
BluetoothSerial SerialBT; // Bluetooth Connection

// Plant settings structure (Main Actuators actions)
struct PlantSettings {
  char id;           
  float fanOn;
  float fanOff;
  float pumpOn;
  float pumpOff;
  float heaterOn;
  float heaterOff;
};

// Setpoints for each plant (Ammi visnaga, Ginger, Tumeric) to on or off
PlantSettings plantList[] = {
  {'A', 28, 24, 50, 70, 18, 22}, 
  {'G', 32, 28, 60, 80, 20, 24}, 
  {'T', 30, 26, 40, 65, 22, 26}  
};

const int numPlants = sizeof(plantList) / sizeof(plantList[0]); // Number of plants to select in database

float temp, hum, soilPct, lux;

// Default actuators actions without plant selection 
float fanOnTemp = 30, fanOffTemp = 25;
float pumpOnSoil = 50, pumpOffSoil = 70;
float heaterOnTemp = 18, heaterOffTemp = 22;

// Default state of each actuator
bool fanState = false;
bool pumpState = false;
bool heaterState = false;

// Relay Controller according to state of actuator
void setRelay(int pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
}

// Setting default serial monitor, functionating  relays, sensors, led settings
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);

  setRelay(RELAY_FAN, false);
  setRelay(RELAY_PUMP, false);
  setRelay(RELAY_HEATER, false);

  // PWM on these pins (LED_R and LED_B) with the given frequency and resolution.
  ledcAttach(LED_R, freq, resolution);
  ledcAttach(LED_B, freq, resolution);

  SerialBT.begin("ESP32_Plant_Monitor"); 
}

// Main loop
void loop() {
  temp = dht.readTemperature(); // temperature reading by dht
  hum  = dht.readHumidity(); // humidity reading by dht
  
  // make a delay 2 seconds if there is no value for humidity or temperature
  if (isnan(temp) || isnan(hum)) {
    delay(2000);
    return;
  }

  // Soil moisture readings settings
  int soilRaw = analogRead(SOIL_PIN); // Reads the soil moisture sensor as an analog value
  soilPct = map(soilRaw, 4095, 1500, 0, 100); // Calibrating voltage from sensor to readable percent (another range) (Range = 0 → 4095 (12-bit ADC))
  soilPct = constrain(soilPct, 0, 100); // Limiting the value from 0 to 100 percent

  // Setting light dependant resistor and its calibration for lux value
  int ldrRaw = analogRead(LDR_PIN);
  float normalized = (float)ldrRaw / 4095.0; // converting range of voltage to normal value
  float corrected = pow(normalized, 2.0);   // realistic scaling
  lux = corrected * 30000; // approximation for lux value
  lux = constrain(lux, 0, 30000); // constrains in its values
  int ledBrightness = constrain(map(ldrRaw, 0, 4095, 255, 0), 0, 255); // Calibrating voltage from sensor to readable value (another range) (Range = 0 → 4095 (12-bit ADC))
  // Controlling Red, Blue LEDs Brightness
  ledcWrite(LED_R, ledBrightness);
  ledcWrite(LED_B, ledBrightness);

  // Setting values according to plant from database
  if (SerialBT.available()) {
    char c = SerialBT.read();
    for (int i = 0; i < numPlants; i++) {
      if (c == plantList[i].id) {
        fanOnTemp     = plantList[i].fanOn;
        fanOffTemp    = plantList[i].fanOff;
        pumpOnSoil    = plantList[i].pumpOn;
        pumpOffSoil   = plantList[i].pumpOff;
        heaterOnTemp  = plantList[i].heaterOn;
        heaterOffTemp = plantList[i].heaterOff;
      }
    }
  }

  // setting the state of each actuator according to workability
  if (temp > fanOnTemp) fanState = true;
  else if (temp < fanOffTemp) fanState = false;

  if (temp < heaterOnTemp) heaterState = true;
  else if (temp > heaterOffTemp) heaterState = false;

  if (fanState) heaterState = false;
  if (heaterState) fanState = false;

  if (soilPct < pumpOnSoil) pumpState = true;
  else if (soilPct > pumpOffSoil) pumpState = false;

  // Actuating relay modules according to sent working state of each actuator
  setRelay(RELAY_FAN, fanState);
  setRelay(RELAY_HEATER, heaterState);
  setRelay(RELAY_PUMP, pumpState);

  // printing values for serial monitor and sending to the application
  Serial.printf("T:%.1f H:%.1f S:%.1f L:%.1f | F:%d H:%d P:%d\n",
                temp, hum, soilPct, lux,
                fanState, heaterState, pumpState);

  if (SerialBT.hasClient()) {
    SerialBT.printf("T:%.1f H:%.1f S:%.1f L:%.1f\n", temp, hum, soilPct, lux);
  }

  delay(2000); // delat 2 seconds between each reading by esp32
}
