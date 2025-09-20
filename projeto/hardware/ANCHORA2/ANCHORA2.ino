#include <WiFi.h>
#include <HTTPClient.h>
#include "BLEDevice.h"

// ===================== CONFIGURAÇÕES =====================
#define WIFI_SSID     "arthur"
#define WIFI_PASS     "12345678"
#define SERVER_URL    "http://10.120.54.15:8000/api/reading/ble"
#define ANCHOR_ID     "A2"      // Identificador da âncora
#define TAG_FILTER    "TAG01"   // Nome da tag BLE
#define SCAN_TIME     30        // Tempo de varredura (segundos)
// ========================================================

// Variáveis globais para RSSI
volatile int rssiSum = 0;
volatile int rssiCount = 0;

// Callback do scanner BLE
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveName() && advertisedDevice.getName() == TAG_FILTER) {
            int rssi = advertisedDevice.getRSSI();
            Serial.printf("[%s] Pacote de %s | RSSI: %d\n", ANCHOR_ID, TAG_FILTER, rssi);
            rssiSum += rssi;
            rssiCount++;
        }
    }
};

void sendDataToServer(int averageRssi) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi desconectado, não enviando.");
        return;
    }

    HTTPClient http;
    http.setTimeout(1500);
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    // Monta a string JSON para o corpo da requisição
    String payload = "{\"tagId\":\"" + String(TAG_FILTER) +
                     "\",\"anchorId\":\"" + String(ANCHOR_ID) +
                     "\",\"rssi\":" + String(averageRssi) + "}";

    Serial.printf("[%s] Enviando JSON: %s\n", ANCHOR_ID, payload.c_str());
    int httpCode = http.POST(payload);

    if (httpCode > 0) {
        Serial.printf("[%s] Enviado! Código resposta: %d\n", ANCHOR_ID, httpCode);
    } else {
        Serial.printf("[%s] ERRO envio: %s\n", ANCHOR_ID, http.errorToString(httpCode).c_str());
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    Serial.printf("\n--- Iniciando Âncora %s ---\n", ANCHOR_ID);

    // Conexão WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Conectando ao WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado!");

    // Inicialização do BLE
    BLEDevice::init("");
}

void loop() {
    // === 1. Reseta contadores e faz a varredura ===
    rssiSum = 0;
    rssiCount = 0;

    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    Serial.printf("\nIniciando scan de %d segundos...\n", SCAN_TIME);
    pBLEScan->start(SCAN_TIME, false); // bloqueia a execução pelo tempo do scan

    // === 2. Calcula a média do RSSI ===
    if (rssiCount > 0) {
        int avgRssi = rssiSum / rssiCount;
        Serial.printf("[%s] Média RSSI = %d (com %d amostras)\n", ANCHOR_ID, avgRssi, rssiCount);

        // === 3. Envia para o servidor ===
        sendDataToServer(avgRssi);
    } else {
        Serial.printf("[%s] Nenhum pacote de %s detectado neste ciclo.\n", ANCHOR_ID, TAG_FILTER);
    }

    // === 4. Aguarda antes de reiniciar o ciclo ===
    Serial.println("Aguardando 1 segundo para o próximo ciclo.");
    delay(1000);
}