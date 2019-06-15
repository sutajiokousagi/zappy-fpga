#!/usr/bin/env python3

from litex import RemoteClient

from litescope import LiteScopeAnalyzerDriver

wb = RemoteClient()
wb.open()

# # #

analyzer = LiteScopeAnalyzerDriver(wb.regs, "analyzer", debug=True)
analyzer.configure_group(0)
analyzer.configure_trigger()  # this clears triggers
analyzer.configure_subsampler(1)
#analyzer.add_falling_edge_trigger("vmon0_cs_n")
analyzer.add_trigger(cond={"analyzer_state": 1})

analyzer.run(offset=32, length=256)
analyzer.wait_done()
analyzer.upload()
analyzer.save("dump.vcd")

# # #

wb.close()
