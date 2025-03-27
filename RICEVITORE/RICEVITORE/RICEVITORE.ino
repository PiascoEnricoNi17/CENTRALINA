// -------- LIBRERIE --------
#include <WiFi.h>
#include <HTTPClient.h>
#include <LoRa.h>

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

void setup() {
    Serial.begin(115200);
    
    // CONNESSIONE WIFI
    WiFi.begin(ssid, password);
    Serial.print("Connessione al WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connesso!");

    // INIZIALIZZAZIONE LoRa
    SPI.begin(SCK, MISO, MOSI, SS);
    LoRa.setPins(SS, RST, DIO0);
    
    if (!LoRa.begin(868E6)) {
        Serial.println("❌ Errore: LoRa non inizializzato!");
        while (1);
    }
    Serial.println("✅ LoRa inizializzato!");
}

void loop() {
    int packetSize = LoRa.parsePacket();
    
    if (packetSize) {
        String dati = "";
        while (LoRa.available()) {
            dati += (char)LoRa.read();
        }

        Serial.println("📩 Dati ricevuti: " + dati);

        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(serverURL);
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            
            String postData = "dati=" + dati;
            int httpResponseCode = http.POST(postData);
            
            if (httpResponseCode > 0) {
                Serial.println("✅ Dati inviati con successo!");
            } else {
                Serial.print("❌ Errore nell'invio: ");
                Serial.println(httpResponseCode);
            }
            http.end();
        } else {
            Serial.println("⚠️ WiFi disconnesso! Ritento...");
            WiFi.reconnect();
        }
    }
}