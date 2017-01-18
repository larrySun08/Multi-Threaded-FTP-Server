#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <libgen.h>
#include <ftp.h>

/* Checks if a path is a directory*/
int isDir(char *path) {
    int i = 0;
    struct stat info;
    if (stat(path, &info) == -1) {
        // perror("stat");
        // exit(1);
    }
    i = S_ISDIR(info.st_mode);
    return i;
}

/* Gets number of Regular Files in a directory */
int numRegFiles(char* path) {
    int count = 0;
    /* Open ditectory */
    DIR *pt_Dir;
    struct dirent* dirEnt;
    struct stat info;

    if ((pt_Dir = opendir(path) ) == NULL) {
        perror ("opendir");
        exit(2);
    }

    while ((dirEnt = readdir(pt_Dir)) != NULL) {
        stat(dirEnt->d_name, &info);
        if (S_ISREG(info.st_mode) && (strcmp(dirEnt->d_name,".DS_Store") != 0)) {
            count++;
        }
    }
    closedir (pt_Dir);
    return count;
}

void sendFileCont(int fd, int* sock) {
    void* content;
    int size = 1;
    content = malloc(1024);
    while(size > 0) {
        if ((size = read(fd, content, 1024)) < 0) {
            perror("read");
            exit(1);
        }
        if (write(*sock, content, size) == -1) {
            perror("write content");
            exit(2);
        }
    }
}


void receiveFileCont(int fd, int* sock) {
    void* content;
    int size = 1;
   content = malloc(1024);
   while(size > 0) {
       if ((size = read(*sock, content, 1024)) < 0) {
           perror("read");
           exit(1);
       }
       if (write(fd, content, size) == -1) {
           perror("write");
           exit(2);
       }
   }
}


void upload(char* name, int* sock) {
    int i, fd;
    char filename[96];

    for (i = 0; i < 96; i++) {
        filename[i] = '\0';
    }
    strcpy(filename, name);
    if((fd = open(filename, O_RDONLY, 0666)) == -1) {
        perror("open");
        exit(1);
    }

    //Send File Name
    if (send(*sock, name, strlen(name)+1, 0) == -1) {
        perror("write fname");
        exit(2);
    }

    sendFileCont(fd, sock);
    printf("SENT FILE >> %s\n", name);
}


void answerUpload(int* sock) {
    int fd;
    char filename[96];

    /*** Acquire filename ***/
    if (read(*sock, filename, sizeof(char)*96) < 0) {
        perror("read");
        exit(1);
    }
    // char* name = basename(filename);
    char* name = filename;

    if ((fd = open(name, O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0) {
        perror("open");
        exit(1);
    }
    receiveFileCont(fd, sock);
    printf("CREATED FILE >> %s\n", name);
}

void download(char* name, int* sock) {
    int fd;
    if ((fd = open(name, O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0) {
        perror("open");
        exit(1);
    }
    receiveFileCont(fd, sock);
    printf("DOWNLOADED FILE >> %s\n", name);
}

void answerDownload(char* name, int* sock) {
    int fd;
    if((fd = open(name, O_RDONLY, 0666)) == -1) {
        perror("open");
        exit(1);
    }
    sendFileCont(fd, sock);
    printf("SENT FILE >> %s\n", name);
}

void create_threads(char* dirName, struct sockaddr_in dest, void* (*threadFunc)(void*) ) {
    /* Open ditectory */
    DIR *pt_Dir;
    struct dirent* dirEnt;
    struct stat info;
    char* filename;
    int i = 0, tsock;
    int nFiles = numRegFiles(dirName);
    pthread_t localtid[nFiles];

    if ((pt_Dir = opendir(dirName) ) == NULL) {
        perror ("opendir");
        exit(2);
    }

    while ((dirEnt = readdir(pt_Dir)) != NULL) {
        stat(dirEnt->d_name, &info);
        if (S_ISREG(info.st_mode) && (strcmp(dirEnt->d_name,".DS_Store") != 0)) {
            asprintf(&filename, "%s/%s", dirName, dirEnt->d_name);
            printf("%s\n", filename);

            //Create new socket
            if ((tsock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
                perror("socket");
                exit(1);
            }
            fileInfo* fInfo = malloc(sizeof(*fInfo));
            fInfo->sock = tsock;
            fInfo->filename = filename;
            /* Establish connection */
            if (connect(tsock, (struct sockaddr *) &dest, sizeof(dest)) == -1) {
                perror("connect");
                exit(1);
            }

            pthread_create(&localtid[i++], 0, threadFunc, fInfo);
        }
    }

    for (i = 0; i < nFiles; i++) {
        pthread_join(localtid[i], 0);
    }
    printf("All threads Terminated\n");
    closedir (pt_Dir);
}

void sendFileNamesinDir(char* dirName, int* sock) {
    /* Open ditectory */
    DIR *pt_Dir;
    struct dirent* dirEnt;
    struct stat info;
    char filename[48];
    int received = 1;

    if ((pt_Dir = opendir(dirName) ) == NULL) {
        perror ("opendir");
        exit(2);
    }

    while ((dirEnt = readdir(pt_Dir)) != NULL) {
        stat(dirEnt->d_name, &info);
        if (S_ISREG(info.st_mode) && (strcmp(dirEnt->d_name,".DS_Store") != 0) && received) {
            sprintf(&filename, "%s/%s", dirName, dirEnt->d_name);

            //Send File Name
            if (send(*sock, filename, sizeof(filename), 0) == -1) {
                perror("write fname");
                exit(2);
            }

            //Wait for confirmation File Name was Received
            if (read(*sock, &received, sizeof(int)) < 0) {
                perror("read");
                exit(1);
            }
        }
    }

    closedir (pt_Dir);
}
