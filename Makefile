.PHONY: all clean server client test crypto check

CSFLAGS  = -pedantic -Wall -Wextra -Wshadow -Wformat=2 -Wpedantic -Wundef

ifndef ($(CC))
CC       = clang
endif

ifeq ($(CC), gcc)
#Condition variables are implemented since Windows Server 2008
#in macro terms: 0x0600 = _WIN32_WINNT_WS08
CSFLAGS += -Wchkp
ifeq ($(OS), Windows_NT)
CSFLAGS += -D_WIN32_WINNT=0x600 -DWINVER=0x600
endif
endif

ifeq ($(OS), Windows_NT)
#NOTE: We are aware of non-terminated string dangers
#Defined solely for the Microsoft headers
CSFLAGS += -D_CRT_SECURE_NO_WARNINGS
LIB      = -lws2_32
SERVER_E = server.exe
CLIENT_E = client.exe
TEST_E   = test.exe
AUX_E    = testaux.exe
CRYPTO_E = crypto.exe
TEST_OUT = testWIN32_$(CC).txt
else
ifeq ($(CC), winegcc)
CSFLAGS += -mno-cygwin -U__unix__ -U__linux__ -mconsole
LIB      = -lws2_32
SERVER_E = server.exe
CLIENT_E = client.exe
TEST_E   = test.exe
AUX_E    = testaux.exe
CRYPTO_E = crypto.exe
TEST_OUT = testWINE_$(CC).txt
else
CSFLAGS += -D_DEFAULT_SOURCE
LIB      = -pthread
SERVER_E = server
CLIENT_E = client
TEST_E   = test
AUX_E    = testaux
CRYPTO_E = crypto
TEST_OUT = testUNIX_$(CC).txt
endif
endif

SERVER   = serv.c
SERVER_O = $(SERVER:.c=.o)
CLIENT   = client.c
CLIENT_O = $(CLIENT:.c=.o)
TEST     = test.c
TEST_O   = $(TEST:.c=.o)
CRYPTO   = crypto.c
CRYPTO_O = $(CRYPTO:.c=.o)
COMMON   = error.c log.c file.c net.c multi.c sync.c pool.c string.c string_heap.c filewalker.c arguments.c list.c
COMMON_O = $(COMMON:.c=.o)

##### TARGETS ##################################################################

all: server client

server:  $(COMMON_O) crypto
	$(CC) $(CFLAGS) $(CSFLAGS) $(LDFLAGS) -o $(SERVER_E) $(SERVER) $(COMMON_O) $(LIB)

client: $(COMMON_O)
	$(CC) $(CFLAGS) $(CSFLAGS) $(LDFLAGS) -o $(CLIENT_E) $(CLIENT) $(COMMON_O) $(LIB)

check: test
	./$(TEST_E)  >$(TEST_OUT)

test:   $(COMMON_O) crypto testaux.o
	$(CC) $(CFLAGS) $(CSFLAGS) $(LDFLAGS) -o $(AUX_E) testaux.o $(COMMON_O) $(LIB)
	$(CC) $(CFLAGS) $(CSFLAGS) $(LDFLAGS) -o $(TEST_E) $(TEST) $(COMMON_O) $(LIB)

crypto: $(COMMON_O)
	$(CC) $(CFLAGS) $(CSFLAGS) $(LDFLAGS) -o $(CRYPTO_E) $(CRYPTO) $(COMMON_O) $(LIB)

clean:
	rm -f *.o $(SERVER_E) $(CLIENT_E) $(TEST_E) $(CRYPTO_E) $(AUX_E)

%.o: %.c
	$(CC) $(CFLAGS) $(CSFLAGS) -c -o $@ $<
