#include <stdio.h>
// #include "udp.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <math.h>
#include <errno.h>


#define BUFFER_SIZE (4096)
#define COMMAND_NUM 5
#define COMMAND_LEN 16
#define TOTAL_NUM_BYTES 16991232
#define BLOCK_OFFSET 214016
#define INODE_OFFSET 1024
#define BLOCK_BIT_ARR_OFFSET 512
#define INODE_SIZE 52
#define BLOCK_SIZE 4096
#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

char* img_file;
int fd;

int initiate_inode(int inum, int type);
int inode_stat(int inum, int* info);
int verify_inum(int fd, int inum);
int search_free_block();
int search_free_inode();
int add_new_block(int inum, int* info);
int update_info(int inum, int* info);
int scan_directory(int block_index, char* name);
int get_block_index(int inum, int block_num);

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
    for(int i=0; i < TOTAL_NUM_BYTES; i++){
        write(file_desc,&init, 1);
    }
    
    /* initiate the 0th inode: root directory */
    initiate_inode(0,MFS_DIRECTORY);
    
    /* Change the corresponding bit array */
    lseek(file_desc, 0, SEEK_SET);

    char bit_arr = 1<<7;         // get the modified array
    write(file_desc, &bit_arr, 1);      // write the modified array
    
    // /*check the first inode */
    // lseek(file_desc, 0, SEEK_SET);
    // char check[2];
    // read(file_desc, check, 1);
    // printf("check: %d, bit_array: %d\n", (int) check[0], bit_arr);


    printf("rertu");
    close(file_desc);
    return 0;

}

/* Initiate the inode*/
int initiate_inode(int inum, int type){
    printf("initiate\n");
    /* Initiate the inode block */
    int offset = INODE_OFFSET+inum*INODE_SIZE;
    int info[3] = {type, 0,0};
    lseek(fd,offset,SEEK_SET);
    write(fd, &info, 4*3);
    
    return 0;
}
int server_Lookup(int pinum, char *name) {
    printf("lookup\n");
    int info[3];
    inode_stat(pinum,info);
    printf("block num: %d\n",info[2]);
    int *block_indices = malloc(sizeof(int)*info[2]);
    int offset = INODE_OFFSET+pinum*INODE_SIZE+4*3;
    lseek(fd, offset, SEEK_SET);

    for(int i=0;i<info[2];i++){
        read(fd, &block_indices[i],4);
        printf("block index: %d\n", block_indices[i]);
    }

    // read the blocks to see if the name already exists
    int exists;
    for(int i=0;i<info[2];i++){
        exists = scan_directory(block_indices[i],name);
        if (exists >= 0){
            return exists;
        }
        
    }
    return -1;
}


int server_Stat(int inum, char *reply) {
    int info[3];
    char* info_str[3];
    inode_stat(inum,info);
    printf("here");
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
    if (verify_inum(fd,inum) == -1)
        return -1;
    printf("here");
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
        block_index = add_new_block(inum,info);
        if ( block_index== -1)
            return -1;

        // change the size info
        info[1] += 4096;
        update_info(inum,info);

    } else {
        // obtain the block's index
        offset = INODE_OFFSET+inum*INODE_SIZE+4*3+block*4;
        lseek(fd,offset,SEEK_SET);
        read(fd, &block_index, 4);
    }
    // write the file
    offset = BLOCK_OFFSET + block_index*BLOCK_SIZE;
    printf("block_index: %d, offset: %d \n",block_index, offset);
    lseek(fd,offset,SEEK_SET);
    write(fd, buffer, 4096);
    printf("finish write: %s\n", buffer);
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
                printf("new block: %d\n", i*8+k);
                return i*8+k;
            }
        }
    }
    return -1;
}

int server_Read(int inum, char *buffer, int block) {
    if (verify_inum(fd, inum) == -1)
        return -1;
    int info[3];
    inode_stat(inum,info);

    /* validate the block number */
    if(block>=info[2])
        return -1;

    int block_index = get_block_index(inum, block);
    int offset = BLOCK_OFFSET+block_index*BLOCK_SIZE;
    printf("read offset: %d, blockindex: %d \n", offset,block_index);
    lseek(fd, offset,SEEK_SET);
    int i=0;
    if(info[0] == MFS_DIRECTORY){
        while(i<9){
             i++;
            // char cur_name[252];
            char *cur_name = malloc(sizeof(char)*252);
            int name_index;
            read(fd,cur_name,252);
            read(fd,&name_index,4);
            printf("name: %s, name_index: %d\n",cur_name,name_index);
            if (cur_name == NULL || strlen(cur_name) == 0){
                
                break;
            }
           
            strcat(buffer,cur_name);
            strcat(buffer,",");
            char index_str[4];
            sprintf(index_str, "%d", name_index);
            strcat(buffer,index_str);
            strcat(buffer,";");
            printf("buffer: %s\n",buffer);
        }
            
    } else {
        read(fd,buffer,4096);
        printf("buffer: %s",buffer);
    }

    
    return 0;
}


