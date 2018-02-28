/* Copyright 2013-2017 Homegear UG (haftungsbeschränkt) */

#ifndef MYCENTRAL_H_
#define MYCENTRAL_H_

#include <homegear-base/BaseLib.h>
#include "MyPeer.h"

#include <memory>
#include <mutex>
#include <string>

namespace MyFamily
{

class MyCentral : public BaseLib::Systems::ICentral
{
public:
	MyCentral(ICentralEventSink* eventHandler);
	MyCentral(uint32_t deviceType, std::string serialNumber, ICentralEventSink* eventHandler);
	virtual ~MyCentral();
	virtual void dispose(bool wait = true);

	std::string handleCliCommand(std::string command);
	virtual bool onPacketReceived(std::string& senderId, std::shared_ptr<BaseLib::Systems::Packet> packet);

	uint64_t getPeerIdFromSerial(std::string& serialNumber) { std::shared_ptr<MyPeer> peer = getPeer(serialNumber); if(peer) return peer->getID(); else return 0; }
	std::shared_ptr<MyPeer> getPeer(uint64_t id);
	std::shared_ptr<MyPeer> getPeer(std::string serialNumber);
	void updatePeerAddresses(bool booting = false);

	virtual PVariable createDevice(BaseLib::PRpcClientInfo clientInfo, int32_t deviceType, std::string serialNumber, int32_t address, int32_t firmwareVersion, std::string interfaceId);
	virtual PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags);
	virtual PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerId, int32_t flags);
	virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, ParameterGroup::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel, PVariable paramset);
	virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, uint64_t peerId, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteId, int32_t remoteChannel, PVariable paramset);
	virtual PVariable setInterface(BaseLib::PRpcClientInfo clientInfo, uint64_t peerId, std::string interfaceId);
protected:
	const uint16_t _bitMask[16] = { 0b0000000000000001, 0b0000000000000010, 0b0000000000000100, 0b0000000000001000, 0b0000000000010000, 0b0000000000100000, 0b0000000001000000, 0b0000000010000000, 0b0000000100000000, 0b0000001000000000, 0b0000010000000000, 0b0000100000000000, 0b0001000000000000, 0b0010000000000000, 0b0100000000000000, 0b1000000000000000 };

	virtual void init();
	virtual void loadPeers();
	virtual void savePeers(bool full);
	virtual void loadVariables() {}
	virtual void saveVariables() {}
	std::shared_ptr<MyPeer> createPeer(uint32_t type, int32_t address, std::string serialNumber, bool save = true);
	void deletePeer(uint64_t id);
};

}

#endif
