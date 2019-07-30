/* Copyright 2013-2019 Homegear GmbH */

#include "GD.h"
#include "Interfaces.h"
#include "MyFamily.h"
#include "MyCentral.h"

namespace MyFamily
{

MyFamily::MyFamily(BaseLib::SharedObjects* bl, BaseLib::Systems::IFamilyEventSink* eventHandler) : BaseLib::Systems::DeviceFamily(bl, eventHandler, MY_FAMILY_ID, MY_FAMILY_NAME)
{
	GD::bl = bl;
	GD::family = this;
	GD::out.init(bl);
	GD::out.setPrefix(std::string("Module ") + MY_FAMILY_NAME + ": ");
	GD::out.printDebug("Debug: Loading module...");
    if(!enabled()) return;
	_physicalInterfaces.reset(new Interfaces(bl, _settings->getPhysicalInterfaceSettings()));
}

MyFamily::~MyFamily()
{

}

void MyFamily::dispose()
{
	if(_disposed) return;
	DeviceFamily::dispose();

	_central.reset();
}

void MyFamily::createCentral()
{
	try
	{
		_central.reset(new MyCentral(0, "VBF0000001", this));
		GD::out.printMessage("Created central with id " + std::to_string(_central->getId()) + ".");
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::shared_ptr<BaseLib::Systems::ICentral> MyFamily::initializeCentral(uint32_t deviceId, int32_t address, std::string serialNumber)
{
	return std::shared_ptr<MyCentral>(new MyCentral(deviceId, serialNumber, this));
}

PVariable MyFamily::getPairingInfo()
{
	try
	{
		if(!_central) return std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		PVariable info = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

		//{{{ General
		info->structValue->emplace("searchInterfaces", std::make_shared<BaseLib::Variable>(false));
		//}}}

		//{{{ Pairing methods
		PVariable pairingMethods = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

		//{{{ createDevice
		PVariable createDeviceMetadata = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		PVariable createDeviceMetadataInfo = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		PVariable createDeviceFields = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray);
		createDeviceFields->arrayValue->reserve(2);
		createDeviceFields->arrayValue->push_back(std::make_shared<BaseLib::Variable>("deviceType"));
		createDeviceFields->arrayValue->push_back(std::make_shared<BaseLib::Variable>("serialNumber"));
		createDeviceMetadataInfo->structValue->emplace("fields", createDeviceFields);
		createDeviceMetadata->structValue->emplace("metadataInfo", createDeviceMetadataInfo);

		pairingMethods->structValue->emplace("createDevice", createDeviceMetadata);
		//}}}

		info->structValue->emplace("pairingMethods", pairingMethods);
		//}}}

		//{{{ interfaces
		PVariable interfaces = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

		//{{{ BK90x0
		auto interface = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		interface->structValue->emplace("name", std::make_shared<BaseLib::Variable>(std::string("BK90x0")));
		interface->structValue->emplace("ipDevice", std::make_shared<BaseLib::Variable>(true));

		auto field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(0));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.common.id")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		interface->structValue->emplace("id", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(1));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.common.hostname")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		interface->structValue->emplace("host", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(2));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.common.pollinginterval")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("integer")));
		field->structValue->emplace("default", std::make_shared<BaseLib::Variable>(50));
		interface->structValue->emplace("interval", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(3));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.common.watchdogtimeout")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("integer")));
		field->structValue->emplace("default", std::make_shared<BaseLib::Variable>(0));
		interface->structValue->emplace("watchdogTimeout", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		field->structValue->emplace("const", std::make_shared<BaseLib::Variable>(std::string("502")));
		interface->structValue->emplace("port", field);

		interfaces->structValue->emplace("bk90x0", interface);
		//}}}

		info->structValue->emplace("interfaces", interfaces);
		//}}}

		return info;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}
}
