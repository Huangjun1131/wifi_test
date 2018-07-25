all:
	gcc *.c -o wifi -DALLPRINT -Wall -Wpointer-sign -Wdeprecated-declarations -lpthread
clean:
	rm wifi
