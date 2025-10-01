/******************************************************************************
 * DU AN: HE THONG TUOI CAY THONG MINH (PHIEN BAN 3.1 - NGU KET HOP)
 * BOARD: ESP32
 * KET NOI: BLYNK IoT
 * Tinh nang moi:
 * - Tu dong ngu sau 30s khong hoat dong.
 * - Thuc day sau 1 phut HOAC khi co tin hieu tu nut bam vat ly.
 ******************************************************************************/

// ===== KHAI BAO THONG TIN CHO BLYNK =====
#define BLYNK_TEMPLATE_ID "TMPL6uy2tOjt6"
#define BLYNK_TEMPLATE_NAME "He Thong Tuoi Tieu Thong Minh"
#define BLYNK_AUTH_TOKEN "vBRIlMs_JV1vvXbAFsTGVNXLqAnmLPbj"
#define BLYNK_PRINT Serial // Hien thi cac thong bao cua Blynk ra cong Serial de debug

// ===== KHAI BAO CAC THU VIEN CAN THIET =====
#include <WiFi.h>             // Thu vien de ket noi WiFi cho ESP32
#include <BlynkSimpleEsp32.h> // Thu vien de ket noi ESP32 voi Blynk
#include <DHTesp.h>           // Thu vien cho cam bien nhiet do, do am DHT

// ===== CAU HINH MANG WIFI =====
char ssid[] = "Jizy";
char pass[] = "22222222";

// ===== CAU HINH CHAN KET NOI PHAN CUNG =====
const int DHT_PIN = 4;            // Chan GPIO so 4 noi voi cam bien DHT11
const int SOIL_MOISTURE_PIN = 34; // Chan GPIO so 34 noi voi cam bien do am dat (day la chan analog)
const int RELAY_PIN = 18;         // Chan GPIO so 18 noi voi module Relay dieu khien may bom
const int BUTTON_PIN = 23;        // Chan GPIO so 23 noi voi nut bam vat ly (dieu khien bom)
const int WAKE_UP_BUTTON_PIN = 26; // *** MOI *** Chan GPIO so 26 noi voi nut bam danh thuc

// ===== DINH NGHIA CAC KENH VIRTUAL =====
#define VPIN_LED_CONNECT V0      // V0: Den LED bao hieu ket noi
#define VPIN_RELAY_BOM V1        // V1: Nut de bat/tat may bom
#define VPIN_SOIL_MOISTURE V2    // V2: Dong ho hien thi do am dat
#define VPIN_TEMPERATURE V3      // V3: Dong ho hien thi nhiet do
#define VPIN_HUMIDITY V4         // V4: Dong ho hien thi do am khong khi
#define VPIN_AUTO_MODE_SWITCH V5 // V5: Nut de bat/tat che do tuoi tu dong
#define VPIN_STATUS_TEXT V6      // V6: Hien thi trang thai Ngu/Thuc

// ===== CAI DAT NGUONG TUOI TU DONG =====
const int MOISTURE_THRESHOLD_LOW = 60;  // Neu do am dat duoi 60%, bat may bom
const int MOISTURE_THRESHOLD_HIGH = 80; // Neu do am dat dat 80% tro len, tat may bom

// ===== CAI DAT THOI GIAN CHO CHE DO NGU TU DONG =====
const unsigned long INACTIVITY_TIMEOUT = 30000; // 30 giay (30 * 1000 ms)
const uint64_t SLEEP_DURATION_S = 60;            // 1 phut (60 giay)

// ===== KHAI BAO BIEN TOAN CUC VA DOI TUONG =====
DHTesp dht;                             // Tao mot doi tuong de lam viec voi cam bien DHT
BlynkTimer timer;                      // Tao mot doi tuong timer de thuc hien cac tac vu dinh ky
WidgetLED ledConnect(VPIN_LED_CONNECT); // Tao mot doi tuong LED ao tren V0
volatile bool relayStateChanged = false; // Bien co (flag) de kiem tra trang thai relay co thay doi do nut bam vat ly khong
bool isAutoMode = false;               // Bien de luu trang thai cua che do tu dong (bat/tat)
volatile unsigned long lastInteractionTime = 0; // Bien theo doi thoi gian tuong tac cuoi cung

// ===== HAM DAT LAI THOI GIAN TUONG TAC =====
void resetInactivityTimer() {
    lastInteractionTime = millis();
}

// ===== BLYNK_CONNECTED =====
BLYNK_CONNECTED()
{
    Blynk.syncAll();
    ledConnect.on();
    Blynk.virtualWrite(VPIN_STATUS_TEXT, "Thức");
}

// ===== HAM XU LY SU KIEN TU BLYNK =====
BLYNK_WRITE(VPIN_AUTO_MODE_SWITCH)
{
    isAutoMode = param.asInt();
    if (isAutoMode)
    {
        Serial.println("Che do TU DONG da duoc BAT.");
    }
    else
    {
        Serial.println("Che do TU DONG da duoc TAT. Chuyen sang dieu khien thu cong.");
    }
    resetInactivityTimer(); // Reset bo dem khi co tuong tac
}

