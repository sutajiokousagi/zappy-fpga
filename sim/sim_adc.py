#!/usr/bin/env python3

import lxbuildenv_sim

# This variable defines all the external programs that this module
# relies on.  lxbuildenv reads this variable in order to ensure
# the build will finish without exiting due to missing third-party
# programs.
LX_DEPENDENCIES = ["riscv", "vivado"]

import sys

from migen import *

from litex.build.generic_platform import *
from litex.build.xilinx import XilinxPlatform

from litex.soc.integration.builder import *
from litex.soc.cores.clock import *

from gateware.adc121s101 import *
from migen.genlib.cdc import PulseSynchronizer

sim_config = {
    # freqs
    "input_clk_freq": 50e6,
    "sys_clk_freq": 100e6,
    "adc_clk_freq": 20e6,
}


_io = [
    ("clk", 0, Pins("X")),
    ("rst", 0, Pins("X")),
    ("adc", 0,
     Subsignal("cs_n", Pins("P12"), IOStandard("LVCMOS33")),
     Subsignal("dout", Pins("K4"), IOStandard("LVCMOS33")),
     Subsignal("sclk", Pins("P11"), IOStandard("LVCMOS33")),
     ),
]


class Platform(XilinxPlatform):
    def __init__(self):
        XilinxPlatform.__init__(self, "", _io)


class CRG(Module):
    def __init__(self, platform, core_config):
        # build a simulated PLL. You can add more pll.create_clkout() lines to add more clock frequencies as necessary
        self.clock_domains.cd_sys = ClockDomain()
        self.clock_domains.cd_adc = ClockDomain()
        
        self.submodules.pll = pll = S7MMCM()
        self.comb += pll.reset.eq(platform.request("rst"))
        pll.register_clkin(platform.request("clk"), sim_config["input_clk_freq"])
        pll.create_clkout(self.cd_sys, sim_config["sys_clk_freq"])
        pll.create_clkout(self.cd_adc, sim_config["adc_clk_freq"])


adc_vect = [0xaaa,0x0f0,0x000,0x1,0x800,0xFFF,0x000]

class SimpleSim(Module):
    def __init__(self, platform):
        # instantiate the clock module
        crg = CRG(platform, sim_config)
        self.submodules += crg


        # instantiate the DUT
        cs_n = Signal()
        migen_adc_dout = Signal()

        self.submodules.adc = Adc121s101(platform.request("adc"))
        self.comb += cs_n.eq(self.adc.cs_n)
        self.comb += self.adc.dout.eq(migen_adc_dout)

        # connect test vectors to DUT
        adc_val = Signal(12)
        next_adc_val = Signal(12)
        count = Signal(16)
        acquire_count = Signal(16)

        index = Signal(4) # tracks the index of the vector we are in
        self.sync.adc += [  # generate vector index based on count/multiply params
            If(cs_n == 1,
               count.eq(0),
               adc_val.eq(next_adc_val),
            ).Else(
               count.eq(count + 1),
               If((count >= 2) & (count < 14),
                 migen_adc_dout.eq(adc_val[11]),
                 adc_val.eq(Cat(0, adc_val[:11])),
               ).Else(
                 migen_adc_dout.eq(1)  # float high in this simulation, but should technically be "Z"
               ),
            ),
            If(count == 15,
               index.eq(index + 1)
            ),
        ]
        self.sync.adc += [
            If( acquire_count < 19,
                acquire_count.eq(acquire_count + 1),
            ).Else(
                acquire_count.eq(0)
            ),
            If( acquire_count == 1,
                self.adc.acquire.eq(1)
            ).Else(
                self.adc.acquire.eq(0)
            ),

            self.adc.ready.eq(1)
#            If( self.adc.valid,
#                self.adc.ready.eq(1)
#            ).Else(
#                self.adc.ready.eq(0)
#            )
        ]
        # now assign vector to DUT signal(s)
        for k, i in enumerate(adc_vect):
            self.sync.adc += [
                If( (count == 0) & (k == index),
                    next_adc_val.eq(i)
                )
            ]

def generate_top():
    platform = Platform()
    soc = SimpleSim(platform)
    platform.build(soc, build_dir="./run", run=False)  # run=False prevents synthesis from happening, but a top.v file gets kicked out

    
# this generates a test bench wrapper verilog file, needed by the xilinx tools
def generate_top_tb():
    f = open("run/top_tb.v", "w")
    f.write("""
`timescale 1ns/1ps

module top_tb();

reg clk;
initial clk = 1'b1;
always #10 clk = ~clk;

wire top_cs_n;
wire top_sclk;
wire top_dout;

top dut (
    .rst(1'b0),
    .clk(clk),
    .adc_cs_n(top_cs_n),
    .adc_dout(top_dout),
    .adc_sclk(top_sclk)
);

endmodule""")
    f.close()


# this ties it all together
def run_sim(gui=False):
    os.system("rm -rf run/xsim.dir")
    if sys.platform == "win32":
        call_cmd = "call "
    else:
        call_cmd = ""
    os.system(call_cmd + "cd run && xvlog ../glbl.v")
    os.system(call_cmd + "cd run && xvlog top.v -sv")
    os.system(call_cmd + "cd run && xvlog top_tb.v -sv ")
    os.system(call_cmd + "cd run && xelab -debug typical top_tb glbl -s top_tb_sim -L unisims_ver -L unimacro_ver -L SIMPRIM_VER -L secureip -L $xsimdir/xil_defaultlib -timescale 1ns/1ps")
    if gui:
        os.system(call_cmd + "cd run && xsim top_tb_sim -gui")
    else:
        os.system(call_cmd + "cd run && xsim top_tb_sim -runall")


def main():
    generate_top()
    generate_top_tb()
    run_sim(gui=True)


if __name__ == "__main__":
    main()
