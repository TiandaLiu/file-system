inodes_bit_arry 1bit * 4096 = 512 Bytes  = 2**9
blocks_bit_arry 1bit * 4096 = 512 Bytes  = 2**9
       ---------  1024
inodes: 2572 Bytes * 4096 = 200kB 
       ---------  3596
padding:  4 bytes
       ---------  3600

blocks 4096 Bytes * 4096 = 16MB  = 2**24  == 16777216
       ---------  16780816


create a 17M file



Inode:  2572 bytes
int type;   4 bytes
int size;   4 bytes
int blocks; 4 bytes

entries: 256 bytes * 10
name 252 bytes
index: 4 bytes


struct[] inodes-4096: [type, size, num_blocks, blocks[10]]
blocks-4096: (dir: list of (inum, name)) (file: bytes/string)
bool map_inodes[4096]
bool mpa_blocks[4096]











int MFS_Init(char *hostname, int port):
	udp_send("init 2 <hostname> <port>")
	recv()
	succ: 0, 
	fail: -1

int MFS_Lookup(int pinum, char *name):
	udp_send("lookup 2 <pinum> <name>")
	recv()
	succ: inode number of name, 
	fail: -1

int MFS_Stat(int inum, MFS_Stat_t *m):
	# load info to *m
	udp_send("stat 2 <inum> <stat_info>")
	recv()
	succ: 0, 
	fail: -1

int MFS_Write(int inum, char *buffer, int block):  # block = offset
	# buffer -> file block
	udp_send("write 3 <inum> <block_offset>")
	recv()
	succ: 0, 
	fail: -1 Failure modes: invalid inum, invalid block, not a regular file (you cant write to directories)

int MFS_Read(int inum, char *buffer, int block):
	udp_send("read 2 <inum> <block_offset>")
	recv()
	succ: 0, 
	fail: -1 Failure modes: invalid inum, invalid block. # works for file & dir

int MFS_Creat(int pinum, int type, char *name):
	udp_send("create 3 <pinu,> <type> <name>")
	recv()
	succ: 0, 
	fail: -1 Failure modes: pinum does not exist. If name already exists, return success 

int MFS_Unlink(int pinum, char *name):
	udp_send("unlink 2 <pinum> <name>")
	recv()
	succ: 0, 
	fail: -1



