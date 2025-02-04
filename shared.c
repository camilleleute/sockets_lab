#include "shared.h"
#include "send.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAXBUF 1402

void sendWithFlag(int socketNum, char * buffer, int flag) {
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;           //amount of data to send
	int sent = 0;              //actual amount of data sent
	
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