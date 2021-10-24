build: libscheduler.so

libscheduler.so: tema_linux.o priority_queue.o
	gcc -Wall -shared tema_linux.o priority_queue.o -o libscheduler.so

tema_linux.o: tema_linux.c
	gcc -Wall -fPIC -o tema_linux.o -c -lpthread tema_linux.c

priority_queue.o: priority_queue.c
	gcc -Wall -fPIC -o priority_queue.o -c priority_queue.c

.PHONY: clean

clean:
	rm -f *.o libscheduler.so
