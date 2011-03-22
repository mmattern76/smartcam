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
	
	// If not found
	return NULL;
}

Gumstix* findGumstixByAddr(struct sockaddr_in* gumstix_addr){
	int i;

	for(i=0; i < num_gumstix; i++){
		if(gumstix[i].addr.sin_addr.s_addr == gumstix_addr->sin_addr.s_addr){
			return &gumstix[i];
		}
	}

	// If not found
	return NULL;
}

int getGumstixPosition(struct sockaddr_in* gumstix_addr){
	int i;

	for(i = 0; i < num_gumstix; i++){
		if(gumstix[i].addr.sin_addr.s_addr == gumstix_addr->sin_addr.s_addr){
			return i;
		}
	}

	// If not found
	return -1;
}

void addGumstix(char* id_gumstix, struct sockaddr_in gumstix_addr, int socket){
	Gumstix *temp;

	temp = findGumstixById(id_gumstix);

	if (temp != NULL) { // Already known
		if ( temp->addr.sin_addr.s_addr != gumstix_addr.sin_addr.s_addr) { 
			// Error: trying to add a Gumstix with same id
			// and different IP address
			sendCommand(socket, &gumstix_addr, HELLO_ERR, "Id exists");
			printf("Error: name already exists - %s\n", id_gumstix);
		}
		else {
			// Already known: this is a new Hello from
			// the same Gumstix
			gettimeofday(&temp->lastseen, NULL);
			sendCommand(socket, &gumstix_addr, HELLO_ACK, "");
			printf("Sent hello ack to %s\n", id_gumstix);
		}
	}
	else { 
		// Non already known: first Hello
		// Adding new entry
		strcpy(gumstix[num_gumstix].id_gumstix, id_gumstix);
		gumstix[num_gumstix].addr = gumstix_addr;
		gettimeofday(&gumstix[num_gumstix].lastseen, NULL);
		num_gumstix++;

		sendCommand(socket, &gumstix_addr, HELLO_ACK, "");
		printf("Sent hello ack to %s\n", id_gumstix);
		printf("Added new Gumstix: %s\n", id_gumstix);
		printGumstix();
	}
}

void updateLastseen(struct sockaddr_in* gumstix_addr){
	Gumstix* temp;
	temp = findGumstixByAddr(gumstix_addr);
	if(temp != NULL){
		gettimeofday(&temp->lastseen, NULL);
		printGumstix();
	}
}

void displayImage(char *file_name, char *window_name) {
    IplImage *img;

    // Display Image
    cvNamedWindow(window_name, CV_WINDOW_AUTOSIZE);
    
    img = cvLoadImage(file_name, CV_LOAD_IMAGE_COLOR);
    cvShowImage(window_name, img);
    
    cvReleaseImage(&img);
}



void* imagesThread(void* arg){
	// This thread receives images from Gumstixes

	Gumstix *temp;
	int  listen_sd, conn_sd; // Listen and connection socket
	int port, byte_read, img_fd, img_len;
    socklen_t len;
	const int on = 1;
	struct sockaddr_in gumstixaddr, servaddr;
	char buf[1024] = { 0 };
	port = 63173;

	memset ((char *)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);

	listen_sd=socket(AF_INET, SOCK_STREAM, 0);

	if(listen_sd <0)
	{perror("creazione socket "); exit(1);}
	printf("Server: created listen socket for receiving images from gumstixes, fd=%d\n", listen_sd);

	if(setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0)
	{perror("set listen socket options"); exit(1);}

	if(bind(listen_sd,(struct sockaddr *) &servaddr, sizeof(servaddr))<0)
	{perror("bind listen socket"); exit(1);}

	if (listen(listen_sd, 5)<0)
	{perror("listen"); exit(1);}

	while(true){
		len=sizeof(gumstixaddr);

		if((conn_sd=accept(listen_sd,(struct sockaddr *)&gumstixaddr,&len))<0){
			if (errno==EINTR){
				perror("continuing with accept");
				continue;
			}
			else exit(1);
		}

		temp = findGumstixByAddr(&gumstixaddr);
		if(temp == NULL){
			close(conn_sd);
			continue;
		}
		
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
        sprintf(buf, "%s.jpeg", temp->id_gumstix);
        
        displayImage(buf, temp->id_gumstix);

	}

	close(listen_sd);
}

void* serviceThread(void* arg){
	// This thread listens for HELLO, ALIVE, and ALARM
	// commands from Gumstixes

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
	// This thread receives inquiries from Gumstixes
	
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
    
    
    while(true) {
        cvWaitKey(10);
    }


	pthread_join(inqThread, NULL);
	pthread_join(servThread, NULL);
    pthread_join(imgThread, NULL);

	return 0;
}
