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
void get_dest_handles(uint8_t [], Dict *, int, int);
void broadcastHandling(uint8_t [], Dict * , int , int );

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
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0) {
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		uint8_t flag = dataBuffer[0]; 
		
		switch(flag) {
			case 1: // incoming handle from client
				char *handle = (char *)&dataBuffer[1];
				if ((dctget(handle_table, handle)) == NULL) {
					printf("inserting %s with socket %d\n", handle, clientSocket);
					dctinsert(handle_table, handle, (void *)(long)clientSocket);
					sendToClient(clientSocket, handle, 2);
				}
				else {
					sendToClient(clientSocket, handle, 3);
				}
				break;
			case 4: // broadcast %B
				broadcastHandling(dataBuffer, handle_table, clientSocket, messageLen);
				break;
			case 5: // message %M
				get_dest_handles(dataBuffer, handle_table, clientSocket, messageLen);			
				break;
			case 6: // multicast %C
				get_dest_handles(dataBuffer, handle_table, clientSocket, messageLen);
				break;
			case 10: // list %L

				uint8_t firstBuf[5];
				uint32_t num_handles = htonl(handle_table->size);
				firstBuf[0] = 11;
				memcpy(&firstBuf[1], &num_handles, 4);

				int sent = sendPDU(clientSocket, firstBuf, 5);
				if (sent < 0)
				{
					perror("send call");
					exit(-1);
				}

				char ** handles = dctkeys(handle_table);
				for (int i = 0; i < handle_table->size; i++) {
					uint8_t handlePackets[MAXBUF];
					uint8_t handle_len = strlen(handles[i]);
					handlePackets[0] = 12;
					handlePackets[1] = handle_len;
					memcpy(&handlePackets[2], handles[i], handle_len);
					int sent = sendPDU(clientSocket, handlePackets, handle_len+2);
					if (sent < 0)
					{
						perror("send call");
						exit(-1);
					}
					memset(handlePackets, 0, sizeof(handlePackets));	

				}

				uint8_t doneBuf[1];
				doneBuf[0] = 13;
				int donesent = sendPDU(clientSocket, doneBuf, 1);
				if (donesent < 0)
				{
					perror("send call");
					exit(-1);
				}
				break;

			default:
				break;
		}
	} else {
		close(clientSocket);
		printf("Connection closed by other side\n");
		// remove from handle table
		removeFromPollSet(clientSocket);
	}
}

void broadcastHandling(uint8_t dataBuffer[], Dict * handle_table, int clientSocket, int messageLen) {
	char ** sockets = dctkeys(handle_table);
	if (sockets == NULL) {
		return;
	} else {
		for (int i = 0; i < handle_table->size; i++) {

			void* lookupResult = dctget(handle_table, sockets[i]); 
			if (lookupResult == NULL) { 
				printf("ERRORERROR\n");
			} else {
				int destSocket = (intptr_t)lookupResult;
				if (destSocket == clientSocket) continue;
				int sent = sendPDU(destSocket, dataBuffer, messageLen);
				if (sent < 0)
				{
					perror("send call");
					exit(-1);
				}
			}
		}	
		free(sockets); 
	}
}

void get_dest_handles(uint8_t dataBuffer[], Dict * handle_table, int clientSocket, int messageLen){
	
	int num_dest_handles_idx = 1 + 1 + dataBuffer[1];

	uint8_t num_dest_handles = dataBuffer[num_dest_handles_idx];

	int idx = num_dest_handles_idx + 1;
	for (int i = 0; i < num_dest_handles; i++){
		uint8_t dest_handle_len = dataBuffer[idx++];
		char dest_handle[dest_handle_len+1];
		memcpy(dest_handle, &dataBuffer[idx], dest_handle_len);
		dest_handle[dest_handle_len] = '\0';

		void* lookupResult = dctget(handle_table, dest_handle); 

		if (lookupResult == NULL) { 
			sendToClient(clientSocket, dest_handle, 7); 
		} else {
			int destSocket = (intptr_t)lookupResult;
			int sent = sendPDU(destSocket, dataBuffer, messageLen);
			if (sent < 0)
			{
				perror("send call");
				exit(-1);
			}
		}
		idx += dest_handle_len;
	}
}


// similar to send handle ... maybe combine
void sendToClient(int socketNum, char * buffer, int flag)
{
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   */
	
	sendLen = strlen(buffer);
	sendBuf[0] = flag;
	memcpy(sendBuf + 1, buffer, sendLen);
	sendBuf[sendLen + 1] = '\0';

	sent = sendPDU(socketNum, sendBuf, sendLen + 2);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}
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
