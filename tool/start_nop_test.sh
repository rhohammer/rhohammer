#!/bin/bash

source ./start_rhohammer.sh

CMD=$(generate_rhoHammer_cmd 1 21600)
echo $CMD

# Script function: Loop to insert different numbers of nop instructions and execute tests

if [ $# -ge 3 ]; then
    NOP_START=$1
    NOP_END=$2
    NOP_STEP=$3
elif [ $# -ge 2 ]; then
    NOP_START=$1
    NOP_END=$2
    NOP_STEP=10
elif [ $# -ge 1 ]; then
    NOP_START=$1
    NOP_END=1000
    NOP_STEP=10
else
    NOP_START=10
    NOP_END=1000
    NOP_STEP=10
fi

# Switch to the correct directory
cd ../rhohammer/src/Fuzzer

# nop instruction template
NOP_INSTRUCTION='nop;'

# Backup original file
# if [ ! -f "CodeJitter.cpp.backup" ]; then
#     if [ -f "CodeJitter.cpp" ]; then
#         cp CodeJitter.cpp CodeJitter.cpp.backup
#         echo "Created original file backup: CodeJitter.cpp.backup"
#     else
#         echo "Error: CodeJitter.cpp file does not exist"
#         exit 1
#     fi
# fi

# Loop to insert different numbers of nop instructions
for ((i=NOP_START; i<=NOP_END; i+=NOP_STEP)); do
    echo "==============================================="
    echo "Processing: Inserting $i nop instructions"
    echo "==============================================="
    
    # # Restore original file
    # if [ -f "CodeJitter.cpp.backup" ]; then
    #     cp CodeJitter.cpp.backup CodeJitter.cpp
    # else
    #     echo "Error: Backup file CodeJitter.cpp.backup does not exist"
    #     exit 1
    # fi
    
    # Generate specified number of nop instruction string
    NOP_STRING=""
    for ((j=0; j<i; j++)); do
        NOP_STRING+="${NOP_INSTRUCTION}"
    done
    
    # Construct complete asm volatile statement
    ASM_STATEMENT="    asm volatile (\"${NOP_STRING}\");"
    
    # Insert nop instructions between the two NOP tags
    # Use sed to insert content between markers
    sed -i "/\/\/ BEGIN NOP INSERT/,/\/\/ END NOP INSERT/{
        /\/\/ BEGIN NOP INSERT/!{/\/\/ END NOP INSERT/!d;}
        /\/\/ BEGIN NOP INSERT/a\\
${ASM_STATEMENT}
    }" CodeJitter.cpp
    
    echo "Inserted $i nop instructions"
    
    # Compile project
    cd ../../build
    echo "Starting compilation..."
    make -j$(nproc)
    rm -rf stdout.log
    
    if [ $? -eq 0 ]; then
        echo "Compilation successful, starting test execution..."
        # Execute test and redirect output to NOP_COUNT_$i.log
        eval $CMD
        # Save results to log file
        mv stdout.log ../../output/nop_test/stdout_NOP_COUNT_$i.log
        echo "Test completed, saving results..."
        echo "NOP_COUNT_$i: Test completed at $(date)" >> nop_test_results.log
    else
        echo "Compilation failed, skipping this test"
        echo "NOP_COUNT_$i: Compilation failed at $(date)" >> nop_test_results.log
    fi
    
    echo "Waiting 5 seconds before next test round..."
    sleep 5
    cd ../src/Fuzzer
done

echo "All tests completed"