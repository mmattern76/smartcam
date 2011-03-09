ADD_EXECUTABLE (gumstix bt-scan-rssi.c gumstix.c commands.c)
TARGET_LINK_LIBRARIES (gumstix bluetooth pthread) 
