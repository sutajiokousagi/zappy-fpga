# Possible Targets:	all clean Debug cleanDebug Release cleanRelease

##############################################################################################
# Settings
#

# General settings
	# See $(GFXLIB)/tools/gmake_scripts/readme.txt for the list of variables
        OPT_MAKE_LIB                            = yes
	OPT_OS					= raw32
	OPT_THUMB				= no
	OPT_LINK_OPTIMIZE		        = no
	OPT_CPU					= vexriscv

# uGFX settings
	# See $(GFXLIB)/tools/gmake_scripts/library_ugfx.mk for the list of variables
	GFXLIB					= ./
	GFXBOARD				= zappy
	GFXDEMO					= 
	#GFXDRIVERS				=
	GFXSINGLEMAKE			= no


##############################################################################################
# Set these for your project
#

ZAPPY_DIR=/home/bunnie/code/zappy-fpga

PROJECT  = zappy
ARCH     = riscv64-unknown-elf-
SRCFLAGS = -ggdb -g3 -Os
CFLAGS   = 
CXXFLAGS = -fno-rtti
ASFLAGS  =
LDFLAGS  =
ARFLAGS  = --plugin /opt/riscv/libexec/gcc/riscv64-unknown-elf/8.2.0/liblto_plugin.so

SRC      = 

OBJS     =
DEFS     = GFX_OS_HEAP_SIZE=9216
#DEFS     = 
LIBS     =
INCPATH  = $(ZAPPY_DIR)/deps/litex/litex/soc/software/include/base $(ZAPPY_DIR)/deps/litex/litex/soc/software/include $(ZAPPY_DIR)/deps/litex/litex/soc/common $(ZAPPY_DIR)/build/software/include $(ZAPPY_DIR)/firmware $(ZAPPY_DIR)/firmware/riscv $(ZAPPY_DIR)/firmware/assets

LIBPATH  =
LDSCRIPT = 

##############################################################################################
# These should be at the end
#

include $(GFXLIB)/tools/gmake_scripts/library_ugfx.mk
include $(GFXLIB)/tools/gmake_scripts/os_$(OPT_OS).mk
include $(GFXLIB)/tools/gmake_scripts/compiler_gcc.mk
# *** EOF ***
