/* Copyright 2013-2017 Homegear UG (haftungsbeschränkt) */

#include "MyPacket.h"

#include "GD.h"

namespace MyFamily
{
MyPacket::MyPacket()
{
}

MyPacket::MyPacket(uint16_t startBit, uint16_t endBit, std::vector<uint16_t>& data) : _startBit(startBit), _endBit(endBit), _data(data)
{
	_timeReceived = BaseLib::HelperFunctions::getTime();
	_startRegister = _startBit / 16;
	_endRegister = _endBit / 16;
}

MyPacket::MyPacket(uint16_t startBit, uint16_t endBit, uint16_t data) : _startBit(startBit), _endBit(endBit)
{
	_timeReceived = BaseLib::HelperFunctions::getTime();
	_startRegister = _startBit / 16;
	_endRegister = _endBit / 16;
	_data = std::vector<uint16_t> { data };
}

MyPacket::~MyPacket()
{
	_data.clear();
}

}
