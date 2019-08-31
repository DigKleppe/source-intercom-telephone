/*
 * connections.c
 *
 *  Created on: Feb 17, 2019
 *      Author: dig
 */

#include "telefoon.h"
#include "keys.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>	//strlen
#include <sys/socket.h>
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>	//write
#include <stdlib.h>
#include <netinet/in.h>

#define PCADDRESS "192.168.2.6"

int IDerrCntr;
int connectCntrBaseFloor,connectCntrFirstFloor;
int servConnectCtr1, servConnectCtr2;

// alive message to base floor station
//
//
void* clientThread(void* args) {
	struct sockaddr_in address;
	int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char buffer[1024] = {0};
	int written;

	while (1) {

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)  // tcp
		{
			printf("\n Socket creation error \n");
		}

		memset(&serv_addr, '0', sizeof(serv_addr));

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(CHATPORT);

		if(inet_pton(AF_INET, IPADDRESS_BASEFLOOR, &serv_addr.sin_addr)<=0)
			printf("\nInvalid address/ Address not supported \n");

		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		{
			//	printf("\nConnection Failed \n");
			connectCntrBaseFloor = 0;
		}
		else {
			written = write(sock ,  (uint8_t *) &transmitData , sizeof (transmitData));

			//send(sock , buffer , strlen(buffer) , 0 );
			//	printf("keepalive message sent to base floor %d\n",connectCntr1);
			//	valread = read( sock , buffer, 1024);  //
			//	printf("%s\n",buffer );
			connectCntrBaseFloor++;
		}
		// connect to first floor
		if(inet_pton(AF_INET, IPADDRESS_FIRSTFLOOR, &serv_addr.sin_addr)<=0)
			printf("\nInvalid address/ Address not supported \n");

		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		{
			//	printf("\nConnection Failed \n");
			connectCntrFirstFloor = 0;
		}
		else {
		//	sprintf( buffer,"%d,%d" , houseNo, keysRT );
			written = write(sock ,  (uint8_t *) &transmitData , sizeof (transmitData));
		//	sprintf( buffer,"%d,%d" , houseNo, keysRT );
		//	send(sock , buffer , strlen(buffer) , 0 );
			//	printf("keepalive message sent to first floor %d\n",connectCntr2);
			//	valread = read( sock , buffer, 1024);  //
			//	printf("%s\n",buffer );
			connectCntrFirstFloor++;
		}

		close (sock);
		usleep(CHATINTERVAL * 1000);
	}
	return 0;
}

// receives (bell) messages from base stations


void* serverThread (void* args) {
	int socket_fd = 0, accept_fd = 0;
	uint32_t addr_size, sent_data;
	int count;
	threadStatus_t  * pThreadStatus = args;
	char receiveBuf[1024];
	struct sockaddr_in sa, isa;
	int stationID;
	int written;

	char *buf;
	int buflen;
	bool err = false;
	int opt = 1;

	struct timeval receiving_timeout;
	receiving_timeout.tv_sec = 5;
	receiving_timeout.tv_usec = 0;

	while (1) {
		err = false;

		memset(&sa, 0, sizeof(struct sockaddr_in));
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
		sa.sin_port = htons(COMMANDPORT);

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
		//	setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,sizeof(receiving_timeout));

			addr_size = sizeof(isa);
			accept_fd = accept(socket_fd, (struct sockaddr* ) &isa, &addr_size);

			stationID = (inet_lnaof (isa.sin_addr) -100 ); // = low byte address

			if (accept_fd < 0) {
				printf("accept failed\n");
				err = true;
			}
			else {
				///	printf("accepted\n");
				setsockopt(accept_fd, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout));
				if ( stationID== 0) // base floor
					count = recv(accept_fd, (uint8_t *) &receiveDataBaseFloor, sizeof(receiveData_t), 0);
				else
					count = recv(accept_fd, (uint8_t *) &receiveDataFirstFloor, sizeof(receiveData_t), 0);
		//		printf("rec: %d , %d\n",count,servConnectCtr1 );
				written = write(accept_fd ,  (uint8_t *) &transmitData , sizeof (transmitData));
				close(accept_fd);
			}
		}
		close(socket_fd);

		if( err)
			usleep(100000);
	}
	pThreadStatus->isRunning = false;
	pthread_exit(args);
	return ( NULL);
}


void startConnectionThreads( void){
	pthread_t ID1;
	pthread_t ID2;
	int result;

	result = pthread_create(&ID1, NULL, &clientThread, NULL);
	if (result == 0)
		printf("ClientThread created successfully.\n");
	else {
		printf("ClientThread not created.\n");
	}
	result = pthread_create(&ID2, NULL, &serverThread, NULL);
	if (result == 0)
		printf("ServerThread created successfully.\n");
	else {
		printf("ServerThread not created.\n");
	}
}


