#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <commands.h>
#include <string.h>
#include <pthread.h>

#define true 1
#define MAX_GUMSTIX 10

pthread_mutex_t inquiry_sem;
int num_gumstix = 0;
Gumstix gumstix[MAX_GUMSTIX];

//int templateSocketTCP(){
//
//	struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
//	char buf[1024] = { 0 };
//	int s, client, bytes_read, img_fd, img_len, byte_read, temp;
//	socklen_t opt = sizeof(rem_addr);
//	int result = 5;
//
//	// allocate socket
//	s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
//
//	// bind socket to port 1 of the first available
//	// local bluetooth adapter
//	loc_addr.rc_family = AF_BLUETOOTH;
//	loc_addr.rc_bdaddr = *BDADDR_ANY;
//	loc_addr.rc_channel = (uint8_t) 1;
//	bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));
//
//	// put socket into listening mode
//	listen(s, 1);
//
//	printf("Accepting connections ...\n");
//
//	while(true){
//		// accept one connection
//		client = accept(s, (struct sockaddr *)&rem_addr, &opt);
//
//		fprintf(stderr, "accepted connection from %s\n", buf);
//		memset(buf, 0, sizeof(buf));
//
//		img_fd = open("../../data/images/lena.jpeg", O_CREAT | O_WRONLY, S_IRWXU);
//
//		// read image lenght
//		read(client, &img_len, sizeof(img_len));
//		img_len = btohl(img_len);
//		printf("Received image size: %d\n", img_len);
//
//		printf("Receiving image ...\n");
//		byte_read = 0;
//		while(byte_read < img_len){
//			temp = read(client, buf, sizeof(char) * 1024);
//			write(img_fd, buf, temp);
//			byte_read += temp;
//		}
//		printf("Received image.\n");
//		close(img_fd);
//
//		// close connection
//		close(client);
//	}
//	close(s);
//}

void printDevices(Inquiry_data inq_data){
	int i;
	time_t nowtime;
	struct tm *nowtm;
	char tmbuf[64];
    
	nowtime = inq_data.timestamp.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof(tmbuf), "%d-%m-%Y %H:%M:%S", nowtm);
    
	for(i = 0; i < inq_data.num_devices; i++){
		printf("\t%d %s\t%s\t%d\n", i+1, inq_data.devices[i].bt_addr, inq_data.devices[i].name,
               inq_data.devices[i].valid ? inq_data.devices[i].rssi : -999);
        fflush(stdout);
	}
}

void printGumstix(){
	int i;
	char ipaddr[20], port[20];
	time_t nowtime;
	struct tm *nowtm;
	char tmbuf[64];

	for(i = 0; i < num_gumstix; i++){
		getnameinfo((struct sockaddr*)&gumstix[i].addr, sizeof(struct sockaddr_in), ipaddr, sizeof(ipaddr), port, sizeof(port), 0);
		nowtime = gumstix[i].lastseen.tv_sec;
		nowtm = localtime(&nowtime);
		strftime(tmbuf, sizeof(tmbuf), "%d-%m-%Y %H:%M:%S", nowtm);
		printf("Gumstix: %s (%s:%s) - lastseen: %s\n", gumstix[i].id_gumstix, ipaddr, port, tmbuf);
        printDevices(gumstix[i].lastInquiry);
	}
}

Gumstix* findGumstixById(char* id_gumstix){
	int i;

	for(i = 0; i < num_gumstix; i++){
		if(!strcmp(gumstix[i].id_gumstix, id_gumstix)){
			return &gumstix[i];
		}
	}

	return NULL;
}

Gumstix* findGumstixByAddr(struct sockaddr_in* gumstix_addr){
	int i;

	for(i=0; i < num_gumstix; i++){
		if(gumstix[i].addr.sin_addr.s_addr == gumstix_addr->sin_addr.s_addr){
			return &gumstix[i];
		}
	}

	return NULL;
}

