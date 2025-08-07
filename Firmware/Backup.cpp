#include <EEPROM.h>
#include <ModbusRtu.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"
#include "webpage.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ss 5
#define rst 14
#define dio0 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define total_Slave 10

struct LoRaPacket
{
  int id;
  bool data1;
  int data2;
};
LoRaPacket packetToSend;

struct LoRaPacketRec
{
  int id;
  float data1;
  unsigned long data2;
};
LoRaPacketRec receivedPacket;

struct MasterStation
{
  float sensorValues[6];
  String onTime;
  String offTime;
};
MasterStation master;

struct SlaveStation
{
  int id;
  float temperature;
  int time;
  int sliderValue;
  bool isOn;
  bool isConnected;
  String timeString;
};
SlaveStation slaves[total_Slave];

const char *ssid = "ESP32";
const char *password = "123456789";

unsigned long time_post, time_display, time_selec;
int tempp = 0;

String ip_addr;
String time_now_rtc, day_now_rtc;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
uint16_t selecData[22];
WebServer server(80);

Modbus SELEC(0, Serial2, 0);

RTC_DS1307 rtc;

modbus_t selec_t;

uint8_t u8state;
unsigned long time1 = 0;
unsigned long u32wait;
int countIdSlave = 0;
char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

extern const char *htmlPage;

void sendLora(int ID, int stateLed, int valvePwm);
void handleRoot();
void handleToggleAll();
void handleToggle();
void handleSlider();
void handleSchedule();
void handleResetSchedule();
void handleGetSlaveData();
void handleGetSensorData();
void handleGetTime();
void read_SELEC();
float convertFloat(uint16_t reg_low, uint16_t reg_high);
void displayLCD();
void saveToEEPROM(int slaveId, bool isOn, int sliderValue);
void print_data();

void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  SELEC.start();
  SELEC.setTimeOut(2000);
  u32wait = millis() + 1000;
  u8state = 0;

  EEPROM.begin(255);

  IPAddress local_IP(192, 168, 10, 1);
  IPAddress gateway(192, 168, 10, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  Serial.println("ESP32 Access Point Started");
  Serial.println(WiFi.softAPIP());

  ip_addr = WiFi.softAPIP().toString();
  ;
  for (int i = 0; i < total_Slave; i++)
  {
    slaves[i].id = i + 1;
    slaves[i].temperature = random(20, 35);
    slaves[i].time = 0;
    slaves[i].sliderValue = 50;
    slaves[i].isOn = false;
    slaves[i].isConnected = false;
    slaves[i].timeString = "";
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/toggleAll", HTTP_GET, handleToggleAll);
  server.on("/toggle", HTTP_GET, handleToggle);
  server.on("/slider", HTTP_GET, handleSlider);
  server.on("/schedule", HTTP_GET, handleSchedule);
  server.on("/getSlaveData", HTTP_GET, handleGetSlaveData);
  server.on("/getSensorData", HTTP_GET, handleGetSensorData);
  server.on("/getTime", HTTP_GET, handleGetTime);
  server.on("/resetSchedule", HTTP_GET, handleResetSchedule);
  server.begin();

  LoRa.setPins(ss, rst, dio0);
  // if (!LoRa.begin(433E6))
  // {
  //   Serial.println("LoRa initialization failed!");
  //   while (1)
  //     ;
  // }
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initialized Successfully!");

  // if (!rtc.begin())
  // {
  //   Serial.print("Couldn't find RTC");
  //   while (1)
  //     ;
  // }

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 55);
  display.print("IP:  ");
  display.println(WiFi.softAPIP());
  display.display();

  for (int i = 0; i < total_Slave; i++)
  {
    slaves[i].id = i + 1;
    slaves[i].temperature = random(20, 35);
    slaves[i].time = 0;
    slaves[i].sliderValue = 50;
    slaves[i].isOn = false;
    slaves[i].isConnected = false;
    slaves[i].timeString = "";
  }
  Serial.println("Setup completed successfully!");
}

