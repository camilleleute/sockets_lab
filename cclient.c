/******************************************************************************
* myClient.c
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

#define MAXBUF 1024
#define DEBUG_FLAG 1

void sendToServer(int socketNum);
void processStdin(int);
void sendHandle(int , char * );
void processMsgFromServer(int);
void clientControl(int);
void receiveFromServer(int );
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);


int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	// send handle to server
	sendHandle(socketNum, argv[1]);
	// client polling manager
	clientControl(socketNum);
	
	return 0;
}

void sendHandle(int socketNum, char * handle) {
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   */
	sendLen = strlen(handle) + 1;
	sendBuf[0] = 1; // flag = 1
	memcpy(sendBuf+ 1, handle, sendLen);
	
	printf("handle: %s string len: %d (including null)\n", handle, sendLen);
	
	sent = sendPDU(socketNum, sendBuf, sendLen + 1);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Amount of data sent is: %d\n", sent);
	
}

void clientControl(int clientSocket) {
	setupPollSet();
	addToPollSet(clientSocket);

	while(1){
		int curr_socket = pollCall(-1);
		if (curr_socket == clientSocket) {
			processMsgFromServer(curr_socket);

		} else {
			processStdin(clientSocket);
		}
	}

}

void processStdin(int socket){
	sendToServer(socket);
}

void processMsgFromServer(int serverSocket){
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the server_socket
	if ((messageLen = recvPDU(serverSocket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		printf("Message received, length: %d Data: %s\n", messageLen, dataBuffer);
		
		uint8_t flag = dataBuffer[0];
		char *msg = (char *)&dataBuffer[1];

		switch(flag){
			case 2:
				printf("$: ");
				fflush(stdout);
				addToPollSet(STDIN_FILENO);
				break;
			case 3:
				printf("Handle already in use: %s\n", msg);
				exit(-1);
				break;
			case 7:
				printf("Client with handle %s does not exist\n", msg);
				break;

		}

	}

	else
	{
		close(serverSocket);
        printf("Server has terminated\n");
		removeFromPollSet(serverSocket);
		exit(-1);
	}
}

void sendToServer(int socketNum)
{
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   
	char command[3]; 		   //2 bytes + 1 for null terminator
	int flag = 0;			   //numeric value of command

	
	sendLen = readFromStdin(sendBuf); 
	command[0] = sendBuf[0];
	command[1] = sendBuf[1];
	command[2] = '\0'; // Null terminate
	//printf("read: %s string len: %d (including null)\n", sendBuf, sendLen);
	printf("Command is %s\n", command);

	if (strcmp(command, "%M") == 0 || strcmp(command, "%m") == 0) {
        flag = 5;
    } else if (strcmp(command, "%B") == 0 || strcmp(command, "%b") == 0) {
        flag = 4;
    } else if (strcmp(command, "%C") == 0 || strcmp(command, "%c") == 0) {
        flag = 6;
    } else if (strcmp(command, "%L") == 0 || strcmp(command, "%l") == 0) {
        flag = 10;
    } else {
        printf("Invalid command.\n");
    }

	memcpy(sendBuf+ 1, sendBuf + 3, sendLen);
	sendBuf[0] = flag; 

	sent = sendPDU(socketNum, sendBuf, sendLen+1);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Amount of data sent is: %d\n", sent);
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("$: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}

	if ((strlen(argv[1])) >= 101) {
		printf("Invalid handle, handle longer than 100 characters: %s\n", argv[0]);
		exit(1);
	}
}
