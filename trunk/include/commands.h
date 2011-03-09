#ifndef COMMANDS_H
#define COMMANDS_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PARAM_LENGHT 11
#define ERROR 0
#define HELLO 1

typedef struct{
	int id_command;
	char param[PARAM_LENGHT];
} Command;

int sendCommand(int id_command, char* param);
Command receiveCommand();

#endif
