#include "bt-scan-rssi.h"
#include "commands.h"
#include <pthread.h>

extern Inquiry_data inq_data;
extern Configuration config;

int main(int argc, char** argv){


	Command command;
	pthread_t thread;


	// Default configuration
	strcpy(config.id_gumstix, "Gumstix"); // cambiare "Gumstix" con un parametro di argv
	config.alarm_threshold = 4;
	config.scan_lenght = 8;

	// Hello to server
	sendCommand(HELLO, NULL);

	// Create scanning thread
	pthread_create(&thread, NULL, executeInquire, NULL);

	// Receiving commands from server
	while(true){

		command = receiveCommand();
		switch(command.id_command){
		case ERROR:	break;

		default:
			printf("Unknown command\n");
			break;
		}

	}


	return 0;
}
