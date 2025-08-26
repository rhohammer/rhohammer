#pragma once

#include <assert.h>
#include <fcntl.h>
#include <linux/kernel-page-flags.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <signal.h>
#include <random>
#include <cstdint>     
#include <array> 

typedef uint64_t virtaddr_t;
typedef uint64_t physaddr_t;
typedef uint64_t pointer_t;
typedef std::pair<virtaddr_t, virtaddr_t> addrpair_t;

#define K (1024)
#define M (1024 * K)
#define G (1024 * M)
#define PAGE_SIZE 0x1000
#define PAGE_SHIFT 12
#define POINTER_SIZE (8 * sizeof(void *))

#define MAX_LATENCY_SIZE 1000
#define MAX_LATENCY_SIZE_CLOCK 200
#define SUCCESS 0
#define FAILED -1
#define ADDRESSLENGTH (8 * sizeof(void *))
#define TIMING_MODE 1 

#define NUM_OF_THRESHOLD 64
#define FUNC_MODE 1 // 0:use bankfunRE_cluster.cc ; 1:use bankfunRE_2bit.cc
#define MAX_ATTEMPTS 5    
#define CACHELINE_SIZE 64 
#define UNROLL_FACTOR 4   

using std::greater;
using std::list;
using std::make_pair;
using std::map;
using std::next;
using std::pair;
using std::set;
using std::sort;
using std::unordered_map;
using std::vector;
