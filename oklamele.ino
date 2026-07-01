#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>

// =====================================================
// CẤU HÌNH LOGIC
// =====================================================
const int LED_BAT = LOW;   
const int LED_TAT = HIGH; 

LiquidCrystal_I2C lcd(0x27, 16, 2); 
#define DHTTYPE DHT11
const uint8_t PIN_DHT = 8; 
DHT dht(PIN_DHT, DHTTYPE);

const uint8_t PIN_BTN = 2; 

// GIÁ TIỀN & THỜI GIAN
const unsigned long PRICE_BLOCK = 1000; // 1000 VND
const unsigned long TIME_BLOCK  = 30;   // Mỗi 30 giây

// --- SERVO CỔNG VÀO (0 là Mở, 90 là Đóng) ---
const int IN_MO_CONG    = 0;   
const int IN_DONG_CONG  = 90;  

// --- SERVO CỔNG RA (Giữ nguyên) ---
const int DIEM_DUNG = 90; 
const int OUT_DUNG_IM = DIEM_DUNG;
const int OUT_TOC_DO_MO   = 100; 
const int OUT_TOC_DO_DONG = 83; 
const int OUT_THOI_GIAN   = 450; 

const int DIST_OPEN_CM = 8;       
const unsigned long GATE_HOLD_MS = 2000; 
const bool IR_ACTIVE_LOW = true; 

// CẤU HÌNH CHÂN
const uint8_t PIN_SERVO_IN = 14; 
const uint8_t PIN_TRIG_IN  = 16; 
const uint8_t PIN_ECHO_IN  = 17; 

const uint8_t PIN_SERVO_OUT = A1;  
const uint8_t PIN_ECHO_OUT  = 9; 
const uint8_t PIN_TRIG_OUT  = 10; 

const uint8_t IR_PINS[3]  = {3, 4, 5}; 
const uint8_t LED_PINS[3] = {6, 11, 7};

// Biến hệ thống
Servo gateServoIn, gateServoOut;
long lastCmIn = -1, lastCmOut = -1;
unsigned long lastUltraInMs = 0, lastUltraOutMs = 0;
bool gateOpenIn = false, gateOpenOut = false;
unsigned long gateInLastNearMs = 0, gateOutLastNearMs = 0; 
bool isGateOutMoving = false;       
unsigned long gateOutMoveStart = 0; 

// Biến Double Click
int lastBtnState = HIGH;          
unsigned long lastClickTime = 0; 

// Quản lý trang
int currentPage = 0; 
const int MAX_PAGE = 4; 

// QUẢN LÝ XE
bool slotOcc[3] = {false, false, false};       
bool prevSlotOcc[3] = {false, false, false};  
unsigned long slotStartMs[3] = {0, 0, 0};     
unsigned long lastDurSec[3]  = {0, 0, 0};     
unsigned long lastFeeVnd[3]  = {0, 0, 0};  

// HÓA ĐƠN
int receiptSlotIdx = -1;          
unsigned long receiptStartMs = 0; 
const unsigned long RECEIPT_SHOW_MS = 5000; 

// Biến khác
unsigned long lastDhtMs = 0, lastUiMs = 0;
unsigned long lastMemberFlipMs = 0, lastFeeFlipMs = 0; 
float tempC = 0, humi = 0;
uint8_t memberBlock = 0;
int currentSlotViewIdx = 0; 
const unsigned long LED_WARN_MS = 30000; 
const unsigned long BLINK_SPEED = 500;   
bool isQrAuthorized = false;  
unsigned long qrAuthTime = 0; 

// Hàm đổi giây -> mm:ss
String fmtTime(unsigned long s) {
  unsigned long m = s / 60;
  unsigned long sec = s % 60;
  String res = "";
  if (m < 10) res += "0"; res += String(m) + ":";
  if (sec < 10) res += "0"; res += String(sec);
  return res;
}

void updateUltrasonicIn() {
  if (millis() - lastUltraInMs < 80) return;
  lastUltraInMs = millis();
  digitalWrite(PIN_TRIG_IN, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG_IN, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG_IN, LOW);
  long dur = pulseIn(PIN_ECHO_IN, HIGH, 25000UL);
  if(dur > 0) lastCmIn = (dur * 0.0343) / 2;
}

void updateGateIn() {
  bool isNear = (lastCmIn > 0 && lastCmIn <= DIST_OPEN_CM);
  if (isQrAuthorized && (millis() - qrAuthTime > 10000)) { isQrAuthorized = false; }
  if (!gateOpenIn) { 
    if (isQrAuthorized && isNear) {
      gateOpenIn = true; gateServoIn.write(IN_MO_CONG); gateInLastNearMs = millis(); isQrAuthorized = false; 
    }
  } else { 
    if (isNear) { gateInLastNearMs = millis(); } 
    else if (millis() - gateInLastNearMs >= GATE_HOLD_MS) {
      gateOpenIn = false; gateServoIn.write(IN_DONG_CONG); 
    }
  }
}

