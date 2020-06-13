#include "mfs.h"

int split_string(char* input, char** output){
    int concurrent=0;
    const char delim[3]= ";";
    
    // divide multiple commands
    char *command = strtok(input, delim);       
    int i=0;
    while(command != NULL) {
        output[i] = (char *) malloc(sizeof(char)*COMMAND_LEN);
        output[i++] = command;
        command = strtok(NULL, delim);
    }
    output[i] = NULL;
    return 0;
}

int MFS_Init(char *hostname, int port) {
    SOCKET = UDP_Open(20000); //communicate through specified port, add random num later
    int rc = UDP_FillSockAddr(&addr, hostname, port);
    if (rc < 0) return -1;
    return 0;
}

int MFS_Lookup(int pinum, char *name) {
    // return inum of file if success, else -1
    int num_argument = 2, inum = -1;
    char *method_name = "lookup";
    char message[BUFFER_SIZE];
    sprintf(message, "%s;%d;%d;%s", method_name, num_argument, pinum, name);
    int rc = UDP_Write(SOCKET, &addr, message, BUFFER_SIZE);
    if (rc > 0) {
        char recv_buffer[BUFFER_SIZE];
        int rc = UDP_Read(SOCKET, &addr2, recv_buffer, BUFFER_SIZE);
        printf("lookup return value: %d\n", atoi(recv_buffer));
        // check recv_buffer for validation
        inum = atoi(recv_buffer);
    }
    return inum;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    // return 0 if success, else -1
    char *method_name = "stat";
    int num_argument = 1, ret = -1;
    char message[BUFFER_SIZE];
    sprintf(message, "%s;%d;%d", method_name, num_argument, inum);
    int rc = UDP_Write(SOCKET, &addr, message, BUFFER_SIZE);
    if (rc > 0) {
        char recv_buffer[BUFFER_SIZE];
        int rc = UDP_Read(SOCKET, &addr2, recv_buffer, BUFFER_SIZE);
        // check recv_buffer for validation
        char *output[COMMAND_NUM];
        split_string(recv_buffer, output);
        m->type = atoi(output[0]);
        m->size = atoi(output[1]);
        m->blocks = atoi(output[2]);
        ret = 0; // success
        // load data from recv_buffer to MFS_stat_t, need to be done
    }
    return ret;
}

int MFS_Write(int inum, char *buffer, int block) {
    // return 0 if success, else -1
    char *method_name = "write";
    int num_argument = 3, ret = -1;
    // need to be changed, if the size of the buffer is exactly 4096, we may need to allocate more space on message
    char message[BUFFER_SIZE]; 
    sprintf(message, "%s;%d;%d;%d;%s", method_name, num_argument, inum, block, buffer);
    int rc = UDP_Write(SOCKET, &addr, message, BUFFER_SIZE);
    if (rc > 0) {
        char recv_buffer[BUFFER_SIZE];
        int rc = UDP_Read(SOCKET, &addr2, recv_buffer, BUFFER_SIZE);
        printf("write return value: %d\n", atoi(recv_buffer));
        // check recv_buffer for validation
        ret = 0; // success
    }
    return ret;
}

int MFS_Read(int inum, char *buffer, int block) {
    // return 0 if success, else -1
    char *method_name = "read";
    int num_argument = 2, ret = -1;
    char message[BUFFER_SIZE];
    sprintf(message, "%s;%d;%d;%d", method_name, num_argument, inum, block);
    int rc = UDP_Write(SOCKET, &addr, message, BUFFER_SIZE);
    if (rc > 0) {
        char recv_buffer[BUFFER_SIZE];
        int rc = UDP_Read(SOCKET, &addr2, recv_buffer, BUFFER_SIZE);
        // check recv_buffer for validation
        strcpy(buffer, recv_buffer);
        ret = 0; // success
        // load data from recv_buffer to *recv_buffer, need to be done
    }
    return ret;
}

int MFS_Creat(int pinum, int type, char *name) {
    // return 0 if success, else -1
    char *method_name = "create";
    int num_argument = 3, ret = -1;
    char message[BUFFER_SIZE];
    sprintf(message, "%s;%d;%d;%d;%s", method_name, num_argument, pinum, type, name);
    int rc = UDP_Write(SOCKET, &addr, message, BUFFER_SIZE);
    if (rc > 0) {
        char recv_buffer[BUFFER_SIZE];
        int rc = UDP_Read(SOCKET, &addr2, recv_buffer, BUFFER_SIZE);
        printf("create return value: %d\n", atoi(recv_buffer));
        // check recv_buffer for validation
        ret = 0; // success
    }
    return ret;
}

int MFS_Unlink(int pinum, char *name) {
    // return 0 if success, else -1
    char *method_name = "unlink";
    int num_argument = 2, ret = -1;
    char message[BUFFER_SIZE];
    sprintf(message, "%s;%d;%d;%s", method_name, num_argument, pinum, name);
    int rc = UDP_Write(SOCKET, &addr, message, BUFFER_SIZE);
    if (rc > 0) {
        char recv_buffer[BUFFER_SIZE];
        int rc = UDP_Read(SOCKET, &addr2, recv_buffer, BUFFER_SIZE);
        // check recv_buffer for validation
        ret = 0; // success
    }
    return ret;
}

// void send_and_recv(char *message) {
//     int rc = UDP_Write(SOCKET, &addr, message, BUFFER_SIZE); //write message to server@specified-port
//     printf("CLIENT:: sent message (%d)\n", rc);
//     if (rc > 0) {
//         char buffer[BUFFER_SIZE];
//         int rc = UDP_Read(SOCKET, &addr2, buffer, BUFFER_SIZE); //read message from ...
//         printf("CLIENT:: read %d bytes (message: '%s')\n", rc, buffer);
//     }
// }
