#!/usr/bin/env python3

# lxbuildenv must be imported first, because it has a chance to re-exec Python.
# This module ensures the dependencies are present and the environment variables
# are all set appropriately.  Additionally, the PATH will include toolchains,
# and all dependencies get verified.
import lxbuildenv

# This variable defines all the external programs that this module
# relies on.  lxbuildenv reads this variable in order to ensure
# the build will finish without exiting due to missing third-party
# programs.
LX_DEPENDENCIES = ["riscv", "vivado"]

import sys
import os
import argparse

from migen import *
from migen.genlib.resetsync import AsyncResetSynchronizer

from litex.build.generic_platform import *
from litex.build.xilinx import XilinxPlatform, VivadoProgrammer

from litex.soc.integration.soc_sdram import *
from litex.soc.integration.builder import *
from litex.soc.cores import dna, xadc
from litex.soc.cores.frequency_meter import FrequencyMeter

from litedram.modules import K4B2G1646FBCK0
from litedram.phy import a7ddrphy
from litedram.core import ControllerSettings

from litevideo.input import HDMIIn
from litevideo.output.hdmi.s7 import S7HDMIOutEncoderSerializer, S7HDMIOutPHY

from litevideo.output.common import *
from litevideo.output.core import VideoOutCore
from litevideo.output.hdmi.encoder import Encoder

from litex.soc.interconnect.csr import *
from litex.soc.interconnect.csr_eventmanager import *

from liteeth.common import *

from migen.genlib.cdc import MultiReg

from litex.soc.cores import spi_flash
from litex.soc.integration.soc_core import mem_decoder

_io = [
    ("clk50", 0, Pins("J19"), IOStandard("LVCMOS33")),

    ("fpga_led0", 0, Pins("M21"), IOStandard("LVCMOS33")),
    ("fpga_led1", 0, Pins("N20"), IOStandard("LVCMOS33")),
    ("fpga_led2", 0, Pins("L21"), IOStandard("LVCMOS33")),
    ("fpga_led3", 0, Pins("AA21"), IOStandard("LVCMOS33")),
    ("fpga_led4", 0, Pins("R19"), IOStandard("LVCMOS33")),
    ("fpga_led5", 0, Pins("M16"), IOStandard("LVCMOS33")),
    ("fan_pwm", 0, Pins("L14"), IOStandard("LVCMOS33")),

    ("serial", 0,
        Subsignal("tx", Pins("E14")),
        Subsignal("rx", Pins("E13")),
        IOStandard("LVCMOS33"),
    ),


    # RMII PHY Pads
    ("rmii_eth_clocks", 0,
     Subsignal("ref_clk", Pins("D17"), IOStandard("LVCMOS33"))
     ),
    ("rmii_eth", 0,
     Subsignal("rst_n", Pins("F16"), IOStandard("LVCMOS33")),
     Subsignal("rx_data", Pins("A20 B18"), IOStandard("LVCMOS33")),
     Subsignal("crs_dv", Pins("C20"), IOStandard("LVCMOS33")),
     Subsignal("tx_en", Pins("A19"), IOStandard("LVCMOS33")),
     Subsignal("tx_data", Pins("C18 C19"), IOStandard("LVCMOS33")),
     Subsignal("mdc", Pins("F14"), IOStandard("LVCMOS33")),
     Subsignal("mdio", Pins("F13"), IOStandard("LVCMOS33")),
     Subsignal("rx_er", Pins("B20"), IOStandard("LVCMOS33")),
     Subsignal("int_n", Pins("D21"), IOStandard("LVCMOS33")),
     ),

    # SPI Flash
    ("spiflash_4x", 0,  # clock needs to be accessed through STARTUPE2
     Subsignal("cs_n", Pins("T19")),
     Subsignal("dq", Pins("P22", "R22", "P21", "R21")),
     IOStandard("LVCMOS33")
     ),
    ("spiflash_1x", 0,  # clock needs to be accessed through STARTUPE2
     Subsignal("cs_n", Pins("T19")),
     Subsignal("mosi", Pins("P22")),
     Subsignal("miso", Pins("R22")),
     Subsignal("wp", Pins("P21")),
     Subsignal("hold", Pins("R21")),
     IOStandard("LVCMOS33")
     ),
]


