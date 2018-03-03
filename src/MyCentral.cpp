/* Copyright 2013-2017 Homegear UG (haftungsbeschränkt) */

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
		}
		updatePeerAddresses(true);
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
				if(peer->isOutputDevice() || senderID != peer->getPhysicalInterface()->getID()) continue;
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

        std::vector<uint64_t> deletedIds{ id };
		raiseRPCDeleteDevices(deletedIds, deviceAddresses, deviceInfo);

        {
            std::lock_guard<std::mutex> peersGuard(_peersMutex);
            if(_peersBySerial.find(peer->getSerialNumber()) != _peersBySerial.end()) _peersBySerial.erase(peer->getSerialNumber());
            if(_peersById.find(id) != _peersById.end()) _peersById.erase(id);
        }

        if(_currentPeer && _currentPeer->getID() == id) _currentPeer.reset();

        int32_t i = 0;
        while(peer.use_count() > 1 && i < 600)
        {
            if(_currentPeer && _currentPeer->getID() == id) _currentPeer.reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            i++;
        }
        if(i == 600) GD::out.printError("Error: Peer deletion took too long.");

		peer->deleteFromDatabase();
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
		std::vector<std::string> arguments;
		bool showHelp = false;
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
			stringStream << "peers create (pc)   Creates a new peer" << std::endl;
			stringStream << "peers list (ls)     List all peers" << std::endl;
			stringStream << "peers remove (pr)   Remove a peer" << std::endl;
			stringStream << "peers select (ps)   Select a peer" << std::endl;
			stringStream << "peers setname (pn)  Name a peer" << std::endl;
			stringStream << "peers setnext (px)  Assigns the ID of the peer physically following in the installation to a peer" << std::endl;
			stringStream << "send                Sends a raw packet" << std::endl;
			stringStream << "unselect (u)        Unselect this device" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 12, "peers create") == 0 || command.compare(0, 2, "pc") == 0)
		{
			std::string interfaceId;
			int32_t deviceType = 0;
			uint64_t nextPeerId = 0;
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
					nextPeerId = BaseLib::Math::getNumber64(element);
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
				stringStream << "Usage: peers create INTERFACE TYPE NEXT_PEER_ID SERIAL" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  INTERFACE:    The id of the interface to associate the new device to as defined in the familie's configuration file." << std::endl;
				stringStream << "  TYPE:         The 2 byte hexadecimal device type. Example: 0x4001" << std::endl;
				stringStream << "  NEXT_PEER_ID: The ID of the peer physically following in the installation (if known). If currently unknown or this is the last card, set to \"0\". Example: 14" << std::endl;
				stringStream << "  SERIAL:       The 10 to 12 character long serial number of the peer to add. Example: VBF01020304" << std::endl;
				return stringStream.str();
			}

            if(GD::physicalInterfaces.find(interfaceId) == GD::physicalInterfaces.end()) return "Unknown physical interface.\n";

			if(peerExists(serial)) stringStream << "A peer with this serial number is already paired to this central." << std::endl;
			else
			{
				std::shared_ptr<MyPeer> peer = createPeer(deviceType, nextPeerId, serial, false);
				if(!peer || !peer->getRpcDevice()) return "Device type not supported.\n";
				try
				{
					std::unique_lock<std::mutex> peersGuard(_peersMutex);
					if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
                    peersGuard.unlock();
					peer->save(true, true, false);
					peer->initializeCentralConfig();
					peer->setPhysicalInterfaceId(interfaceId);
                    peersGuard.lock();
					_peersById[peer->getID()] = peer;
                    peersGuard.unlock();
					peer->setNextPeerId(nextPeerId); //Set next peer ID again, because otherwise it cannot be saved.
					updatePeerAddresses();
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

				PVariable deviceDescriptions(new Variable(VariableType::tArray));
				deviceDescriptions->arrayValue = peer->getDeviceDescriptions(nullptr, true, std::map<std::string, bool>());
                std::vector<uint64_t> newIds{ peer->getID() };
				raiseRPCNewDevices(newIds, deviceDescriptions);
				GD::out.printMessage("Added peer with ID " + std::to_string(peer->getID()) + ".");
				stringStream << "Added peer " << std::to_string(peer->getID()) << " with ID " << std::to_string(peer->getID()) << " and serial number " << serial << "." << std::dec << std::endl;
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
					stringStream << "  ADDRESS: Filter by address." << std::endl;
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
				const int32_t nextIdWidth = 8;
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
					<< std::setw(nextIdWidth) << "Next ID" << bar
					<< std::setw(typeWidth1) << "Type" << bar
					<< typeStringHeader
					<< std::endl;
				stringStream << "─────────┼───────────────────────────┼───────────────┼─────────┼──────────┼──────┼───────────────────────────" << std::endl;
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << " " << bar
					<< std::setw(nameWidth) << " " << bar
					<< std::setw(serialWidth) << " " << bar
					<< std::setw(addressWidth) << " " << bar
					<< std::setw(nextIdWidth) << " " << bar
					<< std::setw(typeWidth1) << " " << bar
					<< std::setw(typeWidth2)
					<< std::endl;
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
				{
					PMyPeer myPeer = std::dynamic_pointer_cast<MyPeer>(i->second);
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
						<< std::setw(nextIdWidth) << myPeer->getNextPeerId() << bar
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
						else typeID.resize(typeWidth2, ' ');
						stringStream << typeID << bar;
					}
					else stringStream << std::setw(typeWidth2);
					stringStream << std::endl << std::dec;
				}
				_peersMutex.unlock();
				stringStream << "─────────┴───────────────────────────┴───────────────┴─────────┴──────────┴──────┴───────────────────────────" << std::endl;

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
		else if(BaseLib::HelperFunctions::checkCliCommand(command, "peers setnext", "px", "", 2, arguments, showHelp))
		{
			if(showHelp)
			{
				stringStream << "Description: This command removes a peer." << std::endl;
				stringStream << "Usage: peers setnext PEER_ID NEXT_PEER_ID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEER_ID: The id of the peer to set the next peer for. Example: 25" << std::endl;
				stringStream << "  NEXT_PEER_ID: The id of the peer following in the physical installation. Example: 13" << std::endl;
				return stringStream.str();
			}

			uint64_t peerId = BaseLib::Math::getNumber64(arguments.at(0), false);
			if(peerId == 0) return "Invalid id.\n";

			uint64_t nextPeerId = BaseLib::Math::getNumber64(arguments.at(1), false);

			if(!peerExists(peerId) || (nextPeerId != 0 && !peerExists(nextPeerId))) stringStream << "At least one of the peers is not paired to this central." << std::endl;
			else
			{
				PMyPeer peer = getPeer(peerId);
				PMyPeer nextPeer;
				if(nextPeerId != 0) nextPeer = getPeer(nextPeerId);
				peer->setNextPeerId(nextPeerId);
				updatePeerAddresses();
				stringStream << "NEXT_PEER_ID of peer " << peerId << " was set to " << nextPeerId << ".";
				if(nextPeerId != 0) stringStream << " Address (= bit position) of peer " << nextPeerId << " now is " << nextPeer->getAddress() << ".";
				stringStream << std::endl;
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
		peer->setNextPeerId(address);
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

void MyCentral::updatePeerAddresses(bool booting)
{
	try
	{
		struct Pots
		{
			uint64_t firstPeer = 0;
			uint32_t analogInputBits = 0;
			uint32_t analogOutputBits = 0;
			uint32_t digitalInputBits = 0;
			uint32_t digitalOutputBits = 0;
			std::list<PMyPeer> analogInputs;
			std::list<PMyPeer> digitalInputs;
			std::list<PMyPeer> analogOutputs;
			std::list<PMyPeer> digitalOutputs;
		};
		typedef std::shared_ptr<Pots> PPots;

		std::unordered_map<std::string, uint64_t> firstPeers;
		std::unordered_map<std::string, std::set<uint64_t>> interfacePeers;
		std::unordered_map<std::string, uint32_t> peersWithoutAddress;

		{
			std::lock_guard<std::mutex> peersGuard(_peersMutex);

			for(auto& peer : _peersById)
			{
				PMyPeer myPeer = std::dynamic_pointer_cast<MyPeer>(peer.second);
				if(!myPeer) continue;
				if(myPeer->getNextPeerId() == myPeer->getID())
				{
					GD::out.printCritical("Critical: Peer " + std::to_string(myPeer->getID()) + " points to itself. Please set NEXT_PEER_ID to a valid value.");
					continue;
				}
                auto firstPeersIterator = firstPeers.find(myPeer->getPhysicalInterface()->getID());
                if(firstPeersIterator == firstPeers.end()) firstPeersIterator = firstPeers.emplace(myPeer->getPhysicalInterface()->getID(), 0).first;
				if(myPeer->getNextPeerId() > 0 && myPeer->getNextPeerId() == firstPeersIterator->second)
				{
					if(interfacePeers[myPeer->getPhysicalInterface()->getID()].find(myPeer->getID()) == interfacePeers[myPeer->getPhysicalInterface()->getID()].end())
					{
						firstPeers[myPeer->getPhysicalInterface()->getID()] = myPeer->getID();
					}
					else
					{
						firstPeers[myPeer->getPhysicalInterface()->getID()] = 0;
					}
				}
				else if(firstPeersIterator->second == 0) firstPeers[myPeer->getPhysicalInterface()->getID()] = myPeer->getID();
				if(myPeer->getNextPeerId() > 0) interfacePeers[myPeer->getPhysicalInterface()->getID()].emplace(myPeer->getNextPeerId());
				else
				{
					auto peersWithoutAddressIterator = peersWithoutAddress.find(myPeer->getPhysicalInterface()->getID());
					if(peersWithoutAddressIterator == peersWithoutAddress.end()) peersWithoutAddress.emplace(myPeer->getPhysicalInterface()->getID(), 1);
					else peersWithoutAddressIterator->second++;
				}
			}

			for(auto& element : firstPeers) //Find first peer, if not found yet
			{
				if(element.second == 0)
				{
					for(auto& peer : _peersById)
					{
						if(interfacePeers[element.first].find(peer.first) == interfacePeers[element.first].end())
						{
							element.second = peer.first;
							break;
						}
					}
				}
			}
		}

		for(auto& element : firstPeers)
		{
			if(element.second == 0)
			{
				if(interfacePeers[element.first].size() > 1) GD::out.printCritical("Critical: Address loop detected on interface " + element.first + ". Please check NEXT_PEER_ID of all peers.");
				continue;
			}

			PPots currentPots = std::make_shared<Pots>();
			currentPots->firstPeer = element.second;

			PMyPeer peer = getPeer(element.second);
			if(!peer)
			{
				GD::out.printCritical("Critical: Can't find first peer and can't generate addresses for interface " + element.first + ". Please recheck NEXT_PEER_ID of all peers.");
				continue;
			}
			if(peer->isAnalog())
			{
				if(peer->isOutputDevice()) currentPots->analogOutputs.push_back(peer);
				else currentPots->analogInputs.push_back(peer);
			}
			else
			{
				if(peer->isOutputDevice()) currentPots->digitalOutputs.push_back(peer);
				else currentPots->digitalInputs.push_back(peer);
			}
			currentPots->analogInputBits = peer->getPhysicalInterface()->analogInputBits();
			currentPots->analogOutputBits = peer->getPhysicalInterface()->analogOutputBits();
			currentPots->digitalInputBits = peer->getPhysicalInterface()->digitalInputBits();
			currentPots->digitalOutputBits = peer->getPhysicalInterface()->digitalOutputBits();

			uint32_t usedAnalogInputBits = 0;
			uint32_t usedAnalogOutputBits = 0;
			uint32_t usedDigitalInputBits = 0;
			uint32_t usedDigitalOutputBits = 0;

			int32_t maxIterations = interfacePeers[element.first].size() + 1;
			for(int32_t i = 0; i < maxIterations; i++) //Limit number of iterations
			{
				if(peer->getNextPeerId() == 0) break;

				PMyPeer nextPeer = getPeer(peer->getNextPeerId());
				if(!nextPeer)
				{
					GD::out.printCritical("Critical: Peer " + std::to_string(peer->getID()) + " points to peer " + std::to_string(peer->getNextPeerId()) + ", which doesn't exist or isn't a Beckhoff peer.");
					continue;
				}
				if(nextPeer->getPhysicalInterface()->getID() != peer->getPhysicalInterface()->getID())
				{
					GD::out.printCritical("Critical: Peer " + std::to_string(peer->getID()) + " points to peer " + std::to_string(nextPeer->getID()) + ", but the two peers are connected to different interfaces.");
					continue;
				}

				if(nextPeer->isAnalog())
				{
					if(nextPeer->isOutputDevice()) currentPots->analogOutputs.push_back(nextPeer);
					else currentPots->analogInputs.push_back(nextPeer);
				}
				else
				{
					if(nextPeer->isOutputDevice()) currentPots->digitalOutputs.push_back(nextPeer);
					else currentPots->digitalInputs.push_back(nextPeer);
				}

				peer = nextPeer;
			}

			uint32_t currentAddress = 0;
			for(auto& peer : currentPots->analogInputs)
			{
				if(currentAddress + peer->getMemorySize() > currentPots->analogInputBits && !booting)
				{
					GD::out.printError("Error: The calculated address of peer " + std::to_string(peer->getID()) + " exceeds number of analog input bits returned by interface " + element.first + ". Recheck that the cards configured in Homegear mirror the actually installed devices.");
				}
				peer->setAddress(currentAddress);
				currentAddress += peer->getMemorySize();
				usedAnalogInputBits += peer->getMemorySize();
			}

			for(auto& peer : currentPots->digitalInputs)
			{
				if(currentAddress + peer->getMemorySize() > currentPots->analogInputBits + currentPots->digitalInputBits && !booting)
				{
					GD::out.printError("Error: The calculated address of peer " + std::to_string(peer->getID()) + " exceeds number of digital input bits returned by interface " + element.first + ". Recheck that the cards configured in Homegear mirror the actually installed devices.");
				}
				peer->setAddress(currentAddress);
				currentAddress += peer->getMemorySize();
				usedDigitalInputBits += peer->getMemorySize();
			}

			currentAddress = 0;
			for(auto& peer : currentPots->analogOutputs)
			{
				if(currentAddress + peer->getMemorySize() > currentPots->analogOutputBits && !booting)
				{
					GD::out.printError("Error: The calculated address of peer " + std::to_string(peer->getID()) + " exceeds number of analog output bits returned by interface " + element.first + ". Recheck that the cards configured in Homegear mirror the actually installed devices.");
				}
				peer->setAddress(currentAddress);
				currentAddress += peer->getMemorySize();
				usedAnalogOutputBits += peer->getMemorySize();
			}

			for(auto& peer : currentPots->digitalOutputs)
			{
				if(currentAddress + peer->getMemorySize() > currentPots->analogOutputBits + currentPots->digitalOutputBits && !booting)
				{
					GD::out.printError("Error: The calculated address of peer " + std::to_string(peer->getID()) + " exceeds number of digital output bits returned by interface " + element.first + ". Recheck that the cards configured in Homegear mirror the actually installed devices.");
				}
				peer->setAddress(currentAddress);
				currentAddress += peer->getMemorySize();
				usedDigitalOutputBits += peer->getMemorySize();
			}


			if(peersWithoutAddress[element.first] > 1)
			{
				GD::out.printWarning("Warning: " + std::to_string(peersWithoutAddress[element.first] - 1) + " peer(s) on interface " + element.first + " have/has an unset NEXT_PEER_ID. Please complete the peer configuration.");
			}
			else if(!booting)
			{
				if(usedAnalogInputBits + usedAnalogOutputBits < currentPots->analogInputBits) GD::out.printWarning("Warning: Interface " + element.first + " returned " + std::to_string(currentPots->analogInputBits) + " analog input and output bits but only " + std::to_string(usedAnalogInputBits + usedAnalogOutputBits) + " are used.");
				if(usedDigitalInputBits < currentPots->digitalInputBits) GD::out.printWarning("Warning: Interface " + element.first + " returned " + std::to_string(currentPots->digitalInputBits) + " digital input bits but only " + std::to_string(usedDigitalInputBits) + " are used.");
				if(usedDigitalOutputBits < currentPots->digitalOutputBits) GD::out.printWarning("Warning: Interface " + element.first + " returned " + std::to_string(currentPots->digitalOutputBits) + " digital output bits but only " + std::to_string(usedDigitalOutputBits) + " are used.");
			}
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

PVariable MyCentral::createDevice(BaseLib::PRpcClientInfo clientInfo, int32_t deviceType, std::string serialNumber, int32_t address, int32_t firmwareVersion, std::string interfaceId)
{
	try
	{
		if(serialNumber.size() < 10 || serialNumber.size() > 12) return Variable::createError(-1, "The serial number needs to have a size between 10 and 12.");
		if(peerExists(serialNumber)) return Variable::createError(-5, "This peer is already paired to this central.");

        if(GD::physicalInterfaces.find(interfaceId) == GD::physicalInterfaces.end()) return Variable::createError(-6, "Unknown physical interface.");

		std::shared_ptr<MyPeer> peer = createPeer(deviceType, address, serialNumber, false);
		if(!peer || !peer->getRpcDevice()) return Variable::createError(-6, "Unknown device type.");

		try
		{
			peer->save(true, true, false);
			peer->initializeCentralConfig();
			peer->setPhysicalInterfaceId(interfaceId);

            {
                std::lock_guard<std::mutex> peersGuard(_peersMutex);
                _peersById[peer->getID()] = peer;
                _peersBySerial[peer->getSerialNumber()] = peer;
            }

			updatePeerAddresses();
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
        std::vector<uint64_t> newIds{ peer->getID() };
		raiseRPCNewDevices(newIds, deviceDescriptions);
		GD::out.printMessage("Added peer " + std::to_string(peer->getID()) + ".");

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
