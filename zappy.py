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

from migen.genlib.resetsync import AsyncResetSynchronizer

from litex.soc.interconnect.csr import *
from litex.soc.interconnect.csr_eventmanager import *

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
        Subsignal("col", Pins("K11" "B2" "E4" "B1" "C5" "D3" "A4" "D2" "B5" "E2" "B6" "J11"), IOStandard("LVCMOS33")),
        Subsignal("row", Pins("A3" "K12" "A2" "L13"), IOStandard("LVCMOS33")),
    ),
    ("drv_cap", 0, Pins("L12"), IOStandard("LVCMOS33")),
    ("drv_rdis", 0, Pins("A5"), IOStandard("LVCMOS33")),

    # other GPIOs
    ("blinkenlight0", 0, Pins("M13"), IOStandard("LVCMOS33")),
    ("blinkenlight1", 0, Pins("N14"), IOStandard("LVCMOS33")),
    ("blinkenlight2", 0, Pins("J1"), IOStandard("LVCMOS33")),

    ("noplate0", 0, Pins("H12"), IOStandard("LVCMOS33")),
    ("noplate1", 0, Pins("L14"), IOStandard("LVCMOS33")),
    ("noplate2", 0, Pins("M14"), IOStandard("LVCMOS33")),
    ("noplate3", 0, Pins("D4"), IOStandard("LVCMOS33")),

    ("fan_pwm", 0, Pins("E12"), IOStandard("LVCMOS33")),
    ("fan_tach", 0, Pins("D13"), IOStandard("LVCMOS33")),
    ("hv_engage", 0, Pins("F3"), IOStandard("LVCMOS33")),
    ("l25_open_dark_lv", 0, Pins("M12"), IOStandard("LVCMOS33")),
    ("l25_pos_dark_lv", 0, Pins("M11"), IOStandard("LVCMOS33")),
    ("mcu_int0", 0, Pins("D1"), IOStandard("LVCMOS33")),
    ("mcu_int1", 0, Pins("C1"), IOStandard("LVCMOS33")),
    ("mb_unplugged", 0, Pins("P10"), IOStandard("LVCMOS33")),
    ("mk_unplugged", 0, Pins("P3"), IOStandard("LVCMOS33")),
    ("buzzer_drv", 0, Pins("F4"), IOStandard("LVCMOS33")),

    # SPI to display
    ("oled", 0,
        Subsignal("cs_n", 0, Pins("G1"), IOStandard("LVCMOS33")),
        Subsignal("dc", 0, Pins("H2"), IOStandard("LVCMOS33")),
        Subsignal("res", 0, Pins("G4"), IOStandard("LVCMOS33")),
        Subsignal("sclk", 0, Pins("J2"), IOStandard("LVCMOS33")),
        Subsignal("sdin", 0, Pins("H1"), IOStandard("LVCMOS33")),
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

class Platform(XilinxPlatform):
    def __init__(self, toolchain="vivado", programmer="vivado"):
        part = "xc7s50ftgb196-2"
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
            Instance("BUFR", i_I=clk50, o_O=clk50_distbuf),
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

class ZappySoC(SoCCore):
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
    parser.add_argument(
        "-t", "--target", help="which FPGA environment to build for", choices=["zappy"], default="zappy"
    )
    args = parser.parse_args()

    platform = Platform()
    if args.target == "zappy":
        soc = ZappySoC(platform)
    builder = Builder(soc, output_dir="build", csr_csv="test/csr.csv")
    vns = builder.build()
    soc.do_exit(vns)

if __name__ == "__main__":
    main()
