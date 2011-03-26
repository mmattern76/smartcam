ADD_EXECUTABLE (gumstix bt-scan-rssi.c gumstix.c commands.c)
TARGET_LINK_LIBRARIES (gumstix bluetooth pthread opencv_calib3d
							opencv_contrib
							opencv_core
							opencv_features2d
							opencv_highgui
							opencv_imgproc
							opencv_legacy
							opencv_ml
							opencv_objdetect
							opencv_video ) 
