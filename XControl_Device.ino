#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

#define BUTTON_PIN 0  // Butonun bağlı olduğu GPIO pini (örnek olarak GPIO 0)

const char* apSSID = "XControl-AP";    // AP modundaki SSID
const char* apPassword = "12345678"; // AP modundaki şifre (isteğe bağlı)

ESP8266WebServer server(80);  // Web sunucusu

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Butonu input olarak ayarla

  // EEPROM'dan WiFi bilgilerini al
  char ssid[32];
  char password[32];
  loadCredentials(ssid, password);

  // Eğer SSID ve parola geçerli ise STA modunda başlat
  if (strlen(ssid) > 0 && strlen(password) > 0) {
    Serial.println("STA modunda başlatılıyor...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(1000);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi'ya başarıyla bağlanıldı.");
    } else {
      Serial.println("WiFi'ya bağlanılamadı. Ancak AP moduna geçilmeyecek.");
    }
  } else {
    // Eğer EEPROM boşsa AP moduna geç
    Serial.println("EEPROM boş, AP moduna geçiliyor...");
    startAPMode();
  }

  // Web sunucusu yolunu tanımla
  server.on("/connect", HTTP_POST, handleConnect);
  server.begin();
  Serial.println("Web Sunucusu Başladı");
}

void loop() {
  server.handleClient();  // Web sunucusu isteklerini dinle

  // Butona uzun süre basıldı mı kontrol et
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);  // Debouncing için kısa gecikme
    if (digitalRead(BUTTON_PIN) == LOW) {
      long pressTime = millis();
      while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
      long pressDuration = millis() - pressTime;
      if (pressDuration > 5000) {
        // Butona 5 saniyeden fazla basıldıysa WiFi ayarlarını sıfırla
        resetWiFiSettings();
        startAPMode();
      }
    }
  }
}

// SSID ve parola bilgisini POST isteği ile alır ve WiFi'ya bağlanır
void handleConnect() {
  if (server.hasArg("plain") == false) {  // JSON verisi var mı kontrol et
    server.send(400, "application/json", "{\"message\":\"Veri bulunamadı\"}");
    return;
  }

  String jsonData = server.arg("plain");  // JSON verisini al
  Serial.println("Alınan JSON: " + jsonData);

  // JSON verisini parse et
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    Serial.print(F("JSON parse hatası: "));
    Serial.println(error.f_str());
    server.send(400, "application/json", "{\"message\":\"JSON parse hatası\"}");
    return;
  }

  // SSID ve parola bilgilerini al
  const char* newSSID = doc["ssid"];
  const char* newPassword = doc["password"];

  Serial.println("Yeni SSID: " + String(newSSID));
  Serial.println("Yeni Parola: " + String(newPassword));

  // WiFi bilgilerini EEPROM'a kaydet
  saveCredentials(newSSID, newPassword);

  // AP modunu kapatıp STA moduna geçiş yap
  switchToSTA(newSSID, newPassword);
}

// SSID ve parolayı EEPROM'a kaydet
void saveCredentials(const char* ssid, const char* password) {
  EEPROM.begin(512);

  for (int i = 0; i < 32; ++i) {
    EEPROM.write(i, ssid[i]);
  }

  for (int i = 0; i < 32; ++i) {
    EEPROM.write(32 + i, password[i]);
  }

  EEPROM.commit();
  EEPROM.end();
}

// EEPROM'dan SSID ve parola bilgilerini yükle
void loadCredentials(char* ssid, char* password) {
  EEPROM.begin(512);

  for (int i = 0; i < 32; ++i) {
    ssid[i] = EEPROM.read(i);
  }

  for (int i = 0; i < 32; ++i) {
    password[i] = EEPROM.read(32 + i);
  }

  EEPROM.end();
}

// AP modunu kapatıp STA moduna geçiş yap
void switchToSTA(const char* ssid, const char* password) {
  WiFi.softAPdisconnect(true);  // AP modunu kapat

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi'ya başarıyla bağlanıldı.");
    server.send(200, "application/json", "{\"message\":\"Bağlantı başarılı\"}");
  } else {
    Serial.println("WiFi'ya bağlanılamadı.");
    server.send(500, "application/json", "{\"message\":\"Bağlantı başarısız\"}");
  }
}

// WiFi ayarlarını sıfırlama fonksiyonu
void resetWiFiSettings() {
  EEPROM.begin(512);
  for (int i = 0; i < 64; ++i) {
    EEPROM.write(i, 0);  // EEPROM'daki WiFi verilerini sil
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("WiFi ayarları sıfırlandı.");
}

// AP modunu başlatan fonksiyon
void startAPMode() {
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(apSSID, apPassword)) {
    Serial.println("AP Modu aktif, IP Adresi: " + WiFi.softAPIP().toString());
  } else {
    Serial.println("AP Modu başlatılamadı!");
  }
}
