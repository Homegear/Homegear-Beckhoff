/* Copyright 2013-2019 Homegear GmbH */

#ifndef GD_H_
#define GD_H_

#define MY_FAMILY_ID 11
#define MY_FAMILY_NAME "Beckhoff"

#include <homegear-base/BaseLib.h>
#include "MyFamily.h"
#include "PhysicalInterfaces/MainInterface.h"

namespace MyFamily
{

class GD
{
public:
	virtual ~GD();

	static BaseLib::SharedObjects* bl;
	static MyFamily* family;
	static std::map<std::string, std::shared_ptr<MainInterface>> physicalInterfaces;
	static std::shared_ptr<MainInterface> defaultPhysicalInterface;
	static BaseLib::Output out;
private:
	GD();
};

}

#endif /* GD_H_ */
