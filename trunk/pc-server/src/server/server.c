#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

#define true 1
#define false 0
#define MAX_GUMSTIX 10
#define CONSOLE_SOCKET_TIMEOUT 5
#define LOG_FILE_NAME "log.txt"

pthread_mutex_t log_sem;
int num_gumstix = 0;
Gumstix gumstix[MAX_GUMSTIX];

void printl( const char* format, ... ) {
	// Print on log file
    
    time_t nowtime;
	struct timeval now;
	struct tm *nowtm;
	char tmbuf[64];
	FILE *log_fd;
	
	gettimeofday(&now, NULL);
	nowtime = now.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof(tmbuf), "[%d-%m-%Y %H:%M:%S]\t", nowtm);
    
    pthread_mutex_lock(&log_sem);
    if ((log_fd = fopen(LOG_FILE_NAME, "a")) == NULL)
    {
        printf("Error: can't open log file");
        exit(1);
    } 
    
    va_list args;
    va_start( args, format );
	fprintf(log_fd, "%s", tmbuf);
    vfprintf( log_fd, format, args );
    va_end( args );
	
    fclose(log_fd);
    
    pthread_mutex_unlock(&log_sem);	
	
}

void printDevices(Inquiry_data inq_data, int write_log){
	int i;

	if(inq_data.num_devices == 0){
		if(write_log)
			printl("\tNo known devices ...\n");
		else
			printf("\tNo known devices ...\n");
		return;
	}

	for(i = 0; i < inq_data.num_devices; i++){
		if(write_log)
			printl("\t%d %s\t%s\t%d\n", i+1, inq_data.devices[i].bt_addr, inq_data.devices[i].name,
					inq_data.devices[i].valid ? inq_data.devices[i].rssi : -999);
		else
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
		printl("Gumstix: %s (%s:%s) - lastseen: %s\n", gumstix[i].id_gumstix, ipaddr, port, tmbuf);
		printDevices(gumstix[i].lastInquiry, true);
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
			printl("Error: name already exists - %s\n", id_gumstix);
		}
		else {
			// Already known: this is a new Hello from
			// the same Gumstix
			gettimeofday(&temp->lastseen, NULL);
			sendCommand(socket, &gumstix_addr, HELLO_ACK, "");
			printl("Sent hello ack to %s\n", id_gumstix);
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
		printl("Sent hello ack to %s\n", id_gumstix);
		printl("Added new Gumstix: %s\n", id_gumstix);
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
	
	printl("Images thread ready ...\n");

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
		
		printl("Gumstix %s is sending an image ...\n", temp->id_gumstix);
		sprintf(buf, "%s.jpeg", temp->id_gumstix);
		img_fd = open(buf, O_CREAT | O_WRONLY, S_IRWXU);

		memset(buf, 0, sizeof(buf));
		// read image lenght
		read(conn_sd, &img_len, sizeof(img_len));
		img_len = ntohl(img_len);
		printl("Received image size: %d\n", img_len);

		printl("Receiving image ...\n");
		byte_read = 0;
		while(byte_read < img_len){
			len = read(conn_sd, buf, sizeof(char) * 1024);
			write(img_fd, buf, len);
			byte_read += len;
		}
		printl("Received image.\n");
		close(img_fd);

		// close connection
		close(conn_sd);
        sprintf(buf, "%s.jpeg", temp->id_gumstix);
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
	
	printl("Service thread ready ...\n");

	while(true){
		command = receiveCommand(sService, &gumstixaddr);

		switch(command.id_command){

		case HELLO:
			getnameinfo((struct sockaddr*)&gumstixaddr, sizeof(struct sockaddr_in), ipaddr, sizeof(ipaddr), port, sizeof(port), 0);
			printl("Service thread: received Hello from %s (%s:%s)\n", command.param, ipaddr, port);
			addGumstix(command.param, gumstixaddr, sService);
			break;
		case ALIVE:
			updateLastseen(&gumstixaddr);
			printl("Service thread: alive received from %s\n", command.param);
			break;
		case ALARM:
			updateLastseen(&gumstixaddr);
			temp = findGumstixByAddr(&gumstixaddr);
			printl("Service thread: alarm received from %s. Num devices: %s\n", temp->id_gumstix, command.param);
			break;
		default:
			printl("Service thread: command not valid ...\n");
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
	
	printl("Inquiry thread ready ...\n");

	while(true){
		inq_data = receiveInquiryData(sInquiry, &gumstixaddr);
		temp = findGumstixByAddr(&gumstixaddr);
		printl("Received inquiry from %s:\n", temp->id_gumstix);
		if(temp != NULL){
			temp->lastInquiry = inq_data;
		}

		printDevices(inq_data, true);
	}
}


enum cmd_id getCommandIdForParameter(char* param_name, int is_set) {
    
    if (!strcasecmp(param_name, "auto_send_inquiry")) {
        return is_set ? SET_AUTO_SEND_INQUIRY : GET_AUTO_SEND_INQUIRY;
    }
    else if (!strcasecmp(param_name, "auto_send_images")) {
        return is_set ? SET_AUTO_SEND_IMAGES : GET_AUTO_SEND_IMAGES;
    }
    else if (!strcasecmp(param_name, "image_resolution")) {
            return is_set ? SET_IMAGE_RESOLUTION : GET_IMAGE_RESOLUTION;
    }
    else if (!strcasecmp(param_name, "scan_length")) {
        return is_set ?  SET_SCAN_LENGTH : GET_SCAN_LENGTH;
    }
	else if (!strcasecmp(param_name, "scan_interval")) {
        return is_set ?  SET_SCAN_INTERVAL : GET_SCAN_INTERVAL;
    }
	else if (!strcasecmp(param_name, "alarm_threshold")) {
		return is_set ?  SET_ALARM_THRESHOLD : GET_ALARM_THRESHOLD;
	}
	else if (!strcasecmp(param_name, "color_threshold")) {
		return is_set ?  SET_COLOR_THRESHOLD : GET_COLOR_THRESHOLD;
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
	
    int sConsole, i;
    char cmd_string[255];
	char cmd_temp[255];
    char delims[] = " ";
    char *cmd_name = NULL;
    char *cmd_target = NULL;
    char *cmd_param_name = NULL;
    char *cmd_param_value = NULL;    
    enum cmd_id command_id;
	struct timeval now;
    Command gumstix_answer;
	Gumstix* target;
    
	sConsole = bindSocketUDP(63170, CONSOLE_SOCKET_TIMEOUT); // with timeout
	printl( "Console thread ready ...\n");
    printf("Server console (try 'help' for commands):\n");
    
	while(true){
		
        printf("> ");

		memset(cmd_temp, 0, 255);
        memset(cmd_string, 0, 255);
        
        if (fgets(cmd_temp, 255, stdin) == NULL) { // Buffer overflow safe
            continue;
        }
		
		strncpy(cmd_string, cmd_temp, strlen(cmd_temp) - 1); // Delete \n
        
        if (strlen(cmd_string) == 0) {
            continue;
        }
		
		// Get timestamp
		gettimeofday(&now, NULL);
        
        cmd_name = strtok( cmd_string, delims );
        
        // Local commands
		/******************** EXIT ********************/
		if (!strcasecmp(cmd_name, "exit")) {
			exit(0);
		}
		/**********************************************/
		
		/******************** LIST ********************/
        if (!strcasecmp(cmd_name, "list")) {
			int isSomeoneAlive = 0;
			for (i = 0; i < num_gumstix; i++) {
				Gumstix *temp = &gumstix[i];
				if(ISALIVE(temp, now)){
					if(!isSomeoneAlive){
						printf("\tAvailable gumstixes:\n");
						isSomeoneAlive = 1;
					}
					printf("\t\t%s\n", gumstix[i].id_gumstix);
				}
			}
			if (!isSomeoneAlive) {
				printf("\t No available gumstixes.\n");
			}
            continue;
        }
		/**********************************************/
		
        /******************** HELP ********************/
        if (!strcasecmp(cmd_name, "help")) {
            printf("\tAvailable commands:\n");
			printf("\t\tlist - displays all available gumstixes id\n\n");
			printf("\t\tSET <gumstix_id> <param_name> <param_value>\n\n");
			printf("\t\tGET <gumstix_id> <param_name>\n\n");
			printf("\t\t\t<gumstix_id>:\n"); 
			printf("\t\t\t\tgumstix id got from 'list'\n\n");
			printf("\t\t\t<param_name>:\n");
			printf("\t\t\t\tIMAGE (get only) - get last image from gumstix\n");
			printf("\t\t\t\tINQUIRY (get only) - get last bluetooth inquiry data from gumstix\n");
			printf("\t\t\t\tAUTO_SEND_INQUIRY (get/set) - auto send inquiries (true/false)\n");
			printf("\t\t\t\tAUTO_SEND_IMAGES (get/set) - auto send images (true/false)\n");
			printf("\t\t\t\tIMAGE_RESOLUTION (get/set) - image resolution (es: 640x480)\n");
			printf("\t\t\t\tSCAN_INTERVAL (get/set) - seconds between two bluetooth scans (0-100)\n");
			printf("\t\t\t\tSCAN_LENGTH (get/set) - bluetooth scan lenght (value * 1,26) (1-10)\n");
			printf("\t\t\t\tALARM_THRESHOLD (get/set) - min number of bluetooth devices to trigger the alarm (1-100)\n");
			printf("\t\t\t\tCOLOR_THRESHOLD (get/set) - conditions change background detection (1-100)\n\n");
			printf("\t\texit - exit program\n\n");	
			printf("\t\thelp - prints this help\n\n");	
			continue;
        }
        /**********************************************/
        
        // Gumstix commands
        cmd_target = strtok( NULL, delims );
        if (cmd_target == NULL) {
            printf("\tUnknown command: %s\n\tType 'help' for more information\n", cmd_name);
            continue;
        }
        
        target = findGumstixById(cmd_target);
  
        if (target == NULL) {
            printf("\tError for command %s: can't find the following gumstix id: %s\n", cmd_name, cmd_target);
			printf("\tTry 'list' to get all available gumstixes\n");
            continue;
        }
        
		// Check if target is alive
		if(!ISALIVE(target, now)){
					printf("\tGumstix %s is not alive. Try 'list' to get all available gumstixes\n", cmd_target);
					continue;
		}       
        
		/******************** GET *********************/
        if (!strcasecmp(cmd_name, "get")) {
            cmd_param_name = strtok( NULL, delims );
            
            if (cmd_param_name == NULL) {
                printf("\tSyntax error: get gumstix_id param_name\n");
                continue;
            }
            
            printf("\tGet %s\n", cmd_param_name);
            
            command_id = getCommandIdForParameter(cmd_param_name, false);
            if (command_id == ERROR) {
                printf("\tUnknown <param_name>: %s\n", cmd_param_name);
                continue;
            }
            
			// Send Command to target
            printf("\tSending command to %s\n", target->id_gumstix);
            sendCommand(sConsole, &target->addr, command_id, "");
            
			// Receive Answer from target
            gumstix_answer = receiveCommand(sConsole, NULL); // Not overwrite gumstix address
			if (gumstix_answer.id_command != PARAM_VALUE) {
				printf("\tError while getting parameter: %s\n", cmd_param_name);
				continue;
			}else {
				printf("\t%s: %s\n", cmd_param_name, gumstix_answer.param);
			}
			
			if (command_id == GET_IMAGE) {
                printf("\tReceiving image ...\n");
				continue;
			}
			
			if (command_id == GET_INQUIRY) { // Show local last inquiry
				printDevices(target->lastInquiry, false);
				continue;
			}
				
			continue;
        }
		/*********************************************/
        
        /******************** SET ********************/
        if (!strcasecmp(cmd_name, "set")) {
            cmd_param_name = strtok( NULL, delims );
            cmd_param_value = strtok( NULL, delims );
            
            if (cmd_param_name == NULL || cmd_param_value == NULL) {
                printf("\tSyntax error: set gumstix_id param_name param_value\n");
                continue;
            }
			
            printf("\tSet %s to value: %s\n", cmd_param_name, cmd_param_value);
            
            command_id = getCommandIdForParameter(cmd_param_name, true);
            if (command_id == ERROR) {
                printf("\t<param_name> is unknown or unsettable: %s\n", cmd_param_name);
                continue;
            }
            
			// Send Command to target
            printf("\tSending command to %s\n", target->id_gumstix);
            sendCommand(sConsole, &target->addr, command_id, cmd_param_value);
            
            // Receiving ACK
            gumstix_answer = receiveCommand(sConsole, &target->addr);
            if (gumstix_answer.id_command != PARAM_ACK ) {
                printf("\tError while setting parameter: %s\n", cmd_param_name);
            }else {
				printf("\tOK\n");
			}
			
            continue;
        }
		/*********************************************/
        
        printf("\tUnknown command: %s\n\tType 'help' for more information\n", cmd_name);
	}
}


int main(int argc, char **argv)
{
		
	printl("\n");
    printl("Starting program ...\n");
    
    pthread_mutex_init(&log_sem, NULL);
    
    pthread_t inqThread, servThread, imgThread, conThread;
    pthread_create(&inqThread, NULL, inquiryThread, NULL);
	pthread_create(&servThread, NULL, serviceThread, NULL);
    pthread_create(&imgThread, NULL, imagesThread, NULL);
    pthread_create(&conThread, NULL, consoleThread, NULL);
    
	pthread_join(inqThread, NULL);
	pthread_join(servThread, NULL);
    pthread_join(imgThread, NULL);
    pthread_join(conThread, NULL);


	return 0;
}
