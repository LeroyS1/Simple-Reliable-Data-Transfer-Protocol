default:
	gcc -g -Wall -Wextra server.c -o server
	gcc -g -Wall -Wextra client.c -o client
clean:
	rm -rf server client *.o *.dSYM *.file
