// ====================================================================
// DESKBUDDY ESP32-C3 + SENSORS + OLED + NTP + WEATHER + BLYNK IOT
// ====================================================================

#define BLYNK_TEMPLATE_ID "TMPL6t7wKzUYm"
#define BLYNK_TEMPLATE_NAME "IOT Ferdi"
#define BLYNK_AUTH_TOKEN "WORWf2qeTrDMfuNNC_BrCNUJKlo04f36"    // Ganti dengan Auth Token Anda

#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BlynkSimpleEsp32.h>
#include "time.h"
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// --- ALOKASI PIN ESP32-C3 SUPERMINI ---
#define SDA_PIN 8     // OLED SDA
#define SCL_PIN 9     // OLED SCL
#define TOUCH_PIN 5   // Touch Sensor TTP223 (Digital)
#define RAIN_PIN 0    // Sensor Hujan YL-83 (Analog ADC0)
#define MQ6_PIN 1     // Sensor Gas MQ-6 (Analog ADC1)
#define BUZZER_PIN 2  // Buzzer Output

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- KONFIGURASI WI-FI & OPENWEATHERMAP ---
const char* ssid = "Modemku";
const char* password = "Ferdi1234";
String apiKey = "45fcf5807a5920e2006c2b8a077d423f";
String city = "Jakarta";
String countryCode = "ID";
const char* ntpServer = "pool.ntp.org";
const char* tzString = "WIB-7";

// Data Cuaca
float temperature = 0.0;
int humidity = 0;
String weatherMain = "Clear";
unsigned long lastWeatherUpdate = 0;

// BITMAP PARTIKEL AMARAH (16x16)
const unsigned char bmp_anger[] PROGMEM = { 
  0x00, 0x00, 0x11, 0x10, 0x2a, 0x90, 0x44, 0x40, 
  0x80, 0x20, 0x80, 0x20, 0x44, 0x40, 0x2a, 0x90, 
  0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
};

// DEFINISI EMOJI / MOOD
#define MOOD_NORMAL 0
#define MOOD_HAPPY 1
#define MOOD_SURPRISED 2
#define MOOD_SLEEPY 3
#define MOOD_ANGRY 4
#define MOOD_SAD 5
#define MOOD_SUSPICIOUS 6
int currentMood = MOOD_NORMAL;
int manualMood = MOOD_NORMAL;

// VARIABEL SENSOR
int rainVal = 4095;
int mq6Val = 0;
bool isGasDetected = false;
bool isRaining = false;
bool lastTouchState = false;

// --- AMBANG BATAS SENSOR ---
const int GAS_THRESHOLD = 3500;   
const int RAIN_THRESHOLD = 2800;  

// TIMING PATTERN BUZZER & BLYNK SYNC
unsigned long lastBuzzerTime = 0;
unsigned long lastBlynkSync = 0;
int buzzerStep = 0;

// --- PHYSICS ENGINE MATA ---
struct Eye {
  float x, y, w, h;  
  float targetX, targetY, targetW, targetH;
  float pupilX, pupilY, targetPupilX, targetPupilY;
  float velX, velY, velW, velH, pVelX, pVelY;
  float k = 0.12, d = 0.60, pk = 0.08, pd = 0.50;  
  bool blinking;
  unsigned long lastBlink, nextBlinkTime;

  void init(float _x, float _y, float _w, float _h) {
    x = targetX = _x; y = targetY = _y;
    w = targetW = _w; h = targetH = _h;
    pupilX = targetPupilX = 0; pupilY = targetPupilY = 0;
    nextBlinkTime = millis() + random(1000, 4000);
  }

  void update() {
    float ax = (targetX - x) * k, ay = (targetY - y) * k;
    float aw = (targetW - w) * k, ah = (targetH - h) * k;
    velX = (velX + ax) * d; velY = (velY + ay) * d;
    velW = (velW + aw) * d; velH = (velH + ah) * d;
    x += velX; y += velY; w += velW; h += velH;

    float pax = (targetPupilX - pupilX) * pk, pay = (targetPupilY - pupilY) * pk;
    pVelX = (pVelX + pax) * pd; pVelY = (pVelY + pay) * pd;
    pupilX += pVelX; pupilY += pVelY;
  }
};

