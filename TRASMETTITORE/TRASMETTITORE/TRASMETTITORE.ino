// ------------------ LIBRERIE ------------------ //
#include <LoRa.h>
#include <Wire.h>
#include <SPI.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LTR390.h>
#include <Adafruit_BME680.h>
#include <RTClib.h>

// ------------------ DEFINIZIONI HARDWARE ------------------ //
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23
#define DIO0 26

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PIN_SOIL 25
#define PIN_PIOGGIA 4
#define HALL_PIN 13  // Cambiato da GPIO2 a GPIO13

// ------------------ ISTANZE SENSORI ------------------ //
Adafruit_LTR390 ltr = Adafruit_LTR390();
Adafruit_BME680 bme;
RTC_DS3231 rtc;

// ------------------ VARIABILI GLOBALI ------------------ //
float UV = 0;
uint32_t Lux = 0;
float temp = 0;
float hum = 0;
float press = 0;
float gas = 0;
int soilPercent = 0;
float rainMM = 0;
float windSpeed = 0;
float windSpeedKmh = 0;

volatile unsigned int hallCounter = 0;
unsigned long lastWindCalc = 0;
const float anemometerDiameter = 0.20;
const float anemometerCircumference = anemometerDiameter * 3.1416;

unsigned long previousMillis = 0;
const long sensorInterval = 6000;

int screen = 0;
unsigned long screenPreviousMillis = 0;
const long screenInterval = 3000;

unsigned long lastDebug = 0;

// ------------------ INTERRUPT PER ANEMOMETRO ------------------ //
void IRAM_ATTR hallISR() {
  hallCounter++;
}

// ------------------ SETUP ------------------ //
void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Errore: Display OLED non trovato!");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Avvio stazione meteo...");
  display.display();

  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(868E6)) {
    Serial.println("Errore: LoRa non inizializzato!");
    while (1);
  }

  Wire.begin(21, 22);
  if (!ltr.begin()) {
    Serial.println("Errore: LTR-390 non trovato!");
    while (1);
  }
  ltr.setGain(LTR390_GAIN_3);
  ltr.setResolution(LTR390_RESOLUTION_16BIT);
  ltr.enable(true);

  if (!bme.begin()) {
    Serial.println("Errore: BME680 non trovato!");
    while (1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setGasHeater(320, 150);

  if (!rtc.begin()) {
    Serial.println("Errore: RTC non trovato!");
    while (1);
  }

  pinMode(PIN_PIOGGIA, INPUT);
  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallISR, RISING);
}

// ------------------ LOOP PRINCIPALE ------------------ //
void loop() {
  unsigned long currentMillis = millis();

  // Debug temporaneo: stampa impulsi ogni 5 secondi
  if (millis() - lastDebug >= 5000) {
    lastDebug = millis();
    Serial.print("[DEBUG] Impulsi Hall: ");
    Serial.println(hallCounter);
  }

  // Calcolo velocità vento ogni secondo
  if (currentMillis - lastWindCalc >= 1000) {
    lastWindCalc = currentMillis;

    float rps = hallCounter;
    windSpeed = rps * anemometerCircumference;
    windSpeedKmh = windSpeed * 3.6;

    Serial.println("---- LOG VENTO ----");
    Serial.print("Impulsi Hall: ");
    Serial.println(hallCounter);
    Serial.print("RPS (rotazioni/sec): ");
    Serial.println(rps);
    Serial.print("Velocità (m/s): ");
    Serial.println(windSpeed, 2);
    Serial.print("Velocità (km/h): ");
    Serial.println(windSpeedKmh, 2);

    hallCounter = 0;

    if (windSpeedKmh == 0) {
      windSpeedKmh = 0.1;
      Serial.println("Fallback: vento impostato a 0.1 km/h");
    }
    Serial.println("--------------------");
  }

  // Lettura sensori ogni intervallo
  if (currentMillis - previousMillis >= sensorInterval) {
    previousMillis = currentMillis;

    DateTime now = rtc.now();

    ltr.setMode(LTR390_MODE_UVS);
    UV = ltr.readUVS() / 100.0;
    if (UV < 0.1) UV = 0.1;

    ltr.setMode(LTR390_MODE_ALS);
    Lux = ltr.readALS();

    if (bme.performReading()) {
      temp = bme.temperature;
      hum = bme.humidity;
      press = bme.pressure / 100.0;
      gas = bme.gas_resistance / 1000.0;
    }

    // Lettura umidità del suolo
    int rawSoil = 0;
    for (int i = 0; i < 10; i++) {
      rawSoil += analogRead(PIN_SOIL);
      delay(5);
    }
    rawSoil /= 10;
    soilPercent = map(rawSoil, 3300, 1000, 0, 100);
    soilPercent = constrain(soilPercent, 0, 100);

    // Lettura pioggia
    int rawRain = analogRead(PIN_PIOGGIA);
    int rainInverted = 4095 - rawRain;
    rainMM = map(rainInverted, 1000, 3500, 0, 65);
    rainMM = constrain(rainMM, 0, 65);

    if(rainMM == 0.0)
    {
      rainMM = 0.1;
    }

    // Debug pioggia
    Serial.print("Raw Rain: ");
    Serial.print(rawRain);
    Serial.print(" | Inverted: ");
    Serial.print(rainInverted);
    Serial.print(" | Rain mm: ");
    Serial.println(rainMM);

    // Invio dati LoRa
    LoRa.beginPacket();
    LoRa.print("{");
    LoRa.print("\"Uv\":"); LoRa.print(UV); LoRa.print(",");
    LoRa.print("\"Lux\":"); LoRa.print(Lux); LoRa.print(",");
    LoRa.print("\"Temp\":"); LoRa.print(temp); LoRa.print(",");
    LoRa.print("\"Hum\":"); LoRa.print(hum); LoRa.print(",");
    LoRa.print("\"Press\":"); LoRa.print(press); LoRa.print(",");
    LoRa.print("\"Gas\":"); LoRa.print(gas); LoRa.print(",");
    LoRa.print("\"SoilPercent\":"); LoRa.print(soilPercent); LoRa.print(",");
    LoRa.print("\"Rain\":"); LoRa.print(rainMM); LoRa.print(",");
    LoRa.print("\"Wind\":"); LoRa.print(windSpeedKmh);
    LoRa.print("}");
    LoRa.endPacket();
  }

  // Gestione schermate OLED
  if (currentMillis - screenPreviousMillis >= screenInterval) {
    screenPreviousMillis = currentMillis;
    screen = (screen + 1) % 2;
    display.clearDisplay();

    if (screen == 0) {
      DateTime now = rtc.now();
      display.setCursor(0, 0);
      display.print("Data: ");
      display.print(now.day()); display.print("/");
      display.print(now.month()); display.print(" ");
      display.print(now.hour()); display.print(":");
      display.print(now.minute());

      display.setCursor(0, 10);
      display.print("LUX: "); display.print(Lux);
      display.setCursor(0, 20);
      display.print("UV: "); display.print(UV, 1);

      display.setCursor(0, 30);
      display.print("Temp: "); display.print(temp); display.print("C");

      display.setCursor(0, 40);
      display.print("Hum: "); display.print(hum); display.print("%");

      display.setCursor(0, 50);
      display.print("Press: "); display.print(press); display.print("hPa");

    } else {
      display.setCursor(0, 0);
      display.print("Gas: ");
      if (gas > 200)
        display.print("OK");
      else if (gas > 50)
        display.print("Normale");
      else
        display.print("Scarso");
    }

    display.display();
  }
}