int server_Creat(int pinum, int type, char *name) {
    /* judge the validity of the inum*/
    if (verify_inum(fd,pinum) == -1)
        return -1;

    int info[3], offset;
    int stat = inode_stat(pinum, info);

    if(info[0] != MFS_DIRECTORY)
        return -1;

    // judge if the name already exists
    int exists = server_Lookup(pinum,name);
    if (exists == 0){
        return 0;
    }

    // if not exists, write the block entry
    int num_block = (info[1]-1)/BLOCK_SIZE, num_entry = info[1]%BLOCK_SIZE;
    int block_index;
    if (num_entry == 0){  // need a new block to store the info

        block_index = add_new_block(pinum,info);
        if(block_index == -1)
            return -1;

        num_block += 1;
    } else {
        block_index = get_block_index(pinum,num_block);
    }

    // create a new inode for the new file
    int create_inode_index = search_free_inode();
    printf("inode %d\n", create_inode_index);
    if(create_inode_index == -1)
        return -1;
    initiate_inode(create_inode_index, type);

    // update the directory blocks
    offset = BLOCK_OFFSET + block_index * BLOCK_SIZE+num_entry;
    printf("block offset: %d,block_index:%d,num_entry:%d\n", offset,block_index,num_entry);
    lseek(fd, offset, SEEK_SET);
    write(fd,name,252);
    write(fd, &create_inode_index,4);

    /* check write*/
    lseek(fd, offset, SEEK_SET);
    char checkname[252];
    int checkindex;
    read(fd, checkname,252);
    read(fd, &checkindex,4);
    printf("check name : %s, checkindex: %d\n", checkname,checkindex);
    // update the size of the directory
    info[1] += 256;
    update_info(pinum, info);

    return 0;
}


int server_Unlink(int pinum, char *name) {
     /* judge the validity of the inum*/
    if (verify_inum(fd,pinum) == -1)
        return -1;

    int info[3], offset;
    int stat = inode_stat(pinum, info);

    if(info[0] != MFS_DIRECTORY)
        return -1;

    int *block_indices = malloc(sizeof(int)*info[2]);
    int offset = INODE_OFFSET+pinum*INODE_SIZE+4*3;
    lseek(fd, offset, SEEK_SET);

    for(int i=0;i<info[2];i++){
        read(fd, &block_indices[i],4);
        printf("block index: %d\n", block_indices[i]);
    }

    // read the blocks to see if the name already exists
    int exists;
    for(int i=0;i<info[2];i++){
        exists = scan_directory(block_indices[i],name);
        if (exists >= 0){
            return exists;
        }
        
    }

     
    return 0;
}

/* ---------------------Functions Related to blocks------------------------------------------ */


/* search a new block and link it with the inode, and change the info about number of blocks
return: index of the block*/
int add_new_block(int inum, int* info){
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
    offset = INODE_OFFSET + inum*INODE_SIZE + 4*2;
    lseek(fd,offset,SEEK_SET);
    info[2] += 1;
    write(fd, &info[2] ,1);
    return block_index;
}

/* scan the content in the directory to fine name
if delete = 1, need to erase this entry in the block
return: n (n>0) the inode number of the name
        0 successfully delete the entry
        -1 cannot find the entry
        -2 find the entry but cannot delete it*/
int scan_directory(int block_index, char* name, int delete){
    int name_index;

    int offset = BLOCK_OFFSET+block_index * BLOCK_SIZE;
    lseek(fd,offset,SEEK_SET);
    printf("scan block_index :%d offset:%d, name: %s\n", block_index,offset,name);
    int i=0;
    while(1){
        char cur_name[252];
        read(fd,&cur_name,252);
        read(fd,&name_index,4);
        printf("name: %s, name_index: %d\n",cur_name,name_index);
        if (cur_name == NULL || strlen(cur_name) == 0)
            break;
        if (strcmp(cur_name,name) == 0){
            printf("found: %s\n",cur_name);
            if(delete == 0){
                return name_index;
            } else {
                
            }
            
        }
        i++;
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
                return i*8+k;
            }
        }
    }
    return -1;
}


/* ---------------------Functions Related to inode struct-------------------------------------- */

int inode_stat(int inum, int *info){
    int offset = INODE_OFFSET+ inum*INODE_SIZE;
    printf("stat offset:%d \n", offset);
    lseek(fd,offset,SEEK_SET);
    if (read(fd, info, 4*3) != 4*3)
        return -1;
    // printf(info == NULL);
    for(int i=0;i<3;i++){
        printf("stat: %d",info[i]);
    }
    return 0;
}

