BIN     = test
CC      = gcc
STRIP   = strip

#ECHO   =
OFLAGS = -g
ECHO    = @
#OFLAGS  = -O3 -flto
INCLUDE = ../../../drivers/misc/miner
CFLAGS  = $(OFLAGS) -I$(INCLUDE) -Wall -fdata-sections -ffunction-sections
LDFLAGS = $(OFLAGS) -Wl,--gc-sections -Wl,-Map,$(BIN).map

SRC     = $(wildcard *.c)
OBJ     = $(SRC:.c=.o)
DEP     = $(SRC:.c=.d)

all: $(BIN)

-include $(DEP)

$(BIN): $(OBJ)
	@echo "$^ -> $@"
	$(ECHO)$(CC) -o $@ $^ $(LDFLAGS)
#	$(ECHO)$(STRIP) $@

%.o: %.c
	@echo "$< -> $@"
	$(ECHO)$(CC) $(CFLAGS) -MMD -o $@ -c $<

.PHONY: clean

clean:
	@rm -f *.o *.d *.map $(BIN)

