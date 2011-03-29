#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <bt-scan-rssi.h>
#include <commands.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int force_send_inquiry, force_send_image;
extern Inquiry_data inq_data;
extern Configuration config;

pthread_t btscan_thread, alive_thread;
pthread_mutex_t config_sem, images_sem;
struct sockaddr_in servaddr_console, servaddr_service, servaddr_inquiry, servaddr_images;
int sd;

void* alive(void* args) {
	while(true) {
		sendCommand(sd, &servaddr_service, ALIVE, config.id_gumstix);
		sleep(ALIVE_INTERVAL);
	}
}

int sendImage(char* fileName){
	// Sends fileName (absolute path) image to server

	int s, status, img_fd, img_len;
	char buf[1024];

	// allocate a socket
	s = socket(AF_INET, SOCK_STREAM, 0);

	// connect to server
	status = connect(s, (struct sockaddr *)&servaddr_images, sizeof(servaddr_images));

	img_fd = open(fileName, O_RDONLY);
	img_len = 0;

	img_len = lseek(img_fd, 0L, SEEK_END);
	lseek(img_fd, 0L, SEEK_SET);

	printf("Sending image size: %i\n", img_len);
	img_len = htonl(img_len);
	write(s, &img_len, sizeof(img_len));

	printf("Sending image ...\n");
	while((img_len = read(img_fd, buf, sizeof(char) * 1024)) > 0){
		write(s, buf, sizeof(char) * img_len);
	}

	close(img_fd);
	close(s);

	printf("Image sent ...\n");

	return 0;
}

int initParameter(int argc, char** argv) {

	int c, missingS = true, missingN = true;
	opterr = 0;

	// Default configuration
	strcpy(config.id_gumstix, "Gumstix");
	config.alarm_threshold = 10;
	config.scan_length = 8;
	config.scan_interval = 0;
	config.auto_send_inquiry = true;
	config.auto_send_images = true;
	config.color_threshold = 40;
	config.image_width = 640;
	config.image_height = 480;
	force_send_image = false;
	force_send_inquiry = false;

	// i parametri seguiti da : richiedono un argomento obbligatorio
	while ((c = getopt(argc, argv, "hn:a:l:s:")) != -1)
		switch (c) {
		case 'n':
			strcpy(config.id_gumstix, optarg);
			missingN = false;
			break;
		case 'a':
			config.alarm_threshold = atoi(optarg);
			break;
		case 'l':
			config.scan_length = atoi(optarg);
			break;
		case 's':
			strcpy(config.server_ip, optarg); // questo deve essere obbligatorio
			missingS = false;
			break;
		case 'h':
			printf("Gumstix help\n");
			printf("Parameters:\n");
			printf("-s: set server address [required]\n");
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

			fprintf(stderr, "Try 'gumstix -h' for more information.\n");
			return 0;

		default:
			abort();
		}

	if(missingS){
		fprintf(stderr, "-s arg is required\n");
		fprintf(stderr, "Try 'gumstix -h' for more information.\n");
		return 0;
	}

	if(missingN){
		fprintf(stderr, "-n arg is required\n");
		fprintf(stderr, "Try 'gumstix -h' for more information.\n");
		return 0;
	}

	return 1;
}