Eye leftEye, rightEye;

// PEMBACAAN TOUCH SENSOR
void handleTouch() {
  bool currentTouchState = digitalRead(TOUCH_PIN);

  if (currentTouchState && !lastTouchState) {
    manualMood++;
    if (manualMood > MOOD_SUSPICIOUS) {
      manualMood = MOOD_NORMAL;
    }
    delay(50);
  }
  lastTouchState = currentTouchState;
}

// PEMBACAAN SENSOR ADC
void readSensors() {
  rainVal = analogRead(RAIN_PIN);
  mq6Val = analogRead(MQ6_PIN);

  isGasDetected = (mq6Val > GAS_THRESHOLD);
  isRaining = (rainVal < RAIN_THRESHOLD);

  if (isGasDetected) {
    currentMood = MOOD_ANGRY; 
  } else if (isRaining) {
    currentMood = MOOD_SAD;   
  } else {
    currentMood = manualMood;
  }
}

// LOGIKA BUZZER (NON-BLOCKING)
void handleBuzzer() {
  unsigned long now = millis();

  if (isGasDetected) {
    switch (buzzerStep) {
      case 0: digitalWrite(BUZZER_PIN, HIGH); if (now - lastBuzzerTime >= 400) { lastBuzzerTime = now; buzzerStep = 1; } break;
      case 1: digitalWrite(BUZZER_PIN, LOW);  if (now - lastBuzzerTime >= 100) { lastBuzzerTime = now; buzzerStep = 2; } break;
      case 2: digitalWrite(BUZZER_PIN, HIGH); if (now - lastBuzzerTime >= 150) { lastBuzzerTime = now; buzzerStep = 3; } break;
      case 3: digitalWrite(BUZZER_PIN, LOW);  if (now - lastBuzzerTime >= 100) { lastBuzzerTime = now; buzzerStep = 4; } break;
      case 4: digitalWrite(BUZZER_PIN, HIGH); if (now - lastBuzzerTime >= 150) { lastBuzzerTime = now; buzzerStep = 5; } break;
      case 5: digitalWrite(BUZZER_PIN, LOW);  if (now - lastBuzzerTime >= 400) { lastBuzzerTime = now; buzzerStep = 0; } break;
    }
  } else if (isRaining) {
    switch (buzzerStep) {
      case 0: digitalWrite(BUZZER_PIN, HIGH); if (now - lastBuzzerTime >= 250) { lastBuzzerTime = now; buzzerStep = 1; } break;
      case 1: digitalWrite(BUZZER_PIN, LOW);  if (now - lastBuzzerTime >= 250) { lastBuzzerTime = now; buzzerStep = 0; } break;
      default: buzzerStep = 0; break;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerStep = 0;
  }
}

// KIRIM DATA KE BLYNK IOT
void sendDataToBlynk() {
  Blynk.virtualWrite(V0, mq6Val);
  Blynk.virtualWrite(V1, rainVal);
  Blynk.virtualWrite(V2, isGasDetected ? 1 : 0);
  Blynk.virtualWrite(V3, isRaining ? 1 : 0);
  Blynk.virtualWrite(V4, temperature);
  Blynk.virtualWrite(V5, weatherMain);
}

// FETCH DATA CUACA OPENWEATHERMAP
void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[HTTP] Mengambil data cuaca...");
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      JSONVar myObject = JSON.parse(payload);
      if (JSON.typeof(myObject) != "undefined") {
        temperature = double(myObject["main"]["temp"]);
        humidity = int(myObject["main"]["humidity"]);
        weatherMain = (const char*)myObject["weather"][0]["main"];
        Serial.printf("[HTTP] Cuaca Berhasil! Suhu: %.1f C, Status: %s\n", temperature, weatherMain.c_str());
      }
    } else {
      Serial.printf("[HTTP] Gagal mengambil cuaca! HTTP Code: %d\n", httpCode);
    }
    http.end();
  }
}

