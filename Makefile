# Impostor 3DS - build simples (um unico .c) para devkitARM + libctru
ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM nao esta definido. Instale o devkitPro para 3DS.")
endif
DEVKITPRO ?= /opt/devkitpro
CTRULIB   ?= $(DEVKITPRO)/libctru
CC        := $(DEVKITARM)/bin/arm-none-eabi-gcc

TARGET := Impostor3DS
ARCH   := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -O2 -mword-relocations -ffunction-sections -ffast-math $(ARCH) -std=gnu11 -D__3DS__ -I$(CTRULIB)/include
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(TARGET).map -L$(CTRULIB)/lib
LIBS   := -lctru -lm
SRC    := $(wildcard *.c)

.PHONY: all clean cia
all: $(TARGET).elf

$(TARGET).elf: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LIBS)

cia: all
	bash build_cia.sh

clean:
	rm -rf build $(TARGET).elf $(TARGET).map $(TARGET).cia
