LIBUIPDIR=../../third_party/libuip
UIPDIR=.

CFLAGS += \
	-I$(UIP_DIRECTORY)/$(UIPDIR) \
	-I$(UIP_DIRECTORY)/$(LIBUIPDIR) \
	-I$(UIP_DIRECTORY)/$(LIBUIPDIR)/net \
        -I../../third_party/libuip \
        -I../../third_party/libuip/net \
        -I. \
	-Wno-char-subscripts \
	-fno-strict-aliasing -fpack-struct
