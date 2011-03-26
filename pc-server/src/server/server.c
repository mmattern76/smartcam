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

	if(setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0)
	{perror("set listen socket options"); exit(1);}

	if(bind(listen_sd,(struct sockaddr *) &servaddr, sizeof(servaddr))<0)
	{perror("bind listen socket"); exit(1);}

	if (listen(listen_sd, 5)<0)
	{perror("listen"); exit(1);}
	
	printf("Images thread ready ...\n");

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
	
	printf("Service thread ready ...\n");

	while(true){
		command = receiveCommand(sService, &gumstixaddr);

		switch(command.id_command){

		case HELLO:
			getnameinfo((struct sockaddr*)&gumstixaddr, sizeof(struct sockaddr_in), ipaddr, sizeof(ipaddr), port, sizeof(port), 0);
			printf("Service thread: received Hello from %s (%s:%s)\n", command.param, ipaddr, port);
			addGumstix(command.param, gumstixaddr, sService);
			break;
		case ALIVE:
			updateLastseen(&gumstixaddr);
			printf("Service thread: alive received from %s\n", command.param);
			break;
		case ALARM:
			updateLastseen(&gumstixaddr);
			temp = findGumstixByAddr(&gumstixaddr);
			printf("Service thread: alarm received from %s. Num devices: %s\n", temp->id_gumstix, command.param);
			break;
		default:
			printf("Service thread: command not valid ...\n");
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
	
	printf("Inquiry thread ready ...\n");

	while(true){
		inq_data = receiveInquiryData(sInquiry, &gumstixaddr);
		temp = findGumstixByAddr(&gumstixaddr);
		printf("Received inquiry from %s:\n", temp->id_gumstix);
		if(temp != NULL){
			temp->lastInquiry = inq_data;
		}

		printDevices(inq_data);
	}
}


enum cmd_id getCommandIdForParameter(char* param_name, int is_set) {
    
    if (!strcasecmp(param_name, "auto_send_inquiry")) {
        return is_set ? SET_AUTO_SEND_INQUIRY : GET_AUTO_SEND_INQUIRY;
    }
    else if (!strcasecmp(param_name, "auto_send_images")) {
        return is_set ? SET_AUTO_SEND_IMAGES : GET_AUTO_SEND_IMAGES;
    }
    
    else if (!strcasecmp(param_name, "scan_length")) {
        return is_set ?  SET_SCAN_LENGTH : GET_SCAN_LENGTH;
    }
    else if (!strcasecmp(param_name, "image")) {
        return is_set ?  ERROR : GET_IMAGE;
    }
    else if (!strcasecmp(param_name, "inquiry")) {
        return is_set ?  ERROR : GET_INQUIRY;
    }
    
    
    else return ERROR;

}


void* consoleThread(void* arg){
	// This thread manages the interaction with the user 
	
    int sConsole;
    char cmd_string[255];
    
    char delims[] = " ";
    char *cmd_name = NULL;
    char *cmd_target = NULL;
    char *cmd_param_name = NULL;
    char *cmd_param_value = NULL;    
    enum cmd_id command_id;
    
    Command gumstix_answer;
    
	Gumstix* target;
    
	sConsole = bindSocketUDP(63170, 0);
	
	printf("Console thread ready ...\n");
    
	while(true){
        printf("> ");

        memset(cmd_string, 0, 255);
        
        if (gets(cmd_string) == NULL) {
            continue;
        }
        
        if (strlen(cmd_string) == 0) {
            continue;
        }
        
        cmd_name = strtok( cmd_string, delims );
        
        // Comandi che agiscono localmente al server
        if (!strcasecmp(cmd_name, "list")) {
            printf("Stampo la lista di tutti le gumstix\n");
            continue;
        }
        
        if (!strcasecmp(cmd_name, "help")) {
            printf("Stampo la lista di tutti i comandi validi\n");
            continue;
        }
        
        
        // Comandi che scambiano dati con le gumstix
        cmd_target = strtok( NULL, delims );
        target = findGumstixById(cmd_target);
        
//        if (target == NULL) {
//            printf("Error for command %s: can't find the following gumstix id: %s\n", cmd_name, cmd_target);
//            continue;
//        }
        
        
        if (!strcasecmp(cmd_name, "get")) {
            cmd_param_name = strtok( NULL, delims );
            
            if (cmd_param_name == NULL) {
                printf("Syntax error: get gumstix_id param_name\n");
                continue;
            }
            
            printf("Get %s\n", cmd_param_name);
            
            command_id = getCommandIdForParameter(cmd_param_name, false);
            if (command_id == ERROR) {
                printf("Unknown parameter: %s\n", cmd_param_name);
                continue;
            }
            
            printf("OK: invio il comando per richiedere il valore del parametro\n");
            // sendCommand(sConsole, &target->addr, command_id, "");
            
            if (command_id != GET_IMAGE) {
                // gumstix_answer = receiveCommand(sConsole, &target->addr);
                // printf("%s: %s", cmd_param_name, gumstix_answer.param);
                printf("Qui stampo la risposta\n");
            }
            continue;
        }
        
        
        if (!strcasecmp(cmd_name, "set")) {
            cmd_param_name = strtok( NULL, delims );
            cmd_param_value = strtok( NULL, delims );
            
            if (cmd_param_name == NULL || cmd_param_value == NULL) {
                printf("Syntax error: set gumstix_id param_name param_value\n");
                continue;
            }
            
            printf("Set %s to value: %s\n", cmd_param_name, cmd_param_value);
            
            command_id = getCommandIdForParameter(cmd_param_name, true);
            if (command_id == ERROR) {
                printf("Parameter is unknown or unsettable: %s\n", cmd_param_name);
                continue;
            }
            
            printf("OK: invio il comando per modificare il parametro\n");
            // sendCommand(sConsole, &target->addr, command_id, cmd_param_value);
            
            // Lo mettiamo un ACK?
            /*
            gumstix_answer = receiveCommand(sConsole, &target->addr);
            if (gumstix_answer.id_command != PARAM_ACK ) {
                printf("Error while setting parameter: %s\n", cmd_param_name);
            }
             */
            continue;
        }
        
        printf("Unknown command: %s\nType 'help' for more information\n", cmd_name);
	}
}


int main(int argc, char **argv)
{
	pthread_t inqThread, servThread, imgThread, conThread;

	

	pthread_create(&inqThread, NULL, inquiryThread, NULL);
	pthread_create(&servThread, NULL, serviceThread, NULL);
    pthread_create(&imgThread, NULL, imagesThread, NULL);
    pthread_create(&conThread, NULL, consoleThread, NULL);

    
    while(true) {
        if(cvWaitKey(10) == 27){
			cvDestroyAllWindows();
		}
    }


	pthread_join(inqThread, NULL);
	pthread_join(servThread, NULL);
    pthread_join(imgThread, NULL);
    pthread_join(conThread, NULL);


	return 0;
}
