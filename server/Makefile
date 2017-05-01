OBJS 	= server.o functions.o
SOURCE	= server.c functions.c
HEADER  = functions.h
OUT  	= server
CC	= gcc
FLAGS   = -g3 -c -lpthread
# -g3 option enables debugging mode 
# -c flag generates object code for separate files

all: $(OBJS)
	$(CC) -g3 -lpthread $(OBJS) -o $(OUT)

# create/compile the individual files >>separately<< 
server.o: server.c
	$(CC) $(FLAGS) server.c

functions.o: functions.c
	$(CC) $(FLAGS) functions.c

# clean house
clean:
	rm -f $(OBJS) $(OUT)

# do a bit of accounting
count:
	wc $(SOURCE) $(HEADER)