/* Copyright 2013-2019 Homegear GmbH */

#ifndef MAININTERFACE_H_
#define MAININTERFACE_H_

#include "../MyPacket.h"
#include <homegear-base/BaseLib.h>

#include <shared_mutex>

namespace MyFamily {

class MainInterface : public BaseLib::Systems::IPhysicalInterface
{
public:
	MainInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
	virtual ~MainInterface();

	void startListening();
	void stopListening();

	void enableOutputs() { _outputsEnabled = true; }
	uint32_t digitalInputOffset() { return _bk9000Info.analogInputBits; }
	uint32_t digitalOutputOffset() { return _bk9000Info.analogOutputBits; }
	uint32_t analogInputBits() { return _bk9000Info.analogInputBits; }
	uint32_t analogOutputBits() { return _bk9000Info.analogOutputBits; }
	uint32_t digitalInputBits() { return _bk9000Info.digitalInputBits; }
	uint32_t digitalOutputBits() { return _bk9000Info.digitalOutputBits; }

	bool isOpen() { return !_stopped; }

	void setOutputData(std::shared_ptr<MyPacket> packet);
	void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet);
protected:
	struct Bk9000Info
	{
		char busCouplerId[14];		//14	0x1000
		char undefined1[6];			//6		0x1007-0x1009
		uint16_t spsInterface;		//2		0x100A
		uint16_t diag;				//2		0x100B
		uint16_t status;			//2		0x100C
		char undefined2[6];			//6		0x100D-0x100F
		uint16_t analogOutputBits;	//2		0x1010
		uint16_t analogInputBits;	//2		0x1011
		uint16_t digitalOutputBits;	//2		0x1012
		uint16_t digitalInputBits;	//2		0x1013
		char undefined3[24];		//24		0x1014-0x101F
	};

	const uint16_t _bitMask[16] = { 0b0000000000000001, 0b0000000000000010, 0b0000000000000100, 0b0000000000001000, 0b0000000000010000, 0b0000000000100000, 0b0000000001000000, 0b0000000010000000, 0b0000000100000000, 0b0000001000000000, 0b0000010000000000, 0b0000100000000000, 0b0001000000000000, 0b0010000000000000, 0b0100000000000000, 0b1000000000000000 };
	const uint16_t _reversedBitMask[16] = { 0b1111111111111110, 0b1111111111111101, 0b1111111111111011, 0b1111111111110111, 0b1111111111101111, 0b1111111111011111, 0b1111111110111111, 0b1111111101111111, 0b1111111011111111, 0b1111110111111111, 0b1111101111111111, 0b1111011111111111, 0b1110111111111111, 0b1101111111111111, 0b1011111111111111, 0b0111111111111111 };

	BaseLib::Output _out;
	std::mutex _modbusMutex;
	std::shared_ptr<BaseLib::Modbus> _modbus;
	Bk9000Info _bk9000Info;
	std::atomic_bool _outputsEnabled;

	std::shared_timed_mutex _writeBufferMutex;
	std::vector<uint16_t> _writeBuffer;
	std::shared_timed_mutex _readBufferMutex;
	std::vector<uint16_t> _readBuffer;

	void init();
	void listen();
};

}

#endif
