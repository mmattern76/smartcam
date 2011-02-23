#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define true 1

int main(int argc, char **argv)
{
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int s, client, bytes_read, img_fd, img_len, byte_read, temp;
    socklen_t opt = sizeof(rem_addr);
    int result = 5;

    // allocate socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // bind socket to port 1 of the first available
    // local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 1;
    bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));

    // put socket into listening mode
    listen(s, 1);

    printf("Accepting connections ...\n");

    while(true){
    	// accept one connection
    	client = accept(s, (struct sockaddr *)&rem_addr, &opt);

    	ba2str( &rem_addr.rc_bdaddr, buf );
    	fprintf(stderr, "accepted connection from %s\n", buf);
    	memset(buf, 0, sizeof(buf));

    	img_fd = open("../../data/images/lena.jpeg", O_CREAT | O_WRONLY, S_IRWXU);

    	// read image lenght
    	read(client, &img_len, sizeof(img_len));
    	img_len = btohl(img_len);
    	printf("Received image size: %d\n", img_len);

    	printf("Receiving image ...\n");
    	byte_read = 0;
    	while(byte_read < img_len){
    		temp = read(client, buf, sizeof(char) * 1024);
    		write(img_fd, buf, temp);
    		byte_read += temp;
    	}
    	printf("Received image.\n");
    	close(img_fd);

    	// close connection
    	close(client);
    }
    close(s);
    return 0;
}
