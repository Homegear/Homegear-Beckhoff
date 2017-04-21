/* Copyright 2013-2017 Sathya Laufer
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

#ifndef MYPEER_H_
#define MYPEER_H_

#include "PhysicalInterfaces/MainInterface.h"

#include <homegear-base/BaseLib.h>

#include <list>

using namespace BaseLib;
using namespace BaseLib::DeviceDescription;

namespace MyFamily
{
class MyCentral;

class MyPeer : public BaseLib::Systems::Peer, public BaseLib::Rpc::IWebserverEventSink
{
public:
	MyPeer(uint32_t parentID, IPeerEventSink* eventHandler);
	MyPeer(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler);
	virtual ~MyPeer();
	void init();
	void dispose();

	//Features
	virtual bool wireless() { return false; }
	//End features

	//{{{ In table variables
	std::string getPhysicalInterfaceId() { return _physicalInterfaceId; }
	void setPhysicalInterfaceId(std::string);
	//}}}

	std::shared_ptr<MainInterface>& getPhysicalInterface() { return _physicalInterface; }

	virtual void setAddress(int32_t value);

	bool isOutputDevice();
	bool isAnalog();
	int32_t getBitSize() { if(_bitSize == -1) getStorageSize(); return _bitSize; }
	int32_t getRegisterSize() { if(_registerSize == -1) getStorageSize(); return _registerSize; }

	virtual std::string handleCliCommand(std::string command);
	void packetReceived(std::vector<uint16_t>& packet);

	virtual bool load(BaseLib::Systems::ICentral* central);
    virtual void savePeers() {}

	virtual int32_t getChannelGroupedWith(int32_t channel) { return -1; }
	virtual int32_t getNewFirmwareVersion() { return 0; }
	virtual std::string getFirmwareVersionString(int32_t firmwareVersion) { return "1.0"; }
    virtual bool firmwareUpdateAvailable() { return false; }

    std::string printConfig();

    /**
	 * {@inheritDoc}
	 */
    virtual void homegearStarted();

    /**
	 * {@inheritDoc}
	 */
    virtual void homegearShuttingDown();

	//RPC methods
	virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing = false);
	PVariable setInterface(BaseLib::PRpcClientInfo clientInfo, std::string interfaceId);
	virtual PVariable setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait);
	//End RPC methods
protected:
	const uint16_t _bitMask[16] = { 0b0000000000000001, 0b0000000000000010, 0b0000000000000100, 0b0000000000001000, 0b0000000000010000, 0b0000000000100000, 0b0000000001000000, 0b0000000010000000, 0b0000000100000000, 0b0000001000000000, 0b0000010000000000, 0b0000100000000000, 0b0001000000000000, 0b0010000000000000, 0b0100000000000000, 0b1000000000000000 };
	const uint16_t _reversedBitMask[16] = { 0b1111111111111110, 0b1111111111111101, 0b1111111111111011, 0b1111111111110111, 0b1111111111101111, 0b1111111111011111, 0b1111111110111111, 0b1111111101111111, 0b1111111011111111, 0b1111110111111111, 0b1111101111111111, 0b1111011111111111, 0b1110111111111111, 0b1101111111111111, 0b1011111111111111, 0b0111111111111111 };

	//In table variables:
	std::vector<uint16_t> _states;
	std::string _physicalInterfaceId;
	//End

	bool _shuttingDown = false;
	std::shared_ptr<MainInterface> _physicalInterface;
	int32_t _bitSize = -1;
	int32_t _registerSize = -1;
	std::map<int32_t, int64_t> _lastData;
	std::map<int32_t, int32_t> _intervals;
	std::map<int32_t, int32_t> _decimalPlaces;
	std::map<int32_t, int32_t> _minimumInputValues;
	std::map<int32_t, int32_t> _maximumInputValues;
	std::map<int32_t, int32_t> _minimumOutputValues;
	std::map<int32_t, int32_t> _maximumOutputValues;

	std::shared_ptr<BaseLib::Rpc::RpcEncoder> _binaryEncoder;

	virtual void loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows);
    virtual void saveVariables();
    std::vector<char> serializeStates();
	void unserializeStates(std::vector<char>& data);

    virtual int32_t getStorageSize();

    virtual void setPhysicalInterface(std::shared_ptr<MainInterface> interface);

	virtual std::shared_ptr<BaseLib::Systems::ICentral> getCentral();

	virtual PParameterGroup getParameterSet(int32_t channel, ParameterGroup::Type::Enum type);

	// {{{ Hooks
		/**
		 * {@inheritDoc}
		 */
		virtual bool getAllValuesHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters);

		/**
		 * {@inheritDoc}
		 */
		virtual bool getParamsetHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters);
	// }}}
};

}

#endif
