
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include "BluetoothSerial.h"
#include "HX711.h"

// RFID
#define SS_PIN 21
#define RST_PIN 22
MFRC522 rfid(SS_PIN, RST_PIN);

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Bluetooth
BluetoothSerial SerialBT;

// Buzzer
#define BUZZER 13

// Motor
#define IN1 15
#define IN2 33
#define IN3 27
#define IN4 32

// Load Cell
#define DT 4
#define SCK 5
HX711 scale;

// Product Billing
int totalBill = 0;

String uid1 = "32 49 FB 05";
String uid2 = "93 98 20 ED";
String uid3 = "69 9C C6 C5";
String uid4 = "53 77 4E EC";

int price1 = 50;
int price2 = 30;
int price3 = 40;
int price4 = 60;

// Load cell calibration
float calibrationFactor = 1.0;
const bool LOAD_CELL_CALIBRATED = false;

// Timers
unsigned long lastMotorTime = 0;
unsigned long lastScanTime = 0;
unsigned long lastBuzzerTime = 0;
unsigned long lastDisplayTime = 0;

const unsigned long MOTOR_TIMEOUT = 1500;
const unsigned long SCAN_DELAY = 1500;
const unsigned long BUZZER_TIME = 200;
const unsigned long BILL_DISPLAY_TIME = 2500;

bool motorRunning = false;
bool buzzerOn = false;
bool showBill = false;

String lastScannedUID = "";

// Motor Control
void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void back() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  motorRunning = false;
}

// Buzzer
void startBuzzer() {
  digitalWrite(BUZZER, HIGH);
  buzzerOn = true;
  lastBuzzerTime = millis();
}

// Bluetooth
void bluetoothControl() {
  while (SerialBT.available()) {
    char cmd = SerialBT.read();

    if (cmd == 'F') {
      forward();
      motorRunning = true;
      lastMotorTime = millis();
    }
    else if (cmd == 'B') {
      back();
      motorRunning = true;
      lastMotorTime = millis();
    }
    else if (cmd == 'S') {
      stopCar();
    }
  }
}

// Motor Safety
void checkMotorSafety() {
  if (motorRunning &&
      millis() - lastMotorTime >= MOTOR_TIMEOUT) {
    stopCar();
  }
}

// RFID Billing
void readRFID() {
  if (!rfid.PICC_IsNewCardPresent() ||
      !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }

    uid += String(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1) {
      uid += " ";
    }
  }

  uid.toUpperCase();

  unsigned long now = millis();

  if (uid == lastScannedUID &&
      now - lastScanTime < SCAN_DELAY) {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  lastScannedUID = uid;
  lastScanTime = now;

  String product = "Invalid Card";
  int price = 0;

  if (uid == uid1) {
    product = "Rice";
    price = price1;
  }
  else if (uid == uid2) {
    product = "Milk";
    price = price2;
  }
  else if (uid == uid3) {
    product = "Sugar";
    price = price3;
  }
  else if (uid == uid4) {
    product = "Maggie";
    price = price4;
  }

  totalBill += price;

  Serial.print("Scanned UID: ");
  Serial.println(uid);

  if (price > 0) {
    Serial.print("Product: ");
    Serial.println(product);
  } else {
    Serial.println("Invalid RFID Card");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(product);

  lcd.setCursor(0, 1);
  lcd.print("Rs:");
  lcd.print(price);
  lcd.print(" T:");
  lcd.print(totalBill);

  showBill = true;
  lastDisplayTime = millis();

  startBuzzer();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// Buzzer Timing
void checkBuzzer() {
  if (buzzerOn &&
      millis() - lastBuzzerTime >= BUZZER_TIME) {
    digitalWrite(BUZZER, LOW);
    buzzerOn = false;
  }
}

// Load Cell
void readWeight() {
  if (!scale.is_ready()) {
    return;
  }

  float weight = scale.get_units(3);

  Serial.print("Weight: ");
  Serial.println(weight);

  lcd.clear();
  lcd.setCursor(0, 0);

  if (LOAD_CELL_CALIBRATED) {
    lcd.print("Weight:");
    lcd.setCursor(0, 1);
    lcd.print(weight, 1);
    lcd.print(" g");
  } else {
    lcd.print("Calibrate Scale");
    lcd.setCursor(0, 1);
    lcd.print("Check HX711");
  }
}

// Setup
void setup() {
  Serial.begin(115200);

  SPI.begin(18, 19, 23, SS_PIN);
  rfid.PCD_Init();

  Wire.begin(25, 26);
  lcd.init();
  lcd.backlight();

  SerialBT.begin("SmartCar");

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopCar();

  scale.begin(DT, SCK);

  if (LOAD_CELL_CALIBRATED) {
    scale.set_scale(calibrationFactor);
    scale.tare();
  }

  lcd.setCursor(0, 0);
  lcd.print("Smart Trolley");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");

  delay(1500);
  lcd.clear();
}

// Main Loop
void loop() {
  bluetoothControl();

  checkMotorSafety();

  readRFID();

  checkBuzzer();

  if (showBill &&
      millis() - lastDisplayTime >= BILL_DISPLAY_TIME) {
    showBill = false;
    readWeight();
  }

  if (!showBill &&
      millis() - lastDisplayTime >= 1000) {
    lastDisplayTime = millis();
    readWeight();
  }
}
