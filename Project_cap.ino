#include "BluetoothSerial.h"
#include "DHT.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

// --- تعريف الدبابيس ---
#define DHTPIN 4
#define DHTTYPE DHT22
#define SOIL_PIN 34
#define LDR_PIN 35
#define RELAY_FAN 33
#define RELAY_PUMP 25
#define RELAY_VAPOR 26
#define RELAY_HEATER 27
#define RELAY_NEW 32
#define LED_R 13
#define LED_B 14

const int freq = 5000;
const int resolution = 8; 

DHT dht(DHTPIN, DHTTYPE); 
BluetoothSerial SerialBT;

struct PlantSettings {
  char id;           
  float fanOn;
  float fanOff;
  float pumpOn;
  float pumpOff;
  float vaporOn;
  float vaporOff;
  float heaterOn;
  float heaterOff;
};

// إعدادات النباتات
PlantSettings plantList[] = {
  {'A', 28, 24, 50, 70, 40, 60, 18, 22}, 
  {'G', 32, 28, 60, 80, 50, 70, 20, 24}, 
  {'T', 30, 26, 40, 65, 60, 80, 22, 26}  
};

const int numPlants = sizeof(plantList) / sizeof(plantList[0]);

float temp, hum, soilPct;

// القيم الافتراضية
float fanOnTemp = 30, fanOffTemp = 25;
float pumpOnSoil = 50, pumpOffSoil = 70;
float vaporOnHum = 80, vaporOffHum = 90; 
float heaterOnTemp = 18, heaterOffTemp = 22;

bool fanState = false;
bool pumpState = false;
bool vaporState = false;
bool heaterState = false;
bool lastVaporState = false; 

void setRelay(int pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_VAPOR, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_NEW, OUTPUT);

  setRelay(RELAY_FAN, false);
  setRelay(RELAY_PUMP, false);
  setRelay(RELAY_VAPOR, false);
  setRelay(RELAY_HEATER, false);
  setRelay(RELAY_NEW, false);

  ledcAttach(LED_R, freq, resolution);
  ledcAttach(LED_B, freq, resolution);

  SerialBT.begin("ESP32_Plant_Monitor"); 
  Serial.println("System Ready!");
}

void loop() {
  // 1. قراءة الحساسات
  temp = dht.readTemperature();
  hum  = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    delay(2000);
    return;
  }

  int soilRaw = analogRead(SOIL_PIN);
  soilPct = map(soilRaw, 4095, 1500, 0, 100);
  soilPct = constrain(soilPct, 0, 100);

  int ldrRaw = analogRead(LDR_PIN);
  int ledBrightness = constrain(map(ldrRaw, 0, 4095, 255, 0), 0, 255);

  ledcWrite(LED_R, ledBrightness);
  ledcWrite(LED_B, ledBrightness);

  // 2. استقبال أوامر البلوتوث
  if (SerialBT.available()) {
    char c = SerialBT.read();
    for (int i = 0; i < numPlants; i++) {
      if (c == plantList[i].id) {
        fanOnTemp     = plantList[i].fanOn;
        fanOffTemp    = plantList[i].fanOff;
        pumpOnSoil    = plantList[i].pumpOn;
        pumpOffSoil   = plantList[i].pumpOff;
        vaporOnHum    = plantList[i].vaporOn;
        vaporOffHum   = plantList[i].vaporOff;
        heaterOnTemp  = plantList[i].heaterOn;
        heaterOffTemp = plantList[i].heaterOff;
      }
    }
  }

  // 3. منطق التحكم (Logic)
  
  // المروحة
  if (temp > fanOnTemp) fanState = true;
  else if (temp < fanOffTemp) fanState = false;

  // السخان
  if (temp < heaterOnTemp) heaterState = true;
  else if (temp > heaterOffTemp) heaterState = false;

  if (fanState) heaterState = false; 
  if (heaterState) fanState = false;

  // المضخة
  if (soilPct < pumpOnSoil) pumpState = true;
  else if (soilPct > pumpOffSoil) pumpState = false;

  // الفواحة (تبخير)
  if (hum < vaporOnHum) {
    vaporState = true;
  } else if (hum > vaporOffHum) {
    vaporState = false;
  }

  // 4. تنفيذ الأوامر على الريليهات
  setRelay(RELAY_FAN, fanState);
  setRelay(RELAY_HEATER, heaterState);
  setRelay(RELAY_PUMP, pumpState);
  
  // تشغيل ريلاي البخار أولاً
  setRelay(RELAY_VAPOR, vaporState);

  // --- التعديل المطلوب: نبضة الريلاي الجديد بتأخير بسيط ---
  if (vaporState == true && lastVaporState == false) {
    
    delay(200); // تأخير صغير (200ms) بعد تشغيل ريلاي البخار وقبل النبضة
    
    Serial.println("Vaporizer Started: Delay finished, sending 300ms Pulse...");
    setRelay(RELAY_NEW, true);  
    delay(300);                 // مدة النبضة (الضغط)
    setRelay(RELAY_NEW, false); 
  }
  
  // تحديث الحالة السابقة
  lastVaporState = vaporState;

  // 5. الطباعة للمراقبة
  Serial.printf("T:%.1f H:%.1f S:%.1f | F:%d H:%d P:%d V:%d\n",
                temp, hum, soilPct,
                fanState, heaterState, pumpState, vaporState);

  if (SerialBT.hasClient()) {
    SerialBT.printf("T:%.1f H:%.1f S:%.1f L:%.1f\n", temp, hum, soilPct, ldrRaw);
  }

  delay(2000); 
}