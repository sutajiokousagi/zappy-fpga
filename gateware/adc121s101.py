from migen import *

from litex.soc.interconnect.csr import *
from litex.soc.interconnect.csr_eventmanager import *
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
                ).Elif(go, # allow repeated acquire without acknowledgement
                   NextState("ACQUIRE"),
                   NextValue(self.valid, 0),
                   NextValue(count, 0),
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
        # generate debug clock, it's at half rate but at least whe know where the edges are
        self.dbg_sclk = Signal()
        self.sync.adc += [
            self.dbg_sclk.eq(~self.dbg_sclk)
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
                   NextValue(self.valid.status, 1),
                   NextState("ACK")
                )
        )
        fsm.act("ACK",
                If(~adc_valid_sync,
                   NextValue(self.adc.ready, 0),
                   NextState("IDLE"),
                )
        )

# FIFO wrapper for the Adc121s101 module
#   hvmain_pads PadGroup - see Adc121s101 for spec; ADC connected to HV main
#   cap_pads PadGroup - see Adc121s101 for spec; ADC connected to storage cap
#   vmon_pads PadGroup - see Adc121s101 for spec; ADC connected to HV supply vmon
#   CSR hvmax (wo, 16) - maximum HV voltage, in volts, before safeties are triggered
#   self.*hvengage* Signal() - OUTPUT when high connects MK HV supply to HV main
#   memdepth integer - PARAMETER depth of sample memory

#   CSR acquire (wo) - writing anything to this CSR triggers a sample run acquisition of depth samples
#   CSR depth (wo, 16) - depth of samples to acquire into buffer
#   CSR done (ro) - high when acquisition is finished
#   CSR int_ena (wo) - enable generation of interrupt when done goes high
#   CSR period (wo, 32) - sampling period for depth > 1 sampling, period specified in SYSCLK increments. Should be > 1us.
#   CSR overrun (ro, 32) - number of clock cycles sample timer was overrun on the very most recent sample acquired
#   CSR presample (wo, 16) - number of samples to wait before issuing a trigger. It is up to software to ensure depth > presample
#   self.*ext_trigger* `Signal()` - OUTPUT - single-cycle pulse to indicate when external trigger event should happen based on presample
#   CSR cur_adc (ro, 12) - latest adc value, guaranteed atomic fadc during "acquire" -- for computing/trapping high current conditions
#   CSR cur_fadc (ro, 12) - latest fadc value, guaranteed atomic with adc during "acquire"
#   CSR delta (ro, 16) - difference between adc and fadc

