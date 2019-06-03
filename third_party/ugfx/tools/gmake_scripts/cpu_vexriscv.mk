#
# This file is subject to the terms of the GFX License. If a copy of
# the license was not distributed with this file, you can obtain one at:
#
#             http://ugfx.io/license.html
#

#
# See readme.txt for the make API
#

# Requirements:
#
# NONE
#

SRCFLAGS += -falign-functions=16 -std=gnu99 -MD -MP -march=rv32im -mabi=ilp32 -fomit-frame-pointer -Wall -fno-builtin -nostdinc  -fexceptions -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wno-char-subscripts -fno-strict-aliasing -fpack-struct
LDFLAGS  += 
DEFS     += -D__vexriscv__ 
