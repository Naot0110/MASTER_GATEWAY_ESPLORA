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

char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

WebServer server(80);

uint16_t selecData[22];
Modbus SELEC(0, Serial2, 0);
modbus_t selec_t;

uint8_t u8state;
unsigned long time1 = 0;
unsigned long u32wait;
int countIdSlave = 0;
