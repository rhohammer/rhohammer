#!/bin/bash

source ./start_rhohammer.sh

CMD=$(generate_rhoHammer_cmd 1 7200)

sudo ./hugepage_allocate.sh

cd ../rhohammer/build
echo $CMD
eval $CMD