void drawEyelidMask(float x, float y, float w, float h, int mood, bool isLeft) {
  int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
  display.setTextColor(SSD1306_BLACK);

  if (mood == MOOD_ANGRY) {
    if (isLeft)
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, SSD1306_BLACK);
    else
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, SSD1306_BLACK);
  } else if (mood == MOOD_SAD) {
    if (isLeft)
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, SSD1306_BLACK);
    else
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, SSD1306_BLACK);
  } else if (mood == MOOD_HAPPY) {
    display.fillRect(ix, iy + ih - 12, iw, 14, SSD1306_BLACK);
    display.fillCircle(ix + iw / 2, iy + ih + 6, iw / 1.3, SSD1306_BLACK);  
  } else if (mood == MOOD_SLEEPY) {
    display.fillRect(ix, iy, iw, ih / 2 + 2, SSD1306_BLACK);
  } else if (mood == MOOD_SUSPICIOUS) {
    if (isLeft) display.fillRect(ix, iy, iw, ih / 2 - 2, SSD1306_BLACK);
    else display.fillRect(ix, iy + ih - 8, iw, 8, SSD1306_BLACK);
  }
}

void drawUltraProEye(Eye& e, bool isLeft) {
  int ix = (int)e.x, iy = (int)e.y, iw = (int)e.w, ih = (int)e.h;
  int r = (iw < 20) ? 3 : 8;
  display.fillRoundRect(ix, iy, iw, ih, r, SSD1306_WHITE);

  int cx = ix + iw / 2, cy = iy + ih / 2;
  int pw = iw / 2.2, ph = ih / 2.2;
  int px = cx + (int)e.pupilX - (pw / 2), py = cy + (int)e.pupilY - (ph / 2);

  if (px < ix) px = ix;
  if (px + pw > ix + iw) px = ix + iw - pw;
  if (py < iy) py = iy;
  if (py + ph > iy + ih) py = iy + ih - ph;

  display.fillRoundRect(px, py, pw, ph, r / 2, SSD1306_BLACK);
  if (iw > 15 && ih > 15) display.fillCircle(px + pw - 4, py + 4, 2, SSD1306_WHITE);

  drawEyelidMask(e.x, e.y, e.w, e.h, currentMood, isLeft);
}

void updatePhysicsAndMood() {
  unsigned long now = millis();
  if (now > leftEye.nextBlinkTime) {
    leftEye.blinking = rightEye.blinking = true;
    leftEye.lastBlink = now;
    leftEye.nextBlinkTime = now + random(2000, 6000);
  }
  if (leftEye.blinking) {
    leftEye.targetH = rightEye.targetH = 2;  
    if (now - leftEye.lastBlink > 120) leftEye.blinking = rightEye.blinking = false;
  }

  if (!leftEye.blinking) {
    if (currentMood == MOOD_ANGRY) {
      leftEye.targetW = rightEye.targetW = 34; leftEye.targetH = rightEye.targetH = 32;
    } else if (currentMood == MOOD_SAD) {
      leftEye.targetW = rightEye.targetW = 34; leftEye.targetH = rightEye.targetH = 40;
    } else if (currentMood == MOOD_HAPPY) {
      leftEye.targetW = rightEye.targetW = 40; leftEye.targetH = rightEye.targetH = 32;
    } else if (currentMood == MOOD_SURPRISED) {
      leftEye.targetW = rightEye.targetW = 30; leftEye.targetH = rightEye.targetH = 45;
    } else {
      leftEye.targetW = rightEye.targetW = 36; leftEye.targetH = rightEye.targetH = 36;
    }
  }

  leftEye.update();
  rightEye.update();
}