#   MEMORY block on wishbone is generated by this module
class Zappy_adc(Module, AutoCSR):
    def __init__(self, adc_pads, fadc_pads, memdepth=8192):
        self.submodules.adc = Adc121s101(adc_pads)
        self.submodules.fadc = Adc121s101(fadc_pads)

        self.acquire = CSRStorage(1)
        self.depth = CSRStorage(16)
        self.done = CSRStatus()
        self.int_ena = CSRStorage(1) # enable interrupt on done
        self.period = CSRStorage(32)  # number of clock cycles to elapse for each sampling period
        self.overrun = CSRStatus(32) # amount that the sampling timer was overrun, if any
        # this should be longer than the sampling rate of the ADC or else timing could be uneven
        self.presample = CSRStorage(16)
        self.ext_trigger = Signal()
        self.cur_adc = CSRStatus(12)
        self.cur_fadc = CSRStatus(12)
        self.delta = CSRStatus(16)
        self.livedelta = Signal(16)

        # coefficient is roughly 1.69*10^-9 joules per LSB
        # max possible energy is 10 Joules, so max count is approx 5.9 billion -- longer than a 32 bit number
        self.energy_accumulator = CSRStatus(fields=[
            CSRField("energy", size=40, description="Energy accumulated during the pulse")
        ])
        self.energy_threshold = CSRStorage(fields=[
            CSRField("threshold", size=40, description="Energy cutoff for zap")
        ])
        self.energy_control = CSRStorage(fields = [
            CSRField("enable", size=1, description="Use energy cutoff to terminate a zap"),
            CSRField("reset", size=1, description="Reset the energy accumulator", pulse=1),
        ])
        energy_accumulate = Signal() # gate the accumulator on, must be a single cycle pulse
        fadc_reg = Signal(12)
        sadc_reg = Signal(12)
        self.energy_cutoff  = Signal()  # cut off the zap because energy is past the threshold
        self.sync += [
            If(self.energy_control.fields.reset,
               self.energy_accumulator.fields.energy.eq(0)
            ).Else(
                If(energy_accumulate & (sadc_reg > fadc_reg), # negative results are due to small static offsets, causes instability problems if we sume them in
                   self.energy_accumulator.fields.energy.eq(self.energy_accumulator.fields.energy + ((sadc_reg - fadc_reg) * (fadc_reg)) )
                ).Else(
                    self.energy_accumulator.fields.energy.eq(self.energy_accumulator.fields.energy)
                )
            ),
            If(self.energy_control.fields.enable,
                If( (self.energy_threshold.fields.threshold < self.energy_accumulator.fields.energy) &
                    (self.energy_accumulator.fields.energy[self.energy_accumulator.fields.energy.nbits - 1] == 0), # only trigger if energy is not negative
                  # note: negative energy can happen due to static offsets at low energy levels
                  self.energy_cutoff.eq(1)
                ).Else(
                  self.energy_cutoff.eq(0)
               )
            ).Else(
                self.energy_cutoff.eq(0),
            )
        ]

        # also generate a convenience interrupt when status is done, if interrupt is enabled
        self.submodules.ev = EventManager()
        self.ev.acquisition_done = EventSourcePulse()
        self.ev.finalize()
        self.comb += self.ev.acquisition_done.trigger.eq(self.done.status & self.int_ena.storage)

        mem = Memory(32, memdepth)
        port = mem.get_port(write_capable=True)
        self.specials += port
        self.adr = adr = Signal(log2_int(memdepth))  # should be measured in dw-width words
        data = Signal(32)
        we = Signal()

        self.sampletimer = sampletimer = Signal(32)
        self.sample_reset = sample_reset = Signal()
        self.sync += [
            If(sample_reset,
               sampletimer.eq(0),
            ).Else(
                sampletimer.eq(sampletimer + 1),
            )
        ]

        fsm = FSM(reset_state="IDLE")
        self.submodules.fsm = fsm

        self.count = count = Signal(16)
        zeropad = Signal(4)
        self.comb += zeropad.eq(0)

        adc_valid_sync = Signal()
        fadc_valid_sync = Signal()
        self.specials += MultiReg(self.adc.valid, adc_valid_sync)
        self.specials += MultiReg(self.fadc.valid, fadc_valid_sync)
        pulsetimer = Signal(5)
        fsm.act("IDLE",
                NextValue(count, self.depth.storage),
                NextValue(adr, 0),
                NextValue(pulsetimer, 0),
                If(self.acquire.re,
                   NextState("ACQUIRE"),
                   sample_reset.eq(1), # reset & run the sample counter from 0
                   NextValue(self.done.status, 0), # clear status to 0
                )
        )
        fsm.act("ACQUIRE",  # send an acquire pulse, must be long enough for the ADC module to pick it up
                self.adc.acquire.eq(1),
                self.fadc.acquire.eq(1),
                NextValue(pulsetimer, pulsetimer + 1),
                If(pulsetimer >= 15,
                    NextState("WAITVALID"),
                    NextValue(self.adc.ready, 1),
                    NextValue(self.fadc.ready, 1),
                )
        )
        fsm.act("WAITVALID", # wait for ADC to present valid data, then copy it to the data register
                NextValue(pulsetimer, 0), # reset the pulse timer in case we loop back to ACQUIRE state without going through IDLE
                If(adc_valid_sync & fadc_valid_sync,
                   NextValue(data, Cat(self.adc.data, zeropad, self.fadc.data, zeropad)), # stable copy for RAM
                   NextValue(fadc_reg, self.fadc.data),  # also make a copy for the energy accumulator
                   NextValue(sadc_reg, self.adc.data),
                   NextValue(self.cur_adc.status, self.adc.data),
                   NextValue(self.cur_fadc.status, self.fadc.data),
                   NextValue(self.adc.ready, 0),
                   NextValue(self.fadc.ready, 0),
                   NextState("SAMPLING_WAIT"),
                   If(self.fadc.data < self.adc.data,  # in discharge, adc should be higher than fadc
                      NextValue(self.livedelta, self.adc.data - self.fadc.data),
                      NextValue(self.delta.status, self.adc.data - self.fadc.data),
                    ).Else(
                       NextValue(self.livedelta, 0),  # don't go negative, if inverted during discharge, it's noise
                       NextValue(self.delta.status, 0),
                   )
                )
        )
        fsm.act("SAMPLING_WAIT", # wait until the next sample period
                we.eq(1),  # commit the data to RAM
                # the >= catches the case that waiting for the ADC to finish took longer than the specified period
                If(sampletimer >= (self.period.storage-2),
                   NextState("INCREMENT")
                ),
                # note the statement above is >=, so if sampletimer starts below period, the next
                # statement won't happen and the overrun will be 0
                # this statement runs in parallel with the above statement.
                If(sampletimer > (self.period.storage-2),
                   NextValue(self.overrun.status, sampletimer - self.period.storage)
                ).Else(
                    NextValue(self.overrun.status, 0)
                ),
        )
        fsm.act("INCREMENT", # single cycle in sysclk
                If(count < (self.depth.storage - self.presample.storage),
                     self.ext_trigger.eq(1),
                     energy_accumulate.eq(1),
                   ),
                NextValue(count, count - 1),
                NextValue(adr, adr + 1),
                If(count != 0,
                   NextState("ACQUIRE"),
                   sample_reset.eq(1),
                ).Else(
                   NextState("IDLE"),
                   NextValue(self.done.status, 1), # indicate status is done
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


#  CSR update (wo) - writing anything triggers memory values to update
#  CSR count (wo, 32) - number of data words (32-bits wide) to update
#  CSR seed (wo, 32) - 32-bit seed number for updating
#  CSR done (ro) - 1 when updating is done, 0 when update is running
#  memdepth integer (default 2048) - depth in 32-bit words of the memory to allocate
#  self.*mem* `Memory()` - memdepthx32 memory, one local write-only port allocated, one wishbone read-only port allocated
class Zappy_memtest(Module, AutoCSR):
    def __init__(self, memdepth=2048):
        mem = Memory(32, memdepth)
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

