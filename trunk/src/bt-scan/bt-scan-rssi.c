/*
 * bt-scan-rssi.c
 *
 *  Created on: Feb 25, 2011
 *      Author: luca
 */
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
	char name[NAME_LEN];
	char bt_addr[ADDR_LEN];
	int valid;
	int8_t rssi;
} Device;

typedef struct{
	struct timeval timestamp;
	char id_gumstix[GUMSTIX_NAME_LEN];
	int num_devices;
	Device devices[MAX_RISP];
} Inquiry_data;

int compareDevices(const void* a, const void* b){
	Device *d1 = (Device*) a;
	Device *d2 = (Device*) b;

	if(!d1->valid)
		return 1;
	if(!d2->valid)
		return -1;
	if(d1->rssi > d2->rssi)
		return -1;
	if(d1->rssi == d2->rssi)
		return 0;
	else
		return 1;
}

void printDevices(Inquiry_data inq_data){
	int i;
	time_t nowtime;
	struct tm *nowtm;
	char tmbuf[64];

	nowtime = inq_data.timestamp.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof(tmbuf), "%d-%m-%Y %H:%M:%S", nowtm);

	printf("Gumstix: %s, Timestamp: %s, Devices found: %d\n", inq_data.id_gumstix, tmbuf, inq_data.num_devices);
	for(i = 0; i < inq_data.num_devices; i++){
		printf("\t%d %s\t%s\t%d\n", i+1, inq_data.devices[i].bt_addr, inq_data.devices[i].name,
				inq_data.devices[i].valid ? inq_data.devices[i].rssi : -999);
	}
}

int main(int argc, char** argv){

	Inquiry_data inq_data;

	inquiry_info *ii;
	int i, dev_id, sock, len, flags, num_rsp;
	char name[NAME_LEN] = { 0 };
	uint16_t handle;
	unsigned int ptype;
	struct hci_conn_info_req *cr;
	int8_t rssi;

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev( dev_id );
	if (dev_id < 0 || sock < 0) {
		perror("opening socket");
		return -1;
	}

	len  = 8;
	flags = IREQ_CACHE_FLUSH;

	ii = (inquiry_info*)malloc(MAX_RISP * sizeof(inquiry_info));

	cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
	if (!cr || !ii) {
		perror("Can't allocate memory");
		exit(1);
	}

	strcpy(inq_data.id_gumstix, "Gumstix");

	while(true){

		gettimeofday(&inq_data.timestamp, NULL);

		memset(ii, 0, MAX_RISP * sizeof(inquiry_info));

		printf("Scanning ...\n");

		num_rsp = hci_inquiry(dev_id, len, MAX_RISP, NULL, &ii, flags);
		if( num_rsp < 0 ) perror("hci_inquiry");

		for (i = 0; i < num_rsp; i++) {
			//ba2str(&(ii+i)->bdaddr, dev_addr[i]);
			ba2str(&(ii+i)->bdaddr, inq_data.devices[i].bt_addr);

			memset(name, 0, sizeof(name));
			if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name),
					name, 0) < 0)
				strcpy(name, "[unknown]");
			strcpy(inq_data.devices[i].name, name);

			ptype = HCI_DM1 | HCI_DM3 | HCI_DM5 | HCI_DH1 | HCI_DH3 | HCI_DH5;

			if (hci_create_connection(sock, &(ii+i)->bdaddr, htobs(ptype),
					(ii+i)->clock_offset, 0x01, &handle, 0) < 0){
				perror("Can't create connection");
				inq_data.devices[i].valid = false;
				continue;
			}
			memset(cr, 0, sizeof(*cr) + sizeof(struct hci_conn_info));
			bacpy(&cr->bdaddr, &(ii+i)->bdaddr);
			cr->type = ACL_LINK;
			if (ioctl(sock, HCIGETCONNINFO, (unsigned long) cr) < 0) {
				perror("Get connection info failed");
				inq_data.devices[i].valid = false;
				continue;
			}

			if (hci_read_rssi(sock, htobs(cr->conn_info->handle), &rssi, 1000) < 0) {
				perror("Read RSSI failed");
				inq_data.devices[i].valid = false;
				continue;
			}

			//printf("\t%d %s\t%s\t%d\n", i+1, addr, name, rssi);
			inq_data.devices[i].valid = true;
			inq_data.devices[i].rssi = rssi;
		}

		inq_data.num_devices = i;

		// Sort devices with qsort algorithm by DESC RSSI value (see compareDevices)
		qsort(inq_data.devices, inq_data.num_devices, sizeof(Device), compareDevices);
		printDevices(inq_data);
	}

	free(ii);
	free(cr);
	close( sock );
	return 0;

}
