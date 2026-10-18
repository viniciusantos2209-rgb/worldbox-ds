ifeq ($(strip $(DEVKITARM)),)
$(error "Defina DEVKITARM (instale o devkitPro com o pacote nds-dev)")
endif

include $(DEVKITARM)/ds_rules

DEVKITPRO ?= /opt/devkitpro
LIBNDS    ?= $(DEVKITPRO)/libnds
TARGET    := worldbox

ARCH    := -mthumb -mthumb-interwork
CFLAGS  := -g -Wall -O2 -march=armv5te -mtune=arm946e-s -fomit-frame-pointer \
           -ffast-math $(ARCH) -I$(LIBNDS)/include -DARM9
LDFLAGS := -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(TARGET).map -L$(LIBNDS)/lib

all: $(TARGET).nds

$(TARGET).nds: $(TARGET).elf
	ndstool -c $@ -9 $< -b $(LIBNDS)/icon.bmp "WorldBox DS;Mini simulador de deus"

$(TARGET).elf: source/main.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -lnds9 -o $@

clean:
	rm -f $(TARGET).elf $(TARGET).nds $(TARGET).map
