/* Copyright 2013-2019 Homegear GmbH */

#include "MainInterface.h"
#include "../GD.h"

namespace MyFamily
{

MainInterface::MainInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IPhysicalInterface(GD::bl, GD::family->getFamily(), settings)
{
	_settings = settings;
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "Beckhoff BK90x0 \"" + settings->id + "\": ");

    BaseLib::Modbus::ModbusInfo modbusInfo;
    modbusInfo.hostname = _settings->host;
    modbusInfo.port = BaseLib::Math::getNumber(_settings->port);

    _modbus = std::make_shared<BaseLib::Modbus>(_bl, modbusInfo);

	memset(&_bk9000Info, 0, sizeof(_bk9000Info));
	_outputsEnabled = false;

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
			_modbus->disconnect();
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

bool MainInterface::reconnect()
{
    init();
    return isOpen();
}

void MainInterface::init()
{
	std::lock_guard<std::mutex> modbusGuard(_modbusMutex);
	try
    {
		_modbus->disconnect();
		if(_settings->host.empty())
		{
			_out.printError("Error: Could not connect to BK90x0: Please set \"host\" in \"beckhoffbk90x0.conf\".");
			return;
		}

		_hostname = _settings->host;
		_ipAddress = BaseLib::Net::resolveHostname(_hostname);

		try
		{
			_modbus->connect();
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printError("Error: Could not connect to BK90x0: " + std::string(ex.what()));
			return;
		}

		std::vector<uint16_t> infoBuffer(sizeof(_bk9000Info) / 2); //Size is an even number so division by 2 works
        memset(&_bk9000Info, 0, sizeof(_bk9000Info));
		try
		{
			_modbus->readHoldingRegisters(0x1000, infoBuffer, infoBuffer.size());
            for(int32_t i = 0; i < 7; i++)
            {
                _bk9000Info.busCouplerId[i * 2] = (char)(uint8_t)(infoBuffer[i] & 0xFF);
                _bk9000Info.busCouplerId[(i * 2) + 1] = (char)(uint8_t)(infoBuffer[i] >> 8);
            }
            _bk9000Info.spsInterface = infoBuffer[10];
            _bk9000Info.diag = infoBuffer[11];
            _bk9000Info.status = infoBuffer[12];
            _bk9000Info.analogOutputBits = infoBuffer[16];
            _bk9000Info.analogInputBits = infoBuffer[17];
            _bk9000Info.digitalOutputBits = infoBuffer[18];
            _bk9000Info.digitalInputBits = infoBuffer[19];
		}
		catch(BaseLib::Exception& ex)
		{
			_modbus->disconnect();
			_out.printError("Error: Could not read info registers: " + std::string(ex.what()));
			return;
		}

        //Reset Watchdog
		try
		{
			_modbus->writeSingleRegister(0x1121, 0xBECF);
			_modbus->writeSingleRegister(0x1121, 0xAFFE);

			_modbus->writeSingleRegister(0x1121, 1);
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printError("Error: Could not set watchdog type: " + std::string(ex.what()));
			_modbus->disconnect();
			return;
		}

        if((_bk9000Info.busCouplerId[7] == 0x42 && _bk9000Info.busCouplerId[8] >= 0x43) || _bk9000Info.busCouplerId[7] > 0x42)
        {
        	_out.printInfo("Info: Enabling \"Fast Modbus\"...");
			try
			{
				_modbus->writeSingleRegister(0x1123, 1); //Fast Modbus
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printError("Error: Could not set TCP mode to \"Fast Modbus\": " + std::string(ex.what()));
			}
        }

		try
		{
			_modbus->writeSingleRegister(0x1120, _settings->watchdogTimeout);
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printInfo("Info: Could not set watchdog interval: " + std::string(ex.what()));
		}

        if(_bk9000Info.status != 0)
        {
        	if(_bk9000Info.status & 0x80) _out.printCritical("Critical: Bus error");
        	else if(_bk9000Info.status & 0x02) _out.printCritical("Critical: Bus coupler configuration error");
        	else if(_bk9000Info.status & 0x01) _out.printCritical("Critical: Bus device error");
        }

        int32_t inputRegisters = (_bk9000Info.analogInputBits + _bk9000Info.digitalInputBits) / 16 + ((_bk9000Info.analogInputBits + _bk9000Info.digitalInputBits) % 16 != 0 ? 1 : 0);
        int32_t outputRegisters = (_bk9000Info.analogOutputBits + _bk9000Info.digitalOutputBits) / 16 + ((_bk9000Info.analogOutputBits + _bk9000Info.digitalOutputBits) % 16 != 0 ? 1 : 0);

        {
            std::lock_guard<std::shared_timed_mutex> readBufferGuard(_readBufferMutex);
            _readBuffer.clear();
            _readBuffer.resize(inputRegisters, 0);
        }

        {
            std::lock_guard<std::shared_timed_mutex> writeBufferGuard(_writeBufferMutex);
            _writeBuffer.resize(outputRegisters, 0);
        }

        _out.printInfo("Info: Connected to BK90x0. ID: " + std::string(_bk9000Info.busCouplerId, 12) + ", analog input bits: " + std::to_string(_bk9000Info.analogInputBits) + ", analog output bits: " + std::to_string(_bk9000Info.analogOutputBits) + ", digital input bits: " + std::to_string(_bk9000Info.digitalInputBits) + ", digital output bits: " + std::to_string(_bk9000Info.digitalOutputBits));
        _stopped = false;
        return;
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
    _modbus->disconnect();
}

void MainInterface::listen()
{
    try
    {
    	int64_t startTime = BaseLib::HelperFunctions::getTimeMicroseconds();
    	int64_t endTime;
    	int64_t timeToSleep;

    	std::vector<uint16_t> readBuffer;
        {
            std::lock_guard<std::shared_timed_mutex> readBufferGuard(_readBufferMutex);
            readBuffer.resize(_readBuffer.size(), 0);
        }

        while(!_stopCallbackThread)
        {
        	try
        	{
				if(_stopped || !_modbus)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(2000));
					init();
					if(_stopCallbackThread) return;
					continue;
				}

				bool readBufferEmpty = false;
                {
                    std::shared_lock<std::shared_timed_mutex> readBufferGuard(_readBufferMutex);
                    readBufferEmpty = _readBuffer.empty();
                }

				if(readBufferEmpty)
				{
                    std::shared_lock<std::shared_timed_mutex> writeBufferGuard(_writeBufferMutex);
					if(_outputsEnabled && !_writeBuffer.empty())
					{
						try
						{
							_modbus->writeMultipleRegisters(0x800, _writeBuffer, _writeBuffer.size());
						}
						catch(BaseLib::Exception& ex)
						{
							_stopped = true;
							continue;
						}
					}
				}
				else
				{
                    std::shared_lock<std::shared_timed_mutex> writeBufferGuard(_writeBufferMutex);
                    {
                        std::shared_lock<std::shared_timed_mutex> readBufferGuard(_readBufferMutex);
                        if(readBuffer.size() != _readBuffer.size()) readBuffer.resize(_readBuffer.size(), 0);
                    }

					//std::cerr << 'W' << BaseLib::HelperFunctions::getHexString(_writeBuffer) << std::endl;
					try
					{
						if(_outputsEnabled && !_writeBuffer.empty()) _modbus->readWriteMultipleRegisters(0x0, readBuffer, readBuffer.size(), 0x800, _writeBuffer, _writeBuffer.size());
						else _modbus->readHoldingRegisters(0x0, readBuffer, readBuffer.size());
					}
					catch(BaseLib::Exception& ex)
					{
						_stopped = true;
						continue;
					}

					_lastPacketSent = BaseLib::HelperFunctions::getTime();
					_lastPacketReceived = _lastPacketSent;
                    std::shared_lock<std::shared_timed_mutex> readBufferGuard(_readBufferMutex);
					if(!std::equal(readBuffer.begin(), readBuffer.end(), _readBuffer.begin()))
					{
                        readBufferGuard.unlock();
                        {
                            std::lock_guard<std::shared_timed_mutex> readBufferGuard2(_readBufferMutex);
                            _readBuffer = readBuffer;
                        }
						//std::cerr << 'R' << BaseLib::HelperFunctions::getHexString(readBuffer) << std::endl;
						std::shared_ptr<MyPacket> packet(new MyPacket(0, readBuffer.size() * 8 - 1, readBuffer));
						raisePacketReceived(packet);
					}
				}

				endTime = BaseLib::HelperFunctions::getTimeMicroseconds();
				timeToSleep = (_settings->interval * 1000) - (endTime - startTime);
				if(timeToSleep < 500) timeToSleep = 500;
				std::this_thread::sleep_for(std::chrono::microseconds(timeToSleep));
				startTime = BaseLib::HelperFunctions::getTimeMicroseconds();
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
        std::lock_guard<std::shared_timed_mutex> writeBufferGuard(_writeBufferMutex);
		while(packet->getStartRegister() >= _writeBuffer.size()) _writeBuffer.push_back(0);

		int32_t startRegister = packet->getStartRegister();
		int32_t endRegister = packet->getEndRegister();
		int32_t startBit = packet->getStartBit() % 16;
		int32_t endBit = 15;
		std::vector<uint16_t>& data = packet->getData();
		if(data.empty()) return;
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

        std::lock_guard<std::shared_timed_mutex> writeBufferGuard(_writeBufferMutex);
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
                {
                    if(bitValue) _writeBuffer[i] |= bitValue;
                    else _writeBuffer[i] &= _reversedBitMask[dataBitPos + offset];
                }
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
