# You can use clang if you prefer
CC = gcc

# Feel free to add other C flags
CFLAGS += -c -std=gnu99 -Wall -Werror -Wextra -O2
# By default, we colorize the output, but this might be ugly in log files, so feel free to remove the following line.
CFLAGS += -D_COLOR

# You may want to add something here
LDFLAGS +=

# Adapt these as you want to fit with your project
SENDER_SOURCES = $(wildcard src/sender.c src/log.c src/packet_implem.c src/create_socket.c  \
								src/real_address.c src/queue.c)
RECEIVER_SOURCES = $(wildcard src/receiver.c src/log.c src/packet_implem.c src/create_socket.c src/real_address.c \
                    			src/wait_for_client.c src/queue.c)
NEW_SENDER_S = $(wildcard src/new_sender.c src/log.c src/packet_implem.c src/create_socket.c  \
               					src/real_address.c src/queue.c)
NEW_RECEIVER_S = $(wildcard src/new_receiver.c src/log.c src/packet_implem.c src/create_socket.c src/real_address.c \
                                src/wait_for_client.c src/queue.c)

SENDER_OBJECTS = $(SENDER_SOURCES:.c=.o)
RECEIVER_OBJECTS = $(RECEIVER_SOURCES:.c=.o)
NEW_SENDER_OBJECTS = $(NEW_SENDER_S:.c=.o)
NEW_RECEIVER_OBJECTS = $(NEW_RECEIVER_S:.c=.o)

SENDER = sender
RECEIVER = receiver
NEW_SENDER = new_sender
NEW_RECEIVER = new_receiver


all: $(SENDER) $(RECEIVER)

$(SENDER): $(SENDER_OBJECTS)
	$(CC) $(SENDER_OBJECTS) -o $@ $(LDFLAGS) -lz

$(RECEIVER): $(RECEIVER_OBJECTS)
	$(CC) $(RECEIVER_OBJECTS) -o $@ $(LDFLAGS) -lz

$(NEW_SENDER): $(NEW_SENDER_OBJECTS)
	$(CC) $(NEW_SENDER_OBJECTS) -o $@ $(LDFLAGS) -lz

$(NEW_RECEIVER): $(NEW_RECEIVER_OBJECTS)
	$(CC) $(NEW_RECEIVER_OBJECTS) -o $@ $(LDFLAGS) -lz

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

.PHONY: clean mrproper

clean:
	rm -f $(SENDER_OBJECTS) $(RECEIVER_OBJECTS) $(NEW_SENDER_OBJECTS) $(NEW_RECEIVER_OBJECTS)

mrproper:
	rm -f $(SENDER) $(RECEIVER) $(NEW_SENDER) $(NEW_RECEIVER)

# It is likely that you will need to update this
tests: all
	./tests/run_tests.sh

# By default, logs are disabled. But you can enable them with the debug target.
debug: CFLAGS += -D_DEBUG
debug: clean all

# Place the zip in the parent repository of the project
ZIP_NAME="../projet1_ALLEGAERT_LECHAT"

# A zip target, to help you have a proper zip file. You probably need to adapt this code.
zip:
	# Generate the log file stat now. Try to keep the repository clean.
	git log --stat > gitlog.stat
	zip -r $(ZIP_NAME) Makefile README.md src tests rapport.pdf gitlog.stat scribes.txt
	# We remove it now, but you can leave it if you want.
	rm gitlog.stat

run_sender:
	make all
	./sender ::1 8080 2>sender.log < scribe.txt

run_receiver:
	make all
	./receiver :: 8080 2>receiver.log

IN=scribe.txt
FILE=-f $(IN)
sender_o:
	make all
	./sender $(FILE) -s stats_send.csv ::1 8088 2>sender.log
sender_n:
	make all
	./sender -s stats_send.csv ::1 8088 2>sender.log < $(IN)

OUT=out.txt
receiver_n:
	make all
	./receiver -s stats_rec.csv :: 8088 1>$(OUT)  2>receiver.log
