CC = gcc
CFLAGS = -Wall -Wextra -std=c99 $(shell pkg-config --cflags polkit-agent-1 glib-2.0)
LDFLAGS = $(shell pkg-config --libs polkit-agent-1 glib-2.0)

TARGET = polkit-agent
SRC = polkit-agent.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(TARGET)
	mkdir -p $(DESTDIR)/usr/local/bin
	cp $(TARGET) $(DESTDIR)/usr/local/bin/

clean:
	rm -f $(TARGET)

.PHONY: install clean
