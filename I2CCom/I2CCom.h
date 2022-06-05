#ifndef I2CCom_h
#define I2CCom_h

#include <Arduino.h>
#include "Wire.h"
#include "UUID.h"

typedef void (*intFunc)(int a);
typedef void (*intintFunc)(int a, int b);
typedef void (*intintintFunc)(int a, int b, int l);

#ifdef BUFFER_LENGTH
#define _WIRE_BUFFER_SIZE BUFFER_LENGTH
#endif

#ifdef WIRE_BUFFER_SIZE
#define _WIRE_BUFFER_SIZE WIRE_BUFFER_SIZE
#endif

#ifndef _WIRE_BUFFER_SIZE
#define _WIRE_BUFFER_SIZE 32
#endif

//#define DEBUG

#ifdef DEBUG
#define debug(x) Serial.print(x);
#define debugln(x) Serial.println(x);
#else
#define debug(x)
#define debugln(x)
#endif
//Statuses:
//0 - READY
//1 - HAVE DATA
//OTHER - ERROR

#define POOL_START 8
#define POOL_END__ 126
#define ADDRESSING_POOL_START 127
#define ADDRESSING_POOL_END__ 127

struct device
{
    uint8_t device_id;
    bool ignore = false;
};

struct deviceInfo
{
    uint8_t id;
    uint8_t data_ready;
    uint8_t status;
    uint8_t data_length;
};

class I2CCom_Slave
{
public:
    uint8_t device_id;
    uint8_t address;
    uint8_t status;
    uint8_t data_ready;
    uint8_t data_req;
    uint8_t data[_WIRE_BUFFER_SIZE];
    uint8_t data_length;
    uint8_t waiting_addr;
    uint8_t uuid0 = 0;
    long lastPing;
    TwoWire *_wire;

    int ready()
    {
        long diff = (millis() - lastPing);
        if (diff > 500)
        {
            //lastPing = millis();
            begin();
            return 2;
        }
        if (address >= POOL_START && address <= POOL_END__)
        {
            return 0;
        }
        else
        {
            return 3;
        }
        return 1;
    }

    I2CCom_Slave(uint8_t DEV_ID, uint8_t _waiting_addr, TwoWire *wire = &Wire)
    {
        device_id = DEV_ID;
        _wire = wire;
        waiting_addr = _waiting_addr;
    }

    I2CCom_Slave(uint8_t DEV_ID, TwoWire *wire = &Wire)
    {
        device_id = DEV_ID;
        _wire = wire;
        waiting_addr = 127;
    }

    void begin()
    {
        address = waiting_addr;
        _wire->begin(address);
        uuid0 = GetTimeByte();
        status = 2;
        lastPing = millis();
    }

    void Interrupt(uint8_t action_id, uint8_t *_data, uint8_t length)
    {
        Interrupt(action_id);
        memcpy(data, _data, length);
        data_length = length;
    }

    void Interrupt(uint8_t action_id, uint8_t data)
    {
        uint8_t _data[1];
        _data[0] = data;
        Interrupt(action_id, _data, 1);
    }

    void Interrupt(uint8_t action_id, int8_t data)
    {
        Interrupt(action_id, (uint8_t)data);
    }

    void Interrupt(uint8_t action_id, char data)
    {
        char _data[2];
        _data[0] = data;
        _data[1] = 0;
        Interrupt(action_id, _data);
    }

    void Interrupt(uint8_t action_id, const char *data)
    {
        Interrupt(action_id, (uint8_t *)data, strlen(data) + 1);
    }

    void Interrupt(uint8_t action_id)
    {
        data_length = 0;
        if (status > 1)
            return;

        data_ready = action_id;
        status = 1;
    }

