CC     := gcc -g
SERVER := server
CLIENT := client

SVR_SRCS := $(wildcard ./src/*.c)
SVR_SRCS += server.c
CLI_SRCS := client.c

all: ${SERVER} ${CLIENT}

${SERVER} : ${SVR_SRCS}
	${CC} $^ -o $@ -I./src/

${CLIENT} : ${CLI_SRCS}
	${CC} $^ -o $@ -I./src/

.PHONY: clean
clean:
	rm -rf *.o ${SERVER} ${CLIENT}