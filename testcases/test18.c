#include "mfs.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
  MFS_Init(argv[2], atoi(argv[1]));

  if (MFS_Creat(0, MFS_REGULAR_FILE, "file.txt") == -1)
	  return -1;

  int inode = MFS_Lookup(0, "file.txt");
  if (inode == -1 || inode == 0)
	  return -1;

  char buf[MFS_BLOCK_SIZE];
  memset(buf, 99, MFS_BLOCK_SIZE);
  // printf("buf to write: %s\n", buf);
  printf("buf 0 before write = %d\n", buf[0]);
  if (MFS_Write(inode, buf, 0) == -1)
	  return -1;
  printf("buf 0 after write = %d\n", buf[0]);

  char result[MFS_BLOCK_SIZE];
  printf("result address: %p, buf address: %p\n", result, buf);
  printf("buf 0 before memset = %d\n", buf[0]);
  memset(result, 98, MFS_BLOCK_SIZE);
  printf("buf 0 after memset = %d\n", buf[0]);
  if (MFS_Read(inode, result, 0) == -1)
	  return -1;
  printf("buf 0 after read = %d\n", buf[0]);
  // printf("read result: %s\n", result);
  if (memcmp(result, buf, MFS_BLOCK_SIZE) != 0)
	  return -1;

  return 0;
}

