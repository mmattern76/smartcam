
#FIND_PACKAGE(OpenCV REQUIRED)
ADD_EXECUTABLE (cv-test cv-test.c)

#TARGET_LINK_LIBRARIES (cv-test ${OpenCV_LIBS} ) 
TARGET_LINK_LIBRARIES (cv-test 	opencv_calib3d
							opencv_contrib
							opencv_core
							opencv_features2d
							opencv_highgui
							opencv_imgproc
							opencv_legacy
							opencv_ml
							opencv_objdetect
							opencv_video )