int main(int argc, char** argv) {

	Command command, answer;
	int width, height, scan_interval, scan_length;
	char *buf;
	struct hostent *host;
	int result;

	if (!initParameter(argc, argv))
		return 1;

	// Setting server addresses and sockets
	memset((char *)&servaddr_console, 0, sizeof(struct sockaddr_in));
	memset((char *)&servaddr_service, 0, sizeof(struct sockaddr_in));
	memset((char *)&servaddr_inquiry, 0, sizeof(struct sockaddr_in));
	memset((char *)&servaddr_images, 0, sizeof(struct sockaddr_in));
	servaddr_console.sin_family = AF_INET;
	servaddr_service.sin_family = AF_INET;
	servaddr_inquiry.sin_family = AF_INET;
	servaddr_images.sin_family = AF_INET;
	printf("Server: %s\n", config.server_ip);
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
		servaddr_inquiry.sin_addr.s_addr=((struct in_addr *)(host->h_addr))->s_addr;
		servaddr_inquiry.sin_port = htons(63172);
		servaddr_images.sin_addr.s_addr=((struct in_addr *)(host->h_addr))->s_addr;
		servaddr_images.sin_port = htons(63173);
	}

	sd = bindSocketUDP(0, 0);

	// Hello to server
	printf("Sending HELLO to server ...\n");
	result = sendCommand(sd, &servaddr_service, HELLO, config.id_gumstix);
	printf("HELLO sent to server ...\n");
	command = receiveCommand(sd, NULL);
	if(command.id_command == HELLO_ERR){
		printf("%s \"%s\"\n", command.param, config.id_gumstix);
		exit(1);
	}

	// Initialize semaphores
	pthread_mutex_init(&config_sem, NULL);
	pthread_mutex_init(&images_sem, NULL);

	// Create alive thread
	pthread_create(&alive_thread, NULL, alive, NULL);

	// Create scanning thread
	pthread_create(&btscan_thread, NULL, executeInquire, NULL);

	// Receiving commands from server console
	while (true) {

		memset(&answer, 0, sizeof(answer));
		strcpy(answer.param, "");

		command = receiveCommand(sd, NULL);
		switch (command.id_command) {
		case ERROR:
			printf("Received ERROR from server\n");
			break;

		/************************ GET ************************/
		case GET_INQUIRY:
			printf("Received GET_INQUIRY from server\n");
			force_send_inquiry = true;
			answer.id_command = PARAM_VALUE;
			strcpy(answer.param, "OK");
			break;

		case GET_AUTO_SEND_INQUIRY:
			printf("Received GET_AUTO_SEND_INQUIRY from server\n");
			answer.id_command = PARAM_VALUE;
			sprintf(answer.param, "%s", config.auto_send_inquiry ? "true" : "false");
			break;

		case GET_IMAGE:
			printf("Received GET_IMAGE from server\n");
			force_send_image = true;
			answer.id_command = PARAM_VALUE;
			strcpy(answer.param, "OK");
			break;

		case GET_AUTO_SEND_IMAGES:
			printf("Received GET_AUTO_SEND_IMAGES from server\n");
			answer.id_command = PARAM_VALUE;
			sprintf(answer.param, "%s", config.auto_send_images ? "true" : "false");
			break;

		case GET_IMAGE_RESOLUTION:
			printf("Received GET_IMAGE_RESOLUTION from server\n");
			answer.id_command = PARAM_VALUE;
			sprintf(answer.param, "%dx%d", config.image_width, config.image_height);
			break;

		case GET_SCAN_INTERVAL:
			printf("Received GET_SCAN_INTERVAL from server\n");
			answer.id_command = PARAM_VALUE;
			sprintf(answer.param, "%d", config.scan_interval);
			break;

		case GET_SCAN_LENGTH:
			printf("Received GET_SCAN_LENGHT from server\n");
			answer.id_command = PARAM_VALUE;
			sprintf(answer.param, "%d", config.scan_length);
			break;

		/************************ SET ************************/
		case SET_AUTO_SEND_INQUIRY:
			printf("Received SET_AUTO_SEND_INQUIRY (value: %s) from server\n", command.param);
			answer.id_command = PARAM_ACK;
			pthread_mutex_lock(&config_sem);
			if(!strcasecmp(command.param, "false")){
				config.auto_send_inquiry = false;
			}else{
				config.auto_send_inquiry = true; // default
			}
			pthread_mutex_lock(&config_sem);
			printf("SET_AUTO_SEND_INQUIRY: %s\n", config.auto_send_inquiry ? "true" : "false");
			break;

		case SET_AUTO_SEND_IMAGES:
			printf("Received SET_AUTO_SEND_IMAGES (value: %s) from server\n", command.param);
			answer.id_command = PARAM_ACK;
			pthread_mutex_lock(&config_sem);
			if(!strcasecmp(command.param, "false")){
				config.auto_send_images = false;
			}else{
				config.auto_send_images = true; // default
			}
			pthread_mutex_lock(&config_sem);
			printf("SET_AUTO_SEND_IMAGES: %s\n", config.auto_send_images ? "true" : "false");
			break;

		case SET_IMAGE_RESOLUTION:
			printf("Received SET_IMAGE_RESOLUTION (value: %s) from server\n", command.param);
			answer.id_command = PARAM_ACK;
			// Normalizing resolution
			buf = strtok(command.param, "x");
			width = atoi(buf);
			buf = strtok(NULL, "x");
			height = atoi(buf);
			if(width >= 0 && width <= 320){
				width = 320;
				height = 240;
			}else if(width > 320 && width <= 640){
				width = 640;
				height = 480;
			}else if(width > 640 && width <= 1024){
				width = 1024;
				height = 768;
			}else if(width > 1024 && width <= 1280){
				width = 1280;
				height = 1024;
			}else if(width > 1280 && width <= 1600){
				width = 1600;
				height = 1200;
			}else if(width > 1600 && width <= 2048){
				width = 2048;
				height = 1536;
			}else{ // default
				width = 640;
				height = 480;
			}
			pthread_mutex_lock(&config_sem);
			config.image_width = width;
			config.image_height = height;
			pthread_mutex_lock(&config_sem);
			printf("SET_IMAGE_RESOLUTION: %dx%d\n", config.image_width, config.image_height);
			break;

		case SET_SCAN_INTERVAL:
			printf("Received SET_SCAN_INTERVAL (value: %s) from server\n", command.param);
			answer.id_command = PARAM_ACK;
			scan_interval = atoi(command.param);
			if(scan_interval < 0 || scan_interval > 100)
				scan_interval = 0; // default
			pthread_mutex_lock(&config_sem);
			config.scan_interval = scan_interval;
			pthread_mutex_lock(&config_sem);
			printf("SET_SCAN_INTERVAL: %d\n", config.scan_interval);
			break;

		case SET_SCAN_LENGTH:
			printf("Received SET_SCAN_LENGTH (value: %s) from server\n", command.param);
			answer.id_command = PARAM_ACK;
			scan_length = atoi(command.param);
			if(scan_length < 0 || scan_length > 10)
				scan_length = 0; // default
			pthread_mutex_lock(&config_sem);
			config.scan_length = scan_length;
			pthread_mutex_lock(&config_sem);
			printf("SET_SCAN_LENGTH: %d\n", config.scan_length);
			break;

		default:
			printf("Unknown command\n");
			break;
		}

		printf("Sending answer to server\n");
		sendCommand(sd, &servaddr_console, answer.id_command, answer.param);

	}

	return 0;
}
