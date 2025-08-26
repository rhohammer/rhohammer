#pragma once
#include "common.h"
#include <unordered_set>


uint64_t get_phys_mem_size();
physaddr_t get_phys_addr(virtaddr_t virtual_addr);



double time_diff(struct timeval &start, struct timeval &end);

physaddr_t get_page_frame_num(int pagemap, virtaddr_t virtual_address);
// bool valid_new_set(std::set<physaddr_t> &new_set, size_t total_phy_size);

uint64_t get_timing(virtaddr_t first, virtaddr_t second, uint64_t num_of_read);


void remove_from_sets(std::set<addrpair_t> &set_virt_phys, std::set<physaddr_t> &new_set);
std::map<int, std::vector<uint64_t>> gen_xor_masks(std::vector<int> &vec_bank_bits_range, int max_xor_bits);
bool merge_element_vector2(vector<vector<int>> &matrix, int target1, int target2);

bool find_virt_pair(void *memory_mapping, std::unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                    int bit_index, addrpair_t &virt_pair);
bool find_virt_pair_2bits(void *memory_mapping, std::unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                          int x_index, int y_index, addrpair_t &virt_pair);
bool find_virt_pair_3bits(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                          int bit1, int bit2, int bit3, addrpair_t &virt_pair);
bool find_virt_pair_4bits(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                          int bit1, int bit2, int bit3, int bit4, addrpair_t &virt_pair);
bool find_virt_pair_5bits(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                          int bit1, int bit2, int bit3, int bit4, int bit5, addrpair_t &virt_pair);

void setup_mapping(uint64_t *mapping_size, void **mapping, double fraction);
void store_phy_pages(void *memory_mapping, uint64_t memory_mapping_size,
                     std::unordered_map<physaddr_t, virtaddr_t> &physical_pages);
addrpair_t get_random_address(unordered_map<physaddr_t, virtaddr_t> &physical_pages, uint64_t mapping_size);
int retrive_threshold(unordered_map<physaddr_t, virtaddr_t> &physical_pages, uint64_t mapping_size, uint64_t g_num_reads);
int find_threshold(void *mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages, uint64_t mapping_size, uint64_t g_num_reads, int g_banks_number_total);

// main fun
void detect_bankfun_coarsegrained(void *mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages, vector<int> &vec_bank_bits, vector<int> &vec_bank_bits_2fun,
                                  vector<vector<int>> &bank_fun, int g_approx_latency, int g_available_length, int g_num_reads);
void detect_bankfun_finegrained(void *mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages, vector<int> &vec_bank_bits, vector<int> &vec_bank_bits_2fun,
                                vector<vector<int>> &bank_fun, int g_approx_latency, int g_available_length, int g_num_reads);
void merge_bankfun(void *mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages, vector<vector<int>> &bank_fun, int g_banks_number_total, int g_approx_latency, int g_num_reads);


inline uint64_t rdtsc() __attribute__((always_inline));
inline uint64_t rdtsc()
{
    uint64_t a, d;
    asm volatile(
        "xor %%rax, %%rax\n"
        "cpuid" ::
            : "rax", "rbx", "rcx", "rdx");
    asm volatile("rdtscp"
                 : "=a"(a), "=d"(d)
                 :
                 : "rcx");
    a = (d << 32) | a;
    return a;
}
inline void flush_addresses(volatile void *f, volatile void *s)
{
    asm volatile("clflushopt (%0)\n\t"
                 "clflushopt (%1)\n\t"
                 : : "r"(f), "r"(s) : "memory");
}

inline uint64_t rdtsc2() __attribute__((always_inline));
inline uint64_t rdtsc2()
{
    uint64_t a, d;
    asm volatile("rdtscp"
                 : "=a"(a), "=d"(d)
                 :
                 : "rcx");
    asm volatile("cpuid" ::
                     : "rax", "rbx", "rcx", "rdx");
    a = (d << 32) | a;
    return a;
}


#ifdef RHAPSODY_DEBUG
#define dbg_printf(...)      \
    do                       \
    {                        \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    } while (0);
#else
#define dbg_printf(...)
#endif

#define MEASURE_TIME_COST_START(s) \
    do                             \
    {                              \
        gettimeofday(&s, NULL);    \
    } while (0);

#define MEASURE_TIME_COST_END(logstr, s)                              \
    do                                                                \
    {                                                                 \
        struct timeval e;                                             \
        gettimeofday(&e, NULL);                                       \
        dbg_printf("[+] %s%f sec\n", logstr, time_diff(s, e)); \
    } while (0);

[[gnu::unused]] static inline __attribute__((always_inline)) void mfence()
{
    asm volatile("mfence" ::
                     : "memory");
}