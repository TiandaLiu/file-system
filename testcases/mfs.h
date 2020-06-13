#ifndef __MFS_h__
#define __MFS_h__

#include <stdio.h>
#include <stdlib.h>
#include "udp.h"

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE   (4096)

#define BUFFER_SIZE (8192)
#define COMMAND_NUM (5)
#define COMMAND_LEN (16)
int SOCKET;
struct sockaddr_in addr, addr2;

typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    int blocks; // number of blocks allocated to file
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    int  inum;      // inode number of entry (-1 means entry not used)
    char name[252]; // up to 252 bytes of name in directory (including \0)
} MFS_DirEnt_t;

int split_string(char* input, char** output);
int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);

#endif // __MFS_h__