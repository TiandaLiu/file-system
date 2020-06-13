CC   = gcc
OPTS = -Wall

all: server client libmfs.so

# this generates the target executables
server: server.o udp.o
	$(CC) -o server server.o udp.o 

client: client.o mfs.o udp.o
	$(CC) -o client client.o mfs.o udp.o 

# this is a generic rule for .o files 
%.o: %.c 
	$(CC) $(OPTS) -c $< -o $@

# generate .so file
# libmfs.so: mfs.c udp.c
# 	$(CC) -shared -o $@ -fpic $<
libmfs.so: mfs.c udp.c
	$(CC) -shared -o libmfs.so -fpic mfs.c udp.c
# libudp.so: udp.c
# 	$(CC) -shared -o libudp.so -fpic udp.c

# gcc -shared -o libmfs.so -fpic mfs.c udp.c

clean:
	rm -f *.o *.so server client