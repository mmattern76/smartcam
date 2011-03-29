/*
 * bt-scan-rssi.c
 *
 *  Created on: Feb 25, 2011
 *      Author: luca
 */
#include <pthread.h>
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
#include <bt-scan-rssi.h>
#include <commands.h>
#include <opencv/cv.h>
#include <opencv/cxcore.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>

Inquiry_data inq_data;
Configuration config;
extern pthread_mutex_t config_sem, images_sem;
extern int sd, force_send_inquiry, force_send_image;
extern struct sockaddr_in servaddr_service, servaddr_inquiry;

IplImage *imgBackground;
IplImage *imgDifference;
IplImage *imgForegroundSmooth;
IplImage *imgBinary;
IplImage *imgResult;
IplImage *imgCaptured;
CvCapture *capture;

Configuration copyConfiguration(){
	Configuration temp;

	pthread_mutex_lock(&config_sem);
	temp = config;
	pthread_mutex_unlock(&config_sem);

	return temp;
}

void setBackground(){
	printf("Shooting for background ...\n");
	// Get the background
	imgCaptured = cvQueryFrame(capture);

	printf("Got background\n");
	if(imgCaptured == NULL){
		perror("background == NULL");
		exit(1);
	}

	imgBackground = cvCloneImage(imgCaptured);

	// Save the frames into a file
	cvSaveImage("background.jpg", imgBackground, NULL);
}

void changeDetection(){
	int i,j;
	char imageName[255];
	Configuration temp_config;

	temp_config = copyConfiguration();

	printf("Change detection...\n");
	// Get the foreground
	imgCaptured = cvQueryFrame(capture);

	printf("Got foreground\n");
	if(imgCaptured == NULL){
		perror("foreground == NULL");
		exit(1);
	}

	imgDifference = cvCloneImage(imgCaptured);

	// Save the frames into a file
	cvSaveImage("foreground.jpg", imgDifference, NULL);

	imgResult = cvCreateImage(cvSize(imgBackground->width, imgBackground->height), imgBackground->depth, imgBackground->nChannels);
	imgForegroundSmooth = cvCreateImage(cvSize(imgDifference->width, imgDifference->height), imgDifference->depth, imgDifference->nChannels);
	imgBinary = cvCreateImage(cvSize(imgBackground->width, imgBackground->height), imgBackground->depth, 1);


	// Filtro passa basso: elimina le alte frequenze che contengono tipicamente il rumore
	// 3 indica la dimensione del filtro: più è grande e più l'immagine risulta sfocata
	cvSmooth(imgBackground, imgBackground, CV_GAUSSIAN, 3, 3, 0, 0);
	cvSmooth(imgDifference, imgForegroundSmooth, CV_GAUSSIAN, 3, 3, 0, 0);


	// Per ogni punto dell'immagine calcola la distanza euclidea del colore rispetto al background
	// In pratica si usa il teorema di pitagora in uno spazio a 3 dimensioni (RGB)
	// Alla fine ottengo l'immagine binaria con i soli punti cambiati
	for (i = 0; i < imgBackground->height; i++)
		for (j = 0; j < imgBackground->width; j++) {
			double color_dist = sqrt(
					pow ( CV_IMAGE_ELEM(imgBackground, unsigned char, i, j*3) -
							CV_IMAGE_ELEM(imgForegroundSmooth, unsigned char, i, j*3), 2)
							+
							pow ( CV_IMAGE_ELEM(imgBackground, unsigned char, i, j*3 +1) -
									CV_IMAGE_ELEM(imgForegroundSmooth, unsigned char, i, j*3 +1), 2)
									+
									pow ( CV_IMAGE_ELEM(imgBackground, unsigned char, i, j*3 +2) -
											CV_IMAGE_ELEM(imgForegroundSmooth, unsigned char, i, j*3 +2), 2)

			);

			CV_IMAGE_ELEM(imgBinary, unsigned char, i, j) = (color_dist > temp_config.color_threshold) ? 255 : 0;
		}


	// A questo punto faccio qualche elaborazione sull'immagine binaria

	IplConvKernel* struct_element;

	// Questa operazione toglie tutti i gruppi di pixel dello sfondo che sono più piccoli di un cerchio di diametro 5 pixel
	// Solitamente sono dovuti al rumore, visto che una persona è molto più grande
	struct_element = cvCreateStructuringElementEx(5, 5, 2, 2, CV_SHAPE_ELLIPSE, NULL);
	cvMorphologyEx(imgBinary, imgBinary, NULL, struct_element, CV_MOP_OPEN, 1);

	// Questa operazione è il duale e toglie invece tutti i "buchi" dall'oggetto
	struct_element = cvCreateStructuringElementEx(21, 21, 10, 10, CV_SHAPE_ELLIPSE, NULL);
	cvMorphologyEx(imgBinary, imgBinary, NULL, struct_element, CV_MOP_CLOSE, 1);

	cvAddS(imgDifference, cvScalarAll(0), imgResult, imgBinary);

	CvMemStorage *mem;
	CvSeq	 *contours, *ptr;

	// Cerco i contorni esterni nell'immagine binaria e li disegno sull'immagine originale con dei colori casuali
	// Ogni oggetto distinto ha un colore diverso
	mem = cvCreateMemStorage(0);
	cvFindContours(imgBinary, mem, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));

	for (ptr = contours; ptr != NULL; ptr = ptr->h_next) {
		CvScalar color = CV_RGB( rand()&255, rand()&255, rand()&255 );
		cvDrawContours(imgDifference, ptr, color, CV_RGB(0,0,0), -1, 2, 8, cvPoint(0,0));
	}

	sprintf(imageName, "../data/images/%s.jpeg", temp_config.id_gumstix);
	pthread_mutex_lock(&images_sem);
	cvSaveImage(imageName, imgDifference, NULL);
	pthread_mutex_unlock(&images_sem);

	cvReleaseImage(&imgBinary);
	cvReleaseImage(&imgResult);
	cvReleaseImage(&imgForegroundSmooth);
}

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

	printf("Gumstix: %s, Timestamp: %s, Devices found: %d\n", config.id_gumstix, tmbuf, inq_data.num_devices);
	for(i = 0; i < inq_data.num_devices; i++){
		printf("\t%d %s\t%s\t%d\n", i+1, inq_data.devices[i].bt_addr, inq_data.devices[i].name,
				inq_data.devices[i].valid ? inq_data.devices[i].rssi : -999);
	}
}

