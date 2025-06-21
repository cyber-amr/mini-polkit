CC = gcc
CFLAGS = -Wall -Wextra -std=c99 $(shell pkg-config --cflags polkit-agent-1 glib-2.0)
LDFLAGS = $(shell pkg-config --libs polkit-agent-1 glib-2.0)

VERSION = 1.0.0

MANDIR = $(DESTDIR)/usr/share/man

TARGET = mini-polkit
SRC = mini-polkit.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(TARGET)
	mkdir -p $(DESTDIR)/usr/bin
	cp $(TARGET) $(DESTDIR)/usr/bin/

	mkdir -p $(MANDIR)/man1
	sed "s/VERSION/${VERSION}/g" < mini-polkit.1 > ${MANDIR}/man1/mini-polkit.1
	chmod 644 $(MANDIR)/man1/mini-polkit.1

uninstall:
	rm -f $(DESTDIR)/usr/bin/$(TARGET)
	rm -f $(MANDIR)/man1/mini-polkit.1

clean:
	rm -f $(TARGET)

.PHONY: install uninstall clean
