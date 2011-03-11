/*
 * commands.c
 *
 *  Created on: Mar 9, 2011
 *      Author: luca
 */

#include "commands.h"
#include "bt-scan-rssi.h"

extern Configuration config;

/*
 * Questo file andrebbe riscritto in modo da renderlo più generale
 * e poterlo riutilizzare direttamente anche nel server visto che le
 * operazioni sono simmetriche
 */

// Socket variables
struct hostent *host;
struct sockaddr_in clientaddr, servaddr_console, servaddr_service;
int sd = -1;

int bindSocketUDP(){
	int  timeout = 10;

	memset((char *)&clientaddr, 0, sizeof(struct sockaddr_in));
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = INADDR_ANY;
	clientaddr.sin_port = 0;
	memset((char *)&servaddr_service, 0, sizeof(struct sockaddr_in));
	servaddr_service.sin_family = AF_INET;
	host = gethostbyname(config.server_ip);
	if (host == NULL)
	{
		printf("%s not found in /etc/hosts\n", config.server_ip);
		exit(2);
	}
	else
	{
		servaddr_console.sin_addr.s_addr=((struct in_addr *)(host->h_addr))->s_addr;
		servaddr_console.sin_port = htons(63170);
		servaddr_service.sin_addr.s_addr=((struct in_addr *)(host->h_addr))->s_addr;
		servaddr_service.sin_port = htons(63171);
	}

	sd=socket(AF_INET, SOCK_DGRAM, 0);
	if(sd<0) {perror("apertura socket"); exit(3);}
	printf("Creata la socket sd=%d\n", sd);
	/* BIND SOCKET, a una porta scelta dal sistema --------------- */
	if(bind(sd,(struct sockaddr *) &clientaddr, sizeof(clientaddr))<0)
	{perror("bind socket "); exit(1);}
	printf("Client: bind socket ok, alla porta %i\n", clientaddr.sin_port);
	setsockopt (sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	return sd;

}


int sendCommand(int id_command, char* param){

	Command command;

	if(sd < 0)
		bindSocketUDP();

	command.id_command = id_command;
	strcpy(command.param, param);

	if (sendto(sd, &command, sizeof(command), 0, (struct sockaddr *)&servaddr_service, sizeof(servaddr_service))<0)
	{
		perror("UDP socket error: sendCommand()");
		return -1;
	}

	return 0;
}

Command receiveCommand(){

	Command command;

	if(sd < 0)
		bindSocketUDP();

	if (recvfrom(sd, &command, sizeof(command), 0, NULL, NULL)<0)
	{
		perror("UDP socket error: receiveCommand()");
		command.id_command = ERROR;
	}

	return command;
}

int sendInquiryData(Inquiry_data inq){

	if(sd < 0)
		bindSocketUDP();

	/**
	 * Qui ci vuole sicuramente un semaforo
	 * Se spedisco i dati mentre inquiry_thread sta modificando la struttura
	 * ho dei dati non consistenti
	 */

	if (sendto(sd, &inq, sizeof(inq), 0, (struct sockaddr *)&servaddr_console, sizeof(servaddr_console))<0)
	{
		perror("UDP socket error: sendInquiryData()");
		return -1;
	}

	return 0;
}

Inquiry_data receiveInquiryData(){

	Inquiry_data inq;

	if(sd < 0)
		bindSocketUDP();

	if (recvfrom(sd, &inq, sizeof(inq), 0, NULL, NULL)<0)
	{
		perror("UDP socket error: receiveInquiryData()");
		inq.num_devices = 0;
	}

	return inq;
}

