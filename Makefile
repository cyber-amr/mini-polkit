CC = gcc
CFLAGS = -Wall -Wextra -std=c99 $(shell pkg-config --cflags polkit-agent-1 glib-2.0)
LDFLAGS = $(shell pkg-config --libs polkit-agent-1 glib-2.0)

TARGET = mini-polkit
SRC = mini-polkit.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(TARGET)
	mkdir -p $(DESTDIR)/usr/local/bin
	cp $(TARGET) $(DESTDIR)/usr/local/bin/
	mkdir -p $(HOME)/.config/autostart
	echo '[Desktop Entry]\nName=Polkit dmenu agent\nExec=/usr/local/bin/$(TARGET)\nTerminal=false\nType=Application\nCategories=System;\nStartupNotify=false\nNoDisplay=true' > $(HOME)/.config/autostart/mini-polkit.desktop

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)
	rm -f $(HOME)/.config/autostart/mini-polkit.desktop

clean:
	rm -f $(TARGET)

.PHONY: install uninstall clean
