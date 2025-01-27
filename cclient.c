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
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	clientControl(socketNum);

	close(socketNum);
	
	return 0;
}

void clientControl(int clientSocket) {
	setupPollSet();
	addToPollSet(clientSocket);
	addToPollSet(STDIN_FILENO);

	printf("Enter data: ");
	fflush(stdout);

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
		printf("Enter data: ");
		fflush(stdout);

	}

	else
	{
		close(serverSocket);
		printf("\nConnection closed by other side\n");
		removeFromPollSet(serverSocket);
		exit(-1);
	}
}

void sendToServer(int socketNum)
{
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   */
	
	sendLen = readFromStdin(sendBuf);
	printf("read: %s string len: %d (including null)\n", sendBuf, sendLen);
	
	sent = sendPDU(socketNum, sendBuf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Amount of data sent is: %d\n", sent);
}

void receiveFromServer(int socketNum) {
    uint8_t recvBuf[MAXBUF];
    int recvLen = 0;

    // Receive data from the server
    recvLen = recvPDU(socketNum, recvBuf, MAXBUF);
    if (recvLen == 0) {
        // Server closed the connection
        printf("Server has terminated\n");
        exit(0);
    } else if (recvLen < 0) {
        perror("Receive failed");
        exit(-1);
    }

    recvBuf[recvLen] = '\0'; // Null-terminate the received data
    printf("Received from server: %s\n", recvBuf);
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
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
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}
