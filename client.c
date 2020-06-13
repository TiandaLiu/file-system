#include "udp.h"
#include "mfs.h"

void lookup_test() {
    if (MFS_Lookup(0, "file.txt") != -1) printf("-1.1\n");
    if (MFS_Creat(0,  MFS_REGULAR_FILE, "file.txt") == -1) printf("-1.2\n");
    if (MFS_Lookup(0, "file1.txt") != -1) printf("-1.3\n");
    printf("0\n");
}
void stat_test() {
    MFS_Stat_t stat;

    MFS_Creat(0, MFS_DIRECTORY, "usr");
    int inode1 = MFS_Lookup(0, "usr");

    MFS_Stat(inode1, &stat);
    printf("type: %d\n", stat.type);
    printf("size: %d\n", stat.size);
    printf("blocks: %d\n", stat.blocks);

    MFS_Creat(inode1, MFS_REGULAR_FILE, "file.txt");
    int inode2 = MFS_Lookup(inode1, "file.txt");

    MFS_Stat(inode1, &stat);
    printf("type: %d\n", stat.type);
    printf("size: %d\n", stat.size);
    printf("blocks: %d\n", stat.blocks);
}

void read_test() {
    int resp;
    resp = MFS_Creat(0, MFS_REGULAR_FILE, "file.txt");
    printf("create resp: %d\n", resp);

    int inode = MFS_Lookup(0, "file.txt");
    printf("inode of file.txt: %d\n", inode);

    char buf[MFS_BLOCK_SIZE];
    memset(buf, 99, MFS_BLOCK_SIZE);

    // printf("buf to write: %s\n", buf);
    // for (int i=0;i<MFS_BLOCK_SIZE;i++){
    //     printf("%d ", buf[i]);
    // }
    // printf("length: %d\n", strlen(buf));

    resp = MFS_Write(inode, buf, 0);

    char result[MFS_BLOCK_SIZE];
    memset(result, 98, MFS_BLOCK_SIZE);
    MFS_Read(inode, result, 0);

    for(int i=0;i<MFS_BLOCK_SIZE;i++){
        if (result[i] != buf[i]) {
            printf("diff index: %d, res: %d, buf: %d\n", i, result[i], buf[i]);
        }
    }

    if (memcmp(result, buf, MFS_BLOCK_SIZE) != 0) {
        printf("diff\n");
    }
}

int test_37(){
    int rc;
  rc = MFS_Creat(0, MFS_DIRECTORY, "dir1");
  if (rc == -1) return -1;

  int inum = MFS_Lookup(0, "dir1");
  if (inum <=0) return -1;
    printf("inum = %d\n", inum);
    
    rc = MFS_Creat(0, MFS_REGULAR_FILE, "whatever");
    if (rc == -1) return -1;
    
    int inum3 = MFS_Lookup(0, "whatever");
    printf("inum3 = %d\n", inum3);
    if (inum3 <= 0 || inum3 == inum) return -1;

    rc = MFS_Creat(0, MFS_DIRECTORY, "dir2");
    if (rc == -1) return -1;

    int inum2 = MFS_Lookup(0, "dir2");
    if (inum2 <=0 || inum2 == inum || inum3 == inum2) return -1;


    rc = MFS_Unlink(0, "whatever");
    if (rc == -1) return -1;

  rc = MFS_Unlink(0, "dir1");
  if (rc == -1) return -1;

  rc = MFS_Lookup(0, "dir1");
  if (rc >= 0) return -1;

    rc = MFS_Lookup(0, "dir2");
    if (rc < 0) return -1;
    printf("here pass\n");

    MFS_Stat_t m;     
  rc = MFS_Stat(inum, &m);
  printf("rc: %d",rc);
  if (rc == 0) return -1;

    rc = MFS_Stat(0, &m);
    if (rc == -1) return -1;
    printf("m.size = %d\n", m.size);

// // sanity check
    if (m.size != 3 * sizeof(MFS_DirEnt_t)) return -1;
    char buf[MFS_BLOCK_SIZE];
    if ((rc = MFS_Read(0, buf, 0)) < 0) return -1;
    printf("bufbuf: %s\n", buf);
    int i;
    MFS_DirEnt_t* e;
    int fd2, fc, fp;
    fd2 = fc = fp = 0;
    for (i = 0; i < 5; i++) {

        e = buf + i * sizeof(MFS_DirEnt_t);
        printf("e.inum = %d\n", e->inum);   
        if (e->inum >= 0) {
            if (strcmp(e->name, ".") == 0) fc = 1;
            if (strcmp(e->name, "..") == 0) fp = 1;
            if (strcmp(e->name, "dir2") == 0) fd2 = 1;
        }   
    }

    if (!fc || !fp || !fd2) return -1;
    return 0;
}

int test_26(){
    char* dirs[] = {"dir0", "dir1", "dir2", "dir3", "dir4", "dir5", "dir6",
        "dir7", "dir8", "dir9", "dir10", "dir11", "dir12", "dir13", ".", ".."};
    int inodes[16]; 
    int i;
    for (i = 0; i < 14; i++) {
    if (MFS_Creat(0, MFS_DIRECTORY, dirs[i]) == -1) 
        return -1;
        inodes[i] = MFS_Lookup(0, dirs[i]);
        if (inodes[i] <= 0) return -1;
    }
    inodes[14] = MFS_Lookup(0, ".");
    if (inodes[14] < 0) return -1;
    inodes[15] = MFS_Lookup(0, "..");
    if (inodes[15] < 0) return -1;

    

    // double check if any inode num is reused
    int j;
    for (i = 0; i < 13; i++)
        for (j = i+1; j < 14; j++)
        if (inodes[i] == inodes[j]) return -1;

    // reread the dir block 
  char buf[MFS_BLOCK_SIZE];
  memset(buf, 0, MFS_BLOCK_SIZE);
  if (MFS_Read(0, buf, 1) == 0)
      return -1;
printf("passed .. .. \n");
  if (MFS_Read(0, buf, 0) == -1)
      return -1;
        

    // sanity check
    int b[16];
    char* dirs2[16];
    int inodes2[16];
    memset(b, 0, 16);
    MFS_DirEnt_t* e;
    for (i = 0; i < 16; i ++) {
        e = buf + i * sizeof(MFS_DirEnt_t);
        inodes2[i] = e->inum;
        dirs2[i] = strdup(e->name);
    }
    for (i = 0; i < 16; i ++) {
        int fi = 0;
        int fn = 0;
        for (j = 0; j < 16; j++) 
            if (inodes[i] == inodes2[j]) {
                fi = 1;
                break;
            }
        for (j = 0; j < 16; j ++)
            if (strcmp(dirs[i], dirs2[j]) == 0) {
                fn = 1;
                break;
            }
            if (!fi || !fn) return -1;
    }
    
  return 0;
}

int main(int argc, char *argv[])
{
    if(argc<3)
    {
        printf("Usage: client server-name server-port\n");
        exit(1);
    }

    int resp;
    MFS_Stat_t m;
    char buffer[BUFFER_SIZE];
    MFS_Init(argv[1], atoi(argv[2]));

    // test_26();
    // test_37();
    read_test();

    // MFS_Init(argv[2], atoi(argv[1]));

    // resp = MFS_Lookup(0, "test.txt");

    // resp = MFS_Stat(0, &m);

    // resp = MFS_Write(0, buffer, 0);

    // resp = MFS_Read(0, buffer, 0);

    // resp = MFS_Creat(0, 1, "test.txt");

    // resp = MFS_Unlink(0, "test.txt");
    return 0;
}