CC = gcc
COMPILER_FLAGS = -g -std=gnu99
LINKER_FLAGS = -g -lpthread -lm
BINARYOSS = oss
BINARYUSER = user
OBJCOMMON = common.o osstime.o messages.o
OBJSOSS = oss.o queue.o
OBJSUSER = user.o
HEADERS = common.h queue.h osstime.h messages.h

all: $(BINARYOSS) $(BINARYUSER)

$(BINARYOSS): $(OBJSOSS) $(OBJCOMMON)
	$(CC) -o $(BINARYOSS) $(OBJSOSS) $(OBJCOMMON) $(LINKER_FLAGS)

$(BINARYUSER): $(OBJSUSER) $(OBJCOMMON)
	$(CC) -o $(BINARYUSER) $(OBJSUSER) $(OBJCOMMON) $(LINKER_FLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(COMPILER_FLAGS) -c $<

clean:
	/bin/rm $(OBJSOSS) $(OBJSUSER) $(OBJCOMMON) $(BINARYOSS) $(BINARYUSER)

dist:
	zip -r oss.zip *.c *.h Makefile README .git
