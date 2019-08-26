#!/bin/bash

# Get number of cpus on the system
no_cpus=$(nproc)
echo "Number of cpus $no_cpus"

gcc *.c | xargs --max-args=1 --max-procs=$(no_cpus)
echo $?

