TEST_NUM=$1

make clean
make
killall -9 testclient
rm -rf testclient
gcc -Wall -c udp.c -o udp.o
gcc -Wall -c ./testcases/test$TEST_NUM.c -o testclient.o
gcc -o testclient testclient.o udp.o -L. -lmfs
./testclient 20000 127.0.0.1