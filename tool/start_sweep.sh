#!/bin/bash

source ./start_rhohammer.sh
CMD=$(generate_rhoHammer_cmd 1 21600)
CMD="$CMD -j fuzz-summary.json"
echo $CMD

sudo ./hugepage_allocate.sh

cd ../rhohammer/build

eval $CMD
