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

void sendToServer(int, char *);
void processStdin(int, char *);
void sendHandle(int , char * );
void processMsgFromServer(int);
void clientControl(int, char *);
void receiveFromServer(int );
int readFromStdin(uint8_t *);
void checkArgs(int argc, char * argv[]);
char* handle_parser(int , char [][100], uint8_t []) ;


int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], 0);
	// client polling manager
	clientControl(socketNum, argv[1]);
	
	return 0;
}

void sendHandle(int socketNum, char * handle) {
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   */
	sendLen = strlen(handle);
	sendBuf[0] = 1; // flag = 1
	memcpy(sendBuf+ 1, handle, sendLen);
	sendBuf[sendLen + 1] = '\0';
	
	sent = sendPDU(socketNum, sendBuf, sendLen + 2);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}	
}

void clientControl(int clientSocket, char * myHandle) {
	setupPollSet();
	addToPollSet(clientSocket);
	sendHandle(clientSocket, myHandle);

	while(1){
		int curr_socket = pollCall(-1);
		if (curr_socket == clientSocket) {
			processMsgFromServer(curr_socket);

		} else {
			processStdin(clientSocket, myHandle);
		}
		printf("$: ");
		fflush(stdout);
	}

}

void processStdin(int socket, char * myHandle){
	sendToServer(socket, myHandle);
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
		uint8_t flag = dataBuffer[0];
		char *msg = (char *)&dataBuffer[1];
		

		switch(flag){
			case 2:
				addToPollSet(STDIN_FILENO);
				break;
			case 3:
				printf("Handle already in use: %s\n", msg);
				exit(-1);
				break;
			case 7:
				printf("\nClient with handle %s does not exist\n", msg);
				break;
			default:
				printf("\n%s\n", msg);
				break;
		}
		
	}

	else {
		close(serverSocket);
        printf("\nServer has terminated\n");
		removeFromPollSet(serverSocket);
		exit(-1);
	}
}

void sendToServer(int socketNum, char * myHandle)
{
	uint8_t sendBuf[MAXBUF];   //data buffer
	uint8_t inputMsg[MAXBUF];
	char * msg = "";
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent/* get the data and send it   
	char command[3]; 		   //2 bytes + 1 for null terminator
	int flag = 0;			   //numeric value of command
	int src_handle_len = strlen(myHandle);
	int num_dest_handles = 0;
	char dest_handles[8][100];
	int idx = 0; 				// index into sendBuf

	// |-PDU LEN-||-flag-||-src handle len-||-src handle-||-# dest handles-||-dest handle len-||-dest handle-||-text-|
	//   2 bytes     1            1               XX               1                1                  XX        XX
	memset(sendBuf, 0, sizeof(sendBuf));	// clear buffer
	memset(inputMsg, 0, sizeof(inputMsg));	// clear buffer

	readFromStdin(inputMsg); 
	command[0] = inputMsg[0];
	command[1] = inputMsg[1];
	command[2] = '\0'; // Null terminate
	//printf("read: %s string len: %d (including null)\n", inputMsg, sendLen);
	//printf("Command is %s\n", command);

	if (strcmp(command, "%M") == 0 || strcmp(command, "%m") == 0) {
        flag = 5;
		num_dest_handles = 1;
		msg = handle_parser(num_dest_handles, dest_handles, &inputMsg[3]);
		if (msg == NULL) {
            //printf("\nError parsing handles or message.\n");
            return;
        }
    } else if (strcmp(command, "%B") == 0 || strcmp(command, "%b") == 0) {
        flag = 4;
    } else if (strcmp(command, "%C") == 0 || strcmp(command, "%c") == 0) {
        flag = 6;
    } else if (strcmp(command, "%L") == 0 || strcmp(command, "%l") == 0) {
        flag = 10;
    } else {
        printf("Invalid command.\n");
		fflush(stdout);
		return;
    }

	sendBuf[idx++] = flag; 
	sendBuf[idx++] = src_handle_len;
	memcpy(&sendBuf[idx], myHandle, src_handle_len);	
	idx += src_handle_len;
	sendBuf[idx++] = num_dest_handles;

	for (int i = 0; i < num_dest_handles; i++){
		int dest_handle_len = strlen(dest_handles[i]);
		sendBuf[idx++] = dest_handle_len;
		memcpy(&sendBuf[idx], dest_handles[i], dest_handle_len);
		idx += dest_handle_len;
	}

	int msgLen = strlen(msg);
	memcpy(&sendBuf[idx], msg, msgLen); // Copy only the message content (no null terminator)
	idx += msgLen; // Update the index to point to the end of the message
	sendBuf[idx++] = 0; // null terminate
	sendLen = idx; // Update the total length of the PDU
	
	// printf("sendBuf contents (hex): ");
	// for (int i = 0; i < sendLen; i++) {
	// 	printf("%02X ", sendBuf[i]);
	// }
	// printf("\n");

	sent = sendPDU(socketNum, sendBuf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	//printf("Amount of data sent is: %d\n", sent);
}

char* handle_parser(int num_handles, char handles[][100], uint8_t msg[]) {
    if (msg == NULL || num_handles < 1 || num_handles > 8) {
        printf("\nInvalid input: msg is NULL or num_handles is out of range\n");
        return NULL;
    }

    // Create a writable copy of the message
    char msg_copy[MAXBUF];
    strncpy(msg_copy, (char *)msg, sizeof(msg_copy) - 1);
    msg_copy[sizeof(msg_copy) - 1] = '\0'; // Null-terminate to prevent overflow

    char *token = strtok(msg_copy, " ");

    // Extract handles
    for (int i = 0; i < num_handles; i++) {
        if (token == NULL) {
            printf("\nNot enough handles provided\n");
            return NULL;
        }
        strncpy(handles[i], token, 100 - 1);
        handles[i][100 - 1] = '\0'; // Ensure null-termination
        token = strtok(NULL, " ");
    }

    // Calculate the length of the handles portion in the original message
    size_t handles_length = 0;
    for (int i = 0; i < num_handles; i++) {
        handles_length += strlen(handles[i]) + 1; // +1 for the space or null terminator
    }

    // Return a pointer to the message portion in the original buffer
    return (char *)(msg + handles_length);
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	//printf("\n$: ");
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