int update_info(int inum, int* info){
    int offset = INODE_OFFSET + inum*INODE_SIZE;
    lseek(fd,offset,SEEK_SET);
    write(fd, info, 4*3);
}

int get_block_index(int inum, int block_num){
    int block_index;
    int offset = INODE_OFFSET+inum*INODE_SIZE+4*3+block_num*4;
    lseek(fd,offset,SEEK_SET);
    read(fd, &block_index, 4);
    return block_index;
}


int verify_inum(int fd, int inum) {
    printf("verify\n");
    int byte_num = inum/8, bit_num = inum%8; 
    char mask = 1<<(7-bit_num);
    char block_bit;
    lseek(fd,byte_num,SEEK_SET);
    read(fd, &block_bit, 1);
    printf("block: %d\n",block_bit);
    if( (block_bit & mask) == 0)
        return -1;
    return 0;
}


int main(int argc, char *argv[])
{
    img_file = "image_file.txt";
    // create_img_file();

    fd = open(img_file, O_RDWR);
    // initiate_inode(0,MFS_DIRECTORY);
    // lseek(fd, 0,SEEK_SET);
    // char bit[2];
    // read(fd,bit,2);
    // for(int i=0;i<2;i++){
    //     printf("bit: %d\n",bit[i]);
    // }
    int info[3];
    // info[0] = 0;
    // info[1] = 256;
    // info[2] = 1;
    // read(fd, &info,4*3);
    
    // update_info(0,info);
    // inode_stat(3,info);
    // for(int i=0;i<3;i++){
    //     printf("info %d\n", info[i]);
    // }
    // printf("blcok index %d\n",get_block_index(0,0));
    // printf("here fd %d\n", fd);
    // server_Creat(0,0,"dir1");
    // server_Lookup(0,"hahah");
    // server_Write(3,"hahah's content hahahhah",0);
    // inode_stat(3,info);
    // for(int i=0;i<3;i++){
    //     printf("info %d\n", info[i]);
    // }
    char* buffer = malloc(4096*sizeof(char));
    for(int i=0;i<4096;i++){
        buffer[i] = '\0';
    }
    
    char reply[BUFFER_SIZE];
    for(int i=0;i<BUFFER_SIZE;i++){
        reply[i] = '\0';
    }


    // server_Read(0,buffer,0);
    // // server_Stat(4,reply);
    // inode_stat(4, info);
    // server_Creat(4,MFS_REGULAR_FILE,"dir1's filewa");
    server_Write(2,"miumiu's  content miumiumiu",8);
    server_Read(2,buffer,7);
    close(fd);
    
    // // printf("here");
    // // server_Stat(0,"image_file",reply);


    // int fd = open (img_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    // // close(fd);
    // // fprintf(stderr, "Error opening file: %s\n", strerror( errno ));
    // printf("fd: %d\n", fd);
    // // printf("here");
    // int num1 = 15;
    // // fd = open (img_file, O_RDWR);
    // for(int i=0; i < 3; i++){
    //     printf("%d\n",write(fd, &num1, sizeof(num1)));
    //     fprintf(stderr, "Error opening file: %s\n", strerror( errno ));
    // }
    // close(fd);

    // fd = open(img_file, O_RDONLY);
    // int new_val;
    // for (int i=0;i<3;i++){
    //     read(fd, &new_val, sizeof(new_val));
    //     printf("new_val = %d\n", new_val);
        
    // }
    // close(fd);


    // char a = 48;
    // char b = a | 1<<2;
    // printf("a: %c, b:%c",a,b);



    // printf("%s", reply);
    // if(argc<2)
    // {
    //     printf("Usage: server server-port-number\n");
    //     exit(1);
    // }

    // int portid = atoi(argv[1]);
    // int sd = UDP_Open(portid); //port # 
    // assert(sd > -1);

    // printf("waiting in loop\n");

    // while (1) {
    //     struct sockaddr_in s;
    //     char buffer[BUFFER_SIZE];
    //     int rc = UDP_Read(sd, &s, buffer, BUFFER_SIZE); //read message buffer from port sd
    //     if (rc > 0) {
    //         printf("SERVER:: read %d bytes (message: '%s')\n", rc, buffer);

    //         char *all_commands[COMMAND_NUM];

    //         extract_commands(buffer, all_commands);





    //         char reply[BUFFER_SIZE];
    //         sprintf(reply, "reply");
    //         rc = UDP_Write(sd, &s, reply, BUFFER_SIZE); //write message buffer to port sd
    //     }
    // }

    return 0;
}


