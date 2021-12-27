#/bin/sh

openocd -f /home/andris/Documents/Tools/openocd/tcl/interface/stlink.cfg -f /home/andris/Documents/Tools/openocd/tcl/target/nrf52.cfg -c "program /home/andris/Documents/Projects/__ZEPHYR_PROJECTS/nrf52_haptic/build/zephyr/zephyr.elf verify reset exit"