class Platform(XilinxPlatform):
    def __init__(self, toolchain="vivado", programmer="vivado"):
        part = "xc7s50-ftbg196-2c"
        XilinxPlatform.__init__(self, part, _io,
                                toolchain=toolchain)

        # NOTE: to do quad-SPI mode, the QE bit has to be set in the SPINOR status register
        # OpenOCD won't do this natively, have to find a work-around (like using iMPACT to set it once)
        self.add_platform_command(
            "set_property CONFIG_VOLTAGE 3.3 [current_design]")
        self.add_platform_command(
            "set_property CFGBVS VCCO [current_design]")
        self.add_platform_command(
            "set_property BITSTREAM.CONFIG.CONFIGRATE 66 [current_design]")
        self.add_platform_command(
            "set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 2 [current_design]")
        self.toolchain.bitstream_commands = [
            "set_property CONFIG_VOLTAGE 3.3 [current_design]",
            "set_property CFGBVS VCCO [current_design]",
            "set_property BITSTREAM.CONFIG.CONFIGRATE 66 [current_design]",
            "set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 2 [current_design]",
        ]
        self.toolchain.additional_commands = \
            ["write_cfgmem -verbose -force -format bin -interface spix2 -size 64 "
             "-loadbit \"up 0x0 {build_name}.bit\" -file {build_name}.bin"]
        self.programmer = programmer

    def create_programmer(self):
        if self.programmer == "vivado":
            return VivadoProgrammer(flash_part="n25q128-3.3v-spi-x1_x2_x4")
        else:
            raise ValueError("{} programmer is not supported"
                             .format(self.programmer))

    def do_finalize(self, fragment):
        XilinxPlatform.do_finalize(self, fragment)

class CRG(Module, AutoCSR):
    def __init__(self, platform):
        refclk_freq = 50e6

        clk50 = platform.request("clk50")
        rst = Signal()
        self.clock_domains.cd_sys = ClockDomain()

    # DRP
    self._mmcm_read = CSR()
    self._mmcm_write = CSR()
    self._mmcm_drdy = CSRStatus()
    self._mmcm_adr = CSRStorage(7)
    self._mmcm_dat_w = CSRStorage(16)
    self._mmcm_dat_r = CSRStatus(16)

    pll_locked = Signal()
    pll_fb = Signal()
    pll_sys = Signal()
    clk50_distbuf = Signal()

    self.specials += [
        Instance("BUFG", i_I=clk50, o_O=clk50_distbuf),
        # this allows PLLs/MMCMEs to be placed anywhere and reference the input clock
    ]

    pll_fb_bufg = Signal()
    mmcm_drdy = Signal()
    self.specials += [
        Instance("MMCME2_ADV",
                 p_STARTUP_WAIT="FALSE", o_LOCKED=pll_locked,
                 p_BANDWIDTH="OPTIMIZED",

                 # VCO @ 800 MHz or 600 MHz
                 p_REF_JITTER1=0.01, p_CLKIN1_PERIOD=(1 / refclk_freq) * 1e9,
                 p_CLKFBOUT_MULT_F=16, p_DIVCLK_DIVIDE=1,
                 i_CLKIN1=clk50_distbuf, i_CLKFBIN=pll_fb_bufg, o_CLKFBOUT=pll_fb,

                 # 100 MHz - sysclk
                 p_CLKOUT0_DIVIDE_F=8, p_CLKOUT0_PHASE=0.0,
                 o_CLKOUT0=pll_sys,

                 # DRP
                 i_DCLK=ClockSignal(),
                 i_DWE=self._mmcm_write.re,
                 i_DEN=self._mmcm_read.re | self._mmcm_write.re,
                 o_DRDY=mmcm_drdy,
                 i_DADDR=self._mmcm_adr.storage,
                 i_DI=self._mmcm_dat_w.storage,
                 o_DO=self._mmcm_dat_r.status
                 ),

        # feedback delay compensation buffers
        Instance("BUFG", i_I=pll_fb, o_O=pll_fb_bufg),

        # global distribution buffers
        Instance("BUFG", i_I=pll_sys, o_O=self.cd_sys.clk),

        AsyncResetSynchronizer(self.cd_sys, rst | ~pll_locked),
    ]
    self.sync += [
        If(self._mmcm_read.re | self._mmcm_write.re,
           self._mmcm_drdy.status.eq(0)
           ).Elif(mmcm_drdy,
                  self._mmcm_drdy.status.eq(1)
                  )
    ]

