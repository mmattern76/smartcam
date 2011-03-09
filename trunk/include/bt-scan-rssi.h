#ifndef BTSCANRSSI_H
#define BTSCANRSSI_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#define true 1
#define false 0
#define MAX_RISP 255
#define ADDR_LEN 19
#define NAME_LEN 248
#define GUMSTIX_NAME_LEN 11

typedef struct{
	char id_gumstix[GUMSTIX_NAME_LEN];
	int alarm_threshold;
	int scan_lenght;
	char server_ip[16];
} Configuration;

typedef struct{
	char name[NAME_LEN];
	char bt_addr[ADDR_LEN];
	int valid;
	int8_t rssi;
} Device;

typedef struct{
	struct timeval timestamp;
	int num_devices;
	Device devices[MAX_RISP];
} Inquiry_data;

void * executeInquire(void * args);

#endif
