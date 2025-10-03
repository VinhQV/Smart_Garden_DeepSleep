/******************************************************************************
 * DU AN: HE THONG TUOI CAY THONG MINH (PHIEN BAN 3.2 - NGU AN TOAN)
 * BOARD: ESP32
 * KET NOI: BLYNK IoT
 ******************************************************************************/

// ===== KHAI BAO THONG TIN CHO BLYNK =====
#define BLYNK_TEMPLATE_ID "TMPL6uy2tOjt6"
#define BLYNK_TEMPLATE_NAME "He Thong Tuoi Tieu Thong Minh"
#define BLYNK_AUTH_TOKEN "vBRIlMs_JV1vvXbAFsTGVNXLqAnmLPbj"
#define BLYNK_PRINT Serial

// ===== KHAI BAO CAC THU VIEN CAN THIET =====
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHTesp.h>

// ===== CAU HINH MANG WIFI =====
char ssid[] = "Uy Nam 6 Lau 1";
char pass[] = "0909757323";

// ===== CAU HINH CHAN KET NOI PHAN CUNG =====
const int DHT_PIN = 4;
const int SOIL_MOISTURE_PIN = 34;
const int RELAY_PIN = 18;
const int BUTTON_PIN = 23;
const int WAKE_UP_BUTTON_PIN = 26;
const int STATUS_LED = 22;

// ===== DINH NGHIA CAC KENH VIRTUAL =====
#define VPIN_LED_CONNECT V0
#define VPIN_RELAY_BOM V1
#define VPIN_SOIL_MOISTURE V2
#define VPIN_TEMPERATURE V3
#define VPIN_HUMIDITY V4
#define VPIN_AUTO_MODE_SWITCH V5
#define VPIN_STATUS_TEXT V6

// ===== CAI DAT NGUONG TUOI TU DONG =====
const int MOISTURE_THRESHOLD_LOW = 40;
const int MOISTURE_THRESHOLD_HIGH = 60;

// ===== CAI DAT THOI GIAN CHO CHE DO NGU TU DONG =====
const unsigned long INACTIVITY_TIMEOUT = 15000;
const uint64_t SLEEP_DURATION_S = 60;

// ===== KHAI BAO BIEN TOAN CUC VA DOI TUONG =====
DHTesp dht;
BlynkTimer timer;
WidgetLED ledConnect(VPIN_LED_CONNECT);
volatile bool relayStateChanged = false;
bool isAutoMode = false;
volatile unsigned long lastInteractionTime = 0;

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
    resetInactivityTimer();
}

BLYNK_WRITE(VPIN_RELAY_BOM)
{
    if (!isAutoMode)
    {
        digitalWrite(RELAY_PIN, param.asInt());
    }
    resetInactivityTimer();
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
    }

    int soilValueRaw = analogRead(SOIL_MOISTURE_PIN);
    int soilMoisturePercent = map(soilValueRaw, 4095, 1500, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
    Blynk.virtualWrite(VPIN_SOIL_MOISTURE, soilMoisturePercent);
    Serial.printf("Do am dat: %d%%\n", soilMoisturePercent);

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
    pinMode(STATUS_LED, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(WAKE_UP_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(STATUS_LED, HIGH);
    dht.setup(DHT_PIN, DHTesp::DHT11);
    
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);

    timer.setInterval(1000L, sendSensorAndControl);
    
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_S * 1000000);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_26, 0);

    resetInactivityTimer();
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
        resetInactivityTimer();
    }
    
    // Kiem tra thoi gian khong hoat dong de tu dong ngu // Chi ngu khi bom da tat
    if ((millis() - lastInteractionTime > INACTIVITY_TIMEOUT) && (digitalRead(RELAY_PIN) == LOW)) {
        Serial.println("---------------------------------");
        Serial.println("Khong co tuong tac & bom da tat. Chuan bi vao che do ngu...");
        
        Blynk.virtualWrite(VPIN_STATUS_TEXT, "Ngủ");
        Blynk.run(); 
        
        digitalWrite(STATUS_LED, LOW);
        Serial.println("Thiet bi se thuc day sau 1 phut hoac khi nhan nut.");
        Serial.println("---------------------------------");
        delay(100);
        
        esp_deep_sleep_start();
    }
}