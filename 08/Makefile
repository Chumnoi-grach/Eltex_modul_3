CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lrt

TARGET = udp_sniffer
SOURCES = udp_sniffer.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -f output.txt

run: $(TARGET)
	sudo ./$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

.PHONY: all clean run debug