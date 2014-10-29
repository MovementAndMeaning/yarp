#!/bin/bash

LIB_PATH=${1:-/opt/M+M}
for ii in bin/*
do
	install_name_tool -change libACE.dylib "$LIB_PATH/lib/libACE.dylib" "$ii"
done
