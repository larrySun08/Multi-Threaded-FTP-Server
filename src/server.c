#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ftp.h>

#define NB_THREADS 32

char dirPath[96];


void* process_request(void* arg) {

    char filename[24];
    char* list;
    // void* content;
    int i, *sock = (int*) arg;
    int op;

    // Zero buffers
    for (i = 0; i < 24; i++) {
        filename[i] = '\0';
    }

    /*** Acquire operation ***/
    if (read(*sock, &op, sizeof(op)) < 0) {
        perror("read");
        exit(1);
    }

    if (op == 1) {      //Upload
        answerUpload(sock);

    } else if (op == 2) {   //Upload Directory
        /*** Acquire directory name ***/
        if (read(*sock, filename, sizeof(char)*24) < 0) {
            perror("read");
            exit(1);
        }
        /* Make directory */
        if (mkdir(filename, 0777) == -1) {
            perror("mkdir");
            exit(2);
        }

    } else if (op == 3) {       //Download
        /*** Acquire filename ***/
        if (read(*sock, filename, sizeof(char)*24) < 0) {
            perror("read");
            exit(1);
        }

        int is_Dir = isDir(filename);
        //Send client if path is directory or not
        if (send(*sock, &is_Dir, sizeof(is_Dir), 0) == -1) {
            perror("download request");
            exit(2);
        }

        if (is_Dir) {      //If Directory
            int nFiles = numRegFiles(filename);

            //Send client number of Regular Files in Directory
            if (send(*sock, &nFiles, sizeof(nFiles), 0) == -1) {
                perror("download request");
                exit(2);
            }
            sendFileNamesinDir(filename, sock);

        } else {     //If File
            answerDownload(filename, sock);
        }

    } else if (op == 5) {       //List
        list = malloc(sizeof(char)*512);
        /* Open ditectory */
        DIR *pt_Dir;
        struct dirent* dirEnt;
        struct stat info;
        printf("LISTING FILES IN >> %s\n", dirPath);

        if ((pt_Dir = opendir(dirPath) ) == NULL) {
            perror ("opendir");
            exit(2);
        }

        while ((dirEnt = readdir(pt_Dir)) !=NULL) {
            stat(dirEnt->d_name, &info);
                strcat(list, dirEnt->d_name);
                strcat(list, "\n");
            // }
        }
        strcat(list, "\0");

        int length = strlen(list);
        /*** Send length of list ***/
        if (send(*sock, &length, sizeof(length), 0) == -1) {
            perror("list request");
            exit(2);
        }

        if (write(*sock, list, strlen(list)) == -1) {
            perror("write");
            exit(2);
        }

    }

    // Clean buffers
    for (i = 0; i < 24; i++) {
        filename[i] = '\0';
    }

    /* Close connection */
    shutdown(*sock,2);
    close(*sock);
    pthread_exit(0);
}



int main(int argc, char *argv[]) {
    struct sockaddr_in sin;  /* Name of the connection socket (Server address) */
    struct sockaddr_in exp;  /* Client address */
    int sc ;                 /* Connection socket */
    int scom;		      /* Communication socket */
    uint16_t PORTSERV = (uint16_t) atoi(argv[1]);
    socklen_t clilen = sizeof (exp);
    int i, ch;
    pthread_t tid[NB_THREADS];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s machine\n", argv[0]);
        exit(1);
    }

    // Change to directory to argv[2]
    if ((ch = chdir(argv[2])) == -1 ) {
        perror("chdir");
        exit(1);
    }

    strcpy(dirPath, argv[2]);

    /* Create socket */
    if ((sc = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        perror("socket");
        exit(1);
    }

    memset((void*)&sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(PORTSERV);
    sin.sin_family = AF_INET;

    int reuse = 1;
    setsockopt(sc, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));


    /* Register server on DNS svc */
    if (bind(sc,(struct sockaddr *)&sin,sizeof(sin)) < 0) {
        perror("bind");
        exit(2);
    }

    listen(sc, 5);

    /* Main loop */
    for (i = 0;i < NB_THREADS; i++) {
        if ( (scom = accept(sc, (struct sockaddr *)&exp, &clilen))== -1) {
            perror("accept");
            exit(2);
        }

        /* Create a thread to handle the newly connected client */
        int* tscom = (int*)malloc(sizeof(int));
        *tscom = scom;
        pthread_create(&tid[i], 0, process_request, tscom);
    }

    for (i = 0;i < NB_THREADS; i++)
    pthread_join(tid[i], 0);

    close(sc);

    return 0;
}
