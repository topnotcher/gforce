#!/bin/sh

if [[ $1 == 'saml21' ]] ; then 
	rm -rf CMakeCache.txt CMakeFiles
	cmake -DCMAKE_TOOLCHAIN_FILE=../build/generic-gcc-saml21.cmake -DPLATFORM=saml21

elif [[ $1 == 'xmega' ]] ; then 
	rm -rf CMakeCache.txt CMakeFiles
	cmake -DCMAKE_TOOLCHAIN_FILE=../build/generic-gcc-avr.cmake -DPLATFORM=xmega

else 
	echo "Usage: $0 saml21|xmega"
fi
