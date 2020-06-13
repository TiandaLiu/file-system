#include <stdio.h>
#include "udp.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <math.h>
#include <errno.h>
// #include "mfs.h"

#define MFS_BLOCK_SIZE (4096)
#define BUFFER_SIZE (8192)
#define COMMAND_NUM 5
#define COMMAND_LEN 16
#define TOTAL_NUM_BYTES 16991232
#define BLOCK_OFFSET 214016
#define INODE_OFFSET 1024
#define BLOCK_BIT_ARR_OFFSET 512
#define INODE_SIZE 52
#define BLOCK_SIZE 4096
#define ENTRY_SIZE 256
#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

typedef struct __MFS_DirEnt_t {
    int  inum;      // inode number of entry (-1 means entry not used)
    char name[252]; // up to 252 bytes of name in directory (including \0)
} MFS_DirEnt_t;

char* img_file;
int fd;

int server_Lookup(int pinum, char *name);
int server_Stat(int inum, char *reply);
int server_Write(int inum, char *buffer, int block);
int server_Read(int inum, char *buffer, int block);
int server_Creat(int pinum, int type, char *name);
int server_Unlink(int pinum, char *name);

int find_empty_entry(int pinum, int *block_index, int *entry_index);
int add_new_block(int inum, int *info);
int add_entry(int inum, int block_index, int entry_index, char* name, int inode_index, int* info);
int scan_directory(int block_index, char* name, int *delete);
int scan_directory_empty(int block_index);
int free_block(int block_index);
int free_inode(int inode_index);
int verify_inum(int inum);
int search_free_block();
int search_free_inode();
int check_block_bit(int index);
int check_inode_bit(int index);
int initiate_inode(int inum, int type, int pinum);
int inode_stat(int inum, int *info);
int get_all_blocks(int inum, int *block_indices);
int update_info(int inum, int* info);
int get_block_index(int inum, int block_num);
int extract_commands(char* input, char** all_commands);
int initiate_inode(int inum, int type, int pinum);



/* 待会封装一下  get block_indices*/
int server_Lookup(int pinum, char *name) {
    printf("lookup\n");
    if (verify_inum(pinum)==-1){
        return -1;
    }

    int info[3];
    inode_stat(pinum,info);

    if(info[0] != MFS_DIRECTORY){
        return -1;
    }

    int *block_indices = malloc(sizeof(int)*info[2]);
    get_all_blocks(pinum, block_indices);

    // read the blocks to see if the name already exists
    int exists,delete = 0;
    for(int i=0;i<info[2];i++){
        exists = scan_directory(block_indices[i],name, &delete);
        if (exists >= 0){
            return exists;
        }
        
    }
    return -1;
}


int server_Stat(int inum, char *reply) {
    int info[3];
    char* info_str[3];
    if (inode_stat(inum,info) == -1)
        return -1;
    strcpy(reply, "");
    for(int i=0;i<3;i++){
        info_str[i] = malloc(sizeof(char)*4);
        sprintf(info_str[i], "%d", info[i]);
        strcat(reply,info_str[i]);
        if (i<2)
            strcat(reply,";");
        printf("info: %d, reply:%s\n", info[i], reply);
    }

    return 0;
}



int server_Write(int inum, char *buffer, int block) {

    /* judge the validity of the inum*/
    if (verify_inum(inum) == -1)
        return -1;
    int info[3], offset;
    inode_stat(inum, info);

    /* judge the validity of the request*/
    if (info[0] != MFS_REGULAR_FILE)
        return -1;
    if (block>info[2] || block>=10 || block<0)
        return -1;

    
    /* find the block index and update the inode*/
    int block_index;
    if (block == info[2]){
        // find and add a free block linked to the inode
        block_index = add_new_block(inum, info);
        if ( block_index== -1)
            return -1;

        // change the size info
        info[1] += 4096;
        update_info(inum,info);

    } else {
        // obtain the block's index
        block_index = get_block_index(inum,block);
        // offset = INODE_OFFSET+inum*INODE_SIZE+4*3+block*4;
        // lseek(fd,offset,SEEK_SET);
        // read(fd, &block_index, 4);
    }
    // write the file
    offset = BLOCK_OFFSET + block_index*BLOCK_SIZE;
    printf("block_index: %d, offset: %d \n",block_index, offset);
    lseek(fd,offset,SEEK_SET);
    write(fd, buffer, 4096);
    printf("finish write: %s\n", buffer);
    fsync(fd);
    return 0;
}