void drawIdleScreen() {
  struct tm t;
  if (!getLocalTime(&t)) {
    display.setFont(NULL);
    display.setCursor(20, 20);
    display.print("Syncing Time...");
    return;
  }

  display.setFont(NULL);
  display.setTextSize(2);
  display.setCursor(8, 2);
  display.printf("%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);

  display.setTextSize(1);
  display.setCursor(8, 22);
  display.printf("%02d/%02d/%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
  
  display.setCursor(8, 34);
  display.print(city);
  display.print(": ");
  display.print((int)temperature);
  display.print("C, ");
  display.print(weatherMain);

  display.drawFastHLine(0, 46, 128, SSD1306_WHITE);

  display.setCursor(0, 52);
  display.print("MQ6:");
  display.print(mq6Val);
  display.print(" R:");
  display.print(rainVal);
  display.print(" [NORMAL]");
}

void drawWarningScreen() {
  updatePhysicsAndMood();

  if (currentMood == MOOD_ANGRY) {
    display.drawBitmap(56, 0, bmp_anger, 16, 16, SSD1306_WHITE);
  }

  drawUltraProEye(leftEye, true);
  drawUltraProEye(rightEye, false);

  display.setFont(NULL);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 55);

  if (isGasDetected) {
    display.print("GAS!");
    display.print(mq6Val);
    display.print(" R:");
    display.print(rainVal);
  } else if (isRaining) {
    display.print("RAIN!");
    display.print(rainVal);
    display.print(" G:");
    display.print(mq6Val);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- SYSTEM BOOTING ---");

  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(RAIN_PIN, INPUT);
  pinMode(MQ6_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERROR] OLED SSD1306 Gagal diinisialisasi!");
    for (;;);
  }

  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();

  leftEye.init(18, 10, 36, 36);
  rightEye.init(74, 10, 36, 36);

  display.setCursor(10, 20);
  display.print("Connecting WiFi...");
  display.display();

  // KONEKSI WIFI & BLYNK CONFIG
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.setTxPower(WIFI_POWER_11dBm); // Mencegah lonjakan daya ESP32-C3

  Serial.printf("[WiFi] Menghubungkan ke SSID: %s ...\n", ssid);
  WiFi.begin(ssid, password);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 40) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] TERHUBUNG SUKSES!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());

    // Inisialisasi Blynk menggunakan token yang sudah terhubung WiFi
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect(10000); // Timeout koneksi Blynk 10 detik

    Serial.println("[NTP] Mengonfigurasi waktu via pool.ntp.org...");
    configTime(0, 0, ntpServer);
    setenv("TZ", tzString, 1);
    tzset();

    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 20) {
      Serial.print("[NTP] Meminta waktu... (retrying)\n");
      delay(500);
      retry++;
    }

    if (getLocalTime(&timeinfo)) {
      Serial.println("[NTP] WAKTU BERHASIL DISINKRONKAN!");
      Serial.printf("[NTP] Jam saat ini: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      Serial.println("[NTP ERROR] Gagal mendapatkan waktu dari NTP Server!");
    }

    fetchWeatherData();
    lastWeatherUpdate = millis();
  } else {
    Serial.println("\n[WiFi ERROR] Gagal terhubung ke WiFi!");
  }
}

void loop() {
  handleTouch();   
  readSensors();   
  handleBuzzer();  

  // Jalankan proses Blynk secara berkala
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }

  // Kirim data ke server Blynk IoT setiap 2 detik sekali
  if (millis() - lastBlynkSync > 2000) {
    if (WiFi.status() == WL_CONNECTED) {
      sendDataToBlynk();
    }
    lastBlynkSync = millis();
  }

  static unsigned long lastSerialPrint = 0;
  if (millis() - lastSerialPrint > 2000) {
    Serial.printf("[SENSOR] MQ-6: %d | Rain: %d | Status Gas: %s | Status Rain: %s\n", 
                  mq6Val, rainVal, isGasDetected ? "ALARM" : "OK", isRaining ? "ALARM" : "OK");
    lastSerialPrint = millis();
  }

  if (millis() - lastWeatherUpdate > 600000) {
    fetchWeatherData();
    lastWeatherUpdate = millis();
  }

  display.clearDisplay();

  if (isGasDetected || isRaining || manualMood != MOOD_NORMAL) {
    drawWarningScreen();
  } else {
    drawIdleScreen();
  }

  display.display();
}