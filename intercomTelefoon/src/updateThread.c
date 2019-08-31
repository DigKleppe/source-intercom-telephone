/*
 * updateThread.c
 *
 *  Created on: May 13, 2019
 *      Author: dig
 */

#include "telefoon.h"
#include "keys.h"

#include <stdio.h>
#include <string.h>	//strlen
#include <sys/socket.h>
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>	//write
#include <stdlib.h>
#include <netinet/in.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <openssl/md5.h>

bool reboot;

unsigned char receivedMd5sum[MD5_DIGEST_LENGTH];
unsigned char calculatedMd5sum[MD5_DIGEST_LENGTH];
uint32_t fileLen;


// Print the MD5 sum as hex-digits.
void print_md5_sum(unsigned char* md) {
	int i;
	for(i=0; i <MD5_DIGEST_LENGTH; i++) {
		printf("%02x",md[i]);
	}
}

// -lssl -lcrypto

// Get the size of the file by its file descriptor
unsigned long get_size_by_fd(int fd) {
	struct stat statbuf;
	if(fstat(fd, &statbuf) < 0) exit(-1);
	return statbuf.st_size;
}

int getmd5(char * fileName) {
	int file_descript;
	unsigned long file_size;
	char* file_buffer;

	file_descript = open(fileName, O_RDONLY);
	if(file_descript < 0) exit(-1);

	fileLen = get_size_by_fd(file_descript);
//	printf("file size:\t%lu\n", file_size);

	file_buffer = mmap(0, fileLen, PROT_READ, MAP_SHARED, file_descript, 0);
	MD5((unsigned char*) file_buffer, fileLen, calculatedMd5sum);
	munmap(file_buffer, fileLen);

//	print_md5_sum(result);

	return 0;
}



int charpos( char *p, char ch){
	int max = strlen( p);
	int pos = 0;
	while ( p[pos] != ch && pos < max)
		pos++;
	if (pos == max)
		return -1;
	return pos;
}

FILE * openUpdateFile (){
	FILE *fptr;
	fptr = fopen("tempFile","w");
	return fptr;
}

bool appendUpdateFile(FILE * fptr, char * receiveBuf, int count) {
	int len = fwrite (receiveBuf, 1, count, fptr);
	if (len == count)
		return 0;
	else
		return -1;
}

bool closeUpdateFile(FILE * fptr, char * newFileName) {
	fclose (fptr);
	getmd5("tempFile");
	if (memcmp(calculatedMd5sum, receivedMd5sum, sizeof(receivedMd5sum)) == 0) {
		printf(" md5 ok\n");
	//	return rename( "tempFile" , newFileName);
		remove( newFileName);
		rename( "tempFile" , newFileName);  // todo check file
		if (strcmp( newFileName, "telefoon") == 0) {
			 system("chmod +x telefoon");
		}
		return 0;
	}
	else {
		printf(" md5 failed\n");
		return -1;
	}

}


// receives update messages from base station 100

void* updateServerThread (void* args) {
	int socket_fd = 0, accept_fd = 0;
	uint32_t addr_size, sent_data;
	int count;
	threadStatus_t  * pThreadStatus = args;
	char receiveBuf[1024];
	struct sockaddr_in sa, isa;
	int stationID;
	int state = 0;
	FILE * fptr= NULL;
	char newFileName[100];
	uint32_t fileLen;
	uint32_t md5sum;
	uint32_t receivedLen;
	int n;
	char *cp;

	char *buf;
	int buflen;
	bool err = false;
	bool stop = false;
	bool updateError;
	int opt = 1;

	struct timeval receiving_timeout;
	receiving_timeout.tv_sec = 5;
	receiving_timeout.tv_usec = 0;

	while (!stop) {
		err = false;
		memset(&sa, 0, sizeof(struct sockaddr_in));
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
		sa.sin_port = htons(UPDATEPORT);

		socket_fd = socket(PF_INET, SOCK_STREAM, 0);
		if (socket_fd < 0) {
			printf("socket call failed\n");
			err = true;
		}
		if (!err) {
			if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt)))
			{
				perror("setsockopt");
			}
			if (bind(socket_fd, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
				perror("Bind to Port failed\n");
				err = true;
			}
		}
		if (!err) {
			listen(socket_fd, 5);

			addr_size = sizeof(isa);
			accept_fd = accept(socket_fd, (struct sockaddr* ) &isa, &addr_size);

			stationID = (inet_lnaof (isa.sin_addr) -100 ); // = low byte address

			if (accept_fd < 0) {
				printf("accept failed\n");
				err = true;
			}
			else {
				//	printf("Update accepted State:%d\n", state);
				//	setsockopt(accept_fd, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout));
				count = recv(accept_fd, (uint8_t *) &receiveBuf, sizeof(receiveBuf), 0);
				printf("rec %d, %d\n",count, state);

				switch (state){  // first frame contents: "fileName=/nnnnnn/cccc" or "reboot"
				case 0:
					if ( (count > 3) && ( count < 100 )) {  // no response empty frame
						if( sscanf ( receiveBuf,"fileName=%[^;]s",newFileName) == 1) {
							printf("%s\n",receiveBuf);
							receivedLen = 0;
							cp = strstr(receiveBuf,";len=");
							if ( sscanf ( cp,";len=%d;", &fileLen) == 1){
								cp = strstr(receiveBuf,"md5=") + strlen( "md5=");
								memcpy (receivedMd5sum, cp, sizeof (receivedMd5sum));

								fptr = openUpdateFile(); // make temporary file
								if (fptr)
									state++;
								else {
									err = true;
									printf("updata err temp file\n");
								}
							}
						}
						if( strncmp ( receiveBuf,"reboot", strlen("reboot") )== 0){
							printf("%s\n",receiveBuf);
							if ( !updateError) {  // reboot if update succeeded
								reboot = true;
								stop = true;
							}
						}
						if( strncmp ( receiveBuf,"ready", strlen("ready") )== 0){ // not used
							printf("%s\n",receiveBuf);
							stop = true;
						}
					}
					break;
				case 1:
					receivedLen += count;
					if ( count > 0 )
						appendUpdateFile(fptr, receiveBuf, count);
					if (receivedLen >= fileLen || count == 0 ) { // socket closed
						updateError = closeUpdateFile(fptr, newFileName); // check temproary file and rename if ok
						state = 0;
					}
					break;
				}
				sprintf(receiveBuf,"OK %d\n",count );
				send(accept_fd , receiveBuf , strlen (receiveBuf) , 0 );
				close(accept_fd);
			}
			close(socket_fd);
			if( err){
				state = 0;
				if ( fptr )
					fclose (fptr);  //
				usleep(100000);
			}
		}
	}
	pThreadStatus->isRunning = false;
	pthread_exit(args);
	return ( NULL);
}
