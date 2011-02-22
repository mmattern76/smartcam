cmake_minimum_required (VERSION 2.6)
PROJECT (Smartcam) 

# setup toolchain for cross compiling
SET ( CMAKE_SYSTEM_NAME Linux )
				
SET ( OETMP OVEROTOP_MARK/tmp )

#SET ( OETMP $ENV{OVEROTOP}/tmp )


SET ( TOOLDIR ${OETMP}/sysroots/i686-linux/usr/armv7a/bin )
SET ( STAGEDIR ${OETMP}/sysroots/armv7a-angstrom-linux-gnueabi/usr )

SET ( CMAKE_C_COMPILER ${TOOLDIR}/arm-angstrom-linux-gnueabi-gcc )
SET ( CMAKE_CXX_COMPILER ${TOOLDIR}/arm-angstrom-linux-gnueabi-g++ )
SET ( CMAKE_C_FLAGS -"Wall" )
SET ( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
SET ( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )


LINK_DIRECTORIES ( ${STAGEDIR}/lib )
LINK_DIRECTORIES ( ${PROJECT_SOURCE_DIR}/../lib )

INCLUDE_DIRECTORIES ( ${STAGEDIR}/include )
INCLUDE_DIRECTORIES ( ${PROJECT_SOURCE_DIR}/../include )

SET ( EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../bin )
SET ( LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../lib )
SET ( DOC_OUTPUT_PATH ${CMAKE_BINARY_DIR}/../doc )

#ADD_SUBDIRECTORY (src)

OPTION( BUILD_HELLO_WORLD "Build hello world program" ON )
OPTION( BUILD_HELLO_LIB "Build hello world library" ON )
OPTION( BUILD_BT_TEST "Build bluetooth test program" ON )


IF(BUILD_HELLO_WORLD)
	ADD_SUBDIRECTORY ( helloworld)
	# EXECUTE_PROCESS ( COMMAND ls -l WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin )
ENDIF()

IF(BUILD_HELLO_LIB)
	ADD_SUBDIRECTORY (libHello)
ENDIF()

IF(BUILD_BT_TEST)
	ADD_SUBDIRECTORY (bt-test)
	ADD_SUBDIRECTORY (rfcomm-client)
ENDIF()

