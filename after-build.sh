#!/bin/bash

LIB_PATH=${1:-/usr/local}
for ii in bin/*
do
	install_name_tool -change libACE.dylib "$LIB_PATH/lib/libACE.dylib" "$ii"
done
