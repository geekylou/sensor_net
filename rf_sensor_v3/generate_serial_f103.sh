#!/bin/sh
python generate_uuid.py
arm-none-eabi-objcopy -I binary serial_no.bin --change-addresses=0x801fc00 -O ihex serial_no.ihex
arm-none-eabi-objcopy -I binary serial_no.bin --change-addresses=0x8001fd0 -O ihex serial_no_bl.ihex

dd if=generic_boot20_pc13.bin bs=8144 count=1 of=generic_boot20_pc13_shrink.bin
arm-none-eabi-objcopy -I binary generic_boot20_pc13_shrink.bin --change-addresses=0x8000000 -O ihex generic_boot20_pc13.ihex

cat generic_boot20_pc13.ihex serial_no_bl.ihex >serial_no_bootloader.hex