#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>


typedef struct file_Info {
    int sock;
    char* filename;
}fileInfo;


/* Upload a file through socket */
void upload(char* input, int* sock);

/* Receive file through socket */
void answerUpload(int* sock);

/* Download a file through socket */
void download(char* name, int* sock);

/* Send file through socket */
void answerDownload(char* name, int* sock);

/* Checks if a path is a directory */
int isDir(char* path);

/* Gets number of Regular Files in a directory */
int numRegFiles(char* path);

/* Creates threads for each regular file in a directory */
void create_threads(char* dirName, struct sockaddr_in dest, void* (*threadFunc)(void*) );

/*
Sends the filenames of all regular files in a directory
Includes directory path in name
*/
void sendFileNamesinDir(char* dirName, int* sock);

/* Send file contents to file descriptor fd in packet of 1024 bytes */
void sendFileCont(int fd, int* sock);

/* Receive file contents through file descriptor fd in packetsof 1024 bytes */
void receiveFileCont(int fd, int* sock);
