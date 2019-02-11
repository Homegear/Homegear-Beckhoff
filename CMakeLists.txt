cmake_minimum_required(VERSION 3.8)
project(homegear_beckhoff)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        src/PhysicalInterfaces/MainInterface.cpp
        src/PhysicalInterfaces/MainInterface.h
        src/Factory.cpp
        src/Factory.h
        src/GD.cpp
        src/GD.h
        src/Interfaces.cpp
        src/Interfaces.h
        src/MyCentral.cpp
        src/MyCentral.h
        src/MyFamily.cpp
        src/MyFamily.h
        src/MyPacket.cpp
        src/MyPacket.h
        src/MyPeer.cpp
        src/MyPeer.h
        config.h)

add_custom_target(homegear COMMAND ../../makeAll.sh SOURCES ${SOURCE_FILES})

add_library(homegear_beckhoff ${SOURCE_FILES})