int server_Read(int inum, char *buffer, int block) {
    if (verify_inum(inum) == -1)
        return -1;
    int info[3];
    strcpy(buffer, "");
    inode_stat(inum,info);
    MFS_DirEnt_t* e;

    /* validate the block number */
    if(block>=info[2])
        return -1;

    int block_index = get_block_index(inum, block);
    int offset = BLOCK_OFFSET+block_index*BLOCK_SIZE;
    printf("read offset: %d, blockindex: %d \n", offset,block_index);
    lseek(fd, offset,SEEK_SET);
    int i=0, entry_num = 0;
    if(info[0] == MFS_DIRECTORY){
        strcat(buffer, "0;");
        while(i<16){
            i++;
            MFS_DirEnt_t *entry = malloc(sizeof(MFS_DirEnt_t));
            read(fd, entry, 256);
            printf("name: %s, name_index: %d len: %lu i%d\n",entry->name,entry->inum,strlen(entry->name),i);

            if (strlen(entry->name) == 0){
                continue;
            }
            char *entry_buffer = malloc(sizeof(MFS_DirEnt_t));
            // printf("entry: %d\n",entry);
            memcpy(entry_buffer, entry, ENTRY_SIZE);
            for(int k=0;k<256;k++){
                buffer[2+k+entry_num*ENTRY_SIZE] = entry_buffer[k];
            }
            entry_num++;
        }
       
    } else {
        char file_content[MFS_BLOCK_SIZE];
        read(fd,file_content,MFS_BLOCK_SIZE);

        strcat(buffer, "1;");
        strcat(buffer, file_content);
    }

    
    return 0;
}



int server_Creat(int pinum, int type, char *name) {
    /* judge the validity of the inum*/
    printf("enter create\n");
    if (verify_inum(pinum) == -1)
        return -1;

    int info[3], offset;
    int stat = inode_stat(pinum, info);

    if(info[0] != MFS_DIRECTORY)
        return -1;

    // judge if the name already exists
    int exists = server_Lookup(pinum,name);
    if (exists >= 0){
        return 0;
    }

    // if not exists, find an empty entry in the directory's blocks
    int block_index, entry_index;
    if (find_empty_entry(pinum, &block_index, &entry_index) == -1){
        // if no empty entry in the blocks, add a new block to the directory
        block_index = add_new_block(pinum,info);
        if(block_index == -1)
            return -1;
        entry_index = 0;
    }

    printf("block_index:%d,entry_index:%d\n",block_index,entry_index);

    // create a new inode for the new file
    int create_inode_index = search_free_inode();
    printf("inode %d\n", create_inode_index);
    if(create_inode_index == -1)
        return -1;
    initiate_inode(create_inode_index, type,pinum);

    // update the directory blocks
    add_entry(pinum, block_index, entry_index, name, create_inode_index, info);

    fsync(fd);
    return 0;
}

/* delete the file name entry from the parent's table, its inode entry, free corresponding data blocks*/
int server_Unlink(int pinum, char *name) {
     /* judge the validity of the inum*/
    if (verify_inum(pinum) == -1)
        return -1;

    int info[3], offset;
    int stat = inode_stat(pinum, info);

    if(info[0] != MFS_DIRECTORY)
        return -1;

    int *block_indices = malloc(sizeof(int)*info[2]);
       get_all_blocks(pinum, block_indices);
       // printf("return from get all\n");
    // traverse the blocks to delete the entry
    int inode_num, delete=1;
    printf("info: %d\n", info[2]);
    int inode_info[3];
    for(int i=0;i<info[2];i++){
        inode_num = scan_directory(block_indices[i],name, &delete);

        if (inode_num == -1)
            return 0;

        inode_stat(inode_num, inode_info);

        if (inode_info[0] == MFS_DIRECTORY && inode_info[1]>2*ENTRY_SIZE)
            return -1;

        if (inode_num >= 0){
            printf("unlink find the inode: %d\n", inode_num);
            if(delete == 2){   // need to delete this block
                printf("need to delete the block\n");
                // if the block is the last block
                if (i == info[2]-1){
                    printf("the last block\n");
                    offset = INODE_OFFSET+pinum*INODE_SIZE+4*3+(info[2]-1)*4;
                    lseek(fd,offset,SEEK_SET);
                    int empty_index = 0;
                    write(fd, &empty_index,4);
                } else {
                    // read the last block and move the last block
                    printf("not the last block\n");
                    int last_index = get_block_index(pinum, info[2]-1);
                    offset = INODE_OFFSET+pinum*INODE_SIZE+4*3+i*4;
                    lseek(fd, offset, SEEK_SET);
                    write(fd,&last_index,4);
                }
                info[2]--;
                free_block(block_indices[i]);
            }
            break;
        }
    }
    
    int *inode_block_indices = malloc(sizeof(int)*inode_info[2]);
    get_all_blocks(inode_num, inode_block_indices);

    // free the blocks of the deleted
    for(int i=0;i<inode_info[2];i++){
        free_block(inode_block_indices[i]);
    }

    // free the inode of the deleted
    free_inode(inode_num);

    // update the inode info struct of the pinum
    info[1] -= 256;
    update_info(pinum,info);

    fsync(fd);
    return 0;
}

