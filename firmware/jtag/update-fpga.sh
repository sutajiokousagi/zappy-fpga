sudo openocd -c 'set BSCAN_FILE bscan_spi_xc7s50.bit' -c 'set FPGAIMAGE ../../build/gateware/top.bit' -f cl-spifpga.cfg
