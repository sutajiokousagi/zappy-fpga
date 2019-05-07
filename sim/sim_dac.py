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

from gateware.dac8560 import *
from migen.genlib.cdc import PulseSynchronizer

sim_config = {
    # freqs
    "input_clk_freq": 50e6,
    "sys_clk_freq": 100e6,
    "dac_clk_freq": 20e6,
}


_io = [
    ("clk", 0, Pins("X")),
    ("rst", 0, Pins("X")),
    ("dac", 0,
       Subsignal("din", Pins("N4"), IOStandard("LVCMOS33")),
       Subsignal("sclk", Pins("P4"), IOStandard("LVCMOS33")),
       Subsignal("sync", Pins("P5"), IOStandard("LVCMOS33")),
    ),
]


class Platform(XilinxPlatform):
    def __init__(self):
        XilinxPlatform.__init__(self, "", _io)


class CRG(Module):
    def __init__(self, platform, core_config):
        # build a simulated PLL. You can add more pll.create_clkout() lines to add more clock frequencies as necessary
        self.clock_domains.cd_sys = ClockDomain()
        self.clock_domains.cd_dac = ClockDomain()
        
        self.submodules.pll = pll = S7MMCM()
        self.comb += pll.reset.eq(platform.request("rst"))
        pll.register_clkin(platform.request("clk"), sim_config["input_clk_freq"])
        pll.create_clkout(self.cd_sys, sim_config["sys_clk_freq"])
        pll.create_clkout(self.cd_dac, sim_config["dac_clk_freq"])


dac_vect = [0xaaaa,0x00f0,0x0000,0x1,0x8000,0xFFFF,0xC351]

class SimpleSim(Module):
    def __init__(self, platform):
        # instantiate the clock module
        crg = CRG(platform, sim_config)
        self.submodules += crg

        # instantiate the DUT
        ready = Signal()
        data = Signal(16)
        valid = Signal()

        self.submodules.dac = Dac8560(platform.request("dac"))
        self.comb += ready.eq(self.dac.ready)
        self.comb += self.dac.valid.eq(valid)
        self.comb += self.dac.data.eq(data)

        # connect test vectors to DUT
        next_dac_val = Signal(16)

        self.sync += data.eq(next_dac_val)
        index = Signal(4) # tracks the index of the vector we are in
        count = Signal(4)

        fsm = FSM(reset_state="IDLE")
        self.submodules += fsm
        fsm.act("IDLE",
                If( ready,
                    NextState("WAIT_READY"),
                    valid.eq(1),
                    NextValue(count, 0),
                )
        )
        fsm.act("WAIT_READY",
                valid.eq(1),
                If( ~ready,
                    NextState("ADVANCE"),
                )
        )
        fsm.act("ADVANCE",
                NextValue(count, count + 1),
                If( count == 4,
                    NextState("IDLE"),
                    NextValue(index, index + 1)
                )
        )
        
        # now assign vector to DUT signal(s)
        for k, i in enumerate(dac_vect):
            self.sync += [
                If( k == index,
                    next_dac_val.eq(i)
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

wire top_sync;
wire top_sclk;
wire top_din;

top dut (
    .rst(1'b0),
    .clk(clk),
    .dac_sync(top_sync),
    .dac_din(top_din),
    .dac_sclk(top_sclk)
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
