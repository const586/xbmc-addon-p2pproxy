LIBS   =
LIBDIR = ../../addons/pvr.p2pproxy
LIB    = ../../addons/pvr.p2pproxy/$(ADDONNAME)

lib_LTLIBRARIES = libp2pproxyaddon.la

libp2pproxyaddon_la_SOURCES = \
	client.cpp \
	client.h \
	PVRIptvData.cpp \
	PVRIptvData.h
	
libp2pproxyaddon_la_LDFLAGS = \
	-avoid-version

libp2pproxyaddon_la_LIBADD = \
	$(ZLIB_LIBS)

AM_CPPFLAGS = \
	$(ARCH_DEFINES)

INCLUDES = \
	-I. \
	-I$(top_srcdir)/include -I$(top_srcdir)/include/xbmc -I$(top_srcdir)/src \
	-DUSE_DEMUX

EXTRA_DIST = \
	include

$(ADDONNAME): libp2pproxyaddon.la
	cp -f .libs/libp2pproxyaddon.$(SOEXT) $(ADDONNAME)

$(LIB): $(ADDONNAME)
	cp -f $(ADDONNAME) $(LIB)

all: $(LIB)

clean-local:
	-rm -f $(LIB) $(ADDONNAME)

install:
