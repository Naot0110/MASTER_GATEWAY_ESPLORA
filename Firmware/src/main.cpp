#include <Arduino.h>
#include <EEPROM.h>
#include <LoRa.h>
#include <SPI.h>
#include <time.h>
#include <Wire.h>
#include <WiFi.h>
#include <BLE2902.h>
#include <BLEUtils.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <ModbusRtu.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "esp_task_wdt.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==== Fonts ====
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// ==== IO ====
#define BT1 0
#define BT2 32
#define BT3 26
#define BT4 25

#define LED 27
#define RL1 12 // 32
#define RL2 17 // 34
#define RL3 33
#define BUZ 34
// ==== EEPROMS ====
#define EEPROM_SIZE 255
// ==== Modbus ====
#define MODBUS_ID 1

// ==== ZMPT ====
#define ZMPT_PIN 34

// ==== LoRa ====
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2
#define total_Slave 10

// ==== OLED ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ==== WiFi ====
#define WIFI_SSID "CT-ESP32"
#define WIFI_SETUP_SSID "CT-ESP32-Setup"
#define WIFI_PASSWORD "ctesploranode"

// ==== LoRa Packet ====
typedef struct __attribute__((packed)) {
  uint8_t senderID;
  uint16_t packetID;
  float temperature;
  float voltage;
  float current;
  bool lightStatus;
} SensorData;

// ==== WebServer ====
WebServer server(80);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
TaskHandle_t displayTaskHandle;
Preferences preferences;

// ==== Time & Display ====
unsigned long lastInteraction = 0;
const unsigned long timeout = 50000;
const int nodeID = 1;

// ==== Voltage and Current ====
float voltage = 220.0;
float current = 3.0;
const float ADC_REF = 3.3;
const int ADC_RES = 4095;
const float CALIBRATION = 312.5;

// ==== Temperature ====
float temperature = 0.0;

// ==== Clock ====
int hour = 0, minute = 0, second = 0;
unsigned long lastTime1Millis = 0;
unsigned long lastTime2Millis = 0;
unsigned long lastTime3Millis = 0;

unsigned long lastRTCUpdate = 0;
String timeStr = "";

// ==== Flags ====
int Slave_count = 0;
bool WifiStatus = false;
bool connectionStatus = false;

// ==== RELAY ====
bool relay1Status = false;
bool relay2Status = false;
bool relay3Status = false;
bool buzzerStatus = false;

// ==== NTP ====
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;
bool inWiFiSetup = false;

// ==== Menu ====
int menulevel = 0;
int menucount = 0;
bool inmenu = false;
bool addingslave = false;
bool setupwifi = false;
bool mastersetup = false;
bool slavesetup = false;

// ==== Button Pressed ====
bool bt1held = false;
bool bt2held = false;
bool bt3held = false;
bool bt4held = false;
bool bt1Pressed = false;
bool bt2Pressed = false;
bool bt3Pressed = false;
bool bt4Pressed = false;
unsigned long bt1PressStart = 0;
unsigned long bt2PressStart = 0;
unsigned long bt3PressStart = 0;
unsigned long bt4PressStart = 0;
unsigned long lastBT1Press = 0;
unsigned long lastBT2Press = 0;
unsigned long lastBT3Press = 0;
unsigned long lastBT4Press = 0;
static bool lastBT2State = HIGH;
static bool lastBT3State = HIGH;
static bool lastBT4State = HIGH;
const unsigned long debounceDelay = 200;

