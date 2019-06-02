#!/usr/bin/env python3
import time

from litex.tools.litex_client import RemoteClient

wb = RemoteClient()
wb.open()

#print("trigger_mem_full: ")
#t = wb.read(0xe000b838)
#print(t)

print("Temperature: ")
t = wb.read(0xe00038b0)
t <<= 8
t |= wb.read(0xe00038b4)
print(t * 503.975 / 4096 - 273.15, "C")

print("VCCint: ")
t = wb.read(0xe00038b8)
t <<= 8
t |= wb.read(0xe00038bc)
print(t / 0x555, "V")

print("VCCaux: ")
t = wb.read(0xe00038c0)
t <<= 8
t |= wb.read(0xe00038c4)
print(t / 0x555, "V")

print("VCCbram: ")
t = wb.read(0xe00038c8)
t <<= 8
t |= wb.read(0xe00038cc)
print(t / 0x555, "V")

wb.close()
