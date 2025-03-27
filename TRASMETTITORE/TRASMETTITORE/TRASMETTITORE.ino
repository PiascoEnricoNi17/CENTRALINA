// ------- MODULI / LIBRERIE ------- //
#include <LoRa.h>
#include <Wire.h>
#include <SPI.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LTR390.h>  
#include <Adafruit_BME680.h>  
#include <RTClib.h>          

// ------- DEFINIZIONE PIN LoRa ------- //
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23  
#define DIO0 26

// ------- PIN DISPLAY OLED ------- //
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------- PIN SENSORI ANALOGICI. ------- //
#define PIN_SOIL 25
#define PIN_PIOGGIA 35

// --------- TEMPORIZZAZIONE -------- //
unsigned long previousMillis = 0;
const long interval = 6000; 

// Variabili per gestione schermate OLED
int screen = 0;
unsigned long screenPreviousMillis = 0;
const long screenInterval = 3000; // Cambia schermata ogni 3 secondi

// ------- ISTANZA SENSORI ------- //
Adafruit_LTR390 ltr = Adafruit_LTR390();
Adafruit_BME680 bme;   
RTC_DS3231 rtc;        

// ------- VARIABILI GLOBALI ------- //
uint32_t UV = 0;
uint32_t Lux = 0;
float temp = 0;
float hum = 0;
float press = 0;
float gas = 0;
int soilPercent = 0;
int Rain = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("Avvio della stazione remota...");

    // ------- Inizializzazione display OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("Errore: Display OLED non trovato!");
        while (1);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Avvio in corso...");
    display.display();

    // ------- Inizializzazione LoRa ------- //
    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DIO0);
    delay(1000);
    if (!LoRa.begin(868E6)) {
        Serial.println("Errore: LoRa non inizializzato!");
        return;
    }
    Serial.println("LoRa OK");

    // --------------------- SENROSI I2C ------------------- //

    // ------- Inizializzazione LTR-390 ------- //
    Wire.begin(21, 22);
    if (!ltr.begin()) {
        Serial.println("Errore: LTR-390 non trovato!");
        while (1);
    }
    Serial.println("LTR-390 OK");
    ltr.setMode(LTR390_MODE_ALS);
    ltr.setGain(LTR390_GAIN_3);
    ltr.setResolution(LTR390_RESOLUTION_16BIT);
    ltr.enable(true);


    // ------- Inizializzazione BME688 ------- //
    if (!bme.begin()) {
        Serial.println("Errore: BME688 non trovato!");
        while (1);
    }
    Serial.println("BME688 OK");
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setGasHeater(320, 150); 

    // ------- Inizializzazione RTC ------- //
    if (!rtc.begin()) {
        Serial.println("Errore: RTC non trovato!");
        while (1);
    }
    Serial.println("RTC OK");
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // --- Lettura dati --- //
        DateTime now = rtc.now(); // DATA

        ltr.setMode(LTR390_MODE_UVS);
        UV = ltr.readUVS(); // LUCE UV.

        ltr.setMode(LTR390_MODE_ALS);
        Lux = ltr.readALS(); // LUMINOSITA'.

        // LETTURA BME: TEMP, HUM, PRSS, GAS.
        if (bme.performReading()) {
            temp = bme.temperature;
            hum = bme.humidity;
            press = bme.pressure / 100.0;
            gas = bme.gas_resistance / 1000.0;
        }

        // LETTURA UMIDITA' TERRENO.
        int soilRaw = 0;
        int samples = 10;
        for (int i = 0; i < samples; i++) {
            soilRaw += analogRead(PIN_SOIL);
            delay(5);
        }
        soilRaw /= samples;
        soilPercent = map(soilRaw, 4095, 900, 0, 100);
        soilPercent = constrain(soilPercent, 0, 100);

        Rain = analogRead(PIN_PIOGGIA); // LETTURA PIOGGIA.

        // --------- Invio dati via LoRa --------- //
        LoRa.beginPacket();
        LoRa.print(UV); LoRa.print(",");
        LoRa.print(Lux); LoRa.print(",");
        LoRa.print(temp); LoRa.print(",");
        LoRa.print(hum); LoRa.print(",");
        LoRa.print(press); LoRa.print(",");
        LoRa.print(gas); LoRa.print(",");
        LoRa.print(soilPercent); LoRa.print(",");
        LoRa.print(Rain);
        LoRa.endPacket();
    }

    // --------- Cambio schermata OLED ogni 3 secondi --------- //
    if (currentMillis - screenPreviousMillis >= screenInterval) {
        screenPreviousMillis = currentMillis;
        screen = (screen + 1) % 2;  // Alterna tra 0 e 1
        display.clearDisplay();

        if (screen == 0) {
            // **Schermata 1: Dati ambientali**
            display.setCursor(0, 0);
            display.print("Data e Ora: ");

            display.setCursor(0, 10);
            display.print(rtc.now().day()); display.print("-");
            display.print(rtc.now().month()); display.print("-");
            display.print(rtc.now().year());

            display.print(" ");

            display.print(rtc.now().hour()); display.print(":");
            display.print(rtc.now().minute()); display.print(":");
            display.print(rtc.now().second());


            display.setCursor(0, 20);
            display.print("LUX: "); display.print(Lux); display.print(" - UV: "); display.print(UV);

            display.setCursor(0, 30);
            display.print("Temperatura: "); display.print(temp); display.print("C");

            display.setCursor(0, 40);
            display.print("Umidita': "); display.print(hum); display.print("%");

            display.setCursor(0, 50);
            display.print("Pressione Atm: "); display.print(press); display.print("hPa");
        } 
        else {
            // **Schermata 2: Gas, umidità suolo, pioggia, vento ecc...**
            display.setCursor(0, 0);
            if(gas > 200)
            {
              display.print("Concentrazione di Gas: "); 
              display.setCursor(0, 10);
              display.print("Nulla");
            }
            else if(gas > 50 && gas < 200)
            {
              display.print("Concentrazione Gas: "); 
              display.setCursor(0, 10);
              display.print("Normale.");
            }
            else if(gas < 50 && gas > 10)
            {
              display.print("Concentrazione Gas: "); 
              display.setCursor(0, 10);
              display.print("Alta (Area inquinata).");
            }
            else if(gas < 10)
            {
              display.print("Concentrazione Gas: ");
              display.setCursor(0, 10);
              display.print("Molto Alta (Pericoloso).");
            }

            display.setCursor(0, 20);
            display.print("Umidita' Suolo: "); display.print(soilPercent); display.print("%");

            display.setCursor(0, 30);
            display.print("Pioggia: "); display.print(Rain);
        }

        display.display();
    }
}

 
