avr-gcc *.c -mmcu=atmega32u4 -Os -DF_CPU=8000000UL -std=gnu99 -I../libraries -L../libraries -Wl,-u,vfprintf -lprintf_flt -lcab202_teensy -lm -o astrdz.obj


avr-objcopy -O ihex astrdz.obj astrdz.hex