#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <commands.h>

#define true 1

pthread_mutex_t inquiry_sem;
int sService;

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

	printf("Gumstix: %s, Timestamp: %s, Devices found: %d\n", "PROVA", tmbuf, inq_data.num_devices);
	for(i = 0; i < inq_data.num_devices; i++){
		printf("\t%d %s\t%s\t%d\n", i+1, inq_data.devices[i].bt_addr, inq_data.devices[i].name,
				inq_data.devices[i].valid ? inq_data.devices[i].rssi : -999);
	}
}

int main(int argc, char **argv)
{
	int sConsole;
	Command command;
	Inquiry_data inq_data;
	struct sockaddr_in gumstixaddr;
	char ipaddr[20], port[20];

	sConsole = bindSocketUDP(63170, 0);
	sService = bindSocketUDP(63171, 0);

	printf("Waiting Hello\n");
	command = receiveCommand(sService, &gumstixaddr);
	if(command.id_command == HELLO){
		getnameinfo((struct sockaddr*)&gumstixaddr, sizeof(struct sockaddr_in), ipaddr, sizeof(ipaddr), port, sizeof(port), 0);
		printf("Received Hello from %s (%s:%s)\n", command.param, ipaddr, port);
	}

	while(true){
		printf("Waiting Inquiry_data\n");
		inq_data = receiveInquiryData(sService, NULL);
		printDevices(inq_data);
	}

	return 0;
}
