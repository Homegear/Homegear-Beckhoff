/* Copyright 2013-2019 Homegear GmbH */

#ifndef MYPACKET_H_
#define MYPACKET_H_

#include <homegear-base/BaseLib.h>

namespace MyFamily
{

class MyPacket : public BaseLib::Systems::Packet
{
    public:
        MyPacket();
        MyPacket(uint16_t startBit, uint16_t endBit, std::vector<uint16_t>& data);
        MyPacket(uint16_t startBit, uint16_t endBit, uint16_t data);
        virtual ~MyPacket();

        uint16_t& getStartBit() { return _startBit; }
        uint16_t& getEndBit() { return _endBit; }
        uint8_t& getStartRegister() { return _startRegister; }
        uint8_t& getEndRegister() { return _endRegister; }
        std::vector<uint16_t>& getData() { return _data; }

    protected:
        uint16_t _startBit = 0;
        uint16_t _endBit = 0;
        uint8_t _startRegister = 0;
        uint8_t _endRegister = 0;
        std::vector<uint16_t> _data;
};

}
#endif
