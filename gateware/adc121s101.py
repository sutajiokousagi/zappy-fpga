from migen import *

from litex.soc.interconnect.csr import *
from migen.genlib.cdc import MultiReg

# adc module just drives the ADC121S101 ADC:
#   self.*acquire* Signal() - INPUT on rising edge starts an acquisition cycle. May be asynchronous to current domain.
#   self.*ready* Signal() - INPUT indicates the receiving side is reading out self.data
#   self.*data* Signal(12) - OUTPUT 12-bit value with the latest captured data
#   self.*valid* Signal() - OUTPUT self.valid - high when an unread, valid output is present (drops after self.ready goes high, synchronous to "adc" clock)
#   "adc" Clock (implicit) - CLOCK tie clock to the "adc" domain - should be at 20MHz
#   pads PadGroup - PARAMETER parameter string for ADC. Expects "cs_n" (output), "dout" (input), "sclk" (output) in pad group.
#
#   Clock domain is not left as "sys" because it is UNSAFE for external hardware for module to bind to "sys"
class Adc121s101(Module):
    def __init__(self, pads):
        self.acquire = Signal()
        self.ready = Signal()
        self.data = Signal(12)
        self.valid = Signal()
        # implicit clock domains: "adc"

        self.cs_n = cs_n = getattr(pads, "cs_n")
        self.dout = dout = getattr(pads, "dout")

        start = Signal()
        self.specials += MultiReg(self.acquire, start, "adc")
        start_r = Signal()
        self.sync.adc += start_r.eq(start)
        go = Signal()
        self.comb += go.eq(start & ~start_r)

        fsm = FSM(reset_state="IDLE")
        fsm = ClockDomainsRenamer("adc")(fsm)
        self.submodules += fsm
        count = Signal(5)
        fsm.act("IDLE",
                NextValue(cs_n, 1),
                NextValue(count, 0),
                NextValue(self.valid, 0),
                If(go,
                   NextState("ACQUIRE")
                )
        )
        fsm.act("ACQUIRE",
                NextValue(cs_n, 0),
                NextValue(count, count + 1),
                If((count >= 4) & (count < 15),
                   NextValue(self.data, Cat(dout,self.data[:11])),
                ).Elif(count >= 15,
                   NextState("VALID"),
                   NextValue(self.data, Cat(dout, self.data[:11])),
                   NextValue(self.valid, 1),
                )
        )
        fsm.act("VALID",
                NextValue(cs_n, 1),
                If(self.ready,
                   NextState("IDLE"),
                   NextValue(self.valid, 0),
                ).Else(
                   NextValue(self.valid, 1),
                )
        )

        # generate a clock
        sclk = getattr(pads, "sclk")
        # mirror the clock with zero delay
        self.specials += [
            Instance("ODDR2",
                     p_DDR_ALIGNMENT="NONE",
                     p_INIT="0",
                     p_SRTYPE="SYNC",

                     o_Q=sclk,
                     i_C0=ClockSignal("adc"),
                     i_C1=~ClockSignal("adc"),
                     i_D0=1,
                     i_D1=0,
                     i_R=ResetSignal("adc"),
                     i_S=0,
            ),
        ]

# CSR wrapper for the Adc12s101 module
#   pads PadGroup - see Adc12s101 for spec
#   acquire CSR (wo) - writing 1 to this bit triggers an acquisition. Self-clearing.
#   data CSR (ro, 12) - result of most recent acquisition, 12 bits wide
#   valid CSR (ro) - if data is valid, 1. if not, 0. Is cleared immediately upon acquire being set.
class Adc121s101_csr(Module, AutoCSR):
    def __init__(self, pads):
        self.submodules.adc = Adc121s101(pads)
        self.acquire = CSRStorage(1)
        self.data = CSRStatus(12)
        self.valid = CSRStatus(1)
