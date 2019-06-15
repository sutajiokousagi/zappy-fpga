ZAPPY_DIR=../build

include $(ZAPPY_DIR)/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

include uip/Makefile.mk

OBJECTS=	isr.o \
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
		si1153.o \
		gfxapi.o \
#                assets/rawdata.o \

# prepend our local files to override system ones
CFLAGS := -I. $(CFLAGS) -fdata-sections -ffunction-sections

# UIP enables telnet but adds about 25kiB
# after changing, need to do a make clean.
USE_UIP:=yes

# GFX enables uGFX for OLED display
USE_GFX:=yes

# use IQ motor
USE_IQ:=no

ifeq (yes,$(USE_GFX))
	GFXDIR := /home/bunnie/code/zappy-fpga/third_party/ugfx
        UGFXLIBS := -L$(GFXDIR)/.build -lzappy -Lriscv -lc_nano
	GFXDEPLIBS := $(GFXDIR)/.build/libzappy.a

	CFLAGS += -DGFX_USE_OS_RAW32=GFXON  -I$(GFXDIR) -I$(GFXDIR)/drivers/gdisp/SSD1322 -Iassets
	GFXDEFS += -DGFX_USE_OS_RAW32=GFXON
endif

ifeq (yes,$(USE_UIP))
	UIPFLAGS := -L./uip -luip
        UIPDIR := /home/bunnie/code/zappy-fpga/third_party/libuip

	OBJECTS += telnet.o etherbone.o
	CFLAGS += -DLIBUIP -I$(UIPDIR) -Iuip 
endif

ifeq (yes,$(USE_IQ))
	IQFLAGS := -L./iq -liq
	IQDIR := /home/bunnie/code/zappy-fpga/iq

	OBJECTS += motor.o
	CFLAGS += -DMOTOR -I$(IQDIR) -Iiq
endif

ifeq ($(OS),Windows_NT)
COPY := cmd /c copy
else
COPY := cp
endif

all: firmware.bin

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
ifneq ($(OS),Windows_NT)
	chmod -x $@
endif
	$(COPY) $@ boot.bin


firmware.elf: $(OBJECTS) uip/libuip.a libnet/libnet.a $(GFXDEPLIBS)
	$(LD) $(LDFLAGS) \
		-T linker.ld \
		-N -o $@ \
		 $(ZAPPY_DIR)/software/libbase/crt0-$(CPU)-ctr.o \
		$(OBJECTS) \
		-L$(ZAPPY_DIR)/software/libbase \
		-L$(ZAPPY_DIR)/software/libcompiler_rt \
		-lbase-nofloat -lcompiler_rt -Map=firmware.map \
		-L./libnet \
		-lnet \
                $(UGFXLIBS) \
		$(UIPFLAGS) \
	 	$(IQFLAGS) \
		-L. -lgcc \
		--gc-sections

# -L. -lgcc  bestows upon us floating point routines :P

ifneq ($(OS),Windows_NT)
	chmod -x $@
endif

$(GFXDIR)/.build/libzappy.a: gfxconf.h
	$(MAKE) -C $(GFXDIR)

libnet/libnet.a: libnet/microudp.c libnet/tftp.c
	$(MAKE) -C libnet/ USE_UIP=$(USE_UIP)

uip/libuip.a: 
	$(MAKE) -C uip/

iq/libiq.a:
	$(MAKE) -C iq/

main.o: main.c
	$(compile)

assets/ginkgo-logo.h: assets/ginkgo-logo.bmp
	$(GFXDIR)/tools/file2c/src/file2c -dcs -n ginkgo_logo assets/ginkgo-logo.bmp assets/ginkgo-logo.h
%.o: %.c assets/ginkgo-logo.h
	$(compile)

%.o: %.S
	$(assemble)

load: firmware.bin
	litex_term --kernel firmware.bin COM8

clean:
	$(RM) $(OBJECTS) $(OBJECTS:.o=.d) firmware.elf firmware.bin .*~ *~ uip/*.o uip/*.a libnet/*.o libnet/*.a assets/ginkgo-logo.h
	$(MAKE) -C uip/ clean
	$(MAKE) -C $(GFXDIR) clean

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
