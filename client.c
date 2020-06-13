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
    printf("buf 0 before write = %d\n", buf[0]);
    // printf("buf to write: %s\n", buf);
    // for (int i=0;i<MFS_BLOCK_SIZE;i++){
    //     printf("%d ", buf[i]);
    // }
    // printf("length: %d\n", strlen(buf));

    resp = MFS_Write(inode, buf, 0);
    printf("write of file.txt: %d\n", inode);
    printf("buf 0 after write = %d\n", buf[0]);

    char result[MFS_BLOCK_SIZE];
    memset(result, 98, MFS_BLOCK_SIZE);
    printf("res0 before read: %d\n", result[0]);
    printf("buf0 before read: %d\n", buf[0]);
    MFS_Read(inode, result, 0);
    printf("buf 0 after read = %d\n", buf[0]);
    // printf("read result: %s\n", result);
    for(int i=0;i<MFS_BLOCK_SIZE;i++){
        // printf("res: %d:%d ",i,result[i]);
        // printf("buf: %d:%d ",i,buf[i]);
        if (result[i] != buf[i]) {
            printf("diff index: %d, res: %d, buf: %d\n", i, result[i], buf[i]);
        }
    }
    buf[0] = 'c';
    if (memcmp(result, buf, MFS_BLOCK_SIZE) != 0) {
        printf("diff\n");
    }
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