int getGumstixPosition(struct sockaddr_in* gumstix_addr){
	int i;

	for(i = 0; i < num_gumstix; i++){
		if(gumstix[i].addr.sin_addr.s_addr == gumstix_addr->sin_addr.s_addr){
			return i;
		}
	}

	return -1;
}

void addGumstix(char* id_gumstix, struct sockaddr_in gumstix_addr, int socket){
    Gumstix *temp;
    
    temp = findGumstixById(id_gumstix);
    
    if (temp != NULL) {
        if ( temp->addr.sin_addr.s_addr != gumstix_addr.sin_addr.s_addr) { // Error: conflict name
            sendCommand(socket, &gumstix_addr, HELLO_ERR, "Id exists");
            printf("Error: name already exists - %s\n", id_gumstix);
        }
        else {
            gettimeofday(&temp->lastseen, NULL); // Already known
            sendCommand(socket, &gumstix_addr, HELLO_ACK, "");
            printf("Sent hello ack to %s\n", id_gumstix);
        }
    }
	else { // Non already known
		strcpy(gumstix[num_gumstix].id_gumstix, id_gumstix);
		gumstix[num_gumstix].addr = gumstix_addr;
		gettimeofday(&gumstix[num_gumstix].lastseen, NULL);
		num_gumstix++;
        
        sendCommand(socket, &gumstix_addr, HELLO_ACK, "");
        printf("Sent hello ack to %s\n", id_gumstix);
	}
	printGumstix();
}

void updateLastseen(struct sockaddr_in* gumstix_addr){
	Gumstix* temp;
	temp = findGumstixByAddr(gumstix_addr);
	if(temp != NULL){
		gettimeofday(&temp->lastseen, NULL);
	}
	printGumstix();
}


void* serviceThread(void* arg){

	Command command;
	struct sockaddr_in gumstixaddr;
	char ipaddr[20], port[20];
	int sService;
    Gumstix *temp;

	sService = bindSocketUDP(63171, 0);

	while(true){
		printf("Waiting command ...\n");
		command = receiveCommand(sService, &gumstixaddr);

		switch(command.id_command){

		case HELLO:
			getnameinfo((struct sockaddr*)&gumstixaddr, sizeof(struct sockaddr_in), ipaddr, sizeof(ipaddr), port, sizeof(port), 0);
			printf("Received Hello from %s (%s:%s)\n", command.param, ipaddr, port);
			addGumstix(command.param, gumstixaddr, sService);
			break;
		case ALIVE:
			updateLastseen(&gumstixaddr);
            printf("Service thread: alive received\n");
			break;
        case ALARM:
            updateLastseen(&gumstixaddr);
            temp = findGumstixByAddr(&gumstixaddr);
            printf("Alarm received from %s. Num devices: %s\n", temp->id_gumstix, command.param);
            break;
		default:
			printf("Command not valid ...\n");
		}
	}
}

void* inquiryThread(void* arg){

	Inquiry_data inq_data;
	Gumstix* temp;
	int sInquiry;
	struct sockaddr_in gumstixaddr;

	sInquiry = bindSocketUDP(63172, 0);

	while(true){
		printf("Waiting Inquiry ...\n");
		inq_data = receiveInquiryData(sInquiry, &gumstixaddr);
		temp = findGumstixByAddr(&gumstixaddr);
		if(temp != NULL){
			temp->lastInquiry = inq_data;
		}
        
        printDevices(inq_data);
	}
}

int main(int argc, char **argv)
{
	int sConsole;
	pthread_t inqThread, servThread;

	sConsole = bindSocketUDP(63170, 0);

	pthread_create(&inqThread, NULL, inquiryThread, NULL);
	pthread_create(&servThread, NULL, serviceThread, NULL);

	pthread_join(inqThread, NULL);
	pthread_join(servThread, NULL);

	return 0;
}
