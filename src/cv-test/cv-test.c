#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <opencv/cv.h>
#include <opencv/cxcore.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>

IplImage *imgBackground;
IplImage *imgForeground;
IplImage *result;
CvCapture *pCapturedImage;

int main(int argc, char *argv[]) {
	int i, j, k;
	pCapturedImage = cvCaptureFromCAM(-1);

	cvSetCaptureProperty(pCapturedImage, CV_CAP_PROP_FRAME_WIDTH, 640);
	cvSetCaptureProperty(pCapturedImage, CV_CAP_PROP_FRAME_HEIGHT, 480);

	// Get the frame
	imgBackground = cvQueryFrame(pCapturedImage);
	printf("getframe\n");
	if(imgBackground == NULL){
		perror("pSaveImg == NULL");
		exit(1);
	}
	// Save the frame into a file
	cvSaveImage("background.jpg", imgBackground, NULL);

	cvWaitKey(2000);

	// Get the frame
	imgForeground = cvQueryFrame(pCapturedImage);
	printf("getframe\n");
	if(imgForeground == NULL){
		perror("pSaveImg2 == NULL");
		exit(1);
	}

	result = cvCreateImage(cvSize(imgBackground->width, imgBackground->height), imgBackground->depth, imgBackground->nChannels);

//	for (i = 0; i < imgBackground->height; i++)
//			for (j = 0; j < imgBackground->width; j++)
//				for (k = 0; k < imgBackground->nChannels; k++){
//					result->imageData[i * imgBackground->widthStep + j * imgBackground->nChannels + k] = imgForeground->imageData[i * imgBackground->widthStep + j * imgBackground->nChannels + k] - imgBackground->imageData[i * imgBackground->widthStep + j * imgBackground->nChannels + k];
//				}

	cvAbsDiff(imgBackground, imgForeground, result);
	//cvThreshold(result, result, 10, 0, CV_THRESH_TOZERO);

	// Save the frame into a file
	cvSaveImage("foreground.jpg", imgForeground, NULL);

	// Save the frame into a file
	cvSaveImage("result.jpg", result, NULL);

	//cvReleaseImage(&pSaveImg);
	cvReleaseCapture(&pCapturedImage);

	return 0;

//	if (argc < 2) {
//		printf("Usage: main <image-file-name>\n\7");
//		exit(0);
//	}
//
//	// load an image
//	img = cvLoadImage(argv[1], CV_LOAD_IMAGE_COLOR);
//	if (!img) {
//		printf("Could not load image file: %s\n", argv[1]);
//		exit(0);
//	}
//
//	// get the image data
//	height = img->height;
//	width = img->width;
//	step = img->widthStep;
//	channels = img->nChannels;
//	data = (uchar *) img->imageData;
//	printf("Processing a %dx%d image with %d channels\n", height, width,
//			channels);
//
//	// invert the image
//	for (i = 0; i < height; i++)
//		for (j = 0; j < width; j++)
//			for (k = 0; k < channels; k++)
//				data[i * step + j * channels + k] = 255 - data[i * step + j
//						* channels + k];
//
//	char newName[100];
//	sprintf(newName, "%s-inv.jpeg", argv[1]);
//
//	printf("Saving new image\n");
//
//	cvSaveImage(newName, img, NULL); // check if it works
//
//	// wait for a key
//	//cvWaitKey(0);
//
//	// release the image
//	cvReleaseImage(&img);
//	return 0;
}
