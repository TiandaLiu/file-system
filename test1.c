#include <stdio.h>
#include "udp.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <math.h>
#include <errno.h>

#define MFS_BLOCK_SIZE (4096)
#define BUFFER_SIZE (8192)
#define COMMAND_NUM 5
#define COMMAND_LEN 16
#define TOTAL_NUM_BYTES 16991232
#define BLOCK_OFFSET 214016
#define INODE_OFFSET 1024
#define BLOCK_BIT_ARR_OFFSET 512
#define INODE_SIZE 52
#define BLOCK_SIZE (4096)
#define ENTRY_SIZE 256
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
int scan_directory(int block_index, char* name,int *delete);
int get_block_index(int inum, int block_num);
int server_Lookup(int pinum, char *name);
int server_Stat(int inum, char *reply);
int server_Write(int inum, char *buffer, int block);
int server_Read(int inum, char *buffer, int block);
int server_Creat(int pinum, int type, char *name);
int server_Unlink(int pinum, char *name); 


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
    // for(int i=0; i < TOTAL_NUM_BYTES; i++){
    //     write(file_desc,&init, 1);
    // }
    fd = file_desc;
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

/* 待会封装一下  get block_indices*/
int server_Lookup(int pinum, char *name) {
    printf("lookup\n");
    int info[3];
    inode_stat(pinum,info);
    int *block_indices = malloc(sizeof(int)*info[2]);
    int offset = INODE_OFFSET+pinum*INODE_SIZE+4*3;
    lseek(fd, offset, SEEK_SET);

    for(int i=0;i<info[2];i++){
        read(fd, &block_indices[i],4);
    }

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
    strcpy(reply, "");
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
    write(fd, buffer, MFS_BLOCK_SIZE);
    printf("finish write: %s\n", buffer);
    return 0;
}



int server_Read(int inum, char *buffer, int block) {
    strcpy(buffer, "");
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
    lseek(fd, offset, SEEK_SET);
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
        char file_content[MFS_BLOCK_SIZE];
        read(fd,file_content,MFS_BLOCK_SIZE);
        printf("file content: %s\n", file_content);

        strcat(buffer,"1;");
        strcat(buffer,file_content);
        printf("buffer: %s\n",buffer);
    }

    
    return 0;
}


int server_Creat(int pinum, int type, char *name) {
    /* judge the validity of the inum*/
    printf("enter create\n");
    if (verify_inum(fd,pinum) == -1)
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
    	entry_index = 0;
    } 

    printf("block_index:%d,entry_index:%d\n",block_index,entry_index);

    // create a new inode for the new file
    int create_inode_index = search_free_inode();
    printf("inode %d\n", create_inode_index);
    if(create_inode_index == -1)
        return -1;
    initiate_inode(create_inode_index, type);

    // update the directory blocks
    offset = BLOCK_OFFSET + block_index * BLOCK_SIZE+entry_index*ENTRY_SIZE;
    // printf("block offset: %d,block_index:%d,entry_index:%d\n", offset,block_index,entry_index);
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