void* executeInquire(void * args){
	// This thread executes cyclically a scanning of nearby
	// bluetooth devices and saves it into inq_data

	inquiry_info *ii;
	Inquiry_data temp;
	char numDev[5], imageName[255];
	int i, dev_id, sock, flags, num_rsp;
	char name[NAME_LEN] = { 0 };
	uint16_t handle;
	unsigned int ptype;
	struct hci_conn_info_req *cr;
	int8_t rssi;

	// Camera initialization
	capture = cvCaptureFromCAM(0);

	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, 640);
	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, 480);

	cvWaitKey(5000);

	setBackground();

	// BT initialization

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev( dev_id );
	if (dev_id < 0 || sock < 0) {
		perror("opening socket");
		exit(1);
	}

	flags = IREQ_CACHE_FLUSH;

	ii = (inquiry_info*)malloc(MAX_RISP * sizeof(inquiry_info));

	cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
	if (!cr || !ii) {
		perror("Can't allocate memory");
		exit(1);
	}

	while(true){
		Configuration temp_config = copyConfiguration();

		gettimeofday(&temp.timestamp, NULL);

		memset(ii, 0, MAX_RISP * sizeof(inquiry_info));

		printf("Scanning ...\n");

		num_rsp = hci_inquiry(dev_id, temp_config.scan_length, MAX_RISP, NULL, &ii, flags);
		if( num_rsp < 0 ) perror("hci_inquiry");

		for (i = 0; i < num_rsp && i < MAX_RISP; i++) {
			//ba2str(&(ii+i)->bdaddr, dev_addr[i]);
			ba2str(&(ii+i)->bdaddr, temp.devices[i].bt_addr);

			memset(name, 0, sizeof(name));
			if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name),
					name, 0) < 0)
				strcpy(name, "[unknown]");
			strcpy(temp.devices[i].name, name);

			ptype = HCI_DM1 | HCI_DM3 | HCI_DM5 | HCI_DH1 | HCI_DH3 | HCI_DH5;

			if (hci_create_connection(sock, &(ii+i)->bdaddr, htobs(ptype),
					(ii+i)->clock_offset, 0x01, &handle, 0) < 0){
				//perror("Can't create connection");
				temp.devices[i].valid = false;
				continue;
			}
			memset(cr, 0, sizeof(*cr) + sizeof(struct hci_conn_info));
			bacpy(&cr->bdaddr, &(ii+i)->bdaddr);
			cr->type = ACL_LINK;
			if (ioctl(sock, HCIGETCONNINFO, (unsigned long) cr) < 0) {
				perror("Get connection info failed");
				temp.devices[i].valid = false;
				continue;
			}

			if (hci_read_rssi(sock, htobs(cr->conn_info->handle), &rssi, 1000) < 0) {
				perror("Read RSSI failed");
				temp.devices[i].valid = false;
				continue;
			}

			//printf("\t%d %s\t%s\t%d\n", i+1, addr, name, rssi);
			temp.devices[i].valid = true;
			temp.devices[i].rssi = rssi;
		}

		temp.num_devices = i;

		// Sort devices with qsort algorithm by DESC RSSI value (see compareDevices)
		qsort(temp.devices, temp.num_devices, sizeof(Device), compareDevices);
		printDevices(temp);

		// Save inq_data
		inq_data = temp;

		// If nobody is near the camera, update background
		if(inq_data.num_devices == 0){
			//changeDetection();
			// Set new preferred resolution
			cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, temp_config.image_width);
			cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, temp_config.image_height);
			printf("Setting new background image (%dx%d) ...\n", (int)cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH),
																 (int)cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT));
			setBackground();
		}

		// Send inquiry data if there is some bt device and auto_send_inquiry or force_send_inquiry is true
		if((temp_config.auto_send_inquiry || force_send_inquiry) && inq_data.num_devices > 0){
			printf("Sending inquiry data to server ...\n");
			sendInquiryData(sd, &servaddr_inquiry, inq_data);
			force_send_inquiry = 0;
		}

		// Send image if auto_send_images and alarm is triggered, or if force_send_image is true
		if((temp_config.auto_send_images && inq_data.num_devices >= temp_config.alarm_threshold) || force_send_image){
			changeDetection();
			sprintf(imageName, "../data/images/%s.jpeg", temp_config.id_gumstix);
			sendImage(imageName);
			force_send_image = 0;
		}

		// Send alarm
		if(inq_data.num_devices >= temp_config.alarm_threshold){
			printf("Sending alarm to server ...\n");
			sprintf(numDev, "%d", inq_data.num_devices);
			sendCommand(sd, &servaddr_service, ALARM, numDev);
		}

		sleep(temp_config.scan_interval * 1000);
	}

	free(ii);
	free(cr);
	close( sock );
	cvReleaseCapture(&capture);
	return 0;

}
