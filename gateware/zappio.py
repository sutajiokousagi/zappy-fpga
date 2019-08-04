from migen import *

from litex.soc.interconnect.csr import *
from migen.genlib.cdc import MultiReg
from gateware.dac8560 import Dac8560

# CSR wrapper for Zappy GPIOs
#   pads PadGroup - expects "noplate", "row", "col", "hv_engage", "cap", "discharge", "mb_unplugged", "mk_unplugged", "l25_pos", "l25_open" groups
#   hvdac_pads PadGroup - expects pads that match the DAC spec
#   CSR noplate (ro, 4) - status of the noplate switches
#   CSR row (wo, 4) - select the row to zap; clears hardware trigger when updated
#   CSR col (wo, 12) - select te column to zap; clears hardware trigger when updated
#   self.*scram* `Signal()` - INPUT - when asserted, "scram" mode is active and all row/col IOs should be zero
#   CSR override_safety (wo, 1) - when set, overrides all automatic safeties (use for testing only)
#   CSR scram_status (ro, 1) - final scram value once modified by automated scram logic
#   CSR hv_setting (wo, 16) - DAC code for setting the HV supply (will be overidden in case of a scram conidition)
#   CSR hv_update (wo, 1) - when written to, commits the hv_setting value to the DAC output
#   self.*trigger* `Signal()` - INPUT - when asserted, even for a single cycle, engage the row/col settings
#   CSR triggermode (wo, 1) - if 1, use software trigger. if 0, use hardware trigger
#   CSR triggersoft (wo, 1) - software trigger when set
#   CSR triggerclear (wo, 1) - clear hardware trigger when anything is written; also cleared if row/col mapping updated
#   CSR triggerstatus (ro, 1) - current status of the trigger
#   CSR maxdelta (wo, 16) - maximum delta code for current before scram
#   CSR maxdelta_ena (wo, 1) - enable max delta code scram machine
#   CSR maxdelta_reset (wo, 1) - reset maxdelta SCRAM condition
#   CSR maxdelta_scram (ro, 1) - set if there was a SCRAM condition detected on the last run
#   self.*delta* `Signal(16)` - INPUT - the delta code computed live during the run