/* delete the file name entry from the parent's table, its inode entry, free corresponding data blocks*/
int server_Unlink(int pinum, char *name) {
     /* judge the validity of the inum*/
    if (verify_inum(fd,pinum) == -1)
        return -1;

    int info[3], offset;
    int stat = inode_stat(pinum, info);

    if(info[0] != MFS_DIRECTORY)
        return -1;

    int *block_indices = malloc(sizeof(int)*info[2]);
   	get_all_blocks(pinum, block_indices);
   	printf("return from get all\n");
    // traverse the blocks to delete the entry
    int inode_num, delete=1;
    printf("info: %d,block_indices[i] %d\n", info[2],block_indices[0]);
    for(int i=0;i<info[2];i++){
        inode_num = scan_directory(block_indices[i],name, &delete);
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
        	}
            break;
        } 
    }
    int inode_info[3];
   	inode_stat(inode_num, inode_info);
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
    while(i<4096){
        char cur_name[252];
        read(fd,&cur_name,252);
        read(fd,&name_index,4);
        if (cur_name == NULL || strlen(cur_name) == 0){
        	i++;
            continue;
        }
        
        printf("name: %s, name_index: %d\n",cur_name,name_index);
        entry_num++;

        if (strcmp(cur_name,name) == 0){
            printf("found: %s\n",cur_name);
            if((*delete) == 0){
                return name_index;
            } else {
            	printf("entry need delete\n");
                offset = BLOCK_OFFSET + block_index * BLOCK_SIZE + i*ENTRY_SIZE;
                lseek(fd,offset,SEEK_SET);
                char empty_byte = 0;
                for(int i=0;i<ENTRY_SIZE;i++){
                	write(fd, &empty_byte, 1);
                }
                res = name_index;
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
    while(i<4096){
    	char cur_name[252];
        read(fd,&cur_name,252);
        read(fd,&name_index,4);
        if (strlen(cur_name) == 0){
        	printf("find empty entry: %d\n", i);
        	empty = i;
            break;
        }
        i++;
    }
    
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
}

int free_inode(int inode_index){
	int byte_num = inode_index/8, bit_num = inode_index%8; 
	int offset = byte_num;
	char block_bit;
	char mask = ~(1<<(7-bit_num));
	lseek(fd,offset,SEEK_SET);
	read(fd, &block_bit, 1);
	block_bit = block_bit & mask;
	lseek(fd,offset,SEEK_SET);
	write(fd, &block_bit, 1);
}

int verify_inum(int fd, int inum) {
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
                printf("new block: %d\n", i*8+k);
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
}

/* ---------------------Functions Related to inode struct-------------------------------------- */

int inode_stat(int inum, int *info){
    int offset = INODE_OFFSET+ inum*INODE_SIZE;
    // printf("stat offset:%d \n", offset);
    lseek(fd,offset,SEEK_SET);
    if (read(fd, info, 4*3) != 4*3)
        return -1;
    // printf(info == NULL);
    for(int i=0;i<3;i++){
        printf("stat: %d\n",info[i]);
    }
    return 0;
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
    printf("block total number: %d %d\n", info[2]);
    return info[2];
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


// int main(int argc, char *argv[])
// {
//     img_file = "image_file.txt";
//     create_img_file();

//     fd = open(img_file, O_RDWR);

//     int info[3];
    
//     int resp;
//     // resp = server_Creat(0,0,"dir1");
//     // printf("resp: %d\n", resp);
//     // resp = server_Creat(0,0,"dir2");
//     // printf("resp: %d\n", resp);
//     // resp = server_Creat(1,1,"dir1's file");
//     // printf("resp: %d\n", resp);
//     // resp = server_Lookup(1, "dir1's file");
//     // printf("resp: %d\n", resp);


// 	// resp = server_Creat(2,1,"dir2's file");
// 	// printf("resp: %d\n", resp);
// 	// resp = server_Creat(2,1,"dir2's file2");
// 	// printf("resp: %d\n", resp);

// 	// resp = server_Creat(0,1,"root's file2");
// 	// printf("resp: %d\n", resp);

//  //    char* buffer = malloc(4096*sizeof(char));
//  //    for(int i=0;i<4096;i++){
//  //        buffer[i] = '\0';
//  //    }
    
//  //    char reply[BUFFER_SIZE];
//  //    for(int i=0;i<BUFFER_SIZE;i++){
//  //        reply[i] = '\0';
//  //    }

//     // resp = server_Write(3,"miaomiao",0);
//     // printf("resp: %d\n", resp);
//     // resp = server_Read(3,buffer,0);
//     // printf("resp: %d\n", resp);
//     // server_Unlink(1,"dir1's file");

// 	check_block_bit(0);
//     check_inode_bit(0);
    
//     resp = server_Lookup(0, "test");
//     printf("lookup resp: %d\n", resp);
//     resp = server_Creat(0, 0, "test");
//     printf("create resp: %d\n", resp);
//     resp = server_Lookup(0, "test");
//     printf("lookup resp: %d\n", resp);

//     resp = server_Lookup(0, "test.txt");
//     printf("lookup resp: %d\n", resp);
//     resp = server_Creat(0, 1, "test.txt");
//     printf("create resp: %d\n", resp);
//     resp = server_Lookup(0, "test.txt");
//     printf("lookup resp: %d\n", resp);

//     resp = server_Lookup(0, "test");
//     printf("lookup resp: %d\n", resp);
//     resp = server_Creat(0, 0, "test");
//     printf("create resp: %d\n", resp);
//     resp = server_Lookup(0, "test");
//     printf("lookup resp: %d\n", resp);
    
//     resp = server_Lookup(0, "test.txt");
//     printf("lookup resp: %d\n", resp);
//     resp = server_Creat(0, 1, "test.txt");
//     printf("create resp: %d\n", resp);
//     resp = server_Lookup(0, "test.txt");
//     printf("lookup resp: %d\n", resp);

//     // resp = server_Lookup(1, "dir1's file");
//     // printf("resp: %d\n", resp);
//     // check_block_bit(0);
//     // check_inode_bit(0);
    
//     close(fd);
    
   
//     // printf("%s", reply);
//     // if(argc<2)
//     // {
//     //     printf("Usage: server server-port-number\n");
//     //     exit(1);
//     // }

//     // int portid = atoi(argv[1]);
//     // int sd = UDP_Open(portid); //port # 
//     // assert(sd > -1);

//     // printf("waiting in loop\n");

//     // while (1) {
//     //     struct sockaddr_in s;
//     //     char buffer[BUFFER_SIZE];
//     //     int rc = UDP_Read(sd, &s, buffer, BUFFER_SIZE); //read message buffer from port sd
//     //     if (rc > 0) {
//     //         printf("SERVER:: read %d bytes (message: '%s')\n", rc, buffer);

//     //         char *all_commands[COMMAND_NUM];

//     //         extract_commands(buffer, all_commands);





//     //         char reply[BUFFER_SIZE];
//     //         sprintf(reply, "reply");
//     //         rc = UDP_Write(sd, &s, reply, BUFFER_SIZE); //write message buffer to port sd
//     //     }
//     // }

//     return 0;
// }

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
            server_Stat(atoi(all_commands[2]), reply);
        } else if (strcmp(all_commands[0], "read") == 0) {
            server_Read(atoi(all_commands[2]), reply, atoi(all_commands[3]));
            printf("read to return: %s\n", reply);
        } else if (strcmp(all_commands[0], "create") == 0) {
            resp = server_Creat(atoi(all_commands[2]), atoi(all_commands[3]), all_commands[4]);
            sprintf(reply, "%d", resp);
            // printf("create resp: %d\n", resp);
        } else if (strcmp(all_commands[0], "unlink") == 0) {
            // resp = server_Unlink(atoi(all_commands[2]), atoi(all_commands[3]));
            sprintf(reply, "unlink");
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
    create_img_file();
    fd = open(img_file, O_RDWR);

    // test();
    start_udp(portid);
    return 0;
}

