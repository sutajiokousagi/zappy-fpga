ZAPPY_DIR=../build

include $(ZAPPY_DIR)/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

ifeq ($(OS),Windows_NT)
COPY := cmd /c copy
else
COPY := cp
endif

OBJECTS=isr.o \
		processor.o \
		ci.o \
		i2c.o \
		main.o \
		dump.o \
		mdio.o \
		uptime.o \
	        version.o \
	        version_data.o \
		stdio_wrap.o \
#		etherbone.o \
#		ethernet.o \
#                telnet.o 

CFLAGS += -I. -I../third_party/libuip -Iuip

#include uip/Makefile.mk

all: firmware.bin

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
ifneq ($(OS),Windows_NT)
	chmod -x $@
endif
	$(COPY) $@ boot.bin

firmware.elf: $(OBJECTS)  uip/libuip.a libnet/libnet.a
	$(LD) $(LDFLAGS) \
		-T linker.ld \
		-N -o $@ \
		 $(ZAPPY_DIR)/software/libbase/crt0-$(CPU)-ctr.o \
		$(OBJECTS) \
		-L$(ZAPPY_DIR)/software/libbase \
		-L$(ZAPPY_DIR)/software/libcompiler_rt \
		-lbase-nofloat -lcompiler_rt -Map=firmware.map \
		-L./libnet \
		-lnet
#		-L./uip \
#		-luip

ifneq ($(OS),Windows_NT)
	chmod -x $@
endif

libnet/libnet.a: libnet/microudp.c libnet/tftp.c
	$(MAKE) -C libnet/

uip/libuip.a: 
	$(MAKE) -C uip/

main.o: main.c
	$(compile)

%.o: %.c
	$(compile)

%.o: %.S
	$(assemble)

load: firmware.bin
	litex_term --kernel firmware.bin COM8

clean:
	$(RM) $(OBJECTS) $(OBJECTS:.o=.d) firmware.elf firmware.bin .*~ *~ uip/*.o uip/*.a libnet/*.o libnet/*.a

# Dependencies on generated files
version.o: version_data.h version_data.c
version_data.o: version_data.h version_data.c

# Generated version info
version_data.h: version_data
version_data.c: version_data
version_data:
	bash ./version_data.sh
        # Check the version files exist
	[ -e ./version_data.h ]
	[ -e ./version_data.c ]


.PHONY: all main.o clean load