class Zappio(Module, AutoCSR):
    def __init__(self, pads, hvdac_pads):
        self.scram = Signal()  # this is the emergency shutdown signal
        myscram = Signal()

        self.trigger = Signal()
        self.triggerctl = CSRStorage(2)
        mytrigger = Signal()
        triggerlatch = Signal()

        self.noplate = CSRStatus(4)
        noplate_gpio = getattr(pads, "noplate")
        noplate_sync = Signal(4)
        self.specials += MultiReg(noplate_gpio, noplate_sync)
        self.comb += self.noplate.status.eq(noplate_sync)

        self.override_safety = CSRStorage(1)
        maxdelta_scram = Signal()
        # noplate should be all 0's if a plate is properly present, do not apply HV if plate is absent
        self.comb += myscram.eq( (self.scram | (noplate_sync != 0) | maxdelta_scram) & ~self.override_safety.storage)
        self.scram_status = CSRStatus(1)
        self.comb += self.scram_status.status.eq(myscram)

        # hvengage
        self.hv_engage = CSRStorage(1)
        self.hv_engage_gpio = hv_engage_gpio = getattr(pads, "hv_engage")
        self.comb += hv_engage_gpio.eq(self.hv_engage.storage & ~myscram)

        self.cap = CSRStorage(1)
        cap_gpio = getattr(pads, "cap")
        self.comb += cap_gpio.eq(self.cap.storage & ~myscram)

        self.discharge = CSRStorage(1)
        discharge_gpio = getattr(pads, "discharge")
        self.comb += discharge_gpio.eq(self.discharge.storage & ~self.hv_engage.storage) # only allow cap discharge when HV is not engaged

        # wire up the rows and columns
        self.row = CSRStorage(4)
        row_gpio = getattr(pads, "row")

        self.col = CSRStorage(12)
        col_gpio = getattr(pads, "col")

        # setup the row/col trigger logic
        self.triggermode = CSRStorage(1)
        self.triggersoft = CSRStorage(1)
        self.triggerclear = CSRStorage(1)
        self.triggerstatus = CSRStatus(1)
        self.sync += [
            If(self.triggerclear.re | self.row.re | self.col.re, # auto-clear if row or col is updated
               triggerlatch.eq(0)
            ).Elif(self.trigger, # this should come from the Zappy_adc module
               triggerlatch.eq(1)
            ).Else(
               triggerlatch.eq(triggerlatch)
            )
        ]
        self.comb += [
            mytrigger.eq( (triggerlatch & ~self.triggermode.storage) | (self.triggersoft.storage & self.triggermode.storage) ),
            If(myscram | ~mytrigger,
               row_gpio.eq(0),
               col_gpio.eq(0),
            ).Else(
                row_gpio.eq(self.row.storage),
                col_gpio.eq(self.col.storage),
            ),
            self.triggerstatus.status.eq(mytrigger)
        ]

        self.maxdelta = CSRStorage(16)
        self.maxdelta_ena = CSRStorage(16)
        self.maxdelta_reset = CSRStorage(1)  # probably not really needed because triggerclear also resets maxdelta scram
        self.maxdelta_scram = CSRStatus(1)
        self.delta = Signal(16)
        self.comb += self.maxdelta_scram.status.eq(maxdelta_scram)
        self.sync += [
            If(self.maxdelta_reset.re | self.triggerclear.re,
               maxdelta_scram.eq(0)
            ).Else(
               If( mytrigger & (self.delta >= self.maxdelta.storage) & self.maxdelta_ena.storage, # only consider delta during trigger
                   maxdelta_scram.eq(1),
               ).Else(
                   maxdelta_scram.eq(maxdelta_scram)
               )
            )
        ]

        # report if the HV motherboard has been unplugged and we are somehow operating in either a debug mode or a really bad error mode
        self.mb_unplugged = CSRStatus(1)
        mb_unplugged_gpio = getattr(pads, "mb_unplugged")
        mb_unplugged_sync = Signal()
        self.specials += MultiReg(mb_unplugged_gpio, mb_unplugged_sync)
        self.comb += self.mb_unplugged.status.eq(mb_unplugged_sync)

        # report if the MK HV power supply is unplugged
        self.mk_unplugged = CSRStatus(1)
        mk_unplugged_gpio = getattr(pads, "mk_unplugged")
        mk_unplugged_sync = Signal()
        self.specials += MultiReg(mk_unplugged_gpio, mk_unplugged_sync)
        self.comb += self.mk_unplugged.status.eq(mk_unplugged_sync)

        # wire up the L25 position sensors
        self.l25_pos = CSRStatus(1)
        self.l25_open = CSRStatus(1)
        l25_pos_gpio = getattr(pads, "l25_pos")
        l25_open_gpio = getattr(pads, "l25_open")
        l25_pos_sync = Signal()
        l25_open_sync = Signal()
        self.specials += [
            MultiReg(l25_pos_gpio, l25_pos_sync),
            MultiReg(l25_open_gpio, l25_open_sync),
        ]
        self.comb += [
            self.l25_pos.status.eq(l25_pos_sync),
            self.l25_open.status.eq(l25_open_sync),
        ]


        #### add control of the HV DAC -- include scram override code
        self.hv_setting = CSRStorage(16)
        self.submodules.hvdac = ClockDomainsRenamer({"dac":"adc"})(Dac8560(hvdac_pads))
        self.hv_update = CSRStorage(1)
        self.hv_ready = CSRStatus(1)

        self.specials += MultiReg(self.hvdac.ready, self.hv_ready.status)
        # no synchronizer for the data because we assume it doesn't move while "update" is being written
        self.comb += If(myscram,
                        self.hvdac.data.eq(0),  # in case of scram condition, set HVDAC data to 0
                    ).Else(
                        self.hvdac.data.eq(self.hv_setting.storage)
                    )

        # extend CSR update single-cycle pulse to several cycles so DAC is sure to see it (running in a slower domain)
        trigger = Signal()
        self.comb += self.hvdac.valid.eq(trigger)
        count = Signal(5)
        fsm = FSM(reset_state="IDLE")
        self.submodules += fsm
        fsm.act("IDLE",
                NextValue(count, 0),
                If(self.hv_update.re | myscram, # in case of scram, repeatedly force the DAC output to 0
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


