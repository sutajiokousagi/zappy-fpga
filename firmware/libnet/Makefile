include ../../build/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

OBJECTS=microudp.o tftp.o

ifeq (yes,$(USE_UIP))
	CFLAGS += -DLIBUIP
endif

CFLAGS += -I. -I../../third_party/libuip -I../uip

all: libnet.a

libnet.a: $(OBJECTS)
	$(AR) crs libnet.a $(OBJECTS)

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

%.o: %.c
	$(compile)

%.o: %.S
	$(assemble)

.PHONY: all clean

clean:
	$(RM) $(OBJECTS) libnet.a .*~ *~