/* ---------------------Functions Related to blocks------------------------------------------ */

/* search a empty entry in directory blocks, return -1 if no empty entry, 0 if find
block_index points to the index of the block which has empty entry
entry_index points to the index of the empty entry in the block*/
int find_empty_entry(int pinum, int *block_index, int *entry_index){
    int info[3];
    int stat = inode_stat(pinum, info);
    int *block_indices = malloc(sizeof(int)*info[2]);
    get_all_blocks(pinum, block_indices);
    int empty;
    for(int i=0;i<info[2];i++){
        empty = scan_directory_empty(block_indices[i]);
        if (empty >= 0){
            *block_index = block_indices[i];
            *entry_index = empty;
            return empty;
        }
        
    }
    return -1;
}

/* search a new block and link it with the inode, and change the info about number of blocks
return: index of the block*/
int add_new_block(int inum, int *info){
    if(info[2] == 10)
        return -1;
    printf("add new block inum: %d\n", inum);
    int offset;
    int block_index = search_free_block();
    if (block_index == -1)
        return -1;

    // link the block with the file
    offset = INODE_OFFSET + inum*INODE_SIZE + 4*3 + 4*info[2];
    lseek(fd,offset,SEEK_SET);
    write(fd, &block_index, 4);

    // update the blocks variable in info
    // offset = INODE_OFFSET + inum*INODE_SIZE + 4*2;
    // lseek(fd,offset,SEEK_SET);
    info[2] += 1;
    update_info(inum,info);
    // write(fd, &info[2] ,1);
    return block_index;
}

/* write entry into blocks of a directory*/
int add_entry(int inum, int block_index, int entry_index, char* name, int inode_index, int* info){
    int offset = BLOCK_OFFSET + block_index * BLOCK_SIZE+entry_index*ENTRY_SIZE;
    // printf("block offset: %d,block_index:%d,entry_index:%d,name: %s, inode_index: %d\n", offset,block_index,entry_index,name,inode_index);
    MFS_DirEnt_t *entry = malloc(sizeof(MFS_DirEnt_t));
    strncpy(entry->name, name,252);
    entry->inum = inode_index;
    printf("add_entry: entry name: %s, inum: %d, block index:%d,entry_index:%d,offset:%d\n", entry->name,entry->inum, block_index,entry_index,offset);
    lseek(fd, offset, SEEK_SET);
    write(fd,entry,256);
    // int resp = write(fd,name,252);
    // printf("write: %d", resp);
    // resp = write(fd, &inode_index,4);
    // printf("write: %d", resp);

    
    info[1] += 256;
    update_info(inum, info);

    return 0;
}


/* scan the block in the directory to fine name
if delete = 1, need to erase this entry in the block
return: n (n>0) the inode number of the name
        -1 cannot find the entry
*/
int scan_directory(int block_index, char* name, int *delete){
    printf("enther scan\n");
    int name_index,res=-1;

    int offset = BLOCK_OFFSET+block_index * BLOCK_SIZE;
    lseek(fd,offset,SEEK_SET);
    printf("scan block_index :%d offset:%d\n", block_index,offset);
    int i=0, entry_num=0;
    while(i<16){
        MFS_DirEnt_t *entry = malloc(sizeof(MFS_DirEnt_t));
        read(fd, entry, 256);
        printf("name: %s, name_index: %d len: %lu i%d\n",entry->name,entry->inum,strlen(entry->name),i);
        entry_num++;

        if (strcmp(entry->name,name) == 0){
            printf("found: %s\n",entry->name);
            if((*delete) == 0){
                res = entry->inum;
                // return name_index;
            } else {
                printf("entry need delete\n");
                offset = BLOCK_OFFSET + block_index * BLOCK_SIZE + i*ENTRY_SIZE;
                lseek(fd,offset,SEEK_SET);
                char empty_byte = 0;
                for(int i=0;i<ENTRY_SIZE;i++){
                    write(fd, &empty_byte, 1);
                }
                res = entry->inum;
                entry_num--;
            }
            
        }

        i++;
    }
    if (res != -1){
        // the block is totally empty now, needs to erase
        if (entry_num == 0){
            *delete = 2;
        } else {
            *delete = -1;
        }
        return res;
    }
    return -1;
}