void loop()
{
  server.handleClient();

  int packetSize = LoRa.parsePacket();
  if (packetSize == sizeof(LoRaPacket))
  {
    LoRa.readBytes((uint8_t *)&receivedPacket, sizeof(receivedPacket));
    slaves[receivedPacket.id - 1].temperature = receivedPacket.data1;
    int totalSeconds = receivedPacket.data2;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    char timeBuffer[9];
    sprintf(timeBuffer, "%02d:%02d:%02d", hours, minutes, seconds);
    slaves[receivedPacket.id - 1].timeString = String(timeBuffer);
    slaves[receivedPacket.id - 1].isConnected = 1;
  }

  if ((unsigned long)(millis() - time_post) > 1000)
  {
    countIdSlave++;
    if (countIdSlave > total_Slave)
    {
      countIdSlave = 1;
    }
    sendLora(countIdSlave, slaves[countIdSlave - 1].isOn, slaves[countIdSlave - 1].sliderValue);

    DateTime now = rtc.now();
    day_now_rtc = String(daysOfTheWeek[now.dayOfTheWeek()]) + " - " + String(now.day()) + "/" + String(now.month()) + "/" + String(now.year());
    char timeBuffer_rtc[6];
    sprintf(timeBuffer_rtc, "%02d:%02d", now.hour(), now.minute());
    time_now_rtc = String(timeBuffer_rtc);
    time_post = millis();
  }

  if ((unsigned long)(millis() - time_display) > 60000)
  {
    displayLCD();
    time_display = millis();
  }

  read_SELEC();
  if ((unsigned long)(millis() - time_selec) > 3000)
  {
    master.sensorValues[3] = convertFloat(selecData[0], selecData[1]);
    master.sensorValues[4] = convertFloat(selecData[2], selecData[3]);
    master.sensorValues[5] = convertFloat(selecData[4], selecData[5]);
    master.sensorValues[0] = convertFloat(selecData[16], selecData[17]);
    master.sensorValues[1] = convertFloat(selecData[18], selecData[19]);
    master.sensorValues[2] = convertFloat(selecData[20], selecData[21]);
    time_selec = millis();
    print_data();
  }

  if (master.onTime == time_now_rtc)
  {
    master.onTime = "";
    slaves[0].isOn = 1;
    slaves[0].sliderValue = 80;
    slaves[1].sliderValue = 80;
    slaves[2].sliderValue = 80;
    slaves[1].isOn = 1;
    slaves[2].isOn = 1;
    slaves[3].isOn = 1;
  }

  if (master.offTime == time_now_rtc)
  {
    master.offTime = "";
    slaves[0].isOn = 0;
    slaves[0].sliderValue = 0;
    slaves[1].sliderValue = 0;
    slaves[2].sliderValue = 0;
    slaves[1].isOn = 0;
    slaves[2].isOn = 0;
    slaves[3].isOn = 0;
  }
}

void sendLora(int ID, int stateLed, int valvePwm)
{
  saveToEEPROM(ID - 1, slaves[ID - 1].isOn, slaves[ID - 1].sliderValue);
  slaves[ID - 1].isConnected = 0;
  packetToSend.id = ID;
  packetToSend.data1 = stateLed;
  packetToSend.data2 = valvePwm;
  LoRa.beginPacket();
  LoRa.write((uint8_t *)&packetToSend, sizeof(packetToSend));
  LoRa.endPacket();
}

void handleRoot()
{
  String page = htmlPage;
  String stationHtml = "";
  for (int i = 0; i < total_Slave; i++)
  {
    stationHtml += "<div class='station'>";
    stationHtml += "<h2>Node Station" + String(slaves[i].id) + "</h2>";
    stationHtml += "<p>Connection Status: <span id='connection" + String(slaves[i].id) + "' style='color:";
    stationHtml += (slaves[i].isConnected ? "green'>Connected" : "red'>Disconnected");
    stationHtml += "</span></p>";
    stationHtml += "<p>Time: <span id='time" + String(slaves[i].id) + "'>" + slaves[i].timeString + "</span></p>";
    stationHtml += "<p>Temperature: <span id='temperature" + String(slaves[i].id) + "'>" + String(slaves[i].temperature) + "</span></p>";
    stationHtml += "<p>Slider Value: <span id='sliderValue" + String(slaves[i].id) + "'>" + String(slaves[i].sliderValue) + "</span></p>";
    stationHtml += "<p>Status: <span id='status" + String(slaves[i].id) + "'>" + (slaves[i].isOn ? "ON" : "OFF") + "</span></p>";
    stationHtml += "<button onclick=\"toggleStation(" + String(slaves[i].id) + ", 'off')\">OFF</button>";
    stationHtml += "<input type='range' min='0' max='100' value='" + String(slaves[i].sliderValue) + "' class='slider' id='slider" + String(slaves[i].id) + "' oninput=\"updateSlider(" + String(slaves[i].id) + ", this.value)\">";
    stationHtml += "<button onclick=\"toggleStation(" + String(slaves[i].id) + ", 'on')\">ON</button> ";
    stationHtml += "</div>";
  }
  page.replace("%SLAVE_STATIONS%", stationHtml);
  server.send(200, "text/html", page);
}

void handleToggleAll()
{
  if (server.hasArg("state"))
  {
    bool newState = (server.arg("state") == "on");
    for (int i = 0; i < total_Slave; i++)
    {
      slaves[i].isOn = newState;
      slaves[i].sliderValue = newState ? 80 : 0;
    }
    server.send(200, "text/plain", "Toggled all");
  }
  else
  {
    server.send(400, "text/plain", "Invalid request");
  }
}

