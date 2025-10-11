#!/bin/bash

cd ../rhohammer/build

sudo ./rhoHammer --dimm-id 1 --runtime-limit 21600 --geometry 2,4,4 --samsung -j fuzz-summary.json --sweeping