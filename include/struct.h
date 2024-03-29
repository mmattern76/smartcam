#ifndef STRUCT_H
#define STRUCT_H
/*
 * Altri parametri che potrebbero essere aggiunti:
 * - scan_interval: per non ripetere le scansioni una di seguito all'altra in continuo
 * - auto_send: la gumstix invia automaticamente i risultati dell'ultima scansione (può creare problemi)
 */
#include <netinet/in.h>

#define true 1
#define false 0
#define MAX_RISP 100
#define ADDR_LEN 19
#define NAME_LEN 248
#define GUMSTIX_NAME_LEN 11

typedef struct{
	char id_gumstix[GUMSTIX_NAME_LEN];
	int alarm_threshold;
	int scan_length;
	int scan_interval;
	int auto_send_inquiry;
	int auto_send_images;
	int color_threshold;
	int image_width;
	int image_height;
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

typedef struct{
	char id_gumstix[GUMSTIX_NAME_LEN];
	struct sockaddr_in addr;
	struct timeval lastseen;
	Inquiry_data lastInquiry;
} Gumstix;

#endif