BLYNK_WRITE(VPIN_RELAY_BOM)
{
    Serial.println("======================================");
    Serial.println("Ham BLYNK_WRITE(VPIN_RELAY_BOM) da duoc goi!");

    if (!isAutoMode)
    {
        digitalWrite(RELAY_PIN, param.asInt());
        Serial.println("--> Da gui lenh toi Relay.");
    }
    else
    {
        Serial.println("--> Che do Auto dang BAT, bo qua lenh dieu khien.");
    }
    Serial.println("======================================");
    resetInactivityTimer(); // Reset bo dem khi co tuong tac
}

// ===== HAM XU LY NGAT TU NUT BAM VAT LY (DIEU KHIEN BOM) =====
void IRAM_ATTR handleButton()
{
    if (!isAutoMode)
    {
        static unsigned long last_interrupt_time = 0;
        unsigned long interrupt_time = millis();
        if (interrupt_time - last_interrupt_time > 200)
        {
            digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
            relayStateChanged = true;
            last_interrupt_time = interrupt_time;
        }
    }
}

// ===== HAM DOC CAM BIEN VA XU LY LOGIC =====
void sendSensorAndControl()
{
    TempAndHumidity data = dht.getTempAndHumidity();
    if (dht.getStatus() == DHTesp::ERROR_NONE)
    {
        Blynk.virtualWrite(VPIN_TEMPERATURE, data.temperature);
        Blynk.virtualWrite(VPIN_HUMIDITY, data.humidity);
        Serial.printf("Nhiet do: %.1f*C, Do am KK: %.1f%%", data.temperature, data.humidity);
    }

    int soilValueRaw = analogRead(SOIL_MOISTURE_PIN);
    int soilMoisturePercent = map(soilValueRaw, 4095, 1500, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
    Blynk.virtualWrite(VPIN_SOIL_MOISTURE, soilMoisturePercent);
    Serial.printf(" | Do am dat: %d%%\n", soilMoisturePercent);

    if (isAutoMode)
    {
        if (soilMoisturePercent < MOISTURE_THRESHOLD_LOW && digitalRead(RELAY_PIN) == LOW)
        {
            Serial.println("Tu dong: Dat kho, BAT may bom!");
            digitalWrite(RELAY_PIN, HIGH);
            Blynk.virtualWrite(VPIN_RELAY_BOM, HIGH);
        }
        else if (soilMoisturePercent >= MOISTURE_THRESHOLD_HIGH && digitalRead(RELAY_PIN) == HIGH)
        {
            Serial.println("Tu dong: Dat du am, TAT may bom!");
            digitalWrite(RELAY_PIN, LOW);
            Blynk.virtualWrite(VPIN_RELAY_BOM, LOW);
        }
    }
}

// ===== HAM SETUP =====
void setup()
{
    Serial.begin(115200);
    Serial.println("\n=================================");
    Serial.println("HE THONG KHOI DONG / THUC DAY");
    Serial.println("=================================");

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(WAKE_UP_BUTTON_PIN, INPUT_PULLUP); // Cau hinh chan nut danh thuc
    digitalWrite(RELAY_PIN, LOW);
    
    dht.setup(DHT_PIN, DHTesp::DHT11);
    
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);

    timer.setInterval(10000L, sendSensorAndControl);
    
    // *** THAY DOI *** Cau hinh hai nguon danh thuc:
    // 1. Thuc day bang thoi gian (sau 1 phut)
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_S * 1000000); 
    // 2. Thuc day bang nut bam vat ly (chan 26)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_26, 0);

    resetInactivityTimer(); // Bat dau dem thoi gian tu khi khoi dong
}

// ===== HAM LOOP =====
void loop()
{
    Blynk.run();
    timer.run();

    if (relayStateChanged)
    {
        Blynk.virtualWrite(VPIN_RELAY_BOM, digitalRead(RELAY_PIN));
        relayStateChanged = false;
        resetInactivityTimer(); // Reset bo dem khi co tuong tac nut vat ly
    }
    
    // Kiem tra thoi gian khong hoat dong de tu dong ngu
    if (millis() - lastInteractionTime > INACTIVITY_TIMEOUT) {
        Serial.println("---------------------------------");
        Serial.println("Khong co tuong tac. Chuan bi vao che do ngu tu dong...");
        
        Blynk.virtualWrite(VPIN_STATUS_TEXT, "Đang ngủ");
        Blynk.run(); 
        
        Serial.println("Thiet bi se thuc day sau 1 phut hoac khi nhan nut.");
        Serial.println("---------------------------------");
        delay(100);
        
        esp_deep_sleep_start();
    }
}

