// -------- LIBRERIE --------
#include <WiFi.h>
#include <HTTPClient.h>
#include <LoRa.h>
#include <Adafruit_SSD1306.h>

// -------- CREDENZIALI WIFI --------
const char* ssid = "Sandus";
const char* password = "GGGGGGGG";

// -------- URL SERVER --------
const char* serverURL = "http://smart4future.eu/device-data";

// -------- DEFINIZIONE PIN LoRa --------
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23
#define DIO0 26

// ------- VARIABILI GLOBALI --------
const int maxSamples = 6;
int sampleCount = 0;

float sumUv = 0, sumLux = 0, sumTemp = 0, sumHum = 0, sumPress = 0, sumGas = 0, sumSoil = 0, sumRain = 0, sumWind = 0;

// ------- DISPLAY OLED ------- //
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    Serial.begin(115200);

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

    // Connessione WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connessione WiFi...");
    display.setCursor(0, 10);
    display.println("Connessione Wifi in corso.");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        display.print(".");
        display.display();
    }

    Serial.println("\nWiFi connesso!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Status: ");
    display.setCursor(0, 10);
    display.println("WiFi connesso!");
    display.display();

    // Inizializzazione LoRa
    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DIO0);

    if (!LoRa.begin(868E6)) {
        Serial.println("❌ Errore: LoRa non inizializzato!");
        display.println("Errore: LoRa non inizializzato!");
        while (1);
    }

    Serial.println("✅ LoRa inizializzato!");
    display.setCursor(0, 20);
    display.println("LoRa Inizializzato.");
    display.display();
}

void loop() {
    int packetSize = LoRa.parsePacket();

    if (packetSize) {
        String dati = "";
        while (LoRa.available()) {
            dati += (char)LoRa.read();
        }

        Serial.println("📦 Dati ricevuti: " + dati);

        int uvIndex = dati.indexOf("\"Uv\":");
        if (uvIndex != -1) {
            sumUv += getValue(dati, "\"Uv\":");
            sumLux += getValue(dati, "\"Lux\":");
            sumTemp += getValue(dati, "\"Temp\":");
            sumHum += getValue(dati, "\"Hum\":");
            sumPress += getValue(dati, "\"Press\":");
            sumGas += getValue(dati, "\"Gas\":");
            sumSoil += getValue(dati, "\"SoilPercent\":");
            sumRain += getValue(dati, "\"Rain\":");
            sumWind += getValue(dati, "\"Wind\":");

            sampleCount++;
        }

        if (sampleCount >= maxSamples && WiFi.status() == WL_CONNECTED) {
            float avgUv = sumUv / maxSamples;
            float avgLux = sumLux / maxSamples;
            float avgTemp = sumTemp / maxSamples;
            float avgHum = sumHum / maxSamples;
            float avgPress = sumPress / maxSamples;
            float avgGas = sumGas / maxSamples;
            float avgSoil = sumSoil / maxSamples;
            float avgRain = sumRain / maxSamples;
            float avgWind = sumWind / maxSamples;

            // JSON con campo wind (float)
            String json = "{\"deviceId\":\"lora001\",\"readings\":{";
            json += "\"temperature\":" + String(avgTemp, 2) + ",";
            json += "\"humidity\":" + String(avgHum, 2) + ",";
            json += "\"gas\":" + String(avgGas, 2) + ",";
            json += "\"pressure\":" + String(avgPress, 2) + ",";
            json += "\"uv\":" + String(avgUv, 2) + ",";
            json += "\"lux\":" + String(avgLux, 2) + ",";
            json += "\"soilMoisture\":" + String(avgSoil, 2) + ",";
            json += "\"rain\":" + String(avgRain, 2) + ",";
            json += "\"wind\":" + String(avgWind, 2);
            json += "}}";

            Serial.println("📤 JSON da inviare: " + json);

            HTTPClient http;
            http.begin(serverURL);
            http.addHeader("Content-Type", "application/json");
            int httpResponseCode = http.POST(json);

            if (httpResponseCode > 0) {
                Serial.println("✅ Media inviata con successo!");
            } else {
                Serial.println("❌ Errore nell'invio media: " + String(httpResponseCode));
            }

            http.end();
            resetSums();
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ WiFi disconnesso! Ritento...");
        WiFi.reconnect();
    }
}

float getValue(String input, String key) {
    int index = input.indexOf(key);
    if (index == -1) return 0;
    int start = index + key.length();
    int end = input.indexOf(",", start);
    if (end == -1) end = input.length();
    return input.substring(start, end).toFloat();
}

void resetSums() {
    sampleCount = 0;
    sumUv = sumLux = sumTemp = sumHum = sumPress = sumGas = sumSoil = sumRain = sumWind = 0;
}
