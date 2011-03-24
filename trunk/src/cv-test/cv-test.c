#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <opencv/cv.h>
#include <opencv/cxcore.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>

IplImage *imgBackground;
IplImage *imgGrayBackground;
IplImage *imgForeground;
IplImage *imgGrayForeground;
IplImage *imgAbsDiff;
IplImage *result;
CvCapture *pCapturedImage;

int main(int argc, char *argv[]) {
//	int i, j, k;

	pCapturedImage = cvCaptureFromCAM(0);

	cvSetCaptureProperty(pCapturedImage, CV_CAP_PROP_FRAME_WIDTH, 640);
	cvSetCaptureProperty(pCapturedImage, CV_CAP_PROP_FRAME_HEIGHT, 480);

	// Get the background
	imgBackground = cvQueryFrame(pCapturedImage);
	imgGrayBackground = cvCreateImage(cvSize(imgBackground->width, imgBackground->height), imgBackground->depth, 1);
	// Convert to gray scale
	cvCvtColor(imgBackground, imgGrayBackground, CV_BGR2GRAY);
	printf("got background\n");
	if(imgBackground == NULL){
		perror("pSaveImg == NULL");
		exit(1);
	}

	// Save the frames into a file
	cvSaveImage("background.jpg", imgBackground, NULL);
	cvSaveImage("grayBackground.jpg", imgGrayBackground, NULL);

	cvWaitKey(5000);

	// Get the foreground
	imgForeground = cvQueryFrame(pCapturedImage);
	imgGrayForeground = cvCreateImage(cvSize(imgForeground->width, imgForeground->height), imgForeground->depth, 1);
	// Convert to gray scale
	cvCvtColor(imgForeground, imgGrayForeground, CV_BGR2GRAY);
	printf("got foreground\n");
	if(imgForeground == NULL){
		perror("pSaveImg2 == NULL");
		exit(1);
	}

	// Save the frames into a file
	cvSaveImage("foreground.jpg", imgForeground, NULL);
	cvSaveImage("grayForeground.jpg", imgGrayForeground, NULL);

	imgAbsDiff = cvCreateImage(cvSize(imgBackground->width, imgBackground->height), imgBackground->depth, 1);

	//	for (i = 0; i < imgBackground->height; i++)
	//			for (j = 0; j < imgBackground->width; j++)
	//				for (k = 0; k < imgBackground->nChannels; k++){
	//					result->imageData[i * imgBackground->widthStep + j * imgBackground->nChannels + k] = imgForeground->imageData[i * imgBackground->widthStep + j * imgBackground->nChannels + k] - imgBackground->imageData[i * imgBackground->widthStep + j * imgBackground->nChannels + k];
	//				}

	// Absolute subtraction between gray images (doesn't work with color images)
	cvAbsDiff(imgGrayBackground, imgGrayForeground, imgAbsDiff);
	// Apply Threshold
	cvThreshold(imgAbsDiff, imgAbsDiff, 40, 0, CV_THRESH_TOZERO);

	// Save the absdiff image into a file
	cvSaveImage("absdiff.jpg", imgAbsDiff, NULL);

	// Convert absdiff image into a binary image and save it
	cvThreshold(imgAbsDiff, imgAbsDiff, 70, 255, CV_THRESH_BINARY);
	cvSaveImage("binary.jpg", imgAbsDiff, NULL);

	result = cvCreateImage(cvSize(imgBackground->width, imgBackground->height), imgBackground->depth, imgBackground->nChannels);
	// Apply a mask to color imgAbsDiff
	cvAddS(imgForeground, cvScalarAll(0), result, imgAbsDiff);
	cvSaveImage("result.jpg", result, NULL);

	cvReleaseCapture(&pCapturedImage);

	return 0;
}
