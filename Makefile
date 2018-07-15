
CC = gcc
CFLAGS = -Wall -Wundef
LDLIBS = -lwiringPi -lwiringPiDev -lpthread
OBJECTS =
BIN = airtemp-lcd.elf
TARGETDIR = /root/bin/

airtemp-lcd: airtemp-lcd.c
	$(CC) $(CFLAGS) $^ --output $(BIN) $(LDLIBS)

.PHONY: clean install uninstall

clean:
	rm -f $(BIN)

install:
	if [ ! -d "$(TARGETDIR)" ]; then \
		mkdir "$(TARGETDIR)"; \
	fi
	cp $(BIN) "$(TARGETDIR)"

uninstall:
	rm -f "$(TARGETDIR)/$(BIN)"

