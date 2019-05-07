from migen import *

from litex.soc.interconnect.csr import *
from migen.genlib.cdc import MultiReg, BusSynchronizer

from litex.soc.interconnect import wishbone

# adc module just drives the Dac8560 DAC:
#   self.*ready* `Signal()` - OUTPUT indicates block is ready to receive new data
#   self.*data* `Signal(16)` - INPUT 16-bit value with the data to output
#   self.*valid* `Signal()` - INPUT on rising edge, data presented on data is sent to DAC. Rising edge only detected during ready asserted.
#   "dac" `Clock` (implicit) - CLOCK tie clock to the "dac" domain - maximum 20MHz @ 3.3V
#   pads PadGroup - PARAMETER parameter string for DAC. Expects "sync" (output), "din" (output), "sclk" (output) in pad group.
#
#   Clock domain is not left as "sys" because it is UNSAFE for external hardware for module to accidentally bind to "sys"
class Dac8560(Module):
    def __init__(self, pads):
        self.ready = Signal()
        self.data = Signal(16)
        self.valid = Signal()
        # implicit clock domains: "dac"

        self.sync_n = sync_n = getattr(pads, "sync")
        self.din = din = getattr(pads, "din")

        # derive a rising edge pulse from the valid signal
        start = Signal()
        start_r = Signal()
        self.sync.dac += start_r.eq(start)
        go = Signal()
        self.comb += go.eq(start & ~start_r)

        # sync data and valid together through a single BusSynchronizer
        dout = Signal(16)
        self.submodules.dout_sync = BusSynchronizer(17, "sys", "dac")
        self.comb += [
            self.dout_sync.i.eq(Cat(self.data, self.valid)),
            dout.eq(self.dout_sync.o[:16]),
            start.eq(self.dout_sync.o[16])
        ]

        dout_shift = Signal(16)
        fsm = FSM(reset_state="IDLE")
        fsm = ClockDomainsRenamer("dac")(fsm)
        self.submodules += fsm
        count = Signal(6)
        fsm.act("IDLE",
                NextValue(sync_n, 1),
                NextValue(count, 0),
                NextValue(self.din, 0),
                NextValue(dout_shift, dout),
                If(go,
                   NextState("TX"),
                   NextValue(self.ready, 0),
                ).Else(
                   NextValue(self.ready, 1),
                )
        )
        fsm.act("TX",
                NextValue(sync_n, 0),
                NextValue(count, count + 1),
                If((count >= 8) & (count < 24),
                   NextValue(din, dout_shift[15]),
                   NextValue(dout_shift, Cat(0,dout_shift[:15])),
                   NextValue(self.ready, 0),
                   NextValue(sync_n, 0),
                ).Elif(count >= 24,
                   NextState("IDLE"),
                   NextValue(din, 0),
                   NextValue(sync_n, 1),
                   NextValue(self.ready, 1),
                ).Else(
                   NextValue(sync_n, 0),
                   NextValue(din, 0),
                   NextValue(self.ready, 0),
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
                     i_C0=ClockSignal("dac"),
                     i_C1=~ClockSignal("dac"),
                     i_D0=1,
                     i_D1=0,
                     i_R=ResetSignal("dac"),
                     i_S=0,
            ),
        ]

# CSR wrapper for the Dac8560 module
#   pads PadGroup - see Dac8560 for spec
#   CSR update (wo) - writing anything to this bit triggers an update
#   CSR data (wo, 16) - data to send
#   CSR ready (ro) - If module is ready to accept data, 1. Otherwise sender should wait or else update will be ignored.
#
#   Code loop should: 1) update data, 2) wait until ready is set, 3) trigger update
#   Note that if DAC clock is  max 20MHz by the spec sheet. Code assumes sysclk is around 100-150 MHz.
class Dac8560_csr(Module, AutoCSR):
    def __init__(self, pads):
        self.submodules.dac = Dac8560(pads)
        self.update = CSRStorage(1)
        self.data = CSRStorage(16)
        self.ready = CSRStatus(1)

        self.specials += MultiReg(self.dac.ready, self.ready.status)

        # extend CSR update single-cycle pulse to several cycles so DAC is sure to see it (running in a slower domain)
        trigger = Signal()
        self.comb += self.dac.valid.eq(trigger)
        count = Signal(5)
        fsm = FSM(reset_state="IDLE")
        self.submodules += fsm
        fsm.act("IDLE",
                NextValue(count, 0),
                If(self.update.re,
                   NextState("TRIGGER"),
                   NextValue(trigger, 1),
                ).Else(
                   NextValue(trigger, 0),
                )
        )
        fsm.act("TRIGGER",
                NextValue(count, count + 1),
                If(count >= 15,  # assert acquire pulse long enough so DAC block (at 5x-15x slower clock) is sure to get it
                   NextState("IDLE"),
                   NextValue(trigger, 0),
                ).Else(
                   NextValue(trigger, 1),
                )
        )
