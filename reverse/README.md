# Execute in Root Directory
## Main Program Execution
Modify config.ini file to adapt to different situations
```bash
make && make run
```
include/common.h change the para to suit your env
```c++
#define HIGH_PER_VALID 1.25
// Upper limit of valid new_set size
#define LOW_PER_VALID 0.75
// Lower limit of valid new_set size
#define DIVIDE_LEFT_PER 0.85
// Maximum valid latency
#define MAX_LATENCY_SIZE 1500
#define SUCCESS 0
#define FAILED -1
#define ADDRESSLENGTH (8 * sizeof(void *))
#define MODE 1 // New timing method, 0 for clock_gettime function timing, 1 for cycle count timing
#define PROP_OF_MINCLUSTER 0.4 
#define SIZE_OF_CLUSTER 100 // Size of address pool for brute force attack
#define PASS_PROB 0.85 // Proportion of valid addresses in cluster brute force address pool
#define NUM_OF_THRESHOLD 1024*64 // Number of addresses used for threshold debugging
```
## Single Step Test
```bash
make single_test && make run_single
sudo taskset 0x1 ./bin/single_test
```

## 2bit Brute Force Test
```bash
make 2bit_test && make run_2bit
sudo taskset 0x1 ./bin/2bit_test
```