/* scan the block in the directory to find empty entry
if no empty block, return -1
else return the index of the entry that is empty*/
int scan_directory_empty(int block_index){
    int offset = BLOCK_OFFSET+block_index * BLOCK_SIZE, name_index;
    lseek(fd,offset,SEEK_SET);
    int i=0, empty=-1;
    while(i<16){
        MFS_DirEnt_t *entry = malloc(sizeof(MFS_DirEnt_t));
        read(fd, entry, 256);
        printf("name: %s, name_index: %d len: %lu i%d\n",entry->name,entry->inum,strlen(entry->name),i);
        if (strlen(entry->name) == 0){
            printf("find empty entry: %d\n", i);
            empty = i;
            break;
        }
        i++;
    }
    if(empty == -1)
        printf("no empty entry:");
    return empty;
}


/* ---------------------Functions Related to bit array-------------------------------------- */
/* change the bit of the block_index to 0*/
int free_block(int block_index){
    int byte_num = block_index/8, bit_num = block_index%8;
    int offset = BLOCK_BIT_ARR_OFFSET+byte_num;
    char block_bit;
    char mask = ~(1<<(7-bit_num));
    lseek(fd,offset,SEEK_SET);
    read(fd, &block_bit, 1);
    block_bit = block_bit & mask;
    lseek(fd,offset,SEEK_SET);
    write(fd, &block_bit, 1);

    printf("free block %d\n", block_index);
    return 0;
}

int free_inode(int inode_index){
    printf("free inode: %d\n", inode_index);
    int byte_num = inode_index/8, bit_num = inode_index%8;
    int offset = byte_num;
    char block_bit;
    char mask = ~(1<<(7-bit_num));
    lseek(fd,offset,SEEK_SET);
    read(fd, &block_bit, 1);
    block_bit = block_bit & mask;
    lseek(fd,offset,SEEK_SET);
    write(fd, &block_bit, 1);
    verify_inum(inode_index);
    return 0;
}

/* verify if the inode number is a valid inode number
   return 0, if valid
   return 1, if invalid
*/
int verify_inum(int inum) {
    printf("verify\n");
    if (inum < 0 || inum >= 4096) {
        return -1;
    }
    int byte_num = inum/8, bit_num = inum%8;
    char mask = 1<<(7-bit_num);
    char block_bit;
    lseek(fd,byte_num,SEEK_SET);
    read(fd, &block_bit, 1);
    // printf("block: %d\n",block_bit);
    if( (block_bit & mask) == 0)
        return -1;
    printf("verify success\n");
    return 0;
}

/* search a free block, and once find a valid free block, fill the bit array*/
int search_free_block(){
    int offset = BLOCK_BIT_ARR_OFFSET;
    lseek(fd,offset,SEEK_SET);
    char cur;
    char mask;
    for(int i=0;i<4096;i++){
        read(fd, &cur, 1);
        // find the free bit
        for(int k = 0;k<8;k++){
            mask = 1 << (7-k);
            if ( (cur & mask) == 0){
                lseek(fd,BLOCK_BIT_ARR_OFFSET+i,SEEK_SET);
                cur = cur | mask;
                write(fd, &cur, 1);
                printf("search new block: %d\n", i*8+k);
                return i*8+k;
            }
        }
    }
    return -1;
}

int search_free_inode(){
    lseek(fd,0,SEEK_SET);
    char cur;
    char mask;
    for(int i=0;i<4096;i++){
        read(fd, &cur, 1);
        // find the free bit
        for(int k = 0;k<8;k++){
            mask = 1 << (7-k);
            if ( (cur & mask) == 0){
                lseek(fd,i,SEEK_SET);
                cur = cur | mask;
                write(fd, &cur, 1);
                printf("search new inode: %d\n", i*8+k);
                return i*8+k;
            }
        }
    }
    return -1;
}

