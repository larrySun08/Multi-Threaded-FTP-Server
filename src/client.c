#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <ftp.h>
#include <pthread.h>

pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;


/* Thread function for uploading each file in a directory */
void* dirUploadThread(void* arg) {
    fileInfo* fInfo = (fileInfo*) arg;
    int* sock = &fInfo->sock;
    int fd, op = 1;

    if((fd = open(fInfo->filename, O_RDONLY, 0666)) == -1) {
        perror("open");
        exit(1);
    }
    pthread_mutex_lock(&mx);
    //Send a request to upload file
    if (send(*sock, &op, sizeof(op), 0) == -1) {
        perror("upload request");
        exit(2);
    }
    pthread_mutex_unlock(&mx);

    //Send File Name
    if (send(*sock, fInfo->filename, strlen(fInfo->filename)+1, 0) == -1) {
        perror("write fname");
        exit(2);
    }

    sendFileCont(fd, sock);
    pthread_exit(0);
}

void* dirDownloadThread(void* arg) {
    fileInfo* fInfo = (fileInfo*) arg;
    int op = 3;
    int* sock = &fInfo->sock;
    char filename[48];

    strcpy(filename, fInfo->filename);

    pthread_mutex_lock(&mx);
    //Send a request to download file
    if (send(*sock, &op, sizeof(op), 0) == -1) {
        perror("upload request");
        exit(2);
    }
    pthread_mutex_unlock(&mx);

    //Send File Name
    if (send(*sock, fInfo->filename, sizeof(filename), 0) == -1) {
        perror("upload request");
        exit(2);
    }

    download(filename, sock);
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
	struct sockaddr_in dest; /* Server address */
	struct addrinfo *result;
    uint16_t PORTSERV = (uint16_t) atoi(argv[2]);
	int sock, size, op, listLength, is_Dir = 0, nFiles;
    char* list;
    list = malloc(sizeof(char)*512);

	if (argc != 5) {
        if (argc == 4 && (strcasecmp("LIST", argv[3]) == 0)) {
            //Continue
        } else {
    		fprintf(stderr, "Usage: %s machine\n", argv[0]);
    		exit(1);
        }
	}

	if ((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
		perror("socket");
		exit(1);
	}

	/* Find server address and use it to fill in structure dest */
	struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;
    hints.ai_protocol = 0;

	if (getaddrinfo(argv[1], 0, &hints, &result) != 0) {
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
    }
	memset((void *)&dest,0, sizeof(dest));
	memcpy((void*)&((struct sockaddr_in*)result->ai_addr)->sin_addr,(void*)&dest.sin_addr,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(PORTSERV);

	/* Establish connection */
	if (connect(sock, (struct sockaddr *) &dest, sizeof(dest)) == -1) {
		perror("connect");
		exit(1);
	}

    printf("Connected to server\n");

    size = 1;
    is_Dir = isDir(argv[4]);
    if (strcasecmp("UPLOAD", argv[3]) == 0 && (!is_Dir) ) {
        //Send a request to upload file
        op = 1;
        if (send(sock, &op, sizeof(op), 0) == -1) {
            perror("upload request");
            exit(2);
        }
        upload(argv[4], &sock);
        printf("Uploaded >> %s\n", argv[4]);

    } else if (strcasecmp("UPLOAD", argv[3]) == 0 && is_Dir == 1) { // Uploading a directory
        op = 2;
        if (send(sock, &op, sizeof(op), 0) == -1) {
            perror("upload directory request");
            exit(2);
        }
        char directoryName[24];
        strcpy(directoryName, argv[4]);
        /* Send directory name */
        if (send(sock, directoryName, strlen(directoryName)+1, 0) == -1) {
            perror("write fname");
            exit(2);
        }

        create_threads(argv[4], dest, dirUploadThread);

    } else if (strcasecmp("DOWNLOAD", argv[3]) == 0) {
        //Send a request to download
        op = 3;
        if (send(sock, &op, sizeof(op), 0) == -1) {
            perror("download request");
            exit(2);
        }
        /* Send File Name to Server */
        if (write(sock, argv[4], sizeof(argv[4])) == -1) {
            perror("write content");
            exit(2);
        }
        /*** Acquire Is this a directory ***/
        if (read(sock, &is_Dir, sizeof(is_Dir)) < 0) {
            perror("read");
            exit(1);
        }

        if (is_Dir) {   //If Directory
            /* Make directory */
            if (mkdir(argv[4], 0777) == -1) {
                perror("mkdir");
                exit(2);
            }

            /*** Acquire number of regular files ***/
            if (read(sock, &nFiles, sizeof(nFiles)) < 0) {
                perror("read");
                exit(1);
            }

            //Create nFile threads to download files
            pthread_t localtid[nFiles];
            int i, tsock, received = 1;
            for (i = 0; i < nFiles;i++){
                char* filename = malloc(sizeof(char)*48);
                /*** Acquire File Name ***/
                if (read(sock, filename, sizeof(char)*48) < 0) {
                    perror("read");
                    exit(1);
                }
                //Tell server filename has been received
                if (send(sock, &received, sizeof(received), 0) == -1) {
                    perror("received");
                    exit(2);
                }

                //Create new socket
                if ((tsock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
                    perror("socket");
                    exit(1);
                }
                /* Establish connection */
                if (connect(tsock, (struct sockaddr *) &dest, sizeof(dest)) == -1) {
                    perror("connect");
                    exit(1);
                }

                fileInfo* fInfo = malloc(sizeof(*fInfo));
                fInfo->sock = tsock;
                fInfo->filename = filename;

                pthread_create(&localtid[i], 0, dirDownloadThread, fInfo);
            }

            for (i = 0; i < nFiles; i++) {
                pthread_join(localtid[i], 0);
            }
        } else {    //If File
            download(argv[4], &sock);
            printf("Downloaded >> %s\n", argv[4]);
        }

    } else if (strcasecmp("LIST", argv[3]) == 0) {
        op = 5;
        //Send a request to list files
        if (send(sock, &op, sizeof(op), 0) == -1) {
            perror("list request");
            exit(2);
        }
        /*** Retrieve File List Length ***/
        if (read(sock, &listLength, sizeof(listLength)) < 0) {
            perror("read list length");
            exit(1);
        }
        /*** Retrieve File List ***/
        if ((size = read(sock, list, listLength)) < 0) {
            perror("read list");
            exit(1);
        }
        printf("LISTING FILES >>\n\n");
        printf("%s", list);
        printf("\n");

    } else {
        printf("Invalid, try again\n");
    }

	/* Close connection */
	shutdown(sock,2);
	close(sock);
    printf("Closed!\n");
	return(0);
}
