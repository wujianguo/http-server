#!/bin/sh
cd `dirname $0`
make
if [ $? = 0 ];then
	./webserver
fi
