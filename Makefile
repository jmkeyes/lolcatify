# Makefile

CFLAGS  += -fPIC
LDFLAGS += -shared -lm -ldl

OUTPUT  := lolcatify.so
SOURCES := $(wildcard ./*.c)
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))

# Find the ELF interpreter used by /bin/sh and define it for the preprocessor.
ELF_INTERPRETER := $(shell objcopy -O binary --only-section=.interp /bin/sh /dev/stdout | tr -d '\0')
CFLAGS += -DTHE_MAGICS_LET_ME_SHOW_YOU_THEM=\"$(ELF_INTERPRETER)\" -D_GNU_SOURCE

all: $(OUTPUT)

clean:
	-$(RM) $(OBJECTS) $(OUTPUT)

test: $(OUTPUT)
	-/usr/bin/env LD_PRELOAD=./$(OUTPUT) /bin/cat Makefile

$(OUTPUT): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

