include ../../build/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

CC=riscv64-unknown-elf-g++
CFLAGS=-std=gnu++0x
INCLUDES=-Iiq-module-communication-cpp/src -Iiq-module-communication-cpp/inc

OBJS=iq-module-communication-cpp/src/generic_interface.o iq-module-communication-cpp/inc/client_communication.o iq-module-communication-cpp/src/byte_queue.o iq-module-communication-cpp/src/crc_helper.o iq-module-communication-cpp/src/packet_finder.o iqmotor.o

default: libiq.a

# How to build an object file (x.o) from a corresponding C source file (x.c)
.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c -o $@ $<

# How to build an object file from a corresponding C++ source file (x.cpp)
.cpp.o:
	$(CXX) $(INCLUDES) $(CFLAGS) -c -o $@ $<

libiq.a: $(OBJS) 
	$(AR) clr libiq.a $(OBJS)

.PHONY: clean
clean:
	rm -r *.o
	rm -r *.a
