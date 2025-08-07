#pragma once
#include <Arduino.h>

struct modbus_t {
    uint8_t u8id;
    uint8_t u8fct;
    uint16_t u16RegAdd;
    uint16_t u16CoilsNo;
    uint16_t* au16reg;
};

class SimpleModbus {
public:
    SimpleModbus(uint8_t slaveId, HardwareSerial& serial, uint8_t txEnablePin = 0);
    void start();
    void setTimeOut(uint16_t timeout);
    void query(modbus_t& frame);
    void poll(modbus_t &frame);
    int getState();
private:
    HardwareSerial* _serial;
    uint16_t _timeout;
    int _state;
};

#define COM_IDLE 0
#define COM_WAITING 1
#define COM_DONE 2