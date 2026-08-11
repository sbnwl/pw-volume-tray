CC     ?= gcc
PKG    := gtk+-3.0
CFLAGS += -std=gnu11 -Wall -Wextra -O2 $(shell pkg-config --cflags $(PKG))
LDLIBS += $(shell pkg-config --libs $(PKG))

TARGET := pw-tray
SRC    := $(wildcard src/*.c)
OBJ    := $(SRC:.c=.o)
DEP    := $(OBJ:.o=.d)

# Defaults to a per-user install (~/.local). For a system-wide install,
# override on the command line, e.g.:
#   sudo make PREFIX=/usr/local install
# DESKTOPDIR can also be overridden independently, e.g. to place the
# desktop entry directly in /usr/share/applications:
#   sudo make PREFIX=/usr DESKTOPDIR=/usr/share/applications install
PREFIX     ?= $(HOME)/.local
BINDIR     := $(PREFIX)/bin
DESKTOPDIR ?= $(PREFIX)/share/applications

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Each .o depends on its own header usage (tracked via -MMD below), so a
# change to pw-tray.h rebuilds every module that includes it, and a change
# to one .c file only rebuilds that one.
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEP)

install: $(TARGET)
	install -Dm755 $(TARGET) $(BINDIR)/$(TARGET)
	install -Dm644 data/pw-tray.desktop $(DESKTOPDIR)/pw-tray.desktop

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(DESKTOPDIR)/pw-tray.desktop

clean:
	rm -f $(TARGET) $(OBJ) $(DEP)
