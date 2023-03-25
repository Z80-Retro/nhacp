TOP=..

include $(TOP)/Make.default
-include $(TOP)/Make.local


# include for CP/M BIOS libraries (memory map, SIO, etc.)
ASM_FLAGS+=-I$(Z80_RETRO_CPM_REPO)/lib


# A local library for volatile things in development
# Note that this will override any same-named files appearing in /prior/ -I paths
# This is ass-backwards from ALL other assemblers and compilers I have ever used. - jrw

ASM_FLAGS+=-I$(TOP)/lib


all:: nt.com

clean::
	rm -f *.lst *.bin *.com *.hex *.sym

DATE := $(shell date --rfc-3339=seconds)
GIT_VERSION := $(shell git describe --long --dirty; git show -s --format='%ci')
%.com: %.asm
	cat $< | sed -e "s/@@DATE@@/$(DATE)/g" -e "s/@@GIT_VERSION@@/$(GIT_VERSION)/g" | z80asm - -o $@ --list=$(basename $@).lst --label=$(basename $@).sym $(ASM_FLAGS)

world:: clean all