boot_offset = 0x1000000
bios_size = 0x8000

class BaseSoC(SoCCore):
    csr_peripherals = [
        "dna",
        "xadc",
        "cpu_or_bridge",
    ]
    csr_map_update(SoCCore.csr_map, csr_peripherals)

    mem_map = {
        "spiflash": 0x20000000,  # (default shadow @0xa0000000)
    }
    mem_map.update(SoCCore.mem_map)

    def __init__(self, platform, spiflash="spiflash_1x", **kwargs):
        clk_freq = int(100e6)

        kwargs['cpu_reset_address']=self.mem_map["spiflash"]+boot_offset
        SoCCore.__init__(self, platform, clk_freq,
                         integrated_rom_size=bios_size,
                         integrated_sram_size=0x20000,
                         ident="Zappy LiteX Base SoC",
                         reserve_nmi_interrupt=False,
                         cpu_type="vexriscv",
                         **kwargs)

        self.submodules.crg = CRG(platform)
        self.platform.add_period_constraint(self.crg.cd_sys.clk, 1e9/clk_freq)

        # spi flash
        spiflash_pads = platform.request(spiflash)
        spiflash_pads.clk = Signal()
        self.specials += Instance("STARTUPE2",
                                  i_CLK=0, i_GSR=0, i_GTS=0, i_KEYCLEARB=0, i_PACK=0,
                                  i_USRCCLKO=spiflash_pads.clk, i_USRCCLKTS=0, i_USRDONEO=1, i_USRDONETS=1)
        spiflash_dummy = {
            "spiflash_1x": 8,  # this is specific to the device populated on the board -- if it changes, must be updated
            "spiflash_4x": 12, # this is almost certainly wrong
        }
        self.submodules.spiflash = spi_flash.SpiFlash(
                spiflash_pads,
                dummy=spiflash_dummy[spiflash],
                div=2)
        self.add_constant("SPIFLASH_PAGE_SIZE", 256)
        self.add_constant("SPIFLASH_SECTOR_SIZE", 0x10000)
        self.add_wb_slave(mem_decoder(self.mem_map["spiflash"]), self.spiflash.bus)
        self.add_memory_region(
            "spiflash", self.mem_map["spiflash"] | self.shadow_base, 8*1024*1024)

        self.flash_boot_address = 0x207b0000  # TODO: this is from XC100T

        self.platform.add_platform_command(
            "create_clock -name clk50 -period 20.0 [get_nets clk50]")


"""
        # SPI for flash already added above
        # SPI for ADC
        self.submodules.spi = SPIMaster(platform.request("spi"))
        # SPI for DAC
        self.submodules.spi_dac = SPIMaster(platform.request("spi_dac"))

        # GPIO
        self.submodules.gi = GPIOIn(platform.request("gpio_in", 0).gi)
        self.submodules.go = GPIOOut(platform.request("gpio_out", 0).go)

        # An external interrupt source
        self.submodules.ev = EventManager()
        self.ev.my_int1 = EventSourceProcess()
        self.ev.finalize()

        self.comb += self.ev.my_int1.trigger.eq(platform.request("int", 0).int)

"""
        


def main():
    if os.environ['PYTHONHASHSEED'] != "1":
        print( "PYTHONHASHEED must be set to 1 for consistent validation results. Failing to set this results in non-deterministic compilation results")
        exit()

    parser = argparse.ArgumentParser(description="Build Zappy bitstream and firmware")
    args = parser.parse_args()

    platform = Platform(part=args.part)
    if args.target == "zappy":
        soc = ZappySoC(platform)
    builder = Builder(soc, output_dir="build", csr_csv="test/csr.csv")
    vns = builder.build()
    soc.do_exit(vns)

if __name__ == "__main__":
    main()
