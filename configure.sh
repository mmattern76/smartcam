#!/bin/bash


if [ -f config/env.config ]
then
	echo "Configuration file exists"
	echo "Config Overo ..."

	source config/env.config
	
	echo "$OVERO_TOP"
	
	echo "# *********************************************" > src/CMakeLists.txt 
	echo "#" >> src/CMakeLists.txt 
	echo "# WARNING: This file is dynamically generated. Don't edit!" >> src/CMakeLists.txt 
	echo "#" >> src/CMakeLists.txt 
	echo "# *********************************************" >> src/CMakeLists.txt 
	cat config/CMakeLists.txt >> src/CMakeLists.txt 

	sed -i "s:OVERO_TOOL_MARK:$OVERO_TOOL:g" src/CMakeLists.txt 
	sed -i "s:OVERO_STAGE_MARK:$OVERO_STAGE:g" src/CMakeLists.txt 
	sed -i "s:OVERO_IP_MARKER:$OVERO_IP:g" src/CMakeLists.txt

	echo "Cleaning previous build files"
	rm -rf bin
	rm -rf lib
	rm -rf build

	echo "Creating eclipse project for Overo ..."
	mkdir bin
	mkdir lib
	mkdir build

	cd build
	cmake -G "Eclipse CDT4 - Unix Makefiles"  ../src/



	echo "Config PC Client ..."

	cd ../pc-server

	echo "Cleaning previous build files ..."
	rm -rf bin
	rm -rf lib
	rm -rf build

	echo "Creating eclipse project for PC Client ..."
	mkdir bin
	mkdir lib
	mkdir build

	cd build
	cmake -G "Eclipse CDT4 - Unix Makefiles" -D CMAKE_BUILD_TYPE=Debug ../src/
else
	echo "Creating new configuration files ..."
	
	echo "#!/bin/bash" > config/env.config
	
	echo "" >> config/env.config
	echo "# folder with cross compiler" >> config/env.config
	echo "export OVERO_TOOL=$OVEROTOP/tmp/sysroots/i686-linux/usr/armv7a/bin" >> config/env.config
	
	echo "" >> config/env.config
	echo "# folder with library and include" >> config/env.config
	echo "export OVERO_STAGE=$OVEROTOP/tmp/sysroots/armv7a-angstrom-linux-gnueabi/usr" >> config/env.config
	
	echo "" >> config/env.config
	echo "# ip of overo gumstix device" >> config/env.config
	echo "export OVERO_IP=192.168.0.1" >> config/env.config
	
	chmod u+x config/env.config
	
	tail -n +2 config/env.config
	
	echo ""
	echo -e "\033[1mWARNING:\033[0m"
	echo "Edit ./config/env.config with your custom parameters and rerun ./configure.sh"
fi


