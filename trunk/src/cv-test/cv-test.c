#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <opencv/cv.h>
#include <opencv/cxcore.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>

#define COLOR_THRESHOLD 40

IplImage *imgBackground;
IplImage *imgDifference;
IplImage *imgForegroundSmooth;
IplImage *imgBinary;
IplImage *imgResult;
IplImage *imgCaptured;
CvCapture *capture;

int main(int argc, char *argv[]) {
    int i,j;


    capture = cvCaptureFromCAM(0);

	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, 640);
	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, 480);

	cvWaitKey(5000);

	// Get the background
	imgCaptured = cvQueryFrame(capture);

	printf("got background\n");
	if(imgCaptured == NULL){
		perror("background == NULL");
		exit(1);
	}

    imgBackground = cvCloneImage(imgCaptured);

	// Save the frames into a file
	cvSaveImage("background.jpg", imgBackground, NULL);

	cvWaitKey(5000);

	// Get the foreground
	imgCaptured = cvQueryFrame(capture);

	printf("got foreground\n");
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

            CV_IMAGE_ELEM(imgBinary, unsigned char, i, j) = (color_dist > COLOR_THRESHOLD) ? 255 : 0;
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

    cvSaveImage("result.jpg", imgDifference, NULL);

    cvWaitKey(10);

	cvReleaseCapture(&capture);
    cvReleaseImage(&imgBinary);
    cvReleaseImage(&imgResult);

    return 0;
}