int check_block_bit(int index){
    char block_bit;
    int offset = BLOCK_BIT_ARR_OFFSET+index;
    lseek(fd,offset,SEEK_SET);
    read(fd, &block_bit, 1);
    int bits[8];
    for(int i=0;i<8;i++){
        bits[i] = block_bit%2;
        block_bit = block_bit/2;
    }
    for(int i=7;i>=0;i--){
        printf("%d", bits[i]);
    }
    printf("\n");
    return 0;
}

int check_inode_bit(int index){
    char block_bit;
    int offset = index;
    lseek(fd,offset,SEEK_SET);
    read(fd, &block_bit, 1);
    int bits[8];
    for(int i=0;i<8;i++){
        bits[i] = block_bit%2;
        block_bit = block_bit/2;
    }
    for(int i=7;i>=0;i--){
        printf("%d", bits[i]);
    }
    printf("\n");
    return 0;
}

/* ---------------------Functions Related to inode struct-------------------------------------- */

/* Initiate the inode*/
int initiate_inode(int inum, int type, int pinum){
    
    /* Initiate the inode block */
    int offset = INODE_OFFSET+inum*INODE_SIZE;
    printf("initiate type: %d, offset:%d\n",type, offset);
    int info[3] = {type, 0,0};
    update_info(inum, info);

    /* if directory, add entries . and .. */
    if(type == MFS_DIRECTORY){
        int dir_block = add_new_block(inum, info);
        add_entry(inum, dir_block, 0, ".",inum,info);
        add_entry(inum, dir_block,1,"..",pinum,info);
    }
   

    return 0;
}

int inode_stat(int inum, int *info){
    if (verify_inum(inum) == -1){
        return -1;
    }
    int offset = INODE_OFFSET+ inum*INODE_SIZE;
    printf("stat offset:%d \n", offset);
    lseek(fd,offset,SEEK_SET);
    if (read(fd, info, 4*3) != 4*3)
        return -1;
    // printf(info == NULL);
    for(int i=0;i<3;i++){
        printf("stat: %d\n",info[i]);
    }
    return 0;
}


int update_info(int inum, int* info){
    int offset = INODE_OFFSET + inum*INODE_SIZE;
    lseek(fd,offset,SEEK_SET);
    write(fd, info, 4*3);
    inode_stat(inum,info);
    return 0;
}

int get_block_index(int inum, int block_num){
    int block_index;
    int offset = INODE_OFFSET+inum*INODE_SIZE+4*3+block_num*4;
    lseek(fd,offset,SEEK_SET);
    read(fd, &block_index, 4);
    return block_index;
}

int get_all_blocks(int inum, int *block_indices){
    printf("get_all blocks\n");
    int info[3], offset;
    int stat = inode_stat(inum, info);
    // block_indices = malloc(sizeof(int)*info[2]);
    offset = INODE_OFFSET+inum*INODE_SIZE+4*3;
    lseek(fd, offset, SEEK_SET);

    for(int i=0;i<info[2];i++){
        read(fd, &block_indices[i],4);
        printf("block index: %d\n", block_indices[i]);
    }
    printf("block total number: %d\n", info[2]);
    return info[2];
}

/* --------------------- Other Functions --------------------------------------  */
int extract_commands(char* input, char** all_commands){
    int concurrent=0;
    const char delim[3]= ";";
    
    // divide multiple commands
    char *command = strtok(input, delim);
    int i=0;
    while(command != NULL ) {
        // printf("extract all: %s input %s\n", command,input);
        all_commands[i] = (char *) malloc(sizeof(char)*COMMAND_LEN);
        all_commands[i++] = command;
        command = strtok(NULL, delim);
    }
    all_commands[i] = NULL;
    // printf("extract_all finish\n");
    return 0;
}

