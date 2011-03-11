#include "bt-scan-rssi.h"
#include "commands.h"
#include <pthread.h>

extern Inquiry_data inq_data;
extern Configuration config;

pthread_t btscan_thread, alive_thread;


void* alive(void* args) {
	while(true) {
		sendCommand(ALIVE, NULL);
		sleep(ALIVE_INTERVAL);
	}

}

int initParameter(int argc, char** argv) {

	int c;
	opterr = 0;

	// Default configuration
	strcpy(config.id_gumstix, "Gumstix");
	config.alarm_threshold = 4;
	config.scan_lenght = 8;

	// i parametri seguiti da : richiedono un argomento obbligatorio
	while ((c = getopt(argc, argv, "hn:a:l:s:")) != -1)
		switch (c) {
		case 'n':
			strcpy(config.id_gumstix, optarg);
			break;
		case 'a':
			config.alarm_threshold = atoi(optarg);
			break;
		case 'l':
			config.scan_lenght = atoi(optarg);
			break;
		case 's':
			strcpy(config.server_ip, optarg); // questo deve essere obbligatorio
			break;
		case 'h':
			printf("Gumstix help\n");
			printf("Parameters:\n");
			printf("-s: set server address\n");
			printf("-n: set gumstix identifier\n");
			printf("-l: set inquiry length\n");
			printf("-a: set alarm threshold\n");
			printf(".h: print this help\n");
			return 0;
			break;
		case '?':
			if (optopt == 'n' || optopt == 'a' || optopt == 'l' || optopt
					== 's')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);

			fprintf(stderr, "Try 'gumstix -h' for more information.");
			return 0;

		default:
			abort();
		}

	return 1;
}

int main(int argc, char** argv) {

	Command command;

	if (!initParameter(argc, argv))
		return 1;

	// Hello to server
	sendCommand(HELLO, NULL);

	pthread_create(&alive_thread, NULL, alive, NULL);

	// Create scanning thread
	pthread_create(&btscan_thread, NULL, executeInquire, NULL);

	// Receiving commands from server
	while (true) {

		command = receiveCommand();
		switch (command.id_command) {
		case ERROR:
			break;

		default:
			printf("Unknown command\n");
			break;
		}

	}

	return 0;
}
