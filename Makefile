###############################################################################
# Makefile for the project buttwarmer
###############################################################################

## General Flags
PROJECT = buttwarmer
MCU = atmega168
TARGET = buttwarmer.elf
CC = avr-gcc

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wextra -pedantic -Wformat=2
CFLAGS += -Wall -gdwarf-2 -std=gnu99   -DF_CPU=8000000UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d
CFLAGS += --param inline-call-cost=2 -finline-limit=3 -fno-inline-small-functions -mcall-prologues

# no benefit
# -fno-tree-scev-cprop

# Save some size, but messier assembly:
# --combine  -fwhole-program

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=buttwarmer.map
#LDFLAGS += -Wl,-u,vfprintf -lprintf_min
#LDFLAGS += -Wl,-u,vfprintf -lprintf_flt -lm
LDFLAGS += -Wl,--relax

# Useful to find unused functions
#CFLAGS +=      -ffunction-sections  -fdata-sections
#LDFLAGS += -Wl,--gc-sections

LIBS = -lm

## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

## Libraries


## Objects that must be built in order to link
OBJECTS = buttwarmer.o

## Objects explicitly added by the user
LINKONLYOBJECTS = 

default: all

## Build
# buttwarmer.eep
all: $(TARGET) buttwarmer.hex buttwarmer.lss size

## Compile
%.o: %.c
	@$(CC) $(INCLUDES) $(CFLAGS) -c  $<



##
%.s: %.c
	@$(CC) $(INCLUDES) $(CFLAGS) -S  $<


##Link
$(TARGET): $(OBJECTS)
	@$(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	@avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

#%.eep: $(TARGET)
#	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	@avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
#@avr-size -C --mcu=${MCU} ${TARGET}
	@avr-size ${TARGET}

SERIAL = COM9

flash: buttwarmer.hex
	avrdude -qq -y -P ${SERIAL} -cstk500v2 -pm168 -Uflash:w:buttwarmer.hex

show:
	avrdude -V -P ${SERIAL} -cstk500v2 -pm168 -v

fuse:
	avrdude -O -P ${SERIAL} -cstk500v2 -pm168
	avrdude -y -P ${SERIAL} -cstk500v2 -pm168 -U lfuse:w:0xE2:m -U hfuse:w:0xDF:m -U efuse:w:0x00:m 

readrom:
	avrdude -y -P ${SERIAL} -cstk500v2 -pm168 -Ueeprom:r:rom.hex:h

z: clean buttwarmer.lss size
	./segcounter -z
	cp buttwarmer.lss buttwarmer.lss.a

ardu: buttwarmer.hex
	- killall -w picocom
	rts-ping
	avrdude -P /dev/ttyUSB0 -cstk500v1 -pm168 -Uflash:w:buttwarmer.hex -F -b38400

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) buttwarmer.elf dep/* buttwarmer.hex buttwarmer.eep buttwarmer.lss buttwarmer.map

tags: *.c *.h
	@ctags -R

## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

