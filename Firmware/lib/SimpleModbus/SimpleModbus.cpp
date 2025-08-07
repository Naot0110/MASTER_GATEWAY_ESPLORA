#include "SimpleModbus.h"

SimpleModbus::SimpleModbus(uint8_t slaveId, HardwareSerial& serial, uint8_t txEnablePin) {
    _serial = &serial;
    _timeout = 1000;
    _state = COM_IDLE;
}

void SimpleModbus::start() {
    _state = COM_IDLE;
}

void SimpleModbus::setTimeOut(uint16_t timeout) {
    _timeout = timeout;
}

void SimpleModbus::query(modbus_t& frame) {
    _state = COM_WAITING;
    // Giả lập gửi request, không thực hiện truyền thông thực
    delay(10);
}

void SimpleModbus::poll(modbus_t& frame) {
    if (_state == COM_WAITING) {
        // Giả lập nhận dữ liệu, gán giá trị mẫu
        for (int i = 0; i < frame.u16CoilsNo; i++) {
            frame.au16reg[i] = random(100, 200); // dữ liệu mẫu
        }
        _state = COM_IDLE;
    }
}

int SimpleModbus::getState() {
    return _state;
}