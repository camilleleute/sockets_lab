/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "send.h"
#include "pollLib.h"
#include "dict.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket, Dict *);
int checkArgs(int argc, char *argv[]);
void addNewSocket(int);
void processClient(int, Dict *);
void serverControl(int);
void sendToClient(int , char * , int );

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);
	// server control
	serverControl(mainServerSocket);
	/* close the sockets */
	close(mainServerSocket);
	return 0;
}

void serverControl(int mainServerSocket) {
	setupPollSet();
	addToPollSet(mainServerSocket);
	Dict *handle_table = dctcreate();

	while (1) {
		int socketNum = pollCall(-1);
		if (socketNum == mainServerSocket) {
			addNewSocket(socketNum);
		} else if (socketNum < 0) {
			perror("Failed to poll");
			return;
		} else {
			processClient(socketNum, handle_table);
		}
	}
}

void addNewSocket(int socketNum){
	int newSocket = tcpAccept(socketNum, DEBUG_FLAG);
	addToPollSet(newSocket);
	return;
}

void processClient(int socketNum, Dict * handle_table){
	recvFromClient(socketNum, handle_table);
	return;
}

void recvFromClient(int clientSocket, Dict * handle_table)
{
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		uint8_t flag = dataBuffer[0]; 
		char *msg = (char *)&dataBuffer[1];
		printf("Message received, flag: %d length: %d Data: %s\n", flag, messageLen, msg);

		switch(flag) {
			case 1: // incoming handle from client
				if ((dctget(handle_table, msg)) == NULL) {
					printf("inserting %s with socket %d\n", msg, clientSocket);
					dctinsert(handle_table, msg, (void *)(long)clientSocket);
					sendToClient(clientSocket, msg, 2);
				}
				else {
					sendToClient(clientSocket, msg, 3);
				}
				break;
			//case 4: // broadcast %B
			case 5: // message %M
				char handle[101], message[200];

				sscanf(msg, "%s %[^\n]", handle, message);

				printf("Handle: %s\n", handle);
				printf("Message: %s\n", message);

				void* lookupResult = dctget(handle_table, handle); 

				if (lookupResult == NULL) { 
					printf("hey. uh oh.\n");
					sendToClient(clientSocket, handle, 7); 
				} else {
					int destSocket = (intptr_t)lookupResult; // Safe cast
					printf("Sending to socket number: %d, with message: %s\n", destSocket, message);
					sendToClient(destSocket, message, 0);
				}
				break;

			//case 6: // multicast %C
			//case 10: // list %L

		}
	}

	else
	{
		close(clientSocket);
		printf("Connection closed by other side\n");
		// remove from handle table
		removeFromPollSet(clientSocket);
	}
}

void sendToClient(int socketNum, char * buffer, int flag)
{
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   */
	
	sendLen = strlen(buffer);
	sendBuf[0] = flag;
	memcpy(sendBuf + 1, buffer, sendLen);
	sendBuf[sendLen + 1] = '\0'; // Ensure null-termination


	printf("sending: %s (string len: %d (including null))\n", sendBuf, sendLen);
	
	sent = sendPDU(socketNum, sendBuf, sendLen + 1);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Amount of data sent is: %d\n", sent);
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}
