#include <stdio.h>
#include <stdlib.h>
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
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

#define true 1
#define MAX_GUMSTIX 10

pthread_mutex_t inquiry_sem;
int num_gumstix = 0;
Gumstix gumstix[MAX_GUMSTIX];


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

void* imagesThread(void* arg){
	Gumstix *temp;
	int  listen_sd, conn_sd; //Socket ascolto e connessione effettiva
	int port, byte_read, img_fd, img_len;
    socklen_t len;
	const int on = 1;
	struct sockaddr_in gumstixaddr, servaddr;
	char buf[1024] = { 0 };
	port = 63173;
    IplImage *received_img;

	memset ((char *)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);

	listen_sd=socket(AF_INET, SOCK_STREAM, 0);

	if(listen_sd <0)
	{perror("creazione socket "); exit(1);}
	printf("Server: created socket for receiving images from gumstix, fd=%d\n", listen_sd);

	if(setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0)
	{perror("set opzioni socket d'ascolto"); exit(1);}

	if(bind(listen_sd,(struct sockaddr *) &servaddr, sizeof(servaddr))<0)
	{perror("bind socket d'ascolto"); exit(1);}

	if (listen(listen_sd, 5)<0)
	{perror("listen"); exit(1);}

	while(true){
		len=sizeof(gumstixaddr);

		if((conn_sd=accept(listen_sd,(struct sockaddr *)&gumstixaddr,&len))<0){
			if (errno==EINTR){
				perror("Forzo la continuazione della accept");
				continue;
			}
			else exit(1);
		}

		temp = findGumstixByAddr(&gumstixaddr);
		printf("Gumstix %s is sending an image ...\n", temp->id_gumstix);
		sprintf(buf, "%s.jpeg", temp->id_gumstix);
		img_fd = open(buf, O_CREAT | O_WRONLY, S_IRWXU);

		memset(buf, 0, sizeof(buf));
		// read image lenght
		read(conn_sd, &img_len, sizeof(img_len));
		img_len = ntohl(img_len);
		printf("Received image size: %d\n", img_len);

		printf("Receiving image ...\n");
		byte_read = 0;
		while(byte_read < img_len){
			len = read(conn_sd, buf, sizeof(char) * 1024);
			write(img_fd, buf, len);
			byte_read += len;
		}
		printf("Received image.\n");
		close(img_fd);

		// close connection
		close(conn_sd);
        
        
//        // Display Image
//        cvNamedWindow(temp->id_gumstix, CV_WINDOW_AUTOSIZE);
//        
//        sprintf(buf, "%s.jpeg", temp->id_gumstix);
//        received_img = cvLoadImage(buf, CV_LOAD_IMAGE_COLOR);
//        cvShowImage(temp->id_gumstix, received_img);
//        
//        cvReleaseImage(&received_img);
//        
//        cvWaitKey(10);
//        
	}

	close(listen_sd);
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
	pthread_t inqThread, servThread, imgThread;

	sConsole = bindSocketUDP(63170, 0);

	pthread_create(&inqThread, NULL, inquiryThread, NULL);
	pthread_create(&servThread, NULL, serviceThread, NULL);
    pthread_create(&imgThread, NULL, imagesThread, NULL);


	pthread_join(inqThread, NULL);
	pthread_join(servThread, NULL);
    pthread_join(imgThread, NULL);

	return 0;
}
