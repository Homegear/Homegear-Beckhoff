/* Copyright 2013-2016 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#include "MainInterface.h"
#include "../GD.h"

namespace MyFamily
{

MainInterface::MainInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IPhysicalInterface(GD::bl, settings)
{
	_settings = settings;
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "Beckhoff BK90x0 \"" + settings->id + "\": ");

	signal(SIGPIPE, SIG_IGN);
}

MainInterface::~MainInterface()
{
	stopListening();
}

void MainInterface::startListening()
{
	try
	{
		stopListening();
		init();
		_stopCallbackThread = false;
		if(_settings->listenThreadPriority > -1) _bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &MainInterface::listen, this);
		else _bl->threadManager.start(_listenThread, true, &MainInterface::listen, this);
		IPhysicalInterface::startListening();
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void MainInterface::stopListening()
{
	try
	{
		_stopCallbackThread = true;
		_bl->threadManager.join(_listenThread);
		_stopped = true;
		{
			std::lock_guard<std::mutex> modbusGuard(_modbusMutex);
			if(_modbus)
			{
				modbus_close(_modbus);
				modbus_free(_modbus);
				_modbus = nullptr;
			}
		}
		IPhysicalInterface::stopListening();
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void MainInterface::init()
{
	try
    {
		std::lock_guard<std::mutex> modbusGuard(_modbusMutex);
		if(_modbus)
		{
			modbus_close(_modbus);
			modbus_free(_modbus);
			_modbus = nullptr;
		}
		if(_settings->host.empty())
		{
			_out.printError("Error: Could not connect to BK90x0: Please set \"host\" in \"beckhoffbk90x0.conf\".");
			return;
		}
		_modbus = modbus_new_tcp(_settings->host.c_str(), BaseLib::Math::getNumber(_settings->port));
		if(!_modbus)
		{
			_out.printError("Error: Could not connect to BK90x0: Could not create modbus handle. Are hostname and port set correctly?");
			return;
		}
		int result = modbus_connect(_modbus);
		if(result == -1)
		{
			_out.printError("Error: Could not connect to BK90x0: " + std::string(modbus_strerror(errno)));
			modbus_free(_modbus);
			_modbus = nullptr;
			return;
		}

        memset(&_bk9000Info, 0, sizeof(_bk9000Info));
        result = modbus_read_registers(_modbus, 0x1000, sizeof(_bk9000Info) / 2, (uint16_t*)(&_bk9000Info));
        if(result == -1)
        {
        	_out.printError("Error: Could not read info registers: " + std::string(modbus_strerror(errno)));
        	modbus_close(_modbus);
			modbus_free(_modbus);
			_modbus = nullptr;
			return;
        }

        //Reset Watchdog
		modbus_write_register(_modbus, 0x1121, 0xBECF);
		modbus_write_register(_modbus, 0x1121, 0xAFFE);

        result = modbus_write_register(_modbus, 0x1122, 1); //Schreibtelegramm Watchdog
        if(result == -1)
        {
        	_out.printError("Error: Could not set watchdog type: " + std::string(modbus_strerror(errno)));
        	modbus_close(_modbus);
			modbus_free(_modbus);
			_modbus = nullptr;
			return;
        }

        if((_bk9000Info.busCouplerId[7] == 0x42 && _bk9000Info.busCouplerId[8] >= 0x43) || _bk9000Info.busCouplerId[7] > 0x42)
        {
        	_out.printInfo("Info: Enabling \"Fast Modbus\"...");
			result = modbus_write_register(_modbus, 0x1123, 1); //Fast Modbus
			if(result == -1)
			{
				_out.printError("Error: Could not set TCP mode to \"Fast Modbus\": " + std::string(modbus_strerror(errno)));
			}
        }

        result = modbus_write_register(_modbus, 0x1120, _settings->watchdogTimeout);
        if(result == -1)
        {
        	_out.printInfo("Info: Could not set watchdog interval: " + std::string(modbus_strerror(errno)));
        }

        if(_bk9000Info.status != 0)
        {
        	if(_bk9000Info.status & 0x80) _out.printCritical("Critical: Bus error");
        	else if(_bk9000Info.status & 0x02) _out.printCritical("Critical: Bus coupler configuration error");
        	else if(_bk9000Info.status & 0x01) _out.printCritical("Critical: Bus device error");
        }

        int32_t inputRegisters = (_bk9000Info.analogInputBits + _bk9000Info.digitalInputBits) / 16 + ((_bk9000Info.analogInputBits + _bk9000Info.digitalInputBits) % 16 != 0 ? 1 : 0);
        int32_t outputRegisters = (_bk9000Info.analogOutputBits + _bk9000Info.digitalOutputBits) / 16 + ((_bk9000Info.analogOutputBits + _bk9000Info.digitalOutputBits) % 16 != 0 ? 1 : 0);

        _readBuffer.clear();
        _readBuffer.resize(inputRegisters, 0);
        _writeBuffer.resize(outputRegisters, 0);

        _out.printInfo("Info: Connected to BK90x0. ID: " + std::string(_bk9000Info.busCouplerId, 12) + ", analog input bits: " + std::to_string(_bk9000Info.analogInputBits) + ", analog output bits: " + std::to_string(_bk9000Info.analogOutputBits) + ", digital input bits: " + std::to_string(_bk9000Info.digitalInputBits) + ", digital output bits: " + std::to_string(_bk9000Info.digitalOutputBits));
        _stopped = false;
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void MainInterface::listen()
{
    try
    {
    	int64_t startTime = BaseLib::HelperFunctions::getTimeMicroseconds();
    	int64_t endTime;
    	int64_t timeToSleep;
    	int result;

    	std::vector<uint16_t> readBuffer(_readBuffer.size(), 0);

        while(!_stopCallbackThread)
        {
        	try
        	{
				if(_stopped || !_modbus || _readBuffer.empty())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(2000));
					init();
					if(_stopCallbackThread) return;
					continue;
				}

				if(_readBuffer.empty()) continue;

				if(_outputsEnabled && !_writeBuffer.empty()) result = modbus_write_and_read_registers(_modbus, 0x800, _writeBuffer.size(), &_writeBuffer.at(0), 0x0, readBuffer.size(), &readBuffer.at(0));
				else result = modbus_read_registers(_modbus, 0x0, _readBuffer.size(), &readBuffer.at(0));

				if(result == -1)
				{
					_stopped = true;
					continue;
				}

				if(readBuffer != _readBuffer)
				{
					_readBuffer = readBuffer;
					std::shared_ptr<MyPacket> packet(new MyPacket(0, _readBuffer.size() * 8 - 1, readBuffer));
					raisePacketReceived(packet);
				}

				endTime = BaseLib::HelperFunctions::getTimeMicroseconds();
				timeToSleep = (_settings->interval * 1000) - (endTime - startTime);
				if(timeToSleep < 500) timeToSleep = 500;
				std::this_thread::sleep_for(std::chrono::microseconds(timeToSleep));
				startTime = endTime;
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
        }
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void MainInterface::setOutputData(std::shared_ptr<MyPacket> packet)
{
	try
	{
		while(packet->getStartRegister() >= _writeBuffer.size()) _writeBuffer.push_back(0);

		int32_t startRegister = packet->getStartRegister();
		int32_t endRegister = packet->getEndRegister();
		int32_t startBit = packet->getStartBit() % 16;
		int32_t endBit = 15;
		std::vector<uint16_t>& data = packet->getData();
		int16_t bitValue = 0;
		int32_t dataRegisterPos = 0;
		int32_t dataBitPos = 0;
		for(int32_t i = startRegister; i <= endRegister; i++)
		{
			if(i >= (signed)_writeBuffer.size()) _writeBuffer.push_back(0);
			if(i == endRegister) endBit = packet->getEndBit() % 16;
			for(int32_t j = startBit; j <= endBit; j++)
			{
				bitValue = (data.at(dataRegisterPos) & _bitMask[dataBitPos]) << startBit;
				if(bitValue) _writeBuffer[i] |= bitValue;
				else _writeBuffer[i] &= _reversedBitMask[dataBitPos + startBit];
				dataBitPos++;
				if(dataBitPos == 16)
				{
					dataBitPos = 0;
					dataRegisterPos++;
				}
			}
			startBit = 0;
		}
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void MainInterface::sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet)
{
	try
	{
		std::shared_ptr<MyPacket> myPacket(std::dynamic_pointer_cast<MyPacket>(packet));
		if(!myPacket) return;

		if(GD::bl->debugLevel >= 5) _out.printInfo("Debug: Queuing packet.");

		if(myPacket->getStartRegister() >= _writeBuffer.size())
		{
			_out.printError("Error: Packet has invalid start register: " + std::to_string(myPacket->getStartRegister()));
			return;
		}

		int32_t startRegister = myPacket->getStartRegister();
		int32_t endRegister = myPacket->getEndRegister();
		int32_t startBit = myPacket->getStartBit() % 16;
		int32_t endBit = 15;
		std::vector<uint16_t>& data = myPacket->getData();
		int16_t bitValue = 0;
		int32_t dataRegisterPos = 0;
		int32_t dataBitPos = 0;
		int32_t offset = startBit;
		for(int32_t i = startRegister; i <= endRegister; i++)
		{
			if(i >= (signed)_writeBuffer.size())
			{
				_out.printError("Error: Packet has invalid data size: " + std::to_string(data.size()));
				break;
			}
			if(i == endRegister) endBit = myPacket->getEndBit() % 16;
			for(int32_t j = startBit; j <= endBit; j++)
			{
				if(offset >= 0) bitValue = (data.at(dataRegisterPos) & _bitMask[dataBitPos]) << offset;
				else bitValue = (data.at(dataRegisterPos) & _bitMask[dataBitPos]) >> (-offset);
				if(bitValue) _writeBuffer[i] |= bitValue;
				else _writeBuffer[i] &= _reversedBitMask[dataBitPos + offset];
				dataBitPos++;
				if(dataBitPos == 16)
				{
					if(offset != 0) offset = 16 + offset;
					dataBitPos = 0;
					dataRegisterPos++;
				}
			}
			startBit = 0;
			if(offset != 0) offset = -16 + offset;
		}
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

}
