#!/bin/bash
# Author - Pavan Kumar Paluri
# RTLAB @ UNIVERSITY OF HOUSTON - 2019
# To verify if go-version is latest or not
# If not the latest version, this script updates the version automatically


if [ $(whoami) != "root" ]; then
	echo "Only root user can make updates to the kernel!"
	exit 1
else
  # VERSION_GO=go version
    go version > $(pwd)/version_go.txt
    cat $(pwd)/version_go.txt | awk '{print $3}'
   # echo "Version GO:$(VERSION)"
    GOROOT=/usr/local/go
    export PATH=$XEN_GOPATH/bin:$GOROOT/bin:$PATH
    if [ $? -eq 0 ]; then
	echo "Go version updated successfully!!!"
	echo "Version Now: $(go version)"
   else 
	echo "Update-failed!"
   fi
fi

