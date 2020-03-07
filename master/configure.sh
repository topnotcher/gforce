#!/bin/sh


if [[ $1 == 'samd51' ]] ; then 
	rm -rf CMakeCache.txt CMakeFiles
	cmake -DCMAKE_TOOLCHAIN_FILE=../build/arm-none-eabi-toolchain.cmake -DPLATFORM=samd51

else 
	echo "Usage: $0 samd51"
fi
