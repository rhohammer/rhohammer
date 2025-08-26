#pragma once
#include "common.h"
bool find_virt_Nop_test(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                        int bit1, int bit2, int bit3, addrpair_t &virt_pair1, addrpair_t &virt_pair2);
uint64_t get_single_timing(virtaddr_t first, uint64_t num_of_read, int mode);
uint64_t get_four_timing(virtaddr_t first, virtaddr_t second, virtaddr_t third, virtaddr_t fourth, uint64_t num_of_read, int mode);
uint64_t get_three_timing_NOP(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode, int no_nop);
uint64_t get_three_timing(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode);
uint64_t get_three_timing_multicolumn(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode);
uint64_t get_three_timing_multicolumn_scatter(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode);
uint64_t get_three_timing_multicolumn_gather(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode);
uint64_t get_three_timing_gather(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode);
uint64_t get_three_timing_scatter(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode);