AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++17 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

libdir = $(localstatedir)/lib/homegear/modules
lib_LTLIBRARIES = mod_beckhoff.la
mod_beckhoff_la_SOURCES = MyFamily.cpp MyFamily.h MyPacket.cpp MyPacket.h MyPeer.cpp MyPeer.h Factory.cpp Factory.h GD.cpp GD.h MyCentral.cpp MyCentral.h Interfaces.h Interfaces.cpp PhysicalInterfaces/MainInterface.h PhysicalInterfaces/MainInterface.cpp
mod_beckhoff_la_LDFLAGS =-module -avoid-version -shared
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_beckhoff.la
