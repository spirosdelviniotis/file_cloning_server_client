OBJS 	= client.o
SOURCE	= client.c
OUT  	= client
CC	= gcc
FLAGS   = -g3 -c
# -g3 option enables debugging mode 
# -c flag generates object code for separate files

all: $(OBJS)
	$(CC) -g3 $(OBJS) -o $(OUT)

# create/compile the individual files >>separately<< 
client.o: client.c
	$(CC) $(FLAGS) client.c

# clean house
clean:
	rm -f $(OBJS) $(OUT)

# do a bit of accounting
count:
	wc $(SOURCE)