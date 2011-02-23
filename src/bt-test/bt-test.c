#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_RISP 255
#define ADDR_LEN 19

int getDevices(char dev_addr[MAX_RISP][19], int max_rsp){

	inquiry_info *ii;
	int i, dev_id, sock, len, flags, num_rsp;
	char addr[ADDR_LEN] = { 0 };
	char name[248] = { 0 };

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev( dev_id );
	if (dev_id < 0 || sock < 0) {
		perror("opening socket");
		return -1;
	}

	len  = 8;
	max_rsp = 255;
	flags = IREQ_CACHE_FLUSH;
	ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));

	printf("Scanning ...\n");

	num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
	if( num_rsp < 0 ) perror("hci_inquiry");

	printf("Select a device: \n");
	for (i = 0; i < num_rsp; i++) {
		ba2str(&(ii+i)->bdaddr, dev_addr[i]);
		memset(name, 0, sizeof(name));
		if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(addr),
				name, 0) < 0)
			strcpy(name, "[unknown]");
		printf("\t%d %s\t%s\n", i+1, dev_addr[i], name);
	}

	free(ii);
	close( sock );
	return num_rsp;
}

int main(int argc, char **argv)
{
	struct sockaddr_rc addr = { 0 };
    int s, status, num_rsp, dev_num, returnInt, img_fd, img_len, i, temp;
    char dev[4], buf[1024];
    char dev_addr[MAX_RISP][ADDR_LEN];

    num_rsp = getDevices(dev_addr, MAX_RISP);

    if(num_rsp < 0){
    	exit(1);
    }

    printf("Choice a number between 1 and %d: ", num_rsp);
    gets(dev);
    dev_num = atoi(dev) - 1;

    // set the connection parameters (who to connect to)
    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = (uint8_t) 1;

    printf("Selected %d\n", dev_num + 1);
    printf("Selected address: %s\n", dev_addr[dev_num]);
    str2ba(dev_addr[dev_num], &addr.rc_bdaddr);

    // allocate a socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // connect to server
    status = connect(s, (struct sockaddr *)&addr, sizeof(addr));

    img_fd = open("../data/images/lena.bmp-inv.jpeg", O_RDONLY);
    img_len = 0;

    while((temp = read(img_fd, buf, sizeof(char) * 1024)) > 0){
    	img_len += temp;
    }
    printf("Sending image size: %i\n", img_len);
    write(s, &htobl(img_len), sizeof(img_len));
    close(img_fd);

    printf("Sending image ...\n");
    img_fd = open("../data/images/lena.bmp-inv.jpeg", O_RDONLY);
    while((img_len = read(img_fd, buf, sizeof(char) * 1024)) > 0){
    	write(s, buf, sizeof(char) * img_len);
    }

    close(img_fd);
    close(s);
    return 0;
}