void updateUltrasonicOut() {
  if (millis() - lastUltraOutMs < 80) return;
  lastUltraOutMs = millis();
  digitalWrite(PIN_TRIG_OUT, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG_OUT, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG_OUT, LOW);
  long dur = pulseIn(PIN_ECHO_OUT, HIGH, 25000UL);
  if(dur > 0) lastCmOut = (dur * 0.0343) / 2;
}

void updateGateOut() {
  bool isNear = (lastCmOut > 0 && lastCmOut <= DIST_OPEN_CM);

  if (!gateOpenOut) { 
    if (isNear) {
      gateOpenOut = true;
      gateServoOut.write(OUT_TOC_DO_MO); 
      isGateOutMoving = true;
      gateOutMoveStart = millis();
      gateOutLastNearMs = millis();
    }
  }
  else { 
    if (isGateOutMoving) {
        if (millis() - gateOutMoveStart >= OUT_THOI_GIAN) {
            gateServoOut.write(OUT_DUNG_IM); 
            isGateOutMoving = false; 
        }
    } else {
        gateServoOut.write(OUT_DUNG_IM);
    }

    if (isNear) {
      gateOutLastNearMs = millis(); 
    } 
    else if (millis() - gateOutLastNearMs >= GATE_HOLD_MS) {
      if (!isGateOutMoving) { 
          gateOpenOut = false;
          gateServoOut.write(OUT_TOC_DO_DONG);
          delay(OUT_THOI_GIAN); 
          gateServoOut.write(OUT_DUNG_IM);
      }
    }
  }
}

void updateAllSlots() {
  for(int i=0; i<3; i++) {
    int sensorVal = digitalRead(IR_PINS[i]);
    slotOcc[i] = (sensorVal == (IR_ACTIVE_LOW ? LOW : HIGH));
    
    // KHI CÓ XE VÀO
    if (slotOcc[i] && !prevSlotOcc[i]) {
      slotStartMs[i] = millis();
      currentPage = 4;
      currentSlotViewIdx = i; 
      lastFeeFlipMs = millis(); 
    }

    // KHI XE RA
    if (!slotOcc[i] && prevSlotOcc[i]) { 
      lastDurSec[i] = (millis() - slotStartMs[i]) / 1000;
      // Tính tiền block 30s
      lastFeeVnd[i] = (lastDurSec[i] / TIME_BLOCK) * PRICE_BLOCK;
      receiptSlotIdx = i;
      receiptStartMs = millis();
      currentPage = 4; 
    }
    prevSlotOcc[i] = slotOcc[i];

    if (slotOcc[i]) { 
      if (millis() - slotStartMs[i] >= LED_WARN_MS) {
        int blinkVal = (millis()/BLINK_SPEED % 2 == 0) ? LED_BAT : LED_TAT;
        digitalWrite(LED_PINS[i], blinkVal);
      } else { digitalWrite(LED_PINS[i], LED_BAT); }
    } else { digitalWrite(LED_PINS[i], LED_TAT); }
  }
}

void readDht() {
  if (millis() - lastDhtMs < 2000) return; lastDhtMs = millis();
  float h = dht.readHumidity(); float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) { humi = h; tempC = t; }
}

