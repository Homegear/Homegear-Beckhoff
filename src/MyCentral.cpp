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

#include "MyCentral.h"
#include "GD.h"

namespace MyFamily {

MyCentral::MyCentral(ICentralEventSink* eventHandler) : BaseLib::Systems::ICentral(MY_FAMILY_ID, GD::bl, eventHandler)
{
	init();
}

MyCentral::MyCentral(uint32_t deviceID, std::string serialNumber, ICentralEventSink* eventHandler) : BaseLib::Systems::ICentral(MY_FAMILY_ID, GD::bl, deviceID, serialNumber, -1, eventHandler)
{
	init();
}

MyCentral::~MyCentral()
{
	dispose();
}

void MyCentral::dispose(bool wait)
{
	try
	{
		if(_disposing) return;
		_disposing = true;
		GD::out.printDebug("Removing device " + std::to_string(_deviceId) + " from physical device's event queue...");
		for(std::map<std::string, std::shared_ptr<MainInterface>>::iterator i = GD::physicalInterfaces.begin(); i != GD::physicalInterfaces.end(); ++i)
		{
			//Just to make sure cycle through all physical devices. If event handler is not removed => segfault
			i->second->removeEventHandler(_physicalInterfaceEventhandlers[i->first]);
		}
	}
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void MyCentral::init()
{
	try
	{
		if(_initialized) return; //Prevent running init two times
		_initialized = true;

		for(std::map<std::string, std::shared_ptr<MainInterface>>::iterator i = GD::physicalInterfaces.begin(); i != GD::physicalInterfaces.end(); ++i)
		{
			_physicalInterfaceEventhandlers[i->first] = i->second->addEventHandler((BaseLib::Systems::IPhysicalInterface::IPhysicalInterfaceEventSink*)this);
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void MyCentral::loadPeers()
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows = _bl->db->getPeers(_deviceId);
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			int32_t peerID = row->second.at(0)->intValue;
			GD::out.printMessage("Loading Beckhoff BK90x0 peer " + std::to_string(peerID));
			std::shared_ptr<MyPeer> peer(new MyPeer(peerID, row->second.at(2)->intValue, row->second.at(3)->textValue, _deviceId, this));
			if(!peer->load(this)) continue;
			if(!peer->getRpcDevice()) continue;
			std::lock_guard<std::mutex> peersGuard(_peersMutex);
			if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
			_peersById[peerID] = peer;
			_peers[peer->getAddress()] = peer;
		}
		for(std::map<std::string, std::shared_ptr<MainInterface>>::iterator i = GD::physicalInterfaces.begin(); i != GD::physicalInterfaces.end(); ++i)
		{
			i->second->enableOutputs();
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::shared_ptr<MyPeer> MyCentral::getPeer(uint64_t id)
{
	try
	{
		std::lock_guard<std::mutex> peersGuard(_peersMutex);
		if(_peersById.find(id) != _peersById.end())
		{
			std::shared_ptr<MyPeer> peer(std::dynamic_pointer_cast<MyPeer>(_peersById.at(id)));
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<MyPeer>();
}

std::shared_ptr<MyPeer> MyCentral::getPeer(int32_t address)
{
	try
	{
		std::lock_guard<std::mutex> peersGuard(_peersMutex);
		if(_peers.find(address) != _peers.end())
		{
			std::shared_ptr<MyPeer> peer(std::dynamic_pointer_cast<MyPeer>(_peers.at(address)));
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<MyPeer>();
}

std::shared_ptr<MyPeer> MyCentral::getPeer(std::string serialNumber)
{
	try
	{
		std::lock_guard<std::mutex> peersGuard(_peersMutex);
		if(_peersBySerial.find(serialNumber) != _peersBySerial.end())
		{
			std::shared_ptr<MyPeer> peer(std::dynamic_pointer_cast<MyPeer>(_peersBySerial.at(serialNumber)));
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<MyPeer>();
}

bool MyCentral::onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet)
{
	try
	{
		if(_disposing) return false;
		std::shared_ptr<MyPacket> myPacket(std::dynamic_pointer_cast<MyPacket>(packet));
		if(!myPacket) return false;
		if(GD::bl->debugLevel >= 5) std::cout << BaseLib::HelperFunctions::getTimeString(myPacket->timeReceived()) << " New data received." << std::endl;

		myPacket->getData().push_back(0);

		std::vector<std::shared_ptr<MyPeer>> peers;
		{
			std::lock_guard<std::mutex> peersGuard(_peersMutex);
			peers.reserve(_peersById.size());
			for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
			{
				std::shared_ptr<MyPeer> peer(std::dynamic_pointer_cast<MyPeer>(i->second));
				if(peer->isOutputDevice() || senderID != peer->getPhysicalInterfaceId()) continue;
				peers.push_back(peer);
			}
		}

		uint32_t startBit = 0;
		uint32_t endBit = 0;
		int32_t offset = 0;
		uint32_t currentSourceByte = 0;
		uint32_t currentSourceBit = 0;
		uint32_t currentDestinationByte = 0;
		uint32_t currentDestinationBit = 0;
		std::vector<uint16_t>& sourceData = myPacket->getData();
		std::vector<uint16_t> destinationData;
		for(std::vector<std::shared_ptr<MyPeer>>::iterator i = peers.begin(); i != peers.end(); ++i)
		{
			startBit = (*i)->getAddress() + ((*i)->isAnalog() ? 0 : (*i)->getPhysicalInterface()->digitalInputOffset());
			endBit = startBit + (*i)->getBitSize() - 1;
			offset = startBit % 16;
			currentSourceByte = startBit / 16;
			currentSourceBit = startBit % 16;
			currentDestinationByte = 0;
			currentDestinationBit = 0;

			if(currentSourceByte >= sourceData.size()) continue;

			destinationData = std::vector<uint16_t>((*i)->getRegisterSize(), 0);

			for(uint32_t j = startBit; j <= endBit; j++)
			{
				if(offset >= 0) destinationData[currentDestinationByte] |= (sourceData[currentSourceByte] & _bitMask[currentSourceBit]) >> offset;
				else destinationData[currentDestinationByte] |= (sourceData[currentSourceByte] & _bitMask[currentSourceBit]) << (offset * -1);
				currentSourceBit++;
				currentDestinationBit++;
				if(currentDestinationBit == 16)
				{
					currentDestinationBit = 0;
					currentDestinationByte++;
					offset = currentSourceBit;
				}
				if(currentSourceBit == 16)
				{
					currentSourceBit = 0;
					offset = -currentDestinationBit;
					currentSourceByte++;
					if(currentSourceByte >= sourceData.size()) break;
				}
			}

			(*i)->packetReceived(destinationData);
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return false;
}

void MyCentral::savePeers(bool full)
{
	try
	{
		std::lock_guard<std::mutex> peersGuard(_peersMutex);
		for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
		{
			GD::out.printInfo("Info: Saving Beckhoff BK90x0 peer " + std::to_string(i->second->getID()));
			i->second->save(full, full, full);
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void MyCentral::deletePeer(uint64_t id)
{
	try
	{
		std::shared_ptr<MyPeer> peer(getPeer(id));
		if(!peer) return;
		peer->deleting = true;
		PVariable deviceAddresses(new Variable(VariableType::tArray));
		deviceAddresses->arrayValue->push_back(PVariable(new Variable(peer->getSerialNumber())));

		PVariable deviceInfo(new Variable(VariableType::tStruct));
		deviceInfo->structValue->insert(StructElement("ID", PVariable(new Variable((int32_t)peer->getID()))));
		PVariable channels(new Variable(VariableType::tArray));
		deviceInfo->structValue->insert(StructElement("CHANNELS", channels));

		for(Functions::iterator i = peer->getRpcDevice()->functions.begin(); i != peer->getRpcDevice()->functions.end(); ++i)
		{
			deviceAddresses->arrayValue->push_back(PVariable(new Variable(peer->getSerialNumber() + ":" + std::to_string(i->first))));
			channels->arrayValue->push_back(PVariable(new Variable(i->first)));
		}

		raiseRPCDeleteDevices(deviceAddresses, deviceInfo);
		peer->deleteFromDatabase();
		_peersMutex.lock();
		if(_peersBySerial.find(peer->getSerialNumber()) != _peersBySerial.end()) _peersBySerial.erase(peer->getSerialNumber());
		if(_peersById.find(id) != _peersById.end()) _peersById.erase(id);
		std::unordered_map<int32_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator peerIterator = _peers.find(peer->getAddress());
		if(peerIterator != _peers.end() && peerIterator->second->getID() == id) _peers.erase(peerIterator);
		_peersMutex.unlock();
		GD::out.printMessage("Removed Beckhoff BK90x0 peer " + std::to_string(peer->getID()));
	}
	catch(const std::exception& ex)
    {
		_peersMutex.unlock();
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_peersMutex.unlock();
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_peersMutex.unlock();
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::string MyCentral::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;
		if(_currentPeer)
		{
			if(command == "unselect" || command == "u")
			{
				_currentPeer.reset();
				return "Peer unselected.\n";
			}
			return _currentPeer->handleCliCommand(command);
		}
		if(command == "help" || command == "h")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "peers create (pc)\t\tCreates a new peer" << std::endl;
			stringStream << "peers list (ls)\t\tList all peers" << std::endl;
			stringStream << "peers remove (pr)\tRemove a peer" << std::endl;
			stringStream << "peers select (ps)\tSelect a peer" << std::endl;
			stringStream << "peers setname (pn)\tName a peer" << std::endl;
			stringStream << "send\t\tSends a raw packet" << std::endl;
			stringStream << "unselect (u)\t\tUnselect this device" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 12, "peers create") == 0 || command.compare(0, 2, "pc") == 0)
		{
			std::string interfaceId;
			int32_t deviceType = 0;
			int32_t address = 0;
			std::string serial;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'c') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					interfaceId = element;
				}
				else if(index == 2 + offset)
				{
					int32_t temp = BaseLib::Math::getNumber(element, true);
					if(temp == 0) return "Invalid device type. Device type has to be provided in hexadecimal format.\n";
					deviceType = temp;
				}
				else if(index == 3 + offset)
				{
					address = BaseLib::Math::getNumber(element, true);
				}
				else if(index == 4 + offset)
				{
					if(element.length() < 10) return "Invalid serial number. Please provide a serial number with at least 10 characters.\n";
					else if(element.length() > 12) return "Invalid serial number. Please provide a serial number with a maximum of 12 characters.\n";
					serial = element;
				}
				index++;
			}
			if(index < 5 + offset)
			{
				stringStream << "Description: This command creates a new peer." << std::endl;
				stringStream << "Usage: peers create INTERFACE TYPE ADDRESS SERIAL" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  INTERFACE: The id of the interface to associate the new device to as defined in the familie's configuration file." << std::endl;
				stringStream << "  TYPE:      The 2 byte hexadecimal device type. Example: 0x4001" << std::endl;
				stringStream << "  ADDRESS:   The bit position of the device. Example: 0x80" << std::endl;
				stringStream << "  SERIAL:    The 10 to 12 character long serial number of the peer to add. Example: VBF01020304" << std::endl;
				return stringStream.str();
			}
			if(peerExists(serial)) stringStream << "A peer with this serial number is already paired to this central." << std::endl;
			else
			{
				std::shared_ptr<MyPeer> peer = createPeer(deviceType, address, serial, false);
				if(!peer || !peer->getRpcDevice()) return "Device type not supported.\n";
				try
				{
					_peersMutex.lock();
					if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
					_peersMutex.unlock();
					peer->save(true, true, false);
					peer->initializeCentralConfig();
					peer->setPhysicalInterfaceId(interfaceId);
					_peersMutex.lock();
					_peers[peer->getAddress()] = peer;
					_peersById[peer->getID()] = peer;
					_peersMutex.unlock();
					peer->setAddress(address); //Set address again, because otherwise IP_ADDRESS cannot be saved.
				}
				catch(const std::exception& ex)
				{
					_peersMutex.unlock();
					GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
				}
				catch(BaseLib::Exception& ex)
				{
					_peersMutex.unlock();
					GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
				}
				catch(...)
				{
					_peersMutex.unlock();
					GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
				}

				PVariable deviceDescriptions(new Variable(VariableType::tArray));
				deviceDescriptions->arrayValue = peer->getDeviceDescriptions(nullptr, true, std::map<std::string, bool>());
				raiseRPCNewDevices(deviceDescriptions);
				GD::out.printMessage("Added peer 0x" + BaseLib::HelperFunctions::getHexString(peer->getID()) + ".");
				stringStream << "Added peer " << std::to_string(peer->getID()) << " with IP address 0x" << BaseLib::HelperFunctions::getHexString(address, 8) << " and serial number " << serial << "." << std::dec << std::endl;
			}
			return stringStream.str();
		}
		else if(command.compare(0, 12, "peers remove") == 0 || command.compare(0, 2, "pr") == 0)
		{
			uint64_t peerID = 0;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'r') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					peerID = BaseLib::Math::getNumber(element, false);
					if(peerID == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command removes a peer." << std::endl;
				stringStream << "Usage: peers unpair PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to remove. Example: 513" << std::endl;
				return stringStream.str();
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				if(_currentPeer && _currentPeer->getID() == peerID) _currentPeer.reset();
				stringStream << "Removing peer " << std::to_string(peerID) << std::endl;
				deletePeer(peerID);
			}
			return stringStream.str();
		}
		else if(command.compare(0, 10, "peers list") == 0 || command.compare(0, 2, "pl") == 0 || command.compare(0, 2, "ls") == 0)
		{
			try
			{
				std::string filterType;
				std::string filterValue;

				std::stringstream stream(command);
				std::string element;
				int32_t offset = (command.at(1) == 'l' || command.at(1) == 's') ? 0 : 1;
				int32_t index = 0;
				while(std::getline(stream, element, ' '))
				{
					if(index < 1 + offset)
					{
						index++;
						continue;
					}
					else if(index == 1 + offset)
					{
						if(element == "help")
						{
							index = -1;
							break;
						}
						filterType = BaseLib::HelperFunctions::toLower(element);
					}
					else if(index == 2 + offset)
					{
						filterValue = element;
						if(filterType == "name") BaseLib::HelperFunctions::toLower(filterValue);
					}
					index++;
				}
				if(index == -1)
				{
					stringStream << "Description: This command lists information about all peers." << std::endl;
					stringStream << "Usage: peers list [FILTERTYPE] [FILTERVALUE]" << std::endl << std::endl;
					stringStream << "Parameters:" << std::endl;
					stringStream << "  FILTERTYPE:  See filter types below." << std::endl;
					stringStream << "  FILTERVALUE: Depends on the filter type. If a number is required, it has to be in hexadecimal format." << std::endl << std::endl;
					stringStream << "Filter types:" << std::endl;
					stringStream << "  ID: Filter by id." << std::endl;
					stringStream << "      FILTERVALUE: The id of the peer to filter (e. g. 513)." << std::endl;
					stringStream << "  SERIAL: Filter by serial number." << std::endl;
					stringStream << "      FILTERVALUE: The serial number of the peer to filter (e. g. JEQ0554309)." << std::endl;
					stringStream << "  ADDRESS: Filter by saddress." << std::endl;
					stringStream << "      FILTERVALUE: The address of the peer to filter (e. g. 128)." << std::endl;
					stringStream << "  NAME: Filter by name." << std::endl;
					stringStream << "      FILTERVALUE: The part of the name to search for (e. g. \"1st floor\")." << std::endl;
					stringStream << "  TYPE: Filter by device type." << std::endl;
					stringStream << "      FILTERVALUE: The 2 byte device type in hexadecimal format." << std::endl;
					return stringStream.str();
				}

				if(_peersById.empty())
				{
					stringStream << "No peers are paired to this central." << std::endl;
					return stringStream.str();
				}
				std::string bar(" │ ");
				const int32_t idWidth = 8;
				const int32_t nameWidth = 25;
				const int32_t serialWidth = 13;
				const int32_t addressWidth = 7;
				const int32_t typeWidth1 = 4;
				const int32_t typeWidth2 = 25;
				std::string nameHeader("Name");
				nameHeader.resize(nameWidth, ' ');
				std::string typeStringHeader("Type String");
				typeStringHeader.resize(typeWidth2, ' ');
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << "ID" << bar
					<< nameHeader << bar
					<< std::setw(serialWidth) << "Serial Number" << bar
					<< std::setw(addressWidth) << "Address" << bar
					<< std::setw(typeWidth1) << "Type" << bar
					<< typeStringHeader
					<< std::endl;
				stringStream << "─────────┼───────────────────────────┼───────────────┼─────────┼──────┼───────────────────────────" << std::endl;
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << " " << bar
					<< std::setw(nameWidth) << " " << bar
					<< std::setw(serialWidth) << " " << bar
					<< std::setw(addressWidth) << " " << bar
					<< std::setw(typeWidth1) << " " << bar
					<< std::setw(typeWidth2)
					<< std::endl;
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
				{
					if(filterType == "id")
					{
						uint64_t id = BaseLib::Math::getNumber(filterValue, false);
						if(i->second->getID() != id) continue;
					}
					else if(filterType == "name")
					{
						std::string name = i->second->getName();
						if((signed)BaseLib::HelperFunctions::toLower(name).find(filterValue) == (signed)std::string::npos) continue;
					}
					else if(filterType == "serial")
					{
						if(i->second->getSerialNumber() != filterValue) continue;
					}
					else if(filterType == "address")
					{
						int32_t address = BaseLib::Math::getNumber(filterValue, true);
						if(i->second->getAddress() != address) continue;
					}
					else if(filterType == "type")
					{
						int32_t deviceType = BaseLib::Math::getNumber(filterValue, true);
						if((int32_t)i->second->getDeviceType() != deviceType) continue;
					}

					stringStream << std::setw(idWidth) << std::setfill(' ') << std::to_string(i->second->getID()) << bar;
					std::string name = i->second->getName();
					size_t nameSize = BaseLib::HelperFunctions::utf8StringSize(name);
					if(nameSize > (unsigned)nameWidth)
					{
						name = BaseLib::HelperFunctions::utf8Substring(name, 0, nameWidth - 3);
						name += "...";
					}
					else name.resize(nameWidth + (name.size() - nameSize), ' ');
					stringStream << name << bar
						<< std::setw(serialWidth) << i->second->getSerialNumber() << bar
						<< std::setw(addressWidth) << i->second->getAddress() << bar
						<< std::setw(typeWidth1) << BaseLib::HelperFunctions::getHexString(i->second->getDeviceType(), 4) << bar;
					if(i->second->getRpcDevice())
					{
						PSupportedDevice type = i->second->getRpcDevice()->getType(i->second->getDeviceType(), i->second->getFirmwareVersion());
						std::string typeID;
						if(type) typeID = type->id;
						if(typeID.size() > (unsigned)typeWidth2)
						{
							typeID.resize(typeWidth2 - 3);
							typeID += "...";
						}
						stringStream << std::setw(typeWidth2) << typeID;
					}
					else stringStream << std::setw(typeWidth2);
					stringStream << std::endl << std::dec;
				}
				_peersMutex.unlock();
				stringStream << "─────────┴───────────────────────────┴───────────────┴─────────┴──────┴───────────────────────────" << std::endl;

				return stringStream.str();
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
		}
		else if(command.compare(0, 13, "peers setname") == 0 || command.compare(0, 2, "pn") == 0)
		{
			uint64_t peerID = 0;
			std::string name;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'n') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					else
					{
						peerID = BaseLib::Math::getNumber(element, false);
						if(peerID == 0) return "Invalid id.\n";
					}
				}
				else if(index == 2 + offset) name = element;
				else name += ' ' + element;
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command sets or changes the name of a peer to identify it more easily." << std::endl;
				stringStream << "Usage: peers setname PEERID NAME" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to set the name for. Example: 513" << std::endl;
				stringStream << "  NAME:\tThe name to set. Example: \"1st floor light switch\"." << std::endl;
				return stringStream.str();
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				std::shared_ptr<MyPeer> peer = getPeer(peerID);
				peer->setName(name);
				stringStream << "Name set to \"" << name << "\"." << std::endl;
			}
			return stringStream.str();
		}
		else if(command.compare(0, 12, "peers select") == 0 || command.compare(0, 2, "ps") == 0)
		{
			uint64_t id = 0;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 's') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					id = BaseLib::Math::getNumber(element, false);
					if(id == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command selects a peer." << std::endl;
				stringStream << "Usage: peers select PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to select. Example: 513" << std::endl;
				return stringStream.str();
			}

			_currentPeer = getPeer(id);
			if(!_currentPeer) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				stringStream << "Peer with id " << std::hex << std::to_string(id) << " and device type 0x" << _bl->hf.getHexString(_currentPeer->getDeviceType()) << " selected." << std::dec << std::endl;
				stringStream << "For information about the peer's commands type: \"help\"" << std::endl;
			}
			return stringStream.str();
		}
		else if(command.compare(0, 4, "send") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			std::vector<uint8_t> bytes;
			std::vector<uint16_t> data;
			uint16_t startBit = 0;
			uint16_t endBit = 0;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1)
				{
					index++;
					continue;
				}
				else if(index == 1)
				{
					if(element == "help")
					{
						stringStream << "Description: This command sends a raw packet." << std::endl;
						stringStream << "Usage: send STARTBIT ENDBIT DATA" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  STARTBIT:\tThe start bit of the data. Example: 10" << std::endl;
						stringStream << "  ENDBIT:\tThe end bit of the data. Example: 13" << std::endl;
						stringStream << "  DATA:\tThe register aligned data. Example: 03C0" << std::endl;
						return stringStream.str();
					}
					else startBit = BaseLib::Math::getNumber(element);
				}
				else if(index == 2) endBit = BaseLib::Math::getNumber(element);
				else if(index == 3)
				{
					bytes = _bl->hf.getUBinary(element);
					data.resize(bytes.size() / 2 + (bytes.size() % 2 ? 1 : 0));
					for(int32_t i = 0; i < (signed)bytes.size(); i++)
					{
						data.at(i / 2) += bytes.at(i) << (i % 2 ? 0 : 8);
					}
				}
				index++;
			}

			stringStream << "Sending packet " << BaseLib::HelperFunctions::getHexString(bytes) << std::endl;

			std::shared_ptr<MyPacket> packet(new MyPacket(startBit, endBit, data));
			GD::defaultPhysicalInterface->sendPacket(packet);

			return stringStream.str();
		}
		else return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return "Error executing command. See log file for more details.\n";
}

std::shared_ptr<MyPeer> MyCentral::createPeer(uint32_t type, int32_t address, std::string serialNumber, bool save)
{
	try
	{
		std::shared_ptr<MyPeer> peer(new MyPeer(_deviceId, this));
		peer->setDeviceType(type);
		peer->setAddress(address);
		peer->setSerialNumber(serialNumber);
		peer->setRpcDevice(GD::family->getRpcDevices()->find(type, 0x10, -1));
		if(!peer->getRpcDevice()) return std::shared_ptr<MyPeer>();
		if(save) peer->save(true, true, false); //Save and create peerID
		return peer;
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<MyPeer>();
}

void MyCentral::updatePeerAddress(uint64_t peerId, int32_t oldAddress, int32_t newAddress)
{
	try
	{
		std::shared_ptr<MyPeer> peer = getPeer(peerId);
		if(!peer) return;
		std::lock_guard<std::mutex> peersGuard(_peersMutex);
		_peers.erase(oldAddress);
		peer->setAddress(newAddress);
		_peers[newAddress] = peer;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

PVariable MyCentral::createDevice(BaseLib::PRpcClientInfo clientInfo, int32_t deviceType, std::string serialNumber, int32_t address, int32_t firmwareVersion, std::string interfaceId)
{
	try
	{
		if(serialNumber.size() < 10 || serialNumber.size() > 12) return Variable::createError(-1, "The serial number needs to have a size between 10 and 12.");
		if(peerExists(serialNumber)) return Variable::createError(-5, "This peer is already paired to this central.");

		std::shared_ptr<MyPeer> peer = createPeer(deviceType, address, serialNumber, false);
		if(!peer || !peer->getRpcDevice()) return Variable::createError(-6, "Unknown device type.");

		try
		{
			peer->save(true, true, false);
			peer->initializeCentralConfig();
			peer->setAddress(address); //Needs to be set again, so it is saved to CONFIG_PARAMETER IP_ADDRESS
			peer->setPhysicalInterfaceId(interfaceId);
			_peersMutex.lock();
			_peers[peer->getAddress()] = peer;
			_peersById[peer->getID()] = peer;
			_peersBySerial[peer->getSerialNumber()] = peer;
			_peersMutex.unlock();
		}
		catch(const std::exception& ex)
		{
			_peersMutex.unlock();
			GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(BaseLib::Exception& ex)
		{
			_peersMutex.unlock();
			GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		catch(...)
		{
			_peersMutex.unlock();
			GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		}

		PVariable deviceDescriptions(new Variable(VariableType::tArray));
		deviceDescriptions->arrayValue = peer->getDeviceDescriptions(clientInfo, true, std::map<std::string, bool>());
		raiseRPCNewDevices(deviceDescriptions);
		GD::out.printMessage("Added peer 0x" + BaseLib::HelperFunctions::getHexString(peer->getID()) + ".");

		return PVariable(new Variable((uint32_t)peer->getID()));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable MyCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags)
{
	try
	{
		if(serialNumber.empty()) return Variable::createError(-2, "Unknown device.");
		std::shared_ptr<MyPeer> peer = getPeer(serialNumber);
		if(!peer) return PVariable(new Variable(VariableType::tVoid));

		return deleteDevice(clientInfo, peer->getID(), flags);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable MyCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t flags)
{
	try
	{
		if(peerID == 0) return Variable::createError(-2, "Unknown device.");
		std::shared_ptr<MyPeer> peer = getPeer(peerID);
		if(!peer) return PVariable(new Variable(VariableType::tVoid));
		uint64_t id = peer->getID();

		deletePeer(id);

		if(peerExists(id)) return Variable::createError(-1, "Error deleting peer. See log for more details.");

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable MyCentral::getDeviceInfo(BaseLib::PRpcClientInfo clientInfo, uint64_t id, std::map<std::string, bool> fields)
{
	try
	{
		if(id > 0)
		{
			std::shared_ptr<MyPeer> peer(getPeer(id));
			if(!peer) return Variable::createError(-2, "Unknown device.");

			return peer->getDeviceInfo(clientInfo, fields);
		}
		else
		{
			PVariable array(new Variable(VariableType::tArray));

			std::vector<std::shared_ptr<MyPeer>> peers;
			//Copy all peers first, because listDevices takes very long and we don't want to lock _peersMutex too long
			_peersMutex.lock();
			for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
			{
				peers.push_back(std::dynamic_pointer_cast<MyPeer>(i->second));
			}
			_peersMutex.unlock();

			for(std::vector<std::shared_ptr<MyPeer>>::iterator i = peers.begin(); i != peers.end(); ++i)
			{
				//listDevices really needs a lot of resources, so wait a little bit after each device
				std::this_thread::sleep_for(std::chrono::milliseconds(3));
				PVariable info = (*i)->getDeviceInfo(clientInfo, fields);
				if(!info) continue;
				array->arrayValue->push_back(info);
			}

			return array;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable MyCentral::putParamset(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, ParameterGroup::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel, PVariable paramset)
{
	try
	{
		std::shared_ptr<MyPeer> peer(getPeer(serialNumber));
		uint64_t remoteID = 0;
		if(!remoteSerialNumber.empty())
		{
			std::shared_ptr<MyPeer> remotePeer(getPeer(remoteSerialNumber));
			if(!remotePeer) return Variable::createError(-3, "Remote peer is unknown.");
			remoteID = remotePeer->getID();
		}
		if(peer) return peer->putParamset(clientInfo, channel, type, remoteID, remoteChannel, paramset);
		return Variable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable MyCentral::putParamset(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable paramset)
{
	try
	{
		std::shared_ptr<MyPeer> peer(getPeer(peerID));
		if(peer) return peer->putParamset(clientInfo, channel, type, remoteID, remoteChannel, paramset);
		return Variable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable MyCentral::setInterface(BaseLib::PRpcClientInfo clientInfo, uint64_t peerId, std::string interfaceId)
{
	try
	{
		std::shared_ptr<MyPeer> peer(getPeer(peerId));
		if(!peer) return Variable::createError(-2, "Unknown device.");
		return peer->setInterface(clientInfo, interfaceId);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

}
