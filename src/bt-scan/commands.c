/*
 * commands.c
 *
 *  Created on: Mar 9, 2011
 *      Author: luca
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <commands.h>

extern Configuration config;
extern pthread_mutex_t inquiry_sem;

int bindSocketUDP(int localPort, int timeoutSeconds){
	// Binds a new socket to localPort with timeoutSeconds timeout

	struct sockaddr_in localaddr;
	struct timeval timeout;
	int sd;

	memset((char *)&localaddr, 0, sizeof(struct sockaddr_in));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.s_addr = INADDR_ANY;
	localaddr.sin_port = htons(localPort);
	timeout.tv_sec = timeoutSeconds;

	sd=socket(AF_INET, SOCK_DGRAM, 0);
	if(sd<0) {perror("apertura socket"); exit(3);}
	/* BIND SOCKET, a una porta scelta dal sistema --------------- */
	if(bind(sd,(struct sockaddr *) &localaddr, sizeof(localaddr))<0)
	{perror("bind socket "); exit(1);}
    // printl("Bind socket ok, alla porta %i\n", localPort);
	if(timeoutSeconds > 0)
		setsockopt (sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	return sd;

}

int sendCommand(int sd, struct sockaddr_in* destinationaddr, enum cmd_id id_command, char* param){
	// Send command id_command to destinationaddr through sd socket

	Command command;

	command.id_command = id_command;
	if(param != NULL)
		strcpy(command.param, param);

	if (sendto(sd, &command, sizeof(command), 0, (struct sockaddr*)destinationaddr, sizeof(struct sockaddr_in))<0)
	{
		perror("UDP socket error: sendCommand()");
		return -1;
	}

	return 0;
}

Command receiveCommand(int sd, struct sockaddr_in* sender){
	// Receive command from sender through sd socket

	Command command;
	socklen_t len = sizeof(struct sockaddr_in);
	if (recvfrom(sd, &command, sizeof(command), 0, (struct sockaddr *)sender, &len)<0)
	{
		perror("UDP socket error: receiveCommand()");
		command.id_command = ERROR;
	}

	return command;
}

int sendInquiryData(int sd, struct sockaddr_in* destinationaddr, Inquiry_data inq){
	// Send inquiry data to destinationaddr through sd socket

	pthread_mutex_lock(&inquiry_sem);

	if (sendto(sd, &inq, sizeof(inq), 0, (struct sockaddr *)destinationaddr, sizeof(struct sockaddr_in))<0)
	{
		perror("UDP socket error: sendInquiryData()");
		return -1;
	}

	pthread_mutex_unlock(&inquiry_sem);

	return 0;
}

Inquiry_data receiveInquiryData(int sd, struct sockaddr_in* sender){
	// Receive inquiry data from sender through sd socket

	Inquiry_data inq;
	socklen_t len = sizeof(struct sockaddr_in);
	if (recvfrom(sd, &inq, sizeof(inq), 0, (struct sockaddr *)sender, &len)<0)
	{
		perror("UDP socket error: receiveInquiryData()");
		inq.num_devices = 0;
	}

	return inq;
}

