make clean
make
killall -9 server
rm -rf /tmp/p6.fsimage.eFW
rm -rf server-out.txt
./server 20000 /tmp/p6.fsimage.eFW