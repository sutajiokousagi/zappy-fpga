from migen import *

from litex.soc.interconnect.csr import *
from migen.genlib.cdc import MultiReg

from litex.soc.interconnect import wishbone

# adc module just drives the ADC121S101 ADC:
#   self.*acquire* `Signal()` - INPUT on rising edge starts an acquisition cycle. May be asynchronous to current domain.
#   self.*ready* `Signal()` - INPUT indicates the receiving side is reading out self.data
#   self.*data* `Signal(12)` - OUTPUT 12-bit value with the latest captured data
#   self.*valid* `Signal()` - OUTPUT high when an unread, valid output is present (drops after self.ready goes high, synchronous to "adc" clock)
#   "adc" `Clock` (implicit) - CLOCK tie clock to the "adc" domain - should be at 20MHz
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

# CSR wrapper for the Adc121s101 module
#   pads PadGroup - see Adc121s101 for spec
#   CSR acquire (wo) - writing anything to this bit triggers an acquisition. Ignores acquire when valid is not set.
#   CSR data (ro, 12) - result of most recent acquisition, 12 bits wide
#   CSR valid (ro) - if data is valid, 1. if not, 0. Is cleared immediately upon acquire being set.
#
#   Code loop should: 1) wait until valid is set, 2) trigger acquire, 3) wait until valid is set, 4) read out data
#   Note that if ADC clock is nominally 10-20MHz by the spec sheet. Code assumes sysclk is around 100-150 MHz.
class Adc121s101_csr(Module, AutoCSR):
    def __init__(self, pads):
        self.submodules.adc = Adc121s101(pads)
        self.acquire = CSRStorage(1)
        self.data = CSRStatus(12)
        self.valid = CSRStatus(1)

        adc_valid_sync = Signal()
        self.specials += MultiReg(self.adc.valid, adc_valid_sync) # bring valid into the sysclk domain, adds some latency

        count = Signal(5)
        fsm = FSM(reset_state="IDLE")
        self.submodules += fsm
        fsm.act("IDLE",
                NextValue(count, 0),
                If(self.acquire.re,
                   NextState("TRIGGER"),
                   NextValue(self.valid.status, 0)
                ).Else(
                   NextValue(self.valid.status, 1)  # so that on initial power-on valid is set, to indicate we can accept acquire
                )
        )
        fsm.act("TRIGGER",
                NextValue(count, count + 1),
                self.adc.acquire.eq(1),
                If(count >= 15,  # assert acquire pulse long enough so ADC block (at 5x-15x slower clock) is sure to get it
                   NextState("WAIT"),
                   NextValue(self.adc.ready, 1)
                )
        )
        fsm.act("WAIT",
                If(adc_valid_sync,
                   NextValue(self.data.status, self.adc.data),
                   NextValue(self.adc.ready, 0),
                   NextValue(self.valid.status, 1),
                   NextState("IDLE")
                )
        )

# FIFO wrapper for the Adc121s101 module
#   hvmain_pads PadGroup - see Adc121s101 for spec; ADC connected to HV main
#   cap_pads PadGroup - see Adc121s101 for spec; ADC connected to storage cap
#   vmon_pads PadGroup - see Adc121s101 for spec; ADC connected to HV supply vmon
#   CSR hvmax (wo, 16) - maximum HV voltage, in volts, before safeties are triggered
#   self.*hvengage* Signal() - OUTPUT when high connects MK HV supply to HV main

#   CSR acquire (wo) - writing anything to this CSR triggers a sample run acquisition of depth samples
#   CSR depth (wo, 16) - depth of samples to acquire into buffer
#   CSR done (ro) - high when acquisition is finished
#   CSR clear (wo) - reset the acquisition FIFO to zero, regardless of state
#   CSR mode (wo) - selects acquisition mode: 0 for single-shot, 1 for sample run
#   CSR trigger (wo) - fires a single shot acquisition
#   CSR data (ro, 12) - the current data
#   CSR valid (ro) - if data is valid, 1. if not, 0. Cleared immediately upon trigger being set

#   REFACTOR THIS to target the ethernet TFTP transfer routine cleanly
#   MEMORY
#class Zappy_adc(Module, AutoCSR):
#    def __init__(self, pads):
#        self.submodules.adc = Adc121s101(pads)


#  CSR update (wo) - writing anything triggers memory values to update
#  CSR count (wo, 32) - number of data words (32-bits wide) to update
#  CSR seed (wo, 32) - 32-bit seed number for updating
#  CSR done (ro) - 1 when updating is done, 0 when update is running
#  memdepth integer (default 2048) - depth in 32-bit words of the memory to allocate
#  self.*mem* `Memory()` - memdepthx32 memory, one local write-only port allocated, one wishbone read-only port allocated
class Zappy_memtest(Module, AutoCSR):
    def __init__(self, memdepth=2048):
        mem = Memory(32, memdepth)  # looks like memdepth is in bytes, not dw words
        port = mem.get_port(write_capable=True)
        # self.specials.mem = mem  # if you do this, it creates an autoCSR of <instancename>_mem where you can access the memory
        # but, you can only make a read port to access it otherwise you get a synthesis error as there is no template primitive
        # that matches the number of write ports necessary...
        self.specials += port

        self.seed = seed = CSRStorage(32)
        self.update = update = CSRStorage(1)  # any write triggers an update
        adr = Signal(log2_int(memdepth))  # should be measured in dw-width words
        self.count = count = CSRStorage(32)
        self.done = done = CSRStatus(1)
        data = Signal(32)
        we = Signal()

        fsm = FSM(reset_state="IDLE")
        self.submodules += fsm

        fsm.act("IDLE",
                NextValue(adr, 0),
                NextValue(data, seed.storage),
                NextValue(we, 0),
                If(update.re,
                   NextState("UPDATE"),
                   NextValue(we, 1),
                   NextValue(done.status, 0),
                ).Else(
                    NextValue(done.status, 1),
                )
        )
        fsm.act("UPDATE",
                NextValue(adr, adr + 1),
                NextValue(data, data - 5),
                NextValue(done.status, 0),
                If( adr < count.storage - 1, # subtract 1 because pipeline
                    NextValue(we, 1),
                ).Else(
                    NextValue(we, 0),
                    NextState("IDLE"),
                )

        )
        self.comb += [
            port.adr.eq(adr),
            port.dat_w.eq(data),
            port.we.eq(we)
        ]

        self.bus = wishbone.Interface()
        self.submodules.wb_sram_if = wishbone.SRAM(mem, read_only=True)

        decoder_offset = log2_int(memdepth, need_pow2=False)
        def slave_filter(a):
                return a[decoder_offset:32-decoder_offset] == 0  # no aliasing in the block
        wb_con = wishbone.Decoder(self.bus, [(slave_filter, self.wb_sram_if.bus)], register=True)
        self.submodules += wb_con

