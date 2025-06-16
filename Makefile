CC = gcc
CFLAGS = -Wall -Wextra -std=c99 $(shell pkg-config --cflags polkit-agent-1 glib-2.0)
LDFLAGS = $(shell pkg-config --libs polkit-agent-1 glib-2.0)

VERSION = 1.0

MANDIR = $(DESTDIR)/usr/local/share/man

TARGET = mini-polkit
SRC = mini-polkit.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(TARGET)
	mkdir -p $(DESTDIR)/usr/local/bin
	cp $(TARGET) $(DESTDIR)/usr/local/bin/

	mkdir -p $(MANDIR)/man1
	sed "s/VERSION/${VERSION}/g" < mini-polkit.1 > ${MANDIR}/man1/mini-polkit.1
	chmod 644 $(MANDIR)/man1/mini-polkit.1

	mkdir -p $(DESTDIR)/etc/xdg/autostart
	echo '[Desktop Entry]\nName=Mini Polkit\nExec=/usr/local/bin/$(TARGET)\nTerminal=false\nType=Application\nCategories=System;\nStartupNotify=false\nNoDisplay=true' > $(DESTDIR)/etc/xdg/autostart/mini-polkit.desktop

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)
	rm -f $(MANDIR)/man1/mini-polkit.1
	rm -f $(DESTDIR)/etc/xdg/autostart/mini-polkit.desktop

clean:
	rm -f $(TARGET)

.PHONY: install uninstall clean
