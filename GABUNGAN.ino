#include <Arduino.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Kelvinator.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define SS_PIN  5
#define RST_PIN 18

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
const int BUZZER_PIN = 26;

const uint16_t kIrLed = 4;
IRKelvinatorAC ac(kIrLed);

const char* ssid = "MSI 9686";
const char* password = "D555a26!";

void printState() {
  Serial.println("Kelvinator A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());
  unsigned char* ir_code = ac.getRaw();
  Serial.print("IR Code: 0x");
  for (uint8_t i = 0; i < kKelvinatorStateLength; i++)
    Serial.printf("%02X", ir_code[i]);
  Serial.println();
}

String getRFIDCodeFromEndpoint(String rfidAddress) {
  HTTPClient http;
  String rfidCode;

  // Buat URL dengan alamat MAC RFID
  String url = "http://10.10.2.19:8000/api/rfid/" + rfidAddress;
  
  // Kirim permintaan HTTP GET ke endpoint
  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    // Baca respons sebagai String
    String response = http.getString();
    
    // Parsing JSON
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, response);
    
    if (error) {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
    } else {
      // Dapatkan nilai properti "rfid" dari JSON
      rfidCode = jsonDoc["rfid"].as<String>();
    }
  } else {
    Serial.print("HTTP GET request failed with error code: ");
    Serial.println(httpResponseCode);
    rfidCode = httpResponseCode;
  }

  // Akhiri permintaan HTTP
  http.end();

  return rfidCode;
}


void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  

  lcd.init();
  lcd.backlight();

  pinMode(BUZZER_PIN, OUTPUT);

  ac.begin();
  delay(200);

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println("IP address: " + WiFi.localIP().toString());
  Serial.println("RFID Access Control");
  Serial.println("Tempelkan kartu RFID untuk mengakses.");
  Serial.println();
}

void loop() {
  lcd.setCursor(0, 0);
  lcd.print("SCAN DISINI!");

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("UID tag: ");
  String content = "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print("Message: ");
  content.toUpperCase();

  String expectedRFIDCode = getRFIDCodeFromEndpoint(content);
  Serial.println(expectedRFIDCode);
  if (content == expectedRFIDCode) {
    Serial.println("Akses diterima");
    Serial.println();
    lcd.setCursor(0, 0);
    lcd.print("AKSES DITERIMA");
    lcd.setCursor(0, 1);
    lcd.print("SILAHKAN MASUK");
    digitalWrite(BUZZER_PIN, LOW);
    delay(3000);
    lcd.clear();
    Serial.println("Default state of the remote.");
    printState();
    Serial.println("Setting desired state for A/C.");
    ac.on();
    ac.setFan(1);
    ac.setMode(kKelvinatorCool);
    ac.setTemp(16);
    ac.setSwingVertical(false, kKelvinatorSwingVOff);
    ac.setSwingHorizontal(true);
    ac.setXFan(true);
    ac.setIonFilter(false);
    ac.setLight(true);
#if SEND_KELVINATOR
    Serial.println("Sending IR command to A/C ...");
    ac.send();
#endif // SEND_KELVINATOR
    printState();
  } else {
    
    Serial.println("Akses ditolak");
    lcd.setCursor(0, 0);
    lcd.print("AKSES DITOLAK");
    if (expectedRFIDCode == String(404)){
      lcd.setCursor(0, 1);
      lcd.print("RFID BELUM TERDAFTAR");
      delay(1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("RFID Code:");
      lcd.setCursor(0, 1);
      lcd.print(content);
    }
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);
    delay(5000);
    lcd.clear();
    Serial.println("Default state of the remote.");
    printState();
    Serial.println("Setting desired state for A/C.");
    ac.off();
    ac.setFan(1);
    ac.setMode(kKelvinatorCool);
    ac.setTemp(16);
    ac.setSwingVertical(false, kKelvinatorSwingVOff);
    ac.setSwingHorizontal(true);
    ac.setXFan(true);
    ac.setIonFilter(false);
    ac.setLight(true);
#if SEND_KELVINATOR
    Serial.println("Sending IR command to A/C ...");
    ac.send();
#endif // SEND_KELVINATOR
    printState();
    delay(5000);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
