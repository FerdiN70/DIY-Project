/*
  MINI KÖPEK ROBOT v2 - SENKRONİZE BACAKLAR & GENİŞLETİLMİŞ KAFA HAREKETİ
  ======================================================================
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ---------------- SABİT LEHİMLİ PİN TANIMLARI ----------------
const int PIN_HEAD  = 7;   // Kafa Servosu
const int PIN_TAIL  = 1;   // Kuyruk Servosu
const int PIN_RR    = 2;   // Sağ Bacak Servosu
const int PIN_RL    = 3;   // Sol Bacak Servosu

const int I2C_SDA   = 8;   // OLED SDA Pin
const int I2C_SCL   = 9;   // OLED SCK/SCL Pin

// ---------------- OLED AYARLARI ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- SERVOLAR ----------------
Servo headServo, tailServo, rlServo, rrServo;

// Akıcı hareket için anlık konum takipleri
int lastHeadPos = 90;
int lastRlPos = 90;
int lastRrPos = 90;

// ---------------- AÇI AYARLARI ----------------
const int HEAD_CENTER = 90;
const int HEAD_LEFT    = 42;  // %20 Genişletildi (Eski: 50)
const int HEAD_RIGHT   = 138; // %20 Genişletildi (Eski: 130)

const int TAIL_CENTER = 90;
const int TAIL_LEFT    = 60;
const int TAIL_RIGHT   = 120;

const int RL_STAND = 90;
const int RR_STAND = 90;

// Oturma açıları %25 azaltıldı (90 derece bükülme yerine 68 derece bükülme)
const int RL_SIT   = 22;   // (90 - 68)
const int RR_SIT   = 158;  // (90 + 68)

// ---------------- GÖZ BOYUT AYARLARI ----------------
const int EYE_W = 28;       
const int EYE_H = 38;       
const int EYE_Y = 13;       
const int L_EYE_X = 14;     
const int R_EYE_X = 86;     

unsigned long lastBlink = 0;
unsigned long blinkInterval = 3000;

int currentRoutineStep = 0;
unsigned long lastRoutineTime = 0;
unsigned long routineDelay = 1000; 

int currentEyeMode = 0; // 0: Normal, 1: Mutlu, 2: Sinirli

void setup() {
  Serial.begin(115200);
  Serial.println("Gelişmiş senkronize kodlar başlatılıyor...");

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED bulunamadi!");
  }
  
  display.clearDisplay();
  display.display();

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  headServo.setPeriodHertz(50);
  tailServo.setPeriodHertz(50);
  rlServo.setPeriodHertz(50);
  rrServo.setPeriodHertz(50);

  attachAll();
  
  // İlk pozisyonlar
  headServo.write(HEAD_CENTER);
  rlServo.write(RL_STAND);
  rrServo.write(RR_STAND);
  tailServo.write(TAIL_CENTER);
  
  currentEyeMode = 0;
  updateDisplay(0, false);

  randomSeed(analogRead(A0));
  lastRoutineTime = millis();
}

void loop() {
  // 1. DOĞAL GÖZ KIRPMA
  if (millis() - lastBlink > blinkInterval) {
    if (currentEyeMode == 0) {
      blink();
    }
    lastBlink = millis();
    blinkInterval = 3000 + random(0, 4000); 
  }

  // 2. OTOMATİK KOMBİNASYON DÖNGÜSÜ
  if (millis() - lastRoutineTime > routineDelay) {
    executeRoutine();
    lastRoutineTime = millis();
  }
}

// ================= AKICI TEK SERVO HAREKETİ (Kafa İçin) =================
void servoWriteSmooth(Servo &srv, int &lastPos, int targetPos, int speedDelay) {
  if (!srv.attached()) return;
  if (lastPos < targetPos) {
    for (int pos = lastPos; pos <= targetPos; pos += 2) {
      srv.write(pos);
      delay(speedDelay);
    }
  } else {
    for (int pos = lastPos; pos >= targetPos; pos -= 2) {
      srv.write(pos);
      delay(speedDelay);
    }
  }
  lastPos = targetPos;
}

// ================= YENİ: İKİ BACAK AYNI ANDA SENKRONİZE HAREKET FONKSİYONU =================
void bacaklariAyniAndaOynat(int targetRl, int targetRr, int speedDelay) {
  if (!rlServo.attached() || !rrServo.attached()) return;

  // Hedefe ulaşana kadar döngü devam eder
  while (lastRlPos != targetRl || lastRrPos != targetRr) {
    
    // Sol bacağı yaklaştır
    if (lastRlPos < targetRl) {
      lastRlPos += 2; if (lastRlPos > targetRl) lastRlPos = targetRl;
    } else if (lastRlPos > targetRl) {
      lastRlPos -= 2; if (lastRlPos < targetRl) lastRlPos = targetRl;
    }

    // Sağ bacağı yaklaştır
    if (lastRrPos < targetRr) {
      lastRrPos += 2; if (lastRrPos > targetRr) lastRrPos = targetRr;
    } else if (lastRrPos > targetRr) {
      lastRrPos -= 2; if (lastRrPos < targetRr) lastRrPos = targetRr;
    }

    // İki servoya da YENİ pozisyonu AYNI ANDA gönderiyoruz
    rlServo.write(lastRlPos);
    rrServo.write(lastRrPos);
    
    delay(speedDelay); // Hareket akış hızı
  }
}

// ================= KOMBİNASYON SENARYOSU =================
void executeRoutine() {
  attachAll(); 
  
  switch (currentRoutineStep) {
    case 0: // AYAKTA DURUŞ
      Serial.println("[Döngü] -> Ayakta");
      currentEyeMode = 0;
      updateDisplay(0, false);
      
      // Bacaklar AYNI ANDA ayağa kalkar
      bacaklariAyniAndaOynat(RL_STAND, RR_STAND, 15);
      servoWriteSmooth(headServo, lastHeadPos, HEAD_CENTER, 15);
      tailServo.write(TAIL_CENTER);
      
      routineDelay = 2000; 
      currentRoutineStep++;
      break;

    case 1: // SOLA BAKIŞ (%20 daha geniş)
      Serial.println("[Döngü] -> Sola bakıyor");
      currentEyeMode = 0;
      updateDisplay(-6, false);
      servoWriteSmooth(headServo, lastHeadPos, HEAD_LEFT, 20); 
      
      routineDelay = 1200; 
      currentRoutineStep++;
      break;

    case 2: // SAĞA BAKIŞ (%20 daha geniş)
      Serial.println("[Döngü] -> Sağa bakıyor");
      currentEyeMode = 0;
      updateDisplay(6, false);
      servoWriteSmooth(headServo, lastHeadPos, HEAD_RIGHT, 20);  
      
      routineDelay = 1200;
      currentRoutineStep++;
      break;

    case 3: // AYAKTA MUTLU & KUYRUK SALLAMA
      Serial.println("[Döngü] -> Ayakta Mutlu");
      servoWriteSmooth(headServo, lastHeadPos, HEAD_CENTER, 15);
      currentEyeMode = 1; 
      updateDisplay(0, false);
      
      for (int i = 0; i < 3; i++) {
        tailServo.write(TAIL_LEFT);
        delay(140);
        tailServo.write(TAIL_RIGHT);
        delay(140);
      }
      tailServo.write(TAIL_CENTER);
      
      routineDelay = 1000; 
      currentRoutineStep++;
      break;

    case 4: // AYAKTA SİNİRLİ MOD
      Serial.println("[Döngü] -> Ayakta Sinirlendi!");
      currentEyeMode = 2; 
      updateDisplay(0, false);
      
      for (int i = 0; i < 4; i++) {
        tailServo.write(HEAD_CENTER - 15);
        delay(80);
        tailServo.write(HEAD_CENTER + 15);
        delay(80);
      }
      tailServo.write(TAIL_CENTER);
      
      routineDelay = 2000; 
      currentRoutineStep++;
      break;

    case 5: // OTURMA VE MUTLULUK (Azaltılmış %25 açı ve Senkronize)
      Serial.println("[Döngü] -> Oturuyor & Mutlu");
      currentEyeMode = 1; 
      updateDisplay(0, false);
      
      // Bacaklar AYNI ANDA tatlı bir açıyla oturma pozisyonuna geçer
      bacaklariAyniAndaOynat(RL_SIT, RR_SIT, 20);
      
      for (int i = 0; i < 2; i++) {
        tailServo.write(TAIL_LEFT + 10);
        delay(200);
        tailServo.write(TAIL_RIGHT - 10);
        delay(200);
      }
      tailServo.write(TAIL_CENTER);
      
      routineDelay = 3000; 
      currentRoutineStep = 0; 
      break;
  }
}

// ================= YARDIMCI FONKSİYONLAR =================

void attachAll() {
  if (!headServo.attached()) headServo.attach(PIN_HEAD, 500, 2400); 
  if (!tailServo.attached()) tailServo.attach(PIN_TAIL, 500, 2400);
  if (!rlServo.attached())   rlServo.attach(PIN_RL, 500, 2400);
  if (!rrServo.attached())   rrServo.attach(PIN_RR, 500, 2400);
}

void updateDisplay(int pupilOffset, bool closed) {
  display.clearDisplay();
  
  if (closed) {
    display.fillRoundRect(L_EYE_X, EYE_Y + EYE_H / 2 - 2, EYE_W, 4, 2, SSD1306_WHITE);
    display.fillRoundRect(R_EYE_X, EYE_Y + EYE_H / 2 - 2, EYE_W, 4, 2, SSD1306_WHITE);
  } else if (currentEyeMode == 1) {
    drawHappyEye(L_EYE_X, EYE_Y + EYE_H / 2);
    drawHappyEye(R_EYE_X, EYE_Y + EYE_H / 2);
  } else {
    display.fillRoundRect(L_EYE_X, EYE_Y, EYE_W, EYE_H, 8, SSD1306_WHITE);
    display.fillRoundRect(R_EYE_X, EYE_Y, EYE_W, EYE_H, 8, SSD1306_WHITE);

    int pupilR = 6;
    int cx1 = L_EYE_X + EYE_W / 2 + pupilOffset;
    int cx2 = R_EYE_X + EYE_W / 2 + pupilOffset;
    int cy = EYE_Y + EYE_H / 2;
    
    display.fillCircle(cx1, cy, pupilR, BLACK);
    display.fillCircle(cx2, cy, pupilR, BLACK);

    if (currentEyeMode == 2) {
      display.fillTriangle(L_EYE_X + EYE_W - 14, EYE_Y, L_EYE_X + EYE_W, EYE_Y, L_EYE_X + EYE_W, EYE_Y + 14, BLACK);
      display.fillTriangle(R_EYE_X, EYE_Y, R_EYE_X + 14, EYE_Y, R_EYE_X, EYE_Y + 14, BLACK);
    }
  }
  display.display();
}

void drawHappyEye(int x, int centerY) {
  for (int i = 0; i < 5; i++) {
    display.drawLine(x, centerY + 4 - i, x + EYE_W / 2, centerY - 8 - i, SSD1306_WHITE);
    display.drawLine(x + EYE_W / 2, centerY - 8 - i, x + EYE_W, centerY + 4 - i, SSD1306_WHITE);
  }
}

void blink() {
  updateDisplay(0, true);
  delay(140);
  updateDisplay(0, false); 
}