// ==== ICON ====
static const unsigned char PROGMEM image_Temperature_bits[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x80, 0x02, 0x80, 0x02, 0x80, 0x02, 0x80, 0x02, 0x80, 0x02, 0x80, 0x04, 0x40, 0x07, 0xc0, 0x07, 0xc0, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char PROGMEM image_Layer_21_bits[] = {0x78, 0x00, 0xcc, 0x00, 0xcc, 0x00, 0xcc, 0x00, 0x78, 0xf0, 0x01, 0x98, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x98, 0x00, 0xf0};
static const unsigned char PROGMEM image_wifi_not_connected_bits[] = {0x01, 0x00, 0x02, 0x00, 0x3e, 0x00, 0x45, 0x00, 0x88, 0x80, 0x1c, 0x00, 0x22, 0x00, 0x28, 0x00, 0x40, 0x00};
static const unsigned char PROGMEM image_wifi_full_bits[] = {0x3e, 0x00, 0x41, 0x00, 0x80, 0x80, 0x1c, 0x00, 0x22, 0x00, 0x08, 0x00};
static const unsigned char PROGMEM image_network_4_bars_bits[] = {0x20, 0x08, 0x40, 0x04, 0x89, 0x22, 0x92, 0x92, 0x89, 0x22, 0x40, 0x04, 0x21, 0x08, 0x01, 0x00, 0x02, 0x80, 0x02, 0x80, 0x02, 0x80, 0x04, 0x40, 0x04, 0x40, 0x08, 0x40, 0x08, 0x20, 0x0f, 0xe0};
static const unsigned char PROGMEM image_network_not_connected_bits[] = {0x80, 0x04, 0x60, 0x08, 0x60, 0x14, 0xa9, 0x32, 0x92, 0xb2, 0x89, 0x62, 0x44, 0x84, 0x23, 0x08, 0x03, 0x00, 0x03, 0x80, 0x06, 0x80, 0x0a, 0xc0, 0x14, 0x60, 0x24, 0x50, 0x28, 0x50, 0x48, 0x28, 0x8f, 0xe4};

// ==== Loading Icon ====
static const unsigned char PROGMEM Loading1[] = {0x00, 0xfe, 0x00, 0x03, 0x01, 0x80, 0x04, 0x00, 0x40, 0x18, 0x00, 0x30, 0x10, 0x00, 0x10, 0x20, 0x00, 0x08, 0x40, 0x00, 0x04, 0x40, 0x00, 0x04, 0x80, 0x00, 0x02, 0xe0, 0x00, 0x02, 0xff, 0x80, 0x02, 0xff, 0xf0, 0x02, 0x80, 0x00, 0x02, 0x80, 0x00, 0x02, 0x80, 0x00, 0x02, 0x40, 0x00, 0x04, 0x40, 0x00, 0x04, 0x20, 0x00, 0x08, 0x10, 0x00, 0x10, 0x18, 0x00, 0x30, 0x04, 0x00, 0x40, 0x03, 0x01, 0x80, 0x00, 0xfe, 0x00};
static const unsigned char PROGMEM Loading2[] = {0x00, 0xfe, 0x00, 0x03, 0x01, 0x80, 0x04, 0x00, 0x40, 0x18, 0x00, 0x30, 0x18, 0x00, 0x10, 0x3c, 0x00, 0x08, 0x7e, 0x00, 0x04, 0x7f, 0x00, 0x04, 0xff, 0x80, 0x02, 0xff, 0xc0, 0x02, 0xff, 0xe0, 0x02, 0xff, 0xf0, 0x02, 0x80, 0x00, 0x02, 0x80, 0x00, 0x02, 0x80, 0x00, 0x02, 0x40, 0x00, 0x04, 0x40, 0x00, 0x04, 0x20, 0x00, 0x08, 0x10, 0x00, 0x10, 0x18, 0x00, 0x30, 0x04, 0x00, 0x40, 0x03, 0x01, 0x80, 0x00, 0xfe, 0x00};
static const unsigned char PROGMEM Loading3[] = {0x00, 0xfe, 0x00, 0x03, 0xf1, 0x80, 0x07, 0xf0, 0x40, 0x1f, 0xf0, 0x30, 0x1f, 0xf0, 0x10, 0x3f, 0xf0, 0x08, 0x7f, 0xf0, 0x04, 0x7f, 0xf0, 0x04, 0xff, 0xf0, 0x02, 0xff, 0xf0, 0x02, 0xff, 0xf0, 0x02, 0xff, 0xf0, 0x02, 0x80, 0x00, 0x02, 0x80, 0x00, 0x02, 0x80, 0x00, 0x02, 0x40, 0x00, 0x04, 0x40, 0x00, 0x04, 0x20, 0x00, 0x08, 0x10, 0x00, 0x10, 0x18, 0x00, 0x30, 0x04, 0x00, 0x40, 0x03, 0x01, 0x80, 0x00, 0xfe, 0x00};
static const unsigned char PROGMEM Loading4[] = {0x00, 0xfe, 0x00, 0x03, 0xff, 0x80, 0x07, 0xff, 0xc0, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x3f, 0xff, 0xf8, 0x7f, 0xff, 0xfc, 0x7f, 0xff, 0xfc, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0x80, 0x00, 0x02, 0x80, 0x00, 0x02, 0x80, 0x00, 0x02, 0x40, 0x00, 0x04, 0x40, 0x00, 0x04, 0x20, 0x00, 0x08, 0x10, 0x00, 0x10, 0x18, 0x00, 0x30, 0x04, 0x00, 0x40, 0x03, 0x01, 0x80, 0x00, 0xfe, 0x00};
static const unsigned char PROGMEM Loading5[] = {0x00, 0xfe, 0x00, 0x03, 0xff, 0x80, 0x07, 0xff, 0xc0, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x3f, 0xff, 0xf8, 0x7f, 0xff, 0xfc, 0x7f, 0xff, 0xfc, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0x80, 0x1f, 0xfe, 0x80, 0x07, 0xfe, 0x80, 0x03, 0xfe, 0x40, 0x01, 0xfc, 0x40, 0x00, 0xfc, 0x20, 0x00, 0x78, 0x10, 0x00, 0x30, 0x18, 0x00, 0x30, 0x04, 0x00, 0x40, 0x03, 0x01, 0x80, 0x00, 0xfe, 0x00};
static const unsigned char PROGMEM Loading6[] = {0x00, 0xfe, 0x00, 0x03, 0xff, 0x80, 0x07, 0xff, 0xc0, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x3f, 0xff, 0xf8, 0x7f, 0xff, 0xfc, 0x7f, 0xff, 0xfc, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x7f, 0xff, 0xfc, 0x3f, 0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x07, 0xff, 0xc0, 0x03, 0xff, 0x80, 0x00, 0xfe, 0x00};
static const unsigned char PROGMEM Loading7[] = {0x00, 0xfe, 0x00, 0x03, 0xff, 0x80, 0x07, 0xff, 0xc0, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x3f, 0xff, 0xf8, 0x7f, 0xff, 0xfc, 0x7f, 0xff, 0xfc, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0x80, 0x3f, 0xfe, 0x80, 0x7f, 0xfe, 0x81, 0xff, 0xfe, 0x43, 0xff, 0xfc, 0x47, 0xff, 0xfc, 0x27, 0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x07, 0xff, 0xc0, 0x03, 0xff, 0x80, 0x00, 0xfe, 0x00};
static const unsigned char PROGMEM Loading8[] = {0x00, 0xfe, 0x00, 0x03, 0xff, 0x80, 0x07, 0xff, 0xc0, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x3f, 0xff, 0xf8, 0x7f, 0xff, 0xfc, 0x7f, 0xff, 0xfc, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xfc, 0x7f, 0xff, 0xfc, 0x3f, 0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x07, 0xff, 0xc0, 0x03, 0xff, 0x80, 0x00, 0xfe, 0x00};

// ==== ESP Temperature ====
extern "C" uint8_t temprature_sens_read();
float readTemp();
float acvoltaege();
void dataEvent();
void lobbyEvent();
void displayTask(void *parameter);
void wifimanagerEvent();
void serialEvent();
void scan_IIC_Event();
void menuEvent();
void buttonEvent();
void addSlaveEvent();
void masterEvent();
void slaveEvent();
void sendAck(uint16_t id);
void loraEvent();

void setup()
{
  WiFi.begin();
  Wire.begin();
  EEPROM.begin(255);
  Serial.begin(115200);
  scan_IIC_Event();

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  pinMode(BT1, INPUT_PULLUP);
  pinMode(BT2, INPUT_PULLUP);
  pinMode(BT3, INPUT_PULLUP);
  pinMode(BT4, INPUT_PULLUP);
  pinMode(RL1, OUTPUT);
  pinMode(RL2, OUTPUT);
  pinMode(RL3, OUTPUT);
  pinMode(LED, OUTPUT);

  analogReadResolution(12);

  IPAddress local_IP(192, 168, 10, 1);
  IPAddress gateway(192, 168, 10, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("AP Started");
  Serial.println(WiFi.softAPIP());

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 25);
  display.println("Lora Node");
  display.print("Waiting WiFi...");
  display.display();

  WifiStatus = WiFi.status();
  if (WifiStatus)
  {
    Serial.println("WiFi connected!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 25);
    display.println("WiFi connected!");
    display.display();
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
  else
  {
    Serial.println("can't connect to WiFi!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 25);
    display.println("Can't connect to WiFi!");
    display.display();
  }
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  LoRa.setSyncWord(0xF3);
  LoRa.begin(868E6);

  lastInteraction = millis();

  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);

  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, &displayTaskHandle, 0);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop()
{
  unsigned long now = millis();

  if (millis() - lastTime1Millis >= 5000)
  {
    WifiStatus = WiFi.status();
    lastTime1Millis = millis();
  }
  if (millis() - lastTime2Millis >= 1000)
  {
    temperature = readTemp();
    voltage = acvoltaege();
    lastTime2Millis = millis();
  }
  if (millis() - lastTime3Millis >= 1000)
  {
    lastTime3Millis = millis();
  }

  if (digitalRead(BT3) == LOW)
  {
    if (!bt3held)
    {
      bt3held = true;
      bt3PressStart = now;
    }
    else if (now - bt3PressStart >= 5000)
    {
      inmenu = !inmenu;
      menulevel = 1;
      menucount = 0;
      addingslave = false;
      setupwifi = false;
      mastersetup = false;
      slavesetup = false;
      bt3held = false;
      if (inmenu)
      {
        Serial.println("Entering menu...");
      }
      else
      {
        Serial.println("Exiting menu...");
        menulevel = 0;
        menucount = 0;
        addingslave = false;
        setupwifi = false;
        mastersetup = false;
        slavesetup = false;
      }
    }
    lastInteraction = now;
  }
  else
  {
    bt3held = false;
  }

  buttonEvent();
  // === CÁC MODULE KHÁC (nếu muốn bật) ===
  // receiveAndForward();
  // SerialEvent();

  esp_task_wdt_reset();
}

float readTemp()
{
  return (temprature_sens_read() - 32) / 1.8;
}

float acvoltaege()
{
  static float lastVrms = 0;
  static unsigned long lastRead = 0;
  static float vrmsBuffer[10] = {0};
  static int bufIndex = 0;
  static bool bufFilled = false;
  const unsigned long interval = 500;
  unsigned long now = millis();
  if (now - lastRead < interval)
    return lastVrms;

  int samples = 400;
  float sumSq = 0, offset = 0;

  for (int i = 0; i < samples; i++)
  {
    offset += analogRead(ZMPT_PIN);
  }
  offset /= samples;

  float minVal = 4095, maxVal = 0;
  float values[400];
  for (int i = 0; i < samples; i++)
  {
    int val = analogRead(ZMPT_PIN);
    values[i] = val;
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;
  }
  float lower = minVal + 0.05f * (maxVal - minVal);
  float upper = maxVal - 0.05f * (maxVal - minVal);

  int count = 0;
  for (int i = 0; i < samples; i++)
  {
    if (values[i] >= lower && values[i] <= upper)
    {
      float voltage = (values[i] - offset) * ADC_REF / ADC_RES;
      sumSq += voltage * voltage;
      count++;
    }
  }

  float vrms = sqrt(sumSq / (count > 0 ? count : 1));
  vrmsBuffer[bufIndex] = vrms * CALIBRATION;
  bufIndex = (bufIndex + 1) % 10;
  if (bufIndex == 0) bufFilled = true;

  float avgVrms = 0;
  int avgCount = bufFilled ? 10 : bufIndex;
  for (int i = 0; i < avgCount; i++)
    avgVrms += vrmsBuffer[i];
  avgVrms /= avgCount > 0 ? avgCount : 1;

  lastVrms = avgVrms;
  lastRead = now;
  return lastVrms;
}

void dataEvent()
{
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(40, 5);
  display.printf("%02d:%02d:%02d", hour, minute, second);

  display.setCursor(46, 30);
  display.printf("%.1f", temperature);
  display.drawBitmap(32, 24, image_Temperature_bits, 16, 16, 1);
  display.drawBitmap(76, 26, image_Layer_21_bits, 13, 11, 1);

  display.setCursor(2, 30);
  display.printf("%.0fV", voltage);
  display.setCursor(2, 50);
  display.printf("%.1fA", current);

  display.drawLine(0, 20, SCREEN_WIDTH, 20, SSD1306_WHITE);
  display.drawLine(90, 20, 90, SCREEN_HEIGHT, SSD1306_WHITE);
  display.drawLine(90, 35, SCREEN_WIDTH, 35, SSD1306_WHITE);
  display.drawLine(90, 50, SCREEN_WIDTH, 50, SSD1306_WHITE);
  display.drawLine(0, 43, 34, 43, SSD1306_WHITE);
  display.drawLine(34, 20, 34, SCREEN_HEIGHT, SSD1306_WHITE);

  display.setCursor(100, 25);
  display.setTextColor(relay1Status == LOW ? WHITE : BLACK, SSD1306_WHITE);
  display.print("RL1");
  display.setTextColor(WHITE, BLACK);
  display.setCursor(100, 40);
  display.setTextColor(relay2Status == LOW ? WHITE : BLACK, SSD1306_WHITE);
  display.print("RL2");
  display.setTextColor(WHITE, BLACK);
  display.setCursor(100, 55);
  display.setTextColor(relay3Status == LOW ? WHITE : BLACK, SSD1306_WHITE);
  display.print("RL3");
  display.setTextColor(WHITE, BLACK);

  display.drawBitmap(105, 1, WifiStatus ? image_wifi_full_bits : image_wifi_not_connected_bits, 9, 6, 1);
  display.drawBitmap(0, 0, connectionStatus ? image_network_4_bars_bits : image_network_not_connected_bits, 15, 17, 1);
  display.display();
}

void lobbyEvent()
{
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 26);
  display.printf("%02d:%02d:%02d", hour, minute, second);
  display.display();
  display.setTextSize(1);
}

void displayTask(void *parameter)
{
  esp_task_wdt_add(NULL);
  static int frame = 0;
  static unsigned long lastAnim = 0;
  const unsigned long animInterval = 500;
  const unsigned char *loadingFrames[] = {Loading1, Loading2, Loading3, Loading4, Loading5, Loading6, Loading7, Loading8};
  const int frameCount = 8;
  const unsigned long interval = 500;
  for (;;)
  {
    if (addingslave)
    {
      unsigned long now = millis();
      if (now - lastAnim >= animInterval)
      {
        lastAnim = now;
        frame = (frame + 1) % frameCount;
      }
      display.clearDisplay();
      display.setTextSize(1);
      display.drawLine(0, 10, 126, 10, 1);
      display.setCursor(38, 0);
      display.print("ADDING...");
      display.drawBitmap(53, 21, loadingFrames[frame], 23, 23, 1);
      display.display();
    }
    else if (millis() - lastInteraction > timeout)
      lobbyEvent();
    else
    {
      if (inmenu && menulevel == 1)
        menuEvent();
      else if (inmenu == false && menulevel == 0)
        dataEvent();
    }
    vTaskDelay(pdMS_TO_TICKS(interval));
    esp_task_wdt_reset();
  }
}

void wifimanagerEvent()
{
  WiFiManager wm;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("WiFi Setup Mode");
  display.setCursor(0, 20);
  display.printf("Connect to %s \nPassword %s", WIFI_SSID, WIFI_PASSWORD);
  display.display();
  IPAddress apIP(192, 168, 5, 1);
  IPAddress gateway(192, 168, 5, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP(WIFI_SETUP_SSID, WIFI_PASSWORD);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("WiFi Setup Mode");
  display.setCursor(0, 20);
  display.printf("Connect to %s", WIFI_SETUP_SSID);
  display.printf("Password: %s", WIFI_PASSWORD);
  display.printf("IP: %s", WiFi.softAPIP().toString().c_str());
  display.display();

  wm.setConfigPortalTimeout(180);
  bool res = wm.autoConnect(WIFI_SETUP_SSID, WIFI_PASSWORD);

  if (!res)
  {
    Serial.println("Can't connect to WiFi! Restarting...");
    delay(3000);
    ESP.restart();
  }
  WifiStatus = WiFi.isConnected();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void menuEvent()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (menulevel == 1 && menucount == 0)
  {
    display.drawRoundRect(4, 23, 120, 18, 5, 1);
    display.setCursor(15, 8);
    display.print("1.Add Slaves");
    display.setCursor(15, 29);
    display.print("2.Master setting");
    display.setCursor(15, 51);
    display.print("3.Slave setting");
  }
  if (menulevel == 1 && menucount == 1)
  {
    display.drawRoundRect(4, 23, 120, 18, 5, 1);
    display.setCursor(15, 8);
    display.print("2.Master setting");
    display.setCursor(15, 29);
    display.print("3.Slave setting");
    display.setCursor(15, 51);
    display.print("4.Internet setting");
  }
  if (menulevel == 1 && menucount == 2)
  {
    display.drawRoundRect(4, 23, 120, 18, 5, 1);
    display.setCursor(15, 8);
    display.print("3.Slave setting");
    display.setCursor(15, 29);
    display.print("4.Internet setting");
    display.setCursor(15, 51);
    display.print("5.Exits");
  }
  if (menulevel == 1 && menucount == 3)
  {
    display.drawRoundRect(4, 23, 120, 18, 5, 1);
    display.setCursor(15, 8);
    display.print("4.Internet setting");
    display.setCursor(15, 29);
    display.print("5.Exits");
    display.setCursor(15, 51);
    display.print("1.Add Slaves");
  }
  if (menulevel == 1 && menucount == 4)
  {
    display.drawRoundRect(4, 23, 120, 18, 5, 1);
    display.setCursor(15, 8);
    display.print("5.Exits");
    display.setCursor(15, 29);
    display.print("1.Add Slaves");
    display.setCursor(15, 51);
    display.print("2.Master setting");
  }
  display.display();
}

void buttonEvent()
{
  unsigned long currentMillis = millis();

  bool bt2State = digitalRead(BT2);
  bool bt3State = digitalRead(BT3);
  bool bt4State = digitalRead(BT4);

  if (lastBT2State == HIGH && bt2State == LOW && inmenu == false && menulevel == 0 && currentMillis - lastBT2Press > debounceDelay)
  {
    digitalWrite(RL1, !digitalRead(RL1));
    relay1Status = digitalRead(RL1);
    lastBT2Press = currentMillis;
    lastInteraction = currentMillis;
  }
  else if (lastBT3State == HIGH && bt3State == LOW && inmenu == false && menulevel == 0 && currentMillis - lastBT3Press > debounceDelay)
  {
    digitalWrite(RL2, !digitalRead(RL2));
    relay2Status = digitalRead(RL2);
    lastBT3Press = currentMillis;
    lastInteraction = currentMillis;
  }
  else if (lastBT4State == HIGH && bt4State == LOW && inmenu == false && menulevel == 0 && currentMillis - lastBT4Press > debounceDelay)
  {
    digitalWrite(RL3, !digitalRead(RL3));
    relay3Status = digitalRead(RL3);
    lastBT4Press = currentMillis;
    lastInteraction = currentMillis;
  }

  // === BT2: Menu up ===
  if (lastBT2State == HIGH && bt2State == LOW && menulevel == 1 && currentMillis - lastBT2Press > debounceDelay)
  {
    menucount = (menucount + 1) % 5;
    lastBT2Press = currentMillis;
    lastInteraction = currentMillis;
  }

  // === BT4: Menu down ===
  if (lastBT4State == HIGH && bt4State == LOW && menulevel == 1 && currentMillis - lastBT4Press > debounceDelay)
  {
    menucount = (menucount + 4) % 5;
    lastBT4Press = currentMillis;
    lastInteraction = currentMillis;
  }

  // ==== Select Master Setting ==== //
  if (lastBT3State == HIGH && bt3State == LOW && mastersetup == false && menulevel == 1 && menucount == 0 && currentMillis - lastBT3Press > debounceDelay)
  {
    menulevel = 2;
    menucount = 0;
    mastersetup = true;
    lastBT3Press = currentMillis;
    lastInteraction = currentMillis;
  }
  // ==== Back to main menu ==== //
  else if (lastBT3State == HIGH && bt3State == LOW && mastersetup == true && menulevel == 2 && menucount == 0 && currentMillis - lastBT3Press > debounceDelay)
  {
    lastBT3Press = currentMillis;
    mastersetup = false;
    menulevel = 1;
    menucount = 0;
  }
  // ==== Select Slave ==== //
  if (lastBT3State == HIGH && bt3State == LOW && slavesetup == false && menulevel == 1 && menucount == 1 && currentMillis - lastBT3Press > debounceDelay)
  {
    slavesetup = true;
    menulevel = 2;
    menucount = 1;
    lastBT3Press = currentMillis;
    lastInteraction = currentMillis;
  }
  // ==== Back to main menu ==== //
  else if (lastBT3State == HIGH && bt3State == LOW && slavesetup == true && menulevel == 2 && menucount == 1 && currentMillis - lastBT3Press > debounceDelay)
  {
    lastBT3Press = currentMillis;
    slavesetup = false;
    menulevel = 1;
    menucount = 1;
  }
  // ==== Select Internet setup ==== //
  if (lastBT3State == HIGH && bt3State == LOW && inWiFiSetup == false && menulevel == 1 && menucount == 2 && currentMillis - lastBT3Press > debounceDelay)
  {
    inWiFiSetup = true;
    menulevel = 2;
    menucount = 2;
    lastBT3Press = currentMillis;
    lastInteraction = currentMillis;
  }
  // ==== Back to main menu ==== //
  else if (lastBT3State == HIGH && bt3State == LOW && inWiFiSetup == true && menulevel == 2 && menucount == 2 && currentMillis - lastBT3Press > debounceDelay)
  {
    lastBT3Press = currentMillis;
    inWiFiSetup = false;
    menulevel = 1;
    menucount = 2;
  }
  // ==== Select Exits ==== //
  if (lastBT3State == HIGH && bt3State == LOW && menulevel == 1 && menucount == 3 && currentMillis - lastBT3Press > debounceDelay)
  {
    inmenu = false;
    menulevel = 0;
    menucount = 0;
    addingslave = false;
    mastersetup = false;
    slavesetup = false;
    inWiFiSetup = false;
    display.clearDisplay();
    lastBT3Press = currentMillis;
    lastInteraction = currentMillis;
  }
  // ==== Select Add Slave ==== //
  if (lastBT3State == HIGH && bt3State == LOW && addingslave == false && menulevel == 1 && menucount == 4 && currentMillis - lastBT3Press > debounceDelay)
  {
    addingslave = true;
    menulevel = 2;
    menucount = 3;
    lastBT3Press = currentMillis;
    lastInteraction = currentMillis;
  }
  // ==== Back to main menu ==== //
  else if (lastBT3State == HIGH && bt3State == LOW && addingslave == true && menulevel == 2 && menucount == 3 && currentMillis - lastBT3Press > debounceDelay)
  {
    lastBT3Press = currentMillis;
    addingslave = false;
    menulevel = 1;
    menucount = 4;
    lastInteraction = currentMillis;
  }
  // ==== Select Slave monitor up ==== //
  if (lastBT2State == HIGH && bt2State == LOW && inmenu == true && menulevel == 2 && menucount == 1 && currentMillis - lastBT2Press > debounceDelay)
  {
    Slave_count = (Slave_count + 1) % (total_Slave + 1);
    lastBT3Press = currentMillis;
    lastInteraction = currentMillis;
  }
  // ==== Select Slave monitor down ==== //
  if (lastBT4State == HIGH && bt4State == LOW && inmenu == true && menulevel == 2 && menucount == 1 && currentMillis - lastBT4Press > debounceDelay)
  {
    Slave_count = (Slave_count + total_Slave) % (total_Slave + 1);
    lastBT4Press = currentMillis;
    lastInteraction = currentMillis;
  }

  if (inWiFiSetup)
  {
    wifimanagerEvent();
  }

  if (addingslave)
  {
    addSlaveEvent();
  }

  if (mastersetup)
  {
    masterEvent();
  }

  if (slavesetup)
  {
    slaveEvent();
  }

  lastBT2State = bt2State;
  lastBT3State = bt3State;
  lastBT4State = bt4State;
  // Serial.printf("menulevel: %d| menucount: %d| addingslave: %d| mastersetup: %d| slavesetup: %d| inWiFiSetup: %d|\n", menulevel, menucount, addingslave, mastersetup, slavesetup, inWiFiSetup);
}

void addSlaveEvent()
{
  display.clearDisplay();

  const unsigned long animInterval = 500;
  unsigned long lastAnim = 0;
  int frame = 0;
  const unsigned char *loadingFrames[] = {Loading1, Loading2, Loading3, Loading4, Loading5, Loading6, Loading7, Loading8};
  const int frameCount = 8;
  while (addingslave)
  {
    unsigned long now = millis();
    if (now - lastAnim >= animInterval)
    {
      lastAnim = now;
      display.clearDisplay();
      display.setTextSize(1);
      display.drawLine(0, 10, 126, 10, 1);
      display.setCursor(38, 0);
      display.print("ADDING...");
      display.drawBitmap(53, 21, loadingFrames[frame], 23, 23, 1);
      display.display();
      frame = (frame + 1) % frameCount;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void masterEvent()
{
  display.clearDisplay();

  display.setTextColor(1);
  display.setCursor(47, 1);
  display.print("MASTER");

  display.drawLine(0, 10, 127, 10, 1);

  display.setCursor(0, 13);
  display.printf("IP: %s", WiFi.localIP().toString().c_str());

  display.setCursor(1, 23);
  display.printf("Slaves: %d", total_Slave);

  display.display();
}

void slaveEvent()
{
  display.clearDisplay();

  display.setTextColor(1);
  display.setCursor(41, 1);
  display.printf("SLAVES %d", Slave_count);

  display.drawLine(0, 10, 127, 10, 1);

  display.setCursor(0, 15);
  display.printf("Addr: %d%0X2%d", Slave_count, Slave_count, random(0, 11));

  display.setCursor(0, 25);
  display.printf("Status: %s", random(0, 1) ? "Online" : "Offline");
  display.setCursor(0, 35);
  display.printf("Temp: %.1f C", random(30, 40));
  display.setCursor(0, 45);
  display.printf("Volt: %.1f V", random(220, 230));
  display.setCursor(0, 55);
  display.printf("Light: %d", random(0, 1));

  display.display();
}

void serialEvent()
{
  Serial.printf("Button1: %d | Button2: %d | Button3: %d | Button4: %d |\n", digitalRead(BT1), digitalRead(BT2), digitalRead(BT3), digitalRead(BT4));
}

void scan_IIC_Event()
{
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanning for I2C devices ...");
  for (address = 0x01; address < 0x7f; address++)
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0)
    {
      Serial.printf("I2C device found at address 0x%02X\n", address);
      nDevices++;
    }
    else if (error != 2)
    {
      Serial.printf("Error %d at address 0x%02X\n", error, address);
    }
  }
  if (nDevices == 0)
  {
    Serial.println("No I2C devices found");
  }
}

void sendAck(uint16_t id) {
  LoRa.beginPacket();
  LoRa.write(0xAC);
  LoRa.write((uint8_t)(id >> 8));
  LoRa.write((uint8_t)(id & 0xFF));
  LoRa.endPacket();
}

void loraEvent()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize == sizeof(SensorData)) {
    SensorData data;
    for (int i = 0; i < sizeof(SensorData); i++) {
      ((uint8_t*)&data)[i] = LoRa.read();
    }
    Serial.printf("From Node %d | Temp: %.2f | Volt: %.2f | Current: %.2f | Light: %d\n", data.senderID, data.temperature, data.voltage, data.current, data.lightStatus);
    sendAck(data.packetID);
  }
}