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

import lxbuildenv

import argparse

from migen import *
from litex.build.generic_platform import *
from litex.soc.integration.soc_core import *
from litex.build.xilinx import XilinxPlatform, VivadoProgrammer
from litex.soc.integration.builder import *

from litex.soc.cores import spi_flash
from litex.soc.cores import uart

from migen.genlib.resetsync import AsyncResetSynchronizer

from litex.soc.interconnect.csr import *
from litex.soc.interconnect.csr_eventmanager import *

from liteeth.common import *
from liteeth.phy.rmii import LiteEthPHYRMII
from liteeth.core import LiteEthUDPIPCore
from liteeth.core.mac import LiteEthMAC
from liteeth.frontend.etherbone import LiteEthEtherbone

from gateware import info
from gateware import led
from gateware.adc121s101 import Adc121s101_csr, Zappy_adc
from gateware.dac8560 import Dac8560_csr
from gateware.pwm import PWM
from gateware.zappy_i2c import ZappyI2C
from gateware.oled import OLED

_io = [
    # ADCs
    ("fadc", 0,
         Subsignal("cs_n", Pins("N11"), IOStandard("LVCMOS33")),
         Subsignal("dout", Pins("M4"), IOStandard("LVCMOS33")),
         Subsignal("sclk", Pins("N10"), IOStandard("LVCMOS33")),
     ),

    ("adc", 0,
        Subsignal("cs_n", Pins("P12"), IOStandard("LVCMOS33")),
        Subsignal("dout", Pins("K4"), IOStandard("LVCMOS33")),
        Subsignal("sclk", Pins("P11"), IOStandard("LVCMOS33")),
     ),

    ("vmon", 0,
        Subsignal("cs_n", Pins("K3"), IOStandard("LVCMOS33")),
        Subsignal("dout", Pins("L3"), IOStandard("LVCMOS33")),
        Subsignal("sclk", Pins("M3"), IOStandard("LVCMOS33")),
    ),

    ("imon", 0,
        Subsignal("cs_n", Pins("P2"), IOStandard("LVCMOS33")),
        Subsignal("dout", Pins("N1"), IOStandard("LVCMOS33")),
        Subsignal("sclk", Pins("M2"), IOStandard("LVCMOS33")),
    ),

    # DACs
    ("hvdac", 0,
       Subsignal("din", Pins("N4"), IOStandard("LVCMOS33")),
       Subsignal("sclk", Pins("P4"), IOStandard("LVCMOS33")),
       Subsignal("sync", Pins("P5"), IOStandard("LVCMOS33")),
    ),

    # I2C
    ("i2c", 0,
        Subsignal("alert", Pins("J12"), IOStandard("LVCMOS33")),  # wired to thermal sensors
        Subsignal("scl", Pins("H14"), IOStandard("LVCMOS33")),
        Subsignal("sda", Pins("H13"), IOStandard("LVCMOS33")),
     ),
    ("prox_int", 0, Pins("B3"), IOStandard("LVCMOS33")),

    # driver GPIOs
    ("driver", 0,
        Subsignal("col", Pins("K11", "B2", "E4", "B1", "C5", "D3", "A4", "D2", "B5", "E2", "B6", "J11"), IOStandard("LVCMOS33")),
        Subsignal("row", Pins("A3", "K12", "A2", "L13"), IOStandard("LVCMOS33")),
    ),
    ("drv_cap", 0, Pins("L12"), IOStandard("LVCMOS33")),
    ("drv_rdis", 0, Pins("A5"), IOStandard("LVCMOS33")),

    # other GPIOs
    ("blinkenlight", 0, Pins("M13", "N14"), IOStandard("LVCMOS33"), Misc("DRIVE=12")), # higher drive because of LEDs
    ("blinkenlight", 2, Pins("J1"), IOStandard("LVCMOS33"), Misc("DRIVE=12")),

    ("noplate", 0, Pins("H12"), IOStandard("LVCMOS33")),
    ("noplate", 1, Pins("L14"), IOStandard("LVCMOS33")),
    ("noplate", 2, Pins("M14"), IOStandard("LVCMOS33")),
    ("noplate", 3, Pins("D4"), IOStandard("LVCMOS33")),

    ("fan_pwm", 0, Pins("E12"), IOStandard("LVCMOS33")),
    ("fan_tach", 0, Pins("D13"), IOStandard("LVCMOS33")),
    ("hv_engage", 0, Pins("F3"), IOStandard("LVCMOS33"), Misc("DRIVE=12")),
    ("l25_open_dark_lv", 0, Pins("M12"), IOStandard("LVCMOS33")),
    ("l25_pos_dark_lv", 0, Pins("M11"), IOStandard("LVCMOS33")),
    ("mcu_int0", 0, Pins("D1"), IOStandard("LVCMOS33")),
    ("mcu_int1", 0, Pins("C1"), IOStandard("LVCMOS33")),
    ("mb_unplugged", 0, Pins("P10"), IOStandard("LVCMOS33")),
    ("mk_unplugged", 0, Pins("P3"), IOStandard("LVCMOS33")),
    ("buzzer_drv", 0, Pins("F4"), IOStandard("LVCMOS33")),

    # SPI to display
    ("oled", 0,
        Subsignal("cs_n", Pins("G1"), IOStandard("LVCMOS33"), Misc("DRIVE=4"), Misc("SLEW=SLOW")),
        Subsignal("dc", Pins("H2"), IOStandard("LVCMOS33"), Misc("DRIVE=4"), Misc("SLEW=SLOW")),
        Subsignal("res", Pins("G4"), IOStandard("LVCMOS33"), Misc("DRIVE=4"), Misc("SLEW=SLOW")),
        Subsignal("sclk", Pins("J2"), IOStandard("LVCMOS33"), Misc("DRIVE=4"), Misc("SLEW=SLOW")),
        Subsignal("sdin", Pins("H1"), IOStandard("LVCMOS33"), Misc("DRIVE=4"), Misc("SLEW=SLOW")),
    ),

    # SPI Flash
    ("spiflash_4x", 0,
          Subsignal("cs_n", Pins("C11")),
          Subsignal("dq", Pins("B11", "B12", "D10", "C10")),
          IOStandard("LVCMOS33")
    ),
    ("spiflash_1x", 0,
          Subsignal("cs_n", Pins("C11")),
          Subsignal("miso", Pins("B12")),
          Subsignal("mosi", Pins("B11")),
          Subsignal("wp", Pins("D10")),
          Subsignal("hold", Pins("C10")),
          IOStandard("LVCMOS33")
     ),
    # ("spinor_dqs", 0, Pins("G11"), IOStandard("LVCMOS33")),
    # ("spinor_ecsn", 0, Pins("B10"), IOStandard("LVCMOS33")),
    # ("spinor_io2", 0, Pins("D10"), IOStandard("LVCMOS33")),
    # ("spinor_io3", 0, Pins("C10"), IOStandard("LVCMOS33")),
    # ("spinor_io4", 0, Pins("A12"), IOStandard("LVCMOS33")),
    # ("spinor_io5", 0, Pins("A13"), IOStandard("LVCMOS33")),
    # ("spinor_io6", 0, Pins("B13"), IOStandard("LVCMOS33")),
    # ("spinor_io7", 0, Pins("B14"), IOStandard("LVCMOS33")),
    # ("spinor_sck", 0, Pins("A8"), IOStandard("LVCMOS33")),

    # Serial
    ("serial", 0,
         Subsignal("rx", Pins("C3")),
         Subsignal("tx", Pins("C4")),
         IOStandard("LVCMOS33")
    ),

    ("mot", 0,
        Subsignal("rx", Pins("G14"), IOStandard("LVCMOS33")),
        Subsignal("tx", Pins("F12"), IOStandard("LVCMOS33")),
    ),

    ("nuc", 0,
        Subsignal("rx", Pins("F1"), IOStandard("LVCMOS33")),
        Subsignal("tx", Pins("F2"), IOStandard("LVCMOS33")),
    ),

    # RMII PHY Pads
    ("rmii_eth_clocks", 0,
         Subsignal("ref_clk", Pins("H11"), IOStandard("LVCMOS33"))
     ),
    ("rmii_eth", 0,
         Subsignal("rst_n", Pins("P13"), IOStandard("LVCMOS33")),
         Subsignal("rx_data", Pins("D14 D12"), IOStandard("LVCMOS33")),
         Subsignal("crs_dv", Pins("E11"), IOStandard("LVCMOS33")),
         Subsignal("tx_en", Pins("F14"), IOStandard("LVCMOS33")),
         Subsignal("tx_data", Pins("J13 J14"), IOStandard("LVCMOS33")),
         Subsignal("mdc", Pins("C12"), IOStandard("LVCMOS33")),
         Subsignal("mdio", Pins("C14"), IOStandard("LVCMOS33")),
         Subsignal("rx_er", Pins("E13"), IOStandard("LVCMOS33")),
         Subsignal("int_n", Pins("F13"), IOStandard("LVCMOS33")),
     ),

    ("clk50", 0, Pins("F11"), IOStandard("LVCMOS33")),

    # ("adc_n", 0, Pins("H7"), IOStandard("LVCMOS33")),
    # ("adc_p", 0, Pins("G8"), IOStandard("LVCMOS33")),
    # ("jtag_tck", 0, Pins("A7"), IOStandard("LVCMOS33")),
    # ("jtag_tdi", 0, Pins("P7"), IOStandard("LVCMOS33")),
    # ("jtag_tdo", 0, Pins("P6"), IOStandard("LVCMOS33")),
    # ("jtag_tms", 0, Pins("M6"), IOStandard("LVCMOS33")),
    # ("fpga_done", 0, Pins("P9"), IOStandard("LVCMOS33")),
    # ("fpga_init", 0, Pins("P8"), IOStandard("LVCMOS33")),
    # ("tp_18_n", 0, Pins("L1"), IOStandard("LVCMOS33")),
    # ("tp_18_p", 0, Pins("M1"), IOStandard("LVCMOS33")),
    # ("tp_mrcc", 0, Pins("H3"), IOStandard("LVCMOS33")),
    # ("prog_b", 0, Pins("L7"), IOStandard("LVCMOS33")),

    # ("drv_col1", 0, Pins("K11"), IOStandard("LVCMOS33")),
    # ("drv_col2", 0, Pins("B2"), IOStandard("LVCMOS33")),
    # ("drv_col3", 0, Pins("E4"), IOStandard("LVCMOS33")),
    # ("drv_col4", 0, Pins("B1"), IOStandard("LVCMOS33")),
    # ("drv_col5", 0, Pins("C5"), IOStandard("LVCMOS33")),
    # ("drv_col6", 0, Pins("D3"), IOStandard("LVCMOS33")),
    # ("drv_col7", 0, Pins("A4"), IOStandard("LVCMOS33")),
    # ("drv_col8", 0, Pins("D2"), IOStandard("LVCMOS33")),
    # ("drv_col9", 0, Pins("B5"), IOStandard("LVCMOS33")),
    # ("drv_col10", 0, Pins("E2"), IOStandard("LVCMOS33")),
    # ("drv_col11", 0, Pins("B6"), IOStandard("LVCMOS33")),
    # ("drv_col12", 0, Pins("J11"), IOStandard("LVCMOS33")),
    # ("drv_row1", 0, Pins("A3"), IOStandard("LVCMOS33")),
    # ("drv_row2", 0, Pins("K12"), IOStandard("LVCMOS33")),
    # ("drv_row3", 0, Pins("A2"), IOStandard("LVCMOS33")),
    # ("drv_row4", 0, Pins("L13"), IOStandard("LVCMOS33")),
]

# subset of NeTV2 for testing of code before Zappy boards are back from fab
_io_netv2 = [
    ("clk50", 0, Pins("J19"), IOStandard("LVCMOS33")),

    ("blinkenlight", 0, Pins("M21"), IOStandard("LVCMOS33")),
    #("blinkenlight1", 0, Pins("N20"), IOStandard("LVCMOS33")),
    ("blinkenlight", 1, Pins("L21"), IOStandard("LVCMOS33")),
    #("fpga_led3", 0, Pins("AA21"), IOStandard("LVCMOS33")),
    ("blinkenlight", 2, Pins("R19"), IOStandard("LVCMOS33")),
    #("fpga_led5", 0, Pins("M16"), IOStandard("LVCMOS33")),
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
    def __init__(self, toolchain="vivado", programmer="vivado", board="zappy"):
        if board == "zappy":
            part = "xc7s50ftgb196-2"
            XilinxPlatform.__init__(self, part, _io,
                                    toolchain=toolchain)
        elif board == "netv2":
            part = "xc7a100t-fgg484-2"
            XilinxPlatform.__init__(self, part, _io_netv2,
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
        self.clock_domains.cd_eth = ClockDomain()
        self.clock_domains.cd_analog = ClockDomain()
        self.clock_domains.cd_adc = ClockDomain()

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
        pll_clk50 = Signal()
        clk50_distbuf = Signal()
        pll_analog = Signal()
        pll_adc = Signal()

        self.specials += [
            Instance("BUFR", i_I=clk50, o_O=clk50_distbuf),
            # this allows PLLs/MMCMEs to be placed anywhere and reference the input clock
        ]

        pll_fb_bufg = Signal()
        mmcm_drdy = Signal()
        pll_clkout6= Signal()
        self.specials += [
            Instance("MMCME2_ADV",
                     p_STARTUP_WAIT="FALSE", o_LOCKED=pll_locked,
                     p_BANDWIDTH="OPTIMIZED",

                     # VCO @ 800 MHz
                     p_REF_JITTER1=0.01, p_CLKIN1_PERIOD=(1 / refclk_freq) * 1e9,
                     p_CLKFBOUT_MULT_F=16, p_DIVCLK_DIVIDE=1,
                     i_CLKIN1=clk50_distbuf, i_CLKFBIN=pll_fb_bufg, o_CLKFBOUT=pll_fb,

                     # 100 MHz - sysclk
                     p_CLKOUT0_DIVIDE_F=8, p_CLKOUT0_PHASE=0.0,
                     o_CLKOUT0=pll_sys,

                     # 50 MHz - clk50 distribution buffer
                     p_CLKOUT1_DIVIDE=16, p_CLKOUT1_PHASE=0.0,
                     o_CLKOUT1=pll_clk50,

                     # 20 MHz - ADC clock
                     p_CLKOUT2_DIVIDE=40, p_CLKOUT2_PHASE=0.0,
                     o_CLKOUT2=pll_adc,

                     # Cascade to generate a slow clock for timing the ADC sampler subsystem
                     p_CLKOUT4_CASCADE="TRUE",
                     # 1 MHz @ CLKOUT4
                     p_CLKOUT4_DIVIDE=16, p_CLKOUT4_PHASE=0.0,
                     o_CLKOUT4=pll_analog,
                     # 16 MHz @ CLKOUT6 -> cascade to input of CLKOUT4 divider
                     p_CLKOUT6_DIVIDE=50, p_CLKOUT6_PHASE=0.0,
                     o_CLKOUT6=pll_clkout6,

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
            Instance("BUFG", i_I=pll_clk50, o_O=self.cd_eth.clk),
            Instance("BUFG", i_I=pll_adc, o_O=self.cd_adc.clk),
            Instance("BUFG", i_I=pll_analog, o_O=self.cd_analog.clk),

            AsyncResetSynchronizer(self.cd_sys, rst | ~pll_locked),
            AsyncResetSynchronizer(self.cd_eth, ~pll_locked | rst),
            AsyncResetSynchronizer(self.cd_analog, rst | ~pll_locked),
            AsyncResetSynchronizer(self.cd_adc, rst | ~pll_locked),
        ]
        self.sync += [
            If(self._mmcm_read.re | self._mmcm_write.re,
               self._mmcm_drdy.status.eq(0)
               ).Elif(mmcm_drdy,
                      self._mmcm_drdy.status.eq(1)
                      )
        ]


boot_offset = 0x1000000
bios_size = 0x5000

class ZappySoC(SoCCore):
    mem_map = {
        "spiflash": 0x20000000,  # (default shadow @0xa0000000)
        "ethmac":   0x30000000,  # (shadow @0xb0000000)
        "monitor":  0x50000000,  # was: memtest
    }
    mem_map.update(SoCCore.mem_map)

    def __init__(self, platform, spiflash="spiflash_1x", **kwargs):
        clk_freq = int(100e6)

#        self.add_constant("MAIN_RAM_BASE", "SRAM_BASE + 0x10000") # add extra boot memory testing/characterization features to BIOS image
#        kwargs['cpu_reset_address']=self.mem_map["spiflash"]+boot_offset
        SoCCore.__init__(self, platform, clk_freq,
                         integrated_rom_size=bios_size,
                         integrated_sram_size=0x4000,  # stack gets put here at runtime (16k stack)
                         integrated_main_ram_size=0x20000,  # code gets loaded here at runtime
                         ident="Zappy LiteX Base SoC",
                         cpu_type="vexriscv",
                         **kwargs)

        self.submodules.crg = CRG(platform)
        self.add_csr("crg")
        self.platform.add_period_constraint(self.crg.cd_sys.clk, 1e9/clk_freq)

        self.submodules.info = info.Info(platform, self.__class__.__name__)
        self.add_csr("info")
        self.submodules.led = led.ClassicLed(platform.request("blinkenlight", 0))
        self.add_csr("led")

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
            "spiflash", self.mem_map["spiflash"] | self.shadow_base, 512*1024*1024)

        self.flash_boot_address = 0x207b0000

        self.platform.add_platform_command(
            "create_clock -name clk50 -period 20.0 [get_nets clk50]")

        ### ETHERNET
        ethphy = LiteEthPHYRMII(platform.request("rmii_eth_clocks"),
                             platform.request("rmii_eth"))
        self.submodules.ethphy = ethphy = ClockDomainsRenamer("eth")(ethphy)
        self.add_csr("ethphy")
        self.submodules.ethmac = LiteEthMAC(phy=self.ethphy, dw=32,
            interface="wishbone", endianness=self.cpu.endianness)
        self.add_csr("ethmac")
        self.add_interrupt("ethmac")
        self.add_wb_slave(mem_decoder(self.mem_map["ethmac"]), self.ethmac.bus)
        self.add_memory_region("ethmac", self.mem_map["ethmac"] | self.shadow_base, 0x2000)


        self.platform.add_false_path_constraints(
            self.crg.cd_sys.clk,
            self.crg.cd_eth.clk
        )

        # sys led
        self.sys_led = Signal()
        self.comb += platform.request("blinkenlight", 2).eq(self.sys_led)
        sys_counter = Signal(32)
        self.sync += sys_counter.eq(sys_counter + 1)
        self.comb += self.sys_led.eq(sys_counter[26])

        # turn on the fan
        fan_pwm = Signal()
        self.comb += platform.request("fan_pwm", 0).eq(1)

        # add the buzzer, fixed at resonant frequency for loudest alert
        self.submodules.buzzpwm = PWM(platform.request("buzzer_drv", 0))
        self.add_csr("buzzpwm")

        # add I2C interface
        self.submodules.i2c = ZappyI2C(platform, platform.request("i2c", 0))
        self.add_csr("i2c")
        self.add_interrupt("i2c")

        # add OLED interface
        self.submodules.oled = OLED(platform.request("oled", 0))
        self.add_csr("oled")

        # add motor UART interface
        self.submodules.motor_phy = uart.RS232PHY(platform.request("mot", 0), clk_freq, 115200)
        self.submodules.motor = ResetInserter()(uart.UART(self.motor_phy))
        self.add_csr("motor_phy", allow_user_defined=True)
        self.add_csr("motor", allow_user_defined=True)
        self.add_interrupt("motor", allow_user_defined=True)

        # add zap monitoring interface
        memdepth = 16384
        self.submodules.monitor = Zappy_adc(platform.request("adc", 0), platform.request("fadc", 0), memdepth=memdepth)
        self.add_csr("monitor")
        self.add_wb_slave(mem_decoder(self.mem_map["monitor"]), self.monitor.bus)
        self.add_memory_region("monitor", self.mem_map["monitor"] | self.shadow_base, memdepth * 4) # because dw = 32
        self.add_interrupt("monitor")

        # these are primarily for testing at the moment
        self.submodules.hvdac = ClockDomainsRenamer({"dac":"adc"})( Dac8560_csr(platform.request("hvdac", 0)) )
        self.add_csr("hvdac")
        self.submodules.vmon = Adc121s101_csr(platform.request("vmon", 0))
        self.add_csr("vmon")
        self.submodules.imon = Adc121s101_csr(platform.request("imon", 0))
        self.add_csr("imon")
        self.submodules.hvengage = led.ClassicLed(platform.request("hv_engage", 0))
        self.add_csr("hvengage")

#        memdepth = 16384  # 8 RAMB36 per 8192 depth
#        self.submodules.memtest = Zappy_memtest(memdepth=memdepth)
#        self.add_csr("memtest")
#        self.add_wb_slave(mem_decoder(self.mem_map["memtest"]), self.memtest.bus)
#        self.add_memory_region("memtest", self.mem_map["memtest"] | self.shadow_base, memdepth * 4) # because dw = 32 here

        from litescope import LiteScopeAnalyzer

        analyzer_signals = [
            self.vmon.adc.dout,
            self.vmon.adc.cs_n,
            self.vmon.adc.dbg_sclk,
            self.vmon.acquire.storage,
            self.vmon.data.status,
            self.vmon.valid.status,
        ]
        # WHEN NOT USING ANALYZER, COMMENT OUT FOR FASTER COMPILE TIMES
        self.submodules.analyzer = LiteScopeAnalyzer(analyzer_signals, 256, clock_domain="sys")
        self.add_csr("analyzer")
    def do_exit(self, vns):
        self.analyzer.export_csv(vns, "test/analyzer.csv")

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
    parser.add_argument(
        "-t", "--target", help="which FPGA board to build for", choices=["zappy", "netv2"], default="zappy"
    )
    args = parser.parse_args()

    if args.target == "zappy":
        platform = Platform()
    elif args.target == "netv2":
        platform = Platform(board="netv2")
    else:
        exit(1)

    soc = ZappySoC(platform)
    builder = Builder(soc, output_dir="build", csr_csv="test/csr.csv")
    vns = builder.build()
    soc.do_exit(vns)

if __name__ == "__main__":
    main()