int create_img_file(){
    int file_desc = open (img_file, O_RDWR | O_CREAT | O_TRUNC, 0777);
    char init =  0;
    printf("here file_desc: %d\n",file_desc);
    // for(int i=0; i < 4096; i++){
    //     write(file_desc,&init, 1);
    // }
    
    fd = file_desc;
    // close(file_desc);

    // file_desc = open(img_file,O_RDWR);
    
    /* Change the corresponding bit array */
    lseek(file_desc, 0, SEEK_SET);

    char bit_arr = (char) 1<<7;         // get the modified array
    write(file_desc, &bit_arr, 1);      // write the modified array
    
    /* initiate the 0th inode: root directory */
    initiate_inode(0,MFS_DIRECTORY,0);

    // /*check the first inode */
    // lseek(file_desc, 0, SEEK_SET);
    // char check;
    // read(file_desc, &check, 1);
    // printf("check: %d, bit_array: %d\n", (int) check, bit_arr);

    // check the first inode
    // lseek(file_desc, 1024, SEEK_SET);
    // int checkinfo;
    // for(int i=0;i<3;i++){
    //     int resp = read(file_desc, &checkinfo, 4);
    //     printf("read:%d, check: %d\n",resp,checkinfo);
    // }
    

    printf("rertu");
    close(file_desc);
    return 0;

}



void test() {
    int resp;
    resp = server_Creat(0, MFS_REGULAR_FILE, "file.txt");
    printf("create resp: %d\n", resp);

    int inode = server_Lookup(0, "file.txt");
    printf("inode of file.txt: %d\n", inode);

    char buf[MFS_BLOCK_SIZE];
    memset(buf, 99, MFS_BLOCK_SIZE);
    printf("buf to write: %s\n", buf);

    resp = server_Write(inode, buf, 0);
    printf("write resp: %d\n", resp);

    char result[MFS_BLOCK_SIZE];
    memset(result, 98, MFS_BLOCK_SIZE);
    resp = server_Read(inode, result, 0);
    printf("read resp: %d\n", resp);

    printf("read result: %s\n", result);
    if (memcmp(result, buf, MFS_BLOCK_SIZE) != 0) {
        printf("diff\n");
    }
}

void start_udp(portid) {

    // int portid = atoi(argv[1]);
    int sd = UDP_Open(portid); //port # 
    assert(sd > -1);
    printf("waiting in loop\n");

    while (1) {
    struct sockaddr_in s;
    char buffer[BUFFER_SIZE];
    int rc = UDP_Read(sd, &s, buffer, BUFFER_SIZE); //read message buffer from port sd
    if (rc > 0) {
        char *all_commands[COMMAND_NUM], reply[BUFFER_SIZE];
        int resp;
        printf("SERVER:: read %d bytes (message: '%s')\n", rc, buffer);
        extract_commands(buffer, all_commands);
        if (strcmp(all_commands[0], "lookup") == 0) {
            resp = server_Lookup(atoi(all_commands[2]), all_commands[3]);
            sprintf(reply, "%d", resp);
            // printf("lookup resp: %d\n", resp);
        } else if (strcmp(all_commands[0], "stat") == 0) {
            resp = server_Stat(atoi(all_commands[2]), reply);
            if(resp == -1) {
                strcpy(reply, "-1;");
            }
        } else if (strcmp(all_commands[0], "read") == 0) {
            resp = server_Read(atoi(all_commands[2]), reply, atoi(all_commands[3]));
            if(resp == -1) {
                strcpy(reply, "2;");
            }
            // for(int i=0;i<4098;i++){
            //     printf("i %d: %d ",reply[i],i-2);
            // }
            printf("read to return: %s\n", reply);
        } else if (strcmp(all_commands[0], "create") == 0) {
            resp = server_Creat(atoi(all_commands[2]), atoi(all_commands[3]), all_commands[4]);
            sprintf(reply, "%d", resp);
            // printf("create resp: %d\n", resp);
        } else if (strcmp(all_commands[0], "unlink") == 0) {
            resp = server_Unlink(atoi(all_commands[2]), all_commands[3]);
            sprintf(reply, "%d", resp);
        } else {
            printf("write content from client: %s\n", all_commands[4]);
            printf("length: %d\n", strlen(all_commands[4]));
            resp = server_Write(atoi(all_commands[2]), all_commands[4], atoi(all_commands[3]));
            sprintf(reply, "%d", resp);
        }
        // printf("Operation type: %s\n", reply);
        rc = UDP_Write(sd, &s, reply, BUFFER_SIZE); //write message buffer to port sd
    }
    }
}

int main(int argc, char *argv[]) {
    if(argc < 3) {
      printf("Usage: server server-port-number img_file_name\n");
      exit(1);
    }

    int portid = atoi(argv[1]);
    img_file = argv[2];

    if( access( img_file, F_OK ) != -1 ) {
        printf("img exists\n");
    } else {
        // file doesn't exist
        printf("img not exists\n");
        create_img_file();
    }
    fd = open(img_file, O_RDWR);

    // test();
    start_udp(portid);
    return 0;
}