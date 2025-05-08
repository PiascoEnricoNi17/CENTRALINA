// -------- LIBRERIE --------
#include <WiFi.h>
#include <HTTPClient.h>
#include <LoRa.h>

#include <Adafruit_SSD1306.h>

// -------- CREDENZIALI WIFI --------
const char* ssid = "Sandus";
const char* password = "GGGGGGGG";

// URL SERVER
const char* serverURL = "http://tuo-server.com/api/dati";

// -------- DEFINIZIONE PIN LoRa (Devono essere gli stessi del trasmettitore!) --------
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23
#define DIO0 26


// ------- DEFINIZIONE PER LE VARIABILI GLOBALI: 
const int maxSamples = 6;
int sampleCount = 0;

float sumUv = 0, sumLux = 0, sumTemp = 0, sumHum = 0, sumPress = 0, sumGas = 0, sumSoil = 0, sumRain = 0;




// ------- PIN DISPLAY OLED ------- //
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


void setup() {
    Serial.begin(115200);


    // ------- Inizializzazione display OLED ---------
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


    // CONNESSIONE WIFI
    WiFi.begin(ssid, password);
    
    // SCRIVO SUL MONIOR CHE SI SA COLLEGANDO.
    Serial.print("Connessione WiFi...");

    // SCRIVO I DATI SUL MONITOR.
    display.setCursor(0, 10);
    display.println("Connessione Wifi in corso.");

    // TENTATIVO DI CONNESSIONE AL WIFI.
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        display.print(".");
        display.display();
    }
    Serial.println("\nWiFi connesso!");


    display.clearDisplay();
     // DISPLAY
    display.setCursor(0, 0);
    display.println("Status: ");

    display.setCursor(0, 10);
    display.println("WiFi connesso!");

    display.display();


    // INIZIALIZZAZIONE LoRa
    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DIO0);
    
    if (!LoRa.begin(868E6)) {
        Serial.println("❌ Errore: LoRa non inizializzato!");
        display.println("Errore: LoRa non inizializzato!");
        while (1);
    }
    Serial.println("✅ LoRa inizializzato!");
    
    // DISPLAY
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

        Serial.println("📩 Dati ricevuti: " + dati);

        // Parsing e accumulo dei valori
        int uvIndex = dati.indexOf("\"Uv\":");
        if (uvIndex != -1) {
            sumUv += dati.substring(uvIndex + 5, dati.indexOf(",", uvIndex)).toFloat();
            sumLux += dati.substring(dati.indexOf("\"Lux\":") + 6, dati.indexOf(",", dati.indexOf("\"Lux\":"))).toFloat();
            sumTemp += dati.substring(dati.indexOf("\"Temp\":") + 7, dati.indexOf(",", dati.indexOf("\"Temp\":"))).toFloat();
            sumHum += dati.substring(dati.indexOf("\"Hum\":") + 6, dati.indexOf(",", dati.indexOf("\"Hum\":"))).toFloat();
            sumPress += dati.substring(dati.indexOf("\"Press\":") + 8, dati.indexOf(",", dati.indexOf("\"Press\":"))).toFloat();
            sumGas += dati.substring(dati.indexOf("\"Gas\":") + 6, dati.indexOf(",", dati.indexOf("\"Gas\":"))).toFloat();
            sumSoil += dati.substring(dati.indexOf("\"SoilPercent\":") + 14, dati.indexOf(",", dati.indexOf("\"SoilPercent\":"))).toFloat();
            sumRain += dati.substring(dati.indexOf("\"Rain\":") + 7).toFloat(); // Ultimo campo

            sampleCount++;
        }

        // Se abbiamo raccolto 6 pacchetti, calcoliamo la media e inviamo
        if (sampleCount >= maxSamples && WiFi.status() == WL_CONNECTED) {
            float avgUv = sumUv / maxSamples;
            float avgLux = sumLux / maxSamples;
            float avgTemp = sumTemp / maxSamples;
            float avgHum = sumHum / maxSamples;
            float avgPress = sumPress / maxSamples;
            float avgGas = sumGas / maxSamples;
            float avgSoil = sumSoil / maxSamples;
            float avgRain = sumRain / maxSamples;

            String json = "{";
            json += "\"Uv\":" + String(avgUv, 2) + ",";
            json += "\"Lux\":" + String(avgLux, 2) + ",";
            json += "\"Temp\":" + String(avgTemp, 2) + ",";
            json += "\"Hum\":" + String(avgHum, 2) + ",";
            json += "\"Press\":" + String(avgPress, 2) + ",";
            json += "\"Gas\":" + String(avgGas, 2) + ",";
            json += "\"SoilPercent\":" + String(avgSoil, 2) + ",";
            json += "\"Rain\":" + String(avgRain, 2);
            json += "}";

            Serial.println("📦 JSON medio da inviare: " + json);

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

    // Ricontrolla la connessione WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ WiFi disconnesso! Ritento...");
        WiFi.reconnect();
    }
}


// ------ QUESTA FUNZIONE SI OCCUPA DI AZZERARE I DATI OGNI 6 SECONDI.
void resetSums() {
    sampleCount = 0;
    sumUv = sumLux = sumTemp = sumHum = sumPress = sumGas = sumSoil = sumRain = 0;
}