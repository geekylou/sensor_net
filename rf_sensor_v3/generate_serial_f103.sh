#!/bin/sh
python generate_uuid.py
arm-none-eabi-objcopy -I binary serial_no.bin --change-addresses=0x801fc00 -O ihex serial_no.ihex
