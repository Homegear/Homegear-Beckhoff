/* Copyright 2013-2019 Homegear GmbH */

#include "MyPeer.h"

#include "GD.h"
#include "MyPacket.h"
#include "MyCentral.h"

namespace MyFamily
{
std::shared_ptr<BaseLib::Systems::ICentral> MyPeer::getCentral()
{
	try
	{
		if(_central) return _central;
		_central = GD::family->getCentral();
		return _central;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return std::shared_ptr<BaseLib::Systems::ICentral>();
}

MyPeer::MyPeer(uint32_t parentID, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, parentID, eventHandler)
{
	init();
}

MyPeer::MyPeer(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, id, address, serialNumber, parentID, eventHandler)
{
	init();
}

MyPeer::~MyPeer()
{
	try
	{
		dispose();
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void MyPeer::init()
{
	try
	{
		_binaryEncoder.reset(new BaseLib::Rpc::RpcEncoder(GD::bl));
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void MyPeer::dispose()
{
	if(_disposing) return;
	Peer::dispose();
}

void MyPeer::homegearStarted()
{
	try
	{
		Peer::homegearStarted();
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void MyPeer::homegearShuttingDown()
{
	try
	{
		_shuttingDown = true;
		Peer::homegearShuttingDown();
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void MyPeer::setNextPeerId(uint64_t value)
{
	try
	{
		_nextPeerId = value;
		std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = configCentral.find(0);
		if(channelIterator == configCentral.end()) return;
		std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find("NEXT_PEER_ID");
		if(parameterIterator != channelIterator->second.end())
		{
			std::vector<uint8_t> parameterData;
			parameterIterator->second.rpcParameter->convertToPacket(BaseLib::PVariable(new BaseLib::Variable(value)), parameterData);
			parameterIterator->second.setBinaryData(parameterData);
			if(parameterIterator->second.databaseId > 0) saveParameter(parameterIterator->second.databaseId, parameterData);
			else saveParameter(0, ParameterGroup::Type::Enum::config, 0, "NEXT_PEER_ID", parameterData);
			GD::out.printInfo("Info: Parameter NEXT_PEER_ID of peer " + std::to_string(_peerID) + " and channel 0 was set to " + std::to_string(value) + ".");
			raiseRPCUpdateDevice(_peerID, 0, _serialNumber + ":0", 0);
		}
		std::shared_ptr<MyCentral> central = std::dynamic_pointer_cast<MyCentral>(getCentral());
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void MyPeer::setAddress(int32_t value)
{
	try
	{
		if(_address == value) return;
		Peer::setAddress(value);
		_bitSize = -1;
		_registerSize = -1;
		std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = configCentral.find(0);
		if(channelIterator == configCentral.end()) return;
		std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find("ADDRESS");
		if(parameterIterator != channelIterator->second.end())
		{
			std::vector<uint8_t> parameterData;
			parameterIterator->second.rpcParameter->convertToPacket(BaseLib::PVariable(new BaseLib::Variable(_address)), parameterData);
			parameterIterator->second.setBinaryData(parameterData);
			if(parameterIterator->second.databaseId > 0) saveParameter(parameterIterator->second.databaseId, parameterData);
			else saveParameter(0, ParameterGroup::Type::Enum::config, 0, "ADDRESS", parameterData);
			GD::out.printInfo("Info: Parameter ADDRESS of peer " + std::to_string(_peerID) + " and channel 0 was set to 0x" + BaseLib::HelperFunctions::getHexString(value) + ".");
			raiseRPCUpdateDevice(_peerID, 0, _serialNumber + ":0", 0);
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

bool MyPeer::isOutputDevice()
{
	try
	{
		if(!_rpcDevice) return true;
		Functions::iterator functionIterator = _rpcDevice->functions.find(1);
		if(functionIterator == _rpcDevice->functions.end()) return true;
		return ((_deviceType & 0x2000) == 0x2000) || ((_deviceType & 0x4000) == 0x4000) || functionIterator->second->type == "Output";
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return true;
}

std::string MyPeer::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;

		if(command == "help")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "unselect\t\tUnselect this peer" << std::endl;
			stringStream << "channel count\t\tPrint the number of channels of this peer" << std::endl;
			stringStream << "config print\t\tPrints all configuration parameters and their values" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 13, "channel count") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints this peer's number of channels." << std::endl;
						stringStream << "Usage: channel count" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			stringStream << "Peer has " << _rpcDevice->functions.size() << " channels." << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 12, "config print") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints all configuration parameters of this peer. The values are in BidCoS packet format." << std::endl;
						stringStream << "Usage: config print" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			return printConfig();
		}
		else return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return "Error executing command. See log file for more details.\n";
}

std::string MyPeer::printConfig()
{
	try
	{
		std::ostringstream stringStream;
		stringStream << "MASTER" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = configCentral.begin(); i != configCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t[" << j->first << "]: ";
				if(!j->second.rpcParameter) stringStream << "(No RPC parameter) ";
				std::vector<uint8_t> parameterData = j->second.getBinaryData();
				for(std::vector<uint8_t>::const_iterator k = parameterData.begin(); k != parameterData.end(); ++k)
				{
					stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
				}
				stringStream << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;

		stringStream << "VALUES" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = valuesCentral.begin(); i != valuesCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t[" << j->first << "]: ";
				if(!j->second.rpcParameter) stringStream << "(No RPC parameter) ";
				std::vector<uint8_t> parameterData = j->second.getBinaryData();
				for(std::vector<uint8_t>::const_iterator k = parameterData.begin(); k != parameterData.end(); ++k)
				{
					stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
				}
				stringStream << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;

		return stringStream.str();
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return "";
}

int32_t MyPeer::getStorageSize()
{
	try
	{
		if(_registerSize > -1 || !_rpcDevice) return _registerSize;

		int32_t bitSize = -1;
		for(Functions::iterator i = _rpcDevice->functions.begin(); i != _rpcDevice->functions.end(); ++i)
		{
			if(i->second->variablesId == "digital_output_valueset" || i->second->variablesId == "digital_input_valueset") bitSize++;
			else if(i->second->variablesId.compare(0, 22, "analog_output_valueset") == 0 || i->second->variablesId.compare(0, 21, "analog_input_valueset") == 0)
			{
				PParameter parameter = i->second->variables->getParameter("LEVEL");
				if(!parameter) continue;
				if(parameter->logical->type != BaseLib::DeviceDescription::ILogical::Type::tFloat) continue;
				LogicalDecimal* levelParameter = (LogicalDecimal*)parameter->logical.get();
				uint32_t range = (int32_t)levelParameter->maximumValue + ((int32_t)levelParameter->minimumValue * -1);
				while(range)
				{
					range = range >> 1;
					bitSize++;
				}
			}
		}
		if(bitSize == 0)
		{
			_registerSize = 0;
			return 0;
		}

		_registerSize = (bitSize / 16) + 1;
		_bitSize = bitSize + 1;
		return _registerSize;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return 0;
}

bool MyPeer::isAnalog()
{
	try
	{
		if(!_rpcDevice) return false;
		Functions::iterator functionIterator = _rpcDevice->functions.find(1);
		if(functionIterator == _rpcDevice->functions.end()) return false;
		return ((_deviceType & 0x3000) == 0x3000) || ((_deviceType & 0x4000) == 0x4000) || functionIterator->second->variablesId.compare(0, 7, "analog_") == 0;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

void MyPeer::setPhysicalInterfaceId(std::string id)
{
	if(id.empty() || (GD::physicalInterfaces.find(id) != GD::physicalInterfaces.end() && GD::physicalInterfaces.at(id)))
	{
		_physicalInterfaceId = id;
		setPhysicalInterface(id.empty() ? GD::defaultPhysicalInterface : GD::physicalInterfaces.at(_physicalInterfaceId));
		saveVariable(19, _physicalInterfaceId);
	}
	if(!_physicalInterface) _physicalInterface = GD::defaultPhysicalInterface;
}

void MyPeer::setPhysicalInterface(std::shared_ptr<MainInterface> interface)
{
	try
	{
		if(!interface) return;
		_physicalInterface = interface;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::vector<char> MyPeer::serializeStates()
{
	try
	{
		std::vector<char> states;
		std::lock_guard<std::mutex> statesGuard(_statesMutex);
		states.reserve(_states.size() * 2);
		for(std::vector<uint16_t>::iterator i = _states.begin(); i != _states.end(); ++i)
		{
			states.push_back(*i >> 8);
			states.push_back(*i & 0xFF);
		}
		return states;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return std::vector<char>();
}

void MyPeer::unserializeStates(std::vector<char>& data)
{
	try
	{
		std::lock_guard<std::mutex> statesGuard(_statesMutex);
		_states.resize(data.size() / 2, 0);
		for(uint32_t i = 0; i < data.size(); i += 2)
		{
			_states.at(i / 2) = (((uint16_t)(uint8_t)data.at(i)) << 8) | ((uint16_t)(uint8_t)data.at(i + 1));
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void MyPeer::loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows)
{
	try
	{
		if(!rows) rows = _bl->db->getPeerVariables(_peerID);
		Peer::loadVariables(central, rows);

		_rpcDevice = GD::family->getRpcDevices()->find(_deviceType, _firmwareVersion, -1);
		if(!_rpcDevice) return;

		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			switch(row->second.at(2)->intValue)
			{
			case 5:
				if(row->second.at(5)->binaryValue) unserializeStates(*row->second.at(5)->binaryValue);
				break;
			case 19:
				_physicalInterfaceId = row->second.at(4)->textValue;
				if(!_physicalInterfaceId.empty() && GD::physicalInterfaces.find(_physicalInterfaceId) != GD::physicalInterfaces.end()) setPhysicalInterface(GD::physicalInterfaces.at(_physicalInterfaceId));
				break;
			}
		}
		if(!_physicalInterface) _physicalInterface = GD::defaultPhysicalInterface;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void MyPeer::saveVariables()
{
	try
	{
		if(_peerID == 0) return;
		Peer::saveVariables();
		std::vector<char> states = serializeStates();
		saveVariable(5, states);
		saveVariable(19, _physicalInterfaceId);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

bool MyPeer::load(BaseLib::Systems::ICentral* central)
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows;
		loadVariables(central, rows);
		if(!_rpcDevice)
		{
			GD::out.printError("Error loading peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString(_deviceType) + " Firmware version: " + std::to_string(_firmwareVersion));
			return false;
		}

		initializeTypeString();
		std::string entry;
		loadConfig();
		initializeCentralConfig();

		serviceMessages.reset(new BaseLib::Systems::ServiceMessages(_bl, _peerID, _serialNumber, this));
		serviceMessages->load();

		{
			std::lock_guard<std::mutex> statesGuard(_statesMutex);
			if(!_states.empty())
			{
				std::shared_ptr<MyPacket> packet(new MyPacket(_address, _address + ((_states.size() - 1) * 16) + 15, _states));
				_physicalInterface->setOutputData(packet);
			}
		}

		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = configCentral.begin(); i != configCentral.end(); ++i)
		{
			if(i->first == 0)
			{
				std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = i->second.find("NEXT_PEER_ID");
				if(parameterIterator != i->second.end() && parameterIterator->second.rpcParameter)
				{
					std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
					_nextPeerId = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue64;
				}
			}

			int32_t interval = 0;
			int32_t decimalPlaces = 0;
			int32_t inputMin = 0;
			int32_t inputMax = 0;
			int32_t outputMin = 0;
			int32_t outputMax = 0;

			std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = i->second.find("INTERVAL");
			if(parameterIterator != i->second.end() && parameterIterator->second.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				interval = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
			}

			parameterIterator = i->second.find("DECIMAL_PLACES");
			if(parameterIterator != i->second.end() && parameterIterator->second.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				decimalPlaces = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
			}

			parameterIterator = i->second.find("INPUT_MIN");
			if(parameterIterator != i->second.end() && parameterIterator->second.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				inputMin = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
			}

			parameterIterator = i->second.find("INPUT_MAX");
			if(parameterIterator != i->second.end() && parameterIterator->second.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				inputMax = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
			}

			parameterIterator = i->second.find("OUTPUT_MIN");
			if(parameterIterator != i->second.end() && parameterIterator->second.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				outputMin = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
			}

			parameterIterator = i->second.find("OUTPUT_MAX");
			if(parameterIterator != i->second.end() && parameterIterator->second.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				outputMax = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
			}

			_intervals[i->first] = interval;
			_decimalPlaces[i->first] = decimalPlaces;
			_minimumInputValues[i->first] = inputMin;
			_maximumInputValues[i->first] = inputMax;
			_minimumOutputValues[i->first] = outputMin;
			_maximumOutputValues[i->first] = outputMax;

		}

		return true;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

void MyPeer::packetReceived(std::vector<uint16_t>& packet)
{
	try
	{
		if(_disposing || !_rpcDevice) return;
		setLastPacketReceived();

		std::unique_lock<std::mutex> statesGuard(_statesMutex);
		if(packet.size() == _states.size() && std::equal(packet.begin(), packet.end(), _states.begin())) return;

		_states.resize(packet.size(), 0);
		statesGuard.unlock();

		std::map<uint32_t, std::shared_ptr<std::vector<std::string>>> valueKeys;
		std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>> rpcValues;

		if(isAnalog())
		{
			BaseLib::Systems::RpcConfigurationParameter* parameter = nullptr;
			std::string name = "LEVEL";
			BaseLib::PVariable value;

			for(Functions::iterator channelIterator = _rpcDevice->functions.find(1); channelIterator != _rpcDevice->functions.end(); ++channelIterator)
			{
				int32_t index = (channelIterator->first - 1) + channelIterator->second->variables->memoryAddressStart / 16;
				if(index >= (signed)packet.size()) continue;
				statesGuard.lock();
				if(packet.at(index) == _states.at(index))
				{
					statesGuard.unlock();
					continue;
				}
				statesGuard.unlock();

				parameter = &valuesCentral[channelIterator->first][name];
				if(!parameter->rpcParameter) continue;

				{
					std::lock_guard<std::mutex> lastDataGuard(_lastDataMutex);
					if(BaseLib::HelperFunctions::getTime() - _lastData[channelIterator->first] < _intervals[channelIterator->first]) continue;
					_lastData[channelIterator->first] = BaseLib::HelperFunctions::getTime();
				}

				LogicalDecimal* levelParameter = (LogicalDecimal*)parameter->rpcParameter->logical.get();
				bool isSigned = levelParameter->minimumValue < 0;

				statesGuard.lock();
				_states[index] = packet[index];
				statesGuard.unlock();

				double inputMin = 0;
				double inputMax = 0;
				double outputMin = 0;
				double outputMax = 0;
				if(_minimumInputValues[channelIterator->first] != 0 || _maximumInputValues[channelIterator->first] != 0)
				{
					inputMin = _minimumInputValues[channelIterator->first];
					inputMax = _maximumInputValues[channelIterator->first];
				}
				else
				{
					inputMin = levelParameter->minimumValue;
					inputMax = levelParameter->maximumValue;
				}
				if(_minimumOutputValues[channelIterator->first] != 0 || _maximumOutputValues[channelIterator->first] != 0)
				{
					outputMin = _minimumOutputValues[channelIterator->first];
					outputMax = _maximumOutputValues[channelIterator->first];
				}
				else
				{
					outputMin = levelParameter->minimumValue;
					outputMax = levelParameter->maximumValue;
				}

				double doubleValue = isSigned ? (double)(int16_t)packet[index] : (double)packet[index];
				doubleValue = BaseLib::Math::scale(BaseLib::Math::clamp(doubleValue, inputMin, inputMax), inputMin, inputMax, outputMin, outputMax);
				double decimalFactor = BaseLib::Math::Pow10(_decimalPlaces[channelIterator->first]);
				doubleValue = std::round(doubleValue * decimalFactor) / decimalFactor;

				value.reset(new BaseLib::Variable(doubleValue));
				std::vector<uint8_t> data;
				_binaryEncoder->encodeResponse(value, data);
				if(parameter->equals(data)) continue;
				parameter->setBinaryData(data);

				if(!value) continue;

				if(!valueKeys[channelIterator->first] || !rpcValues[channelIterator->first])
				{
					valueKeys[channelIterator->first].reset(new std::vector<std::string>());
					rpcValues[channelIterator->first].reset(new std::vector<PVariable>());
				}

				if(parameter->databaseId > 0) saveParameter(parameter->databaseId, data);
				else saveParameter(0, ParameterGroup::Type::Enum::variables, channelIterator->first, name, data);
				if(_bl->debugLevel >= 4) GD::out.printInfo("Info: " + name + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channelIterator->first) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(data) + ".");

				valueKeys[channelIterator->first]->push_back(name);
				rpcValues[channelIterator->first]->push_back(value);
			}
		}
		else
		{
			for(uint32_t i = 0; i < packet.size(); i++)
			{
				BaseLib::Systems::RpcConfigurationParameter* parameter = nullptr;
				std::string name = "STATE";
				int32_t channel = -1;
				BaseLib::PVariable value;

				for(uint32_t j = 0; j < 16; j++)
				{
					statesGuard.lock();
					if(!((packet.at(i) & _bitMask[j]) ^ (_states.at(i) & _bitMask[j])))
					{
						statesGuard.unlock();
						continue;
					}

					uint16_t bitValue = packet.at(i) & _bitMask[j];
					_states.at(i) &= _reversedBitMask[j];
					_states.at(i) |= bitValue;
					statesGuard.unlock();

					channel = (i * 16) + j + 1;
					parameter = &valuesCentral[channel][name];
					if(!parameter->rpcParameter) continue;

					value.reset(new BaseLib::Variable((bool)bitValue));
					std::vector<uint8_t> parameterData;
					_binaryEncoder->encodeResponse(value, parameterData);
					parameter->setBinaryData(parameterData);

					if(!value) continue;

					if(!valueKeys[channel] || !rpcValues[channel])
					{
						valueKeys[channel].reset(new std::vector<std::string>());
						rpcValues[channel].reset(new std::vector<PVariable>());
					}

					if(parameter->databaseId > 0) saveParameter(parameter->databaseId, parameterData);
					else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, name, parameterData);
					if(_bl->debugLevel >= 4) GD::out.printInfo("Info: " + name + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameterData) + ".");

					valueKeys[channel]->push_back(name);
					rpcValues[channel]->push_back(value);
				}
			}
		}

		if(!rpcValues.empty())
		{
			for(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>::iterator j = valueKeys.begin(); j != valueKeys.end(); ++j)
			{
				if(j->second->empty()) continue;

                std::string eventSource = "device-" + std::to_string(_peerID);
                std::string address(_serialNumber + ":" + std::to_string(j->first));
                raiseEvent(eventSource, _peerID, j->first, j->second, rpcValues.at(j->first));
                raiseRPCEvent(eventSource, _peerID, j->first, address, j->second, rpcValues.at(j->first));
			}
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

PParameterGroup MyPeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		PFunction rpcChannel = _rpcDevice->functions.at(channel);
		if(type == ParameterGroup::Type::Enum::variables) return rpcChannel->variables;
		else if(type == ParameterGroup::Type::Enum::config) return rpcChannel->configParameters;
		else if(type == ParameterGroup::Type::Enum::link) return rpcChannel->linkParameters;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return PParameterGroup();
}

bool MyPeer::getAllValuesHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters)
{
	try
	{
		if(channel == 1)
		{
			if(parameter->id == "PEER_ID")
			{
				std::vector<uint8_t> parameterData;
				parameter->convertToPacket(PVariable(new Variable((int32_t)_peerID)), parameterData);
				valuesCentral[channel][parameter->id].setBinaryData(parameterData);
			}
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

bool MyPeer::getParamsetHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters)
{
	try
	{
		if(channel == 1)
		{
			if(parameter->id == "PEER_ID")
			{
				std::vector<uint8_t> parameterData;
				parameter->convertToPacket(PVariable(new Variable((int32_t)_peerID)), parameterData);
				valuesCentral[channel][parameter->id].setBinaryData(parameterData);
			}
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

PVariable MyPeer::putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool checkAcls, bool onlyPushing)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");
		if(variables->structValue->empty()) return PVariable(new Variable(VariableType::tVoid));

		auto central = getCentral();
		if(!central) return Variable::createError(-32500, "Could not get central.");

		if(type == ParameterGroup::Type::Enum::config)
		{
			bool configChanged = false;
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = configCentral.find(channel);
				if(channelIterator == configCentral.end()) continue;
				std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find(i->first);
				if(parameterIterator == channelIterator->second.end()) continue;
				BaseLib::Systems::RpcConfigurationParameter& parameter = parameterIterator->second;
				if(!parameter.rpcParameter) continue;

				if(channel == 0)
				{
					if(i->first == "NEXT_PEER_ID")
					{
						std::shared_ptr<MyCentral> central = std::dynamic_pointer_cast<MyCentral>(getCentral());
						if(!central) continue;
						if((uint64_t) i->second->integerValue64 != _nextPeerId)
						{
							_nextPeerId = i->second->integerValue64;
							central->updatePeerAddresses();
						}
					}
					else if(i->first == "ADDRESS") continue;
				}
				std::vector<uint8_t> parameterData;
				parameter.rpcParameter->convertToPacket(i->second, parameterData);
				parameter.setBinaryData(parameterData);
				if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
				else saveParameter(0, ParameterGroup::Type::Enum::config, channel, i->first, parameterData);
				GD::out.printInfo("Info: Parameter " + i->first + " of peer " + std::to_string(_peerID) + " and channel " + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameterData) + ".");
				if(parameter.rpcParameter->physical->operationType != IPhysical::OperationType::Enum::config && parameter.rpcParameter->physical->operationType != IPhysical::OperationType::Enum::configString) continue;

				if(i->first == "INPUT_MIN" || i->first == "INPUT_MAX" || i->first == "OUTPUT_MIN" || i->first == "OUTPUT_MAX")
				{
					int32_t inputMin = 0;
					int32_t inputMax = 0;
					int32_t outputMin = 0;
					int32_t outputMax = 0;

					std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find("INPUT_MIN");
					if(parameterIterator != channelIterator->second.end() && parameterIterator->second.rpcParameter)
					{
						std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
						inputMin = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
					}

					parameterIterator = channelIterator->second.find("INPUT_MAX");
					if(parameterIterator != channelIterator->second.end() && parameterIterator->second.rpcParameter)
					{
						std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
						inputMax = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
					}

					parameterIterator = channelIterator->second.find("OUTPUT_MIN");
					if(parameterIterator != channelIterator->second.end() && parameterIterator->second.rpcParameter)
					{
						std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
						outputMin = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
					}

					parameterIterator = channelIterator->second.find("OUTPUT_MAX");
					if(parameterIterator != channelIterator->second.end() && parameterIterator->second.rpcParameter)
					{
						std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
						outputMax = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
					}

					_minimumInputValues[channel] = inputMin;
					_maximumInputValues[channel] = inputMax;
					_minimumOutputValues[channel] = outputMin;
					_maximumOutputValues[channel] = outputMax;
				}
				else if(i->first == "INTERVAL")
				{
					int32_t interval = 0;

					std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find("INTERVAL");
					if(parameterIterator != channelIterator->second.end() && parameterIterator->second.rpcParameter)
					{
						std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
						interval = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
					}

					_intervals[channel] = interval;
				}
				else if(i->first == "DECIMAL_PLACES")
				{
					int32_t decimalPlaces = 0;

					std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find("DECIMAL_PLACES");
					if(parameterIterator != channelIterator->second.end() && parameterIterator->second.rpcParameter)
					{
						std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
						decimalPlaces = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->integerValue;
					}

					_decimalPlaces[channel] = decimalPlaces;
				}

				configChanged = true;
			}

			if(configChanged) raiseRPCUpdateDevice(_peerID, channel, _serialNumber + ":" + std::to_string(channel), 0);
		}
		else if(type == ParameterGroup::Type::Enum::variables)
		{
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;

				if(checkAcls && !clientInfo->acls->checkVariableWriteAccess(central->getPeer(_peerID), channel, i->first)) continue;

				setValue(clientInfo, channel, i->first, i->second, true);
			}
		}
		else
		{
			return Variable::createError(-3, "Parameter set type is not supported.");
		}
		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable MyPeer::setInterface(BaseLib::PRpcClientInfo clientInfo, std::string interfaceId)
{
	try
	{
		if(!interfaceId.empty() && GD::physicalInterfaces.find(interfaceId) == GD::physicalInterfaces.end())
		{
			return Variable::createError(-5, "Unknown physical interface.");
		}
		std::shared_ptr<MainInterface> interface(GD::physicalInterfaces.at(interfaceId));
		setPhysicalInterfaceId(interfaceId);
		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable MyPeer::setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait)
{
	try
	{
		Peer::setValue(clientInfo, channel, valueKey, value, wait); //Ignore result, otherwise setHomegerValue might not be executed
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(valueKey.empty()) return Variable::createError(-5, "Value key is empty.");
		if(channel == 0 && serviceMessages->set(valueKey, value->booleanValue)) return PVariable(new Variable(VariableType::tVoid));
		std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = valuesCentral.find(channel);
		if(channelIterator == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find(valueKey);
		if(parameterIterator == channelIterator->second.end()) return Variable::createError(-5, "Unknown parameter.");
		PParameter rpcParameter = parameterIterator->second.rpcParameter;
		if(!rpcParameter) return Variable::createError(-5, "Unknown parameter.");
		BaseLib::Systems::RpcConfigurationParameter& parameter = parameterIterator->second;
		std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>());
		std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable>());
		if(rpcParameter->readable)
		{
			valueKeys->push_back(valueKey);
			values->push_back(value);
		}

		if(value->floatValue == 0)
		{
			if(value->integerValue != 0) value->floatValue = value->integerValue;
			if(value->integerValue64 != 0) value->floatValue = value->integerValue64;
		}

		if(rpcParameter->physical->operationType == IPhysical::OperationType::Enum::store)
		{
			std::vector<uint8_t> parameterData;
			rpcParameter->convertToPacket(value, parameterData);
			parameter.setBinaryData(parameterData);
			if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
			else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameterData);
			if(!valueKeys->empty())
			{
                std::string address(_serialNumber + ":" + std::to_string(channel));
                raiseEvent(clientInfo->initInterfaceId, _peerID, channel, valueKeys, values);
                raiseRPCEvent(clientInfo->initInterfaceId, _peerID, channel, address, valueKeys, values);
			}
			return PVariable(new Variable(VariableType::tVoid));
		}
		else if(rpcParameter->physical->operationType != IPhysical::OperationType::Enum::command) return Variable::createError(-6, "Parameter is not settable.");
        if(rpcParameter->setPackets.empty() && !rpcParameter->writeable) return Variable::createError(-6, "parameter is read only");

		if(valueKey == "STATE")
		{
			int32_t statesIndex = (channel - 1) / 16;
			int32_t bitIndex = (channel - 1) % 16;
			std::shared_ptr<MyPacket> packet;

			{
				std::lock_guard<std::mutex> statesGuard(_statesMutex);
				while(statesIndex >= (signed)_states.size()) _states.push_back(0);
				if(*value) _states.at(statesIndex) |= 1 << bitIndex;
				else _states.at(statesIndex) &= ~(1 << bitIndex);
				packet = std::make_shared<MyPacket>(_address + (statesIndex * 16) + bitIndex, _address + (statesIndex * 16) + bitIndex, (_states.at(statesIndex) >> bitIndex) & 1);
			}

			_physicalInterface->sendPacket(packet);
		}
		else if(valueKey == "LEVEL") //Analog cards always have 16 bit per channel
		{
			Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
			if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
			int32_t statesIndex = channel + (functionIterator->second->variables->memoryAddressStart / 16) - 1;
			std::unique_lock<std::mutex> statesGuard(_statesMutex);
			while(statesIndex >= (signed)_states.size()) _states.push_back(0);
			statesGuard.unlock();
			if(_minimumInputValues[channel] != 0 || _maximumInputValues[channel] != 0 || _minimumOutputValues[channel] != 0 || _maximumOutputValues[channel] != 0)
			{
				double inputMin = 0;
				double inputMax = 0;
				double outputMin = 0;
				double outputMax = 0;
				if(_minimumInputValues[channel] != 0 || _maximumInputValues[channel] != 0)
				{
					inputMin = _minimumInputValues[channel];
					inputMax = _maximumInputValues[channel];
				}
				else
				{
					std::shared_ptr<LogicalDecimal> logicalDecimalLevel(std::dynamic_pointer_cast<LogicalDecimal>(rpcParameter->logical));
					if(logicalDecimalLevel)
					{
						inputMin = logicalDecimalLevel->minimumValue;
						inputMax = logicalDecimalLevel->maximumValue;
					}
					else
					{
						std::shared_ptr<LogicalInteger> logicalIntegerLevel(std::dynamic_pointer_cast<LogicalInteger>(rpcParameter->logical));
						inputMin = logicalIntegerLevel->minimumValue;
						inputMax = logicalIntegerLevel->maximumValue;
					}
				}
				if(_minimumOutputValues[channel] != 0 || _maximumOutputValues[channel] != 0)
				{
					outputMin = _minimumOutputValues[channel];
					outputMax = _maximumOutputValues[channel];
				}
				else
				{
					std::shared_ptr<LogicalDecimal> logicalDecimalLevel(std::dynamic_pointer_cast<LogicalDecimal>(rpcParameter->logical));
					if(logicalDecimalLevel)
					{
						outputMin = logicalDecimalLevel->minimumValue;
						outputMax = logicalDecimalLevel->maximumValue;
					}
					else
					{
						std::shared_ptr<LogicalInteger> logicalIntegerLevel(std::dynamic_pointer_cast<LogicalInteger>(rpcParameter->logical));
						outputMin = logicalDecimalLevel->minimumValue;
						outputMax = logicalDecimalLevel->maximumValue;
					}
				}
				value->floatValue = BaseLib::Math::clamp(value->floatValue, inputMin, inputMax);
				statesGuard.lock();
				_states.at(statesIndex) = (int16_t)std::lround(BaseLib::Math::scale(value->floatValue, inputMin, inputMax, outputMin, outputMax));
				statesGuard.unlock();
			}
			else
			{
				statesGuard.lock();
				_states.at(statesIndex) = (int16_t)std::lround(value->floatValue);
				statesGuard.unlock();
			}
			uint32_t offset = isAnalog() ? 0 : _physicalInterface->digitalOutputOffset();
			statesGuard.lock();
			std::shared_ptr<MyPacket> packet(new MyPacket(_address + (statesIndex * 16) + offset, _address + (statesIndex * 16) + offset + 15, _states.at(statesIndex)));
			statesGuard.unlock();
			_physicalInterface->sendPacket(packet);
		}
		else return Variable::createError(-5, "Only LEVEL and STATE are supported parameter names.");

		bool fastMode = false;
		bool superFastMode = false;
		auto configChannelIterator = configCentral.find(0);
		if(configChannelIterator != configCentral.end())
		{
			std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = configChannelIterator->second.find("FAST_MODE");
			if(parameterIterator != configChannelIterator->second.end() && parameterIterator->second.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				fastMode = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->booleanValue;
			}
			parameterIterator = configChannelIterator->second.find("SUPER_FAST_MODE");
			if(parameterIterator != configChannelIterator->second.end() && parameterIterator->second.rpcParameter)
			{
				std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
				superFastMode = parameterIterator->second.rpcParameter->convertFromPacket(parameterData)->booleanValue;
			}
		}


		std::vector<uint8_t> parameterData;
		rpcParameter->convertToPacket(value, parameterData);
		parameter.setBinaryData(parameterData);
		if(!fastMode && !superFastMode)
		{
			if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
			else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameterData);
			if(_bl->debugLevel >= 4) GD::out.printInfo("Info: " + valueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameterData) + ".");

			std::vector<char> states = serializeStates();
			saveVariable(5, states);
		}

		if(!superFastMode && !valueKeys->empty())
		{
            std::string address(_serialNumber + ":" + std::to_string(channel));
            raiseEvent(clientInfo->initInterfaceId, _peerID, channel, valueKeys, values);
            raiseRPCEvent(clientInfo->initInterfaceId, _peerID, channel, address, valueKeys, values);
		}

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error. See error log for more details.");
}

}