    bool receiveEvent()
    {
        lastPing = millis();
        data_req = _wire->read();
        if (_wire->available())
        {
            if (data_req == 0xFF && address == 127)
            {
                uint8_t id = _wire->read();
                if (id == uuid0)
                {
                    address = _wire->read();
                    _wire->begin(address);
                    status = 0;
                }
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    bool requestEvent()
    {
        if (data_req == data_ready && status == 1)
        {
            data_ready = 0;
            status = 0;
            if (data_length > 0)
            {
                _wire->write((const uint8_t *)data, data_length);
                return true;
            }
        }
        else if (data_req == 0x00)
        {
            _wire->write(device_id);
            _wire->write(status);
            _wire->write(data_ready);
            _wire->write(data_length);
        }
        else if (data_req == 0xFE && address == 127)
        {
            uint8_t arr[32];
            debug(F("Byte is: "));
            debugln(uuid0);
            GenArr(uuid0, arr);
            for (int i = 0; i < 32; i++)
            {
                uint8_t data = arr[i];
                _wire->write(data);
            }
        }
        else
        {
            return false;
        }
        return true;
    }
};

class I2CCom_Master
{
public:
    device devices[128];
    I2CCom_Master(TwoWire *wire = &Wire)
    {
        _wire = wire;
    }

    void begin()
    {
        _wire->begin();
        for (byte i = 0; i < 128; i++)
        {
            devices[i].device_id = 255;
            devices[i].ignore = false;
        }
    }

    TwoWire *_wire;

    intFunc OnDeviceConnected = NULL;
    intFunc OnDeviceDisconnected = NULL;
    intintintFunc OnData = NULL;

    void SendData(uint8_t address, uint8_t action_id, int8_t *data, uint8_t length)
    {
        _wire->beginTransmission(address);
        _wire->write(action_id);
        _wire->write((const uint8_t *)data, length);
        _wire->endTransmission();
    }

    bool SendDataByID(uint8_t dev_type, uint8_t action_id, int8_t *data, uint8_t length)
    {
        for (uint8_t address = POOL_START; address <= POOL_END__; address++)
        {
            if (devices[address].device_id == dev_type)
            {
                SendData(address, action_id, data, length);
                return true;
            }
        }
        return false;
    }

    uint8_t GetAddrByID(uint8_t dev_type)
    {
        for (uint8_t address = POOL_START; address <= POOL_END__; address++)
        {
            if (devices[address].device_id == dev_type)
            {
                return address;
            }
        }
        return 0;
    }

    size_t RequestData(uint8_t address, uint8_t action_id, uint8_t length)
    {
        _wire->beginTransmission(address);
        _wire->write(action_id);
        _wire->endTransmission();
        return _wire->requestFrom(address, length);
    }

    size_t SendAndRequestData(int8_t *send_buff,int8_t send_len,uint8_t address, uint8_t action_id, uint8_t length)
    {
        _wire->beginTransmission(address);
		_wire->write(action_id);
        _wire->write((const uint8_t *)send_buff,send_len);
        _wire->endTransmission();
        return _wire->requestFrom(address, length);
    }

    size_t RequestData(int8_t *buff, uint8_t length, uint8_t address, uint8_t action_id)
    {
        _wire->beginTransmission(address);
        _wire->write(action_id);
        _wire->endTransmission();
        while (_wire->available())
        {
            _wire->read();
        }
        _wire->requestFrom(address, length);
        if (_wire->available() < 1)
        {
            debug(F("Empty data: "));
            debug(address);
            debug("-");
            debug(action_id);
            return 0;
        }
        size_t s = _wire->readBytes((uint8_t *)buff, length);
        if (_wire->available())
        {
            debug(F("Unused request data: "));
            debug(_wire->available());
            debug("-");
            debug(address);
            debug("-");
            debug(action_id);
        }
        return s;
    }

    deviceInfo GetDeviceInfo(uint8_t address)
    {
        int8_t buff[4];
        RequestData(buff, 4, address, 0x00);
        deviceInfo info;
        info.id = buff[0];
        info.status = buff[1];
        info.data_ready = buff[2];
        info.data_length = buff[3];
        return info;
    }

    void GetDeviceIDs(uint8_t *buff, uint8_t address)
    {
        RequestData((int8_t *)buff, 32, address, 0xFE);
    }

    void Ignore(uint8_t address)
    {
        devices[address].ignore = true;
    }

    void ScanDevices()
    {
        uint8_t address;
        for (address = POOL_START; address <= POOL_END__; address++)
        {
            if (CheckDevice(address) == 0)
            {
                if (!devices[address].ignore)
                {
                    deviceInfo info = GetDeviceInfo(address);
                    if (devices[address].device_id == 255)
                    {
                        if (OnDeviceConnected != NULL)
                        {
                            OnDeviceConnected(address);
                        }
                    }
                    devices[address].device_id = info.id;
                    if (info.status == 1)
                    {
                        if (OnData != NULL)
                        {
                            OnData(address, info.data_ready, info.data_length);
                        }
                    }
                }
            }
            else
            {
                if (devices[address].device_id != 255)
                {
                    if (OnDeviceDisconnected != NULL)
                    {
                        OnDeviceDisconnected(address);
                    }
                }
                devices[address].device_id = 255;
            }
        }
        for (address = ADDRESSING_POOL_START; address <= ADDRESSING_POOL_END__; address++)
        {
            int8_t unadr_status = CheckDevice(address);
            if (unadr_status == 0)
            {
                AssignAddress(address);
            }
        }
    }

    int8_t CheckDevice(uint8_t address)
    {
        _wire->beginTransmission(address);
        int8_t error = _wire->endTransmission();

        if (error == 0)
        {
            return 0;
        }
        else if (error == 4)
        {
            return -1;
        }
        return -1;
    }

    void AssignAddress(uint8_t address)
    {
        //ADDRESS_POOL: <32;126>
        int8_t found_address = 0;
        bool found = false;
        for (int8_t a = POOL_START; a <= POOL_END__; a++)
        {
            if (devices[a].device_id == 255)
            {
                found_address = a;
                found = true;
                break;
            }
        }
        if (found)
        {
            uint8_t data[32];
            GetDeviceIDs(data, address);
            for (int i = 0; i < 32; i++)
            {
                for (int i2 = 0; i2 < 8; i2++)
                {
                    if (bitRead(data[i], i2) == 0)
                    {
                        uint8_t addr[2];
                        addr[0] = i * 8 + i2;
                        addr[1] = found_address;
                        debug(addr[0]);
                        debug(F(" - gets: "));
                        debug(addr[1]);
                        SendData(address, 0xFF, (int8_t *)addr, 2);
                        for (int t = 0; t < 10; t++)
                        {
                            if (CheckDevice(found_address) == 0)
                                break;
                            delay(20);
                        }
                        return;
                    }
                }
            }
        }
    }
};

#endif