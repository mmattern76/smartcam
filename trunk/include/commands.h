#ifndef COMMANDS_H
#define COMMANDS_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <struct.h>

#define PARAM_LENGHT 11
#define ALIVE_INTERVAL 10

// Commands list
#define ERROR 0
#define HELLO 1
#define HELLO_ACK 2
#define HELLO_ERR 3
#define ALIVE 4
#define ALARM 5

typedef struct{
	int id_command;
	char param[PARAM_LENGHT];
} Command;

int bindSocketUDP(int localPort, int timeoutSeconds);

int sendCommand(int sd, struct sockaddr_in* destinationaddr, int id_command, char* param);
Command receiveCommand(int sd, struct sockaddr_in* sender);

int sendInquiryData(int sd, struct sockaddr_in* destinationaddr, Inquiry_data inq);
Inquiry_data receiveInquiryData(int sd, struct sockaddr_in* sender);

#endif
