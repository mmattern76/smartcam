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
#include <ctype.h>


#define true 1
#define false 0
#define MAX_RISP 200
#define ADDR_LEN 19
#define NAME_LEN 248
#define GUMSTIX_NAME_LEN 11


/**
 * La struttura di configurazione la metterei da qualche altra parte
 * visto che i parametri sono usati anche per fare altre cose.
 *
 * Altri parametri che potrebbero essere aggiunti:
 * - scan_interval: per non ripetere le scansioni una di seguito all'altra in continuo
 * - auto_send: la gumstix invia automaticamente i risultati dell'ultima scansione (può creare problemi)
 */

typedef struct{
	char id_gumstix[GUMSTIX_NAME_LEN];
	int alarm_threshold;
	int scan_lenght;
	int auto_send;
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