void handleToggle()
{
  int id = server.arg("id").toInt();
  String state = server.arg("state");
  if (id >= 1 && id <= total_Slave)
  {
    if (state == "on")
    {
      slaves[id - 1].isOn = true;
      slaves[id - 1].sliderValue = 80;
    }
    else if (state == "off")
    {
      slaves[id - 1].isOn = false;
      slaves[id - 1].sliderValue = 0;
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSlider()
{
  int id = server.arg("id").toInt();
  int value = server.arg("value").toInt();
  if (id >= 1 && id <= total_Slave)
  {
    slaves[id - 1].sliderValue = value;
    if (value > 80)
    {
      slaves[id - 1].isOn = true;
    }
    else if (value < 10)
    {
      slaves[id - 1].isOn = false;
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSchedule()
{
  if (server.hasArg("onTime") && server.hasArg("offTime"))
  {
    master.onTime = server.arg("onTime");
    master.offTime = server.arg("offTime");
    Serial.println(master.onTime + "   " + master.offTime);
    server.send(200, "text/plain", "Schedule set");
  }
  else
  {
    server.send(400, "text/plain", "Invalid request");
  }
}

void handleResetSchedule()
{
  master.onTime = "";
  master.offTime = "";
  String response = "{\"onTime\": \"" + master.onTime + "\", \"offTime\": \"" + master.offTime + "\"}";
  server.send(200, "application/json", response);
}

void handleGetSlaveData()
{
  String json = "[";
  for (int i = 0; i < total_Slave; i++)
  {
    if (i > 0)
      json += ",";
    json += "{";
    json += "\"id\":" + String(slaves[i].id) + ",";
    json += "\"temperature\":" + String(slaves[i].temperature) + ",";
    json += "\"isOn\":" + String(slaves[i].isOn ? "true" : "false") + ",";
    json += "\"sliderValue\":" + String(slaves[i].sliderValue) + ",";
    json += "\"timeString\":\"" + slaves[i].timeString + "\",";
    json += "\"isConnected\":" + String(slaves[i].isConnected ? "true" : "false");
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleGetSensorData()
{
  String json = "[";
  for (int i = 0; i < 6; i++)
  {
    json += String(master.sensorValues[i]);
    if (i < 5)
      json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleGetTime()
{
  DateTime now = rtc.now();
  String json = "{";
  json += "\"year\":" + String(now.year()) + ",";
  json += "\"month\":" + String(now.month()) + ",";
  json += "\"day\":" + String(now.day()) + ",";
  json += "\"hour\":" + String(now.hour()) + ",";
  json += "\"minute\":" + String(now.minute()) + ",";
  json += "\"second\":" + String(now.second());
  json += "}";
  server.send(200, "application/json", json);
}

void read_SELEC()
{
  switch (u8state)
  {
  case 0:
    if (millis() > u32wait)
      u8state++;
    break;
  case 1:
    selec_t.u8id = 1;
    selec_t.u8fct = 4;
    selec_t.u16RegAdd = 0x0000;
    selec_t.u16CoilsNo = 22;
    selec_t.au16reg = selecData;
    SELEC.query(selec_t);
    u8state++;
    break;
  case 2:
    SELEC.poll();
    if (SELEC.getState() == COM_IDLE)
    {
      u8state = 0;
      u32wait = millis() + 100;
    }
    break;
  }
}

float convertFloat(uint16_t reg_low, uint16_t reg_high)
{
  uint16_t data_temp[2] = {reg_low, reg_high};
  float value;
  memcpy(&value, data_temp, 4);
  return value;
}

void displayLCD()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(35, 0);
  display.print(time_now_rtc);
  display.setTextSize(1);
  display.setCursor(20, 35);
  display.print(day_now_rtc);
  display.setCursor(0, 55);
  display.print("IP:  ");
  display.println(WiFi.softAPIP());
  display.display();
}

void saveToEEPROM(int slaveId, bool isOn, int sliderValue)
{
  if (slaveId < 0 || slaveId >= total_Slave)
    return;
  int addr = slaveId * 8;
  EEPROM.write(addr, isOn);
  EEPROM.write(addr + 4, sliderValue);
  EEPROM.commit();
}

void print_data()
{

  for (int i = 0; i < total_Slave; i++)
  {
    Serial.print("id: ");
    Serial.print(slaves[i].id);
    Serial.print("    temperature: ");
    Serial.print(slaves[i].temperature);
    Serial.print("    slider: ");
    Serial.print(slaves[i].sliderValue);
    Serial.print("   connection: ");
    Serial.print(slaves[i].isConnected);

    Serial.print("   relay: ");
    Serial.print(slaves[i].isOn);
    Serial.print("   time: ");
    Serial.println(slaves[i].timeString);
  }
  Serial.print("V1: ");
  Serial.print(master.sensorValues[3]);
  Serial.print("V2: ");
  Serial.print(master.sensorValues[4]);
  Serial.print("V3: ");
  Serial.println(master.sensorValues[5]);

  Serial.print("I1: ");
  Serial.print(master.sensorValues[0]);
  Serial.print("I2: ");
  Serial.print(master.sensorValues[1]);
  Serial.print("I3: ");
  Serial.println(master.sensorValues[2]);
}
