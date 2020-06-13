#include "mfs.h"
#include <stdlib.h>

int main(int argc, char* argv[]) {
  MFS_Init(argv[2], atoi(argv[1]));

  if (MFS_Lookup(0, "file.txt") != -1) {
    printf("-1.1\n");
    return -1;
  }
  if (MFS_Creat(0,  MFS_REGULAR_FILE, "file.txt") == -1) {
    printf("-1.2\n");
    return -1;
  }
  if (MFS_Lookup(0, "file1.txt") != -1) {
    printf("-1.3\n");
    return -1;
  }
  printf("0\n");
  return 0;

}