void displayLCD() {
  auto printLine = [](int row, String s) {
    lcd.setCursor(0, row); while(s.length() < 16) s += " "; lcd.print(s.substring(0, 16));
  };

  // --- TRANG 0: ĐỀ TÀI (MẶC ĐỊNH KHI BẬT) ---
  if (currentPage == 0) { 
    printLine(0, "DO AN: GUI XE"); printLine(1, "SMART PARKING"); 
  } 
  else if (currentPage == 1) { 
    if (millis() - lastMemberFlipMs > 2000) { memberBlock = (memberBlock + 1) % 3; lastMemberFlipMs = millis(); }
    if (memberBlock == 0) { printLine(0, "1) Mac Duc Anh"); printLine(1, "2) Tran Huu Long"); }
    else if (memberBlock == 1) { printLine(0, "3) Pham Trung Dung"); printLine(1, "4) Nguyen Duc Anh"); }
    else { printLine(0, "5) Nguyen Ngoc Toan"); printLine(1, ""); }
  }
  else if (currentPage == 2) { 
    int used = (slotOcc[0]?1:0) + (slotOcc[1]?1:0) + (slotOcc[2]?1:0); 
    printLine(0, "FREE:" + String(3 - used) + " USED:" + String(used));
    printLine(1, "S1:" + String(slotOcc[0]?"F":"E") + " S2:" + String(slotOcc[1]?"F":"E") + " S3:" + String(slotOcc[2]?"F":"E"));
  }
  else if (currentPage == 3) {
    printLine(0, "T:" + String(tempC, 1) + "C H:" + String((int)humi) + "%");
    String qrStatus = isQrAuthorized ? " Q" : "";
    printLine(1, "D1:" + String(lastCmIn) + " D2:" + String(lastCmOut) + qrStatus);
  }
  else if (currentPage == 4) {
    // 1. HÓA ĐƠN
    if (receiptSlotIdx != -1) {
       if (millis() - receiptStartMs < RECEIPT_SHOW_MS) {
          printLine(0, "S" + String(receiptSlotIdx+1) + " TONG:" + fmtTime(lastDurSec[receiptSlotIdx]));
          printLine(1, "TIEN:" + String(lastFeeVnd[receiptSlotIdx]) + " VND");
       } else {
          receiptSlotIdx = -1;
       }
    } 
    // 2. THEO DÕI
    else {
       bool anyCar = false;
       for(int k=0; k<3; k++) if(slotOcc[k]) anyCar = true;

       if (!anyCar) {
           printLine(0, "NHA XE TRONG");
           printLine(1, "CHO XE VAO...");
       } 
       else {
           if (millis() - lastFeeFlipMs > 5000) { 
              lastFeeFlipMs = millis(); 
              int nextIdx = currentSlotViewIdx;
              do {
                 nextIdx++;
                 if (nextIdx > 2) nextIdx = 0;
              } while (!slotOcc[nextIdx]); 
              currentSlotViewIdx = nextIdx;
           }
           
           if (!slotOcc[currentSlotViewIdx]) {
              int nextIdx = currentSlotViewIdx;
              do {
                 nextIdx++;
                 if (nextIdx > 2) nextIdx = 0;
              } while (!slotOcc[nextIdx]);
              currentSlotViewIdx = nextIdx;
           }
           
           int idx = currentSlotViewIdx;
           unsigned long currentSec = (millis() - slotStartMs[idx]) / 1000;
           unsigned long currentFee = (currentSec / TIME_BLOCK) * PRICE_BLOCK;
           
           printLine(0, "S" + String(idx+1) + " DANG DO:" + fmtTime(currentSec));
           printLine(1, "TIEN:" + String(currentFee) + " VND");
       }
    }
  }
}

void setup() {
  Serial.begin(9600); 
  pinMode(PIN_TRIG_IN, OUTPUT); pinMode(PIN_ECHO_IN, INPUT);
  gateServoIn.attach(PIN_SERVO_IN); gateServoIn.write(IN_DONG_CONG); 
  pinMode(PIN_TRIG_OUT, OUTPUT); pinMode(PIN_ECHO_OUT, INPUT);
  
  gateServoOut.attach(PIN_SERVO_OUT); 
  gateServoOut.write(OUT_DUNG_IM); 
  
  for(int i=0; i<3; i++) { pinMode(IR_PINS[i], INPUT_PULLUP); pinMode(LED_PINS[i], OUTPUT); }
  pinMode(PIN_BTN, INPUT_PULLUP); lcd.init(); lcd.backlight(); dht.begin();

  // [SỬA LỖI]: KHỞI TẠO TRẠNG THÁI CẢM BIẾN ĐỂ TRÁNH NHẢY TRANG
  delay(500); // Chờ ổn định điện áp
  for(int i=0; i<3; i++) {
    int val = digitalRead(IR_PINS[i]);
    bool isOcc = (val == (IR_ACTIVE_LOW ? LOW : HIGH));
    slotOcc[i] = isOcc;
    prevSlotOcc[i] = isOcc; // Đồng bộ để không kích hoạt "Xe mới vào"
  }

  // [SỬA LỖI]: BẮT BUỘC VỀ TRANG 0
  currentPage = 0; 
  lcd.clear();
  
  Serial.println(F("System Started..."));
}

void loop() {
  if (Serial.available() > 0) {
    char signal = Serial.read();
    if (signal == '1') { isQrAuthorized = true; qrAuthTime = millis(); }
  }

  int currentBtnState = digitalRead(PIN_BTN);
  if (currentBtnState == LOW && lastBtnState == HIGH) {
      unsigned long now = millis();
      if (now - lastClickTime < 500) {
          if (currentPage == 0) currentPage = MAX_PAGE; else currentPage--;
          if (currentPage == 0) currentPage = MAX_PAGE; else currentPage--;
      } else { if (currentPage >= MAX_PAGE) currentPage = 0; else currentPage++; }
      lastClickTime = now; lcd.clear(); delay(50);
  }
  lastBtnState = currentBtnState;

  updateUltrasonicIn(); updateGateIn(); updateUltrasonicOut(); updateGateOut(); 
  updateAllSlots(); readDht();
  
  if (millis() - lastUiMs > 250) { lastUiMs = millis(); displayLCD(); }
}