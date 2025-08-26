#include "../include/test_utility.h"
#include "../include/utility.h"
bool find_virt_Nop_test(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                        int bit1, int bit2, int bit3, addrpair_t &virt_pair1, addrpair_t &virt_pair2)
{
    size_t physical_pages_sz = physical_pages.size();
    // Find different addresses
    for (size_t i = 0; i < physical_pages.size(); ++i)
    {
        size_t offset = (rand() % (physical_pages_sz - 1)) + 1;
        // next: advance offset elements and return
        auto mit = next(physical_pages.begin(), offset);
        // The second position stores the virtual address
        physaddr_t tmp = mit->first;
        physaddr_t tmp1 = tmp ^ (1ul << bit1);
        physaddr_t tmp2 = tmp ^ (1ul << bit2);
        physaddr_t tmp3 = tmp ^ (1ul << bit3);
        if (physical_pages.find(tmp1) != physical_pages.end())
            if (physical_pages.find(tmp2) != physical_pages.end())
                if (physical_pages.find(tmp3) != physical_pages.end())
                {
                    virt_pair1.first = physical_pages[tmp1];
                    virt_pair1.second = physical_pages[tmp2];
                    virt_pair2.first = physical_pages[tmp3];
                    return true;
                }
    }
    dbg_printf("Unable to find an address pair\n");
    return false;
}

uint64_t get_three_timing(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode)
{
    int error_cnt = 0;
    int test_round = 6;
    uint64_t t0 = 0, res = 0, fence_time = 0;
    uint64_t number_of_reads = 0;
    uint64_t sum_res = 0;

    // Determine which timing method to use
    if (mode)
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0;
            number_of_reads = num_of_read;

            volatile size_t *f = (volatile size_t *)first;
            volatile size_t *s = (volatile size_t *)second;
            volatile size_t *p = (volatile size_t *)third;
            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }

            while (number_of_reads-- > 0)
            {
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");
                asm volatile("mfence" ::: "memory");
                t0 = rdtsc();

                *f;
                *s;
                *p;
            
                asm volatile("mfence" ::: "memory");
                res += rdtsc2() - t0;
            }

            res /= num_of_read;

            if (res >= MAX_LATENCY_SIZE)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
    }
    else
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0, res = 0;
            number_of_reads = num_of_read;
            uint64_t *z = &t0;
            volatile size_t *f = (volatile size_t *)first;
            volatile size_t *s = (volatile size_t *)second;
            volatile size_t *p = (volatile size_t *)third;
            struct timespec test_1, test_2;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }

            while (number_of_reads-- > 0)
            {
                struct timespec start, end;

                asm volatile("mfence" ::: "memory");
                if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
                {
                    perror("clock_gettime start");
                    return 1;
                }
                *f;
                asm volatile("clflushopt (%0)"
                             :
                             : "r"(f)
                             : "memory");
                *s;
                asm volatile("clflushopt (%0)"
                             :
                             : "r"(s)
                             : "memory");
                *p;
                asm volatile("clflushopt (%0)"
                             :
                             : "r"(p)
                             : "memory");
                if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
                {
                    perror("clock_gettime end");
                    return 1;
                }
                asm volatile("mfence" ::: "memory");

                res += (end.tv_nsec - start.tv_nsec);
            }
            res /= (num_of_read);
            if (res > 300)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
        fence_time /= (test_round * num_of_read);
    }
    assert(test_round > error_cnt);
    return sum_res / (test_round - error_cnt);
}

uint64_t get_three_timing_multicolumn_scatter(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode)
{
    int error_cnt = 0;
    int test_round = 6;
    uint64_t t0 = 0, res = 0, fence_time = 0;
    uint64_t number_of_reads = 0;
    uint64_t sum_res = 0;
    third^= (1ul << (8));
    volatile size_t *f = (volatile size_t *)first;
    volatile size_t *s = (volatile size_t *)second;
    volatile size_t *p = (volatile size_t *)third;
    if (mode)
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0;
            number_of_reads = num_of_read;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }

            while (number_of_reads-- > 0)
            {
                t0 = rdtsc();
                *f; 
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");
                *s;
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");
                *p;
                asm volatile("clflushopt (%0)"
                :
                : "r"(p)
                : "memory");
                res += rdtsc2() - t0;
            }

            res /= num_of_read;

            if (res >= MAX_LATENCY_SIZE)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
    }
    else
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0, res = 0;
            number_of_reads = num_of_read;
            uint64_t *z = &t0;
            struct timespec test_1, test_2;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }
            struct timespec start, end;
            while (number_of_reads-- > 0)
            {
                if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
                {
                    perror("clock_gettime start");
                    return 1;
                }
                asm volatile("mfence" ::: "memory");
                *f; 
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");

                *s;
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");

                *p;
                asm volatile("clflushopt (%0)"
                :
                : "r"(p)
                : "memory");
                asm volatile("mfence" ::: "memory");
                if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
                {
                    perror("clock_gettime end");
                    return 1;
                }
                res += (end.tv_nsec - start.tv_nsec)+(end.tv_sec-start.tv_sec)*1000000000;
            }
            res /= (num_of_read);
            if (res > 300)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
        fence_time /= (test_round * num_of_read);
    }
    assert(test_round > error_cnt);
    return sum_res / (test_round - error_cnt);
}

uint64_t get_three_timing_scatter(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode)
{
    int error_cnt = 0;
    int test_round = 6;
    uint64_t t0 = 0, res = 0, fence_time = 0;
    uint64_t number_of_reads = 0;
    uint64_t sum_res = 0;
    third^= (1ul << (7));
    volatile size_t *f = (volatile size_t *)first;
    volatile size_t *s = (volatile size_t *)second;
    volatile size_t *p = (volatile size_t *)(third);
    if (mode)
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0;
            number_of_reads = num_of_read;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }

            while (number_of_reads-- > 0)
            {
                t0 = rdtsc();
                *f; 
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");
                *s;
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");
                *p;
                asm volatile("clflushopt (%0)"
                :
                : "r"(p)
                : "memory");
                res += rdtsc2() - t0;
            }

            res /= num_of_read;

            if (res >= MAX_LATENCY_SIZE)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
    }
    else
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0, res = 0;
            number_of_reads = num_of_read;
            uint64_t *z = &t0;
            struct timespec test_1, test_2;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }
            struct timespec start, end;
            while (number_of_reads-- > 0)
            {
                if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
                {
                    perror("clock_gettime start");
                    return 1;
                }
                asm volatile("mfence" ::: "memory");
                *f; 
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");

                *s;
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");

                *p;
                asm volatile("clflushopt (%0)"
                :
                : "r"(p)
                : "memory");
                asm volatile("mfence" ::: "memory");
                if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
                {
                    perror("clock_gettime end");
                    return 1;
                }
                res += (end.tv_nsec - start.tv_nsec)+(end.tv_sec-start.tv_sec)*1000000000;
            }
            res /= (num_of_read);
            if (res > 300)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
        fence_time /= (test_round * num_of_read);
    }
    assert(test_round > error_cnt);
    return sum_res / (test_round - error_cnt);
}

uint64_t get_three_timing_gather(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode)
{
    int error_cnt = 0;
    int test_round = 6;
    uint64_t t0 = 0, res = 0, fence_time = 0;
    uint64_t number_of_reads = 0;
    uint64_t sum_res = 0;
    third^= (1ul << (7));
    volatile size_t *f = (volatile size_t *)first;
    volatile size_t *s = (volatile size_t *)second;
    volatile size_t *p = (volatile size_t *)(third);
    if (mode)
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0;
            number_of_reads = num_of_read;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }

            while (number_of_reads-- > 0)
            {
                t0 = rdtsc();
                *f; 
                
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");
                *s;
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");
                *p;
                asm volatile("clflushopt (%0)"
                :
                : "r"(p)
                : "memory");
                res += rdtsc2() - t0;
            }

            res /= num_of_read;

            if (res >= MAX_LATENCY_SIZE)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
    }
    else
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0, res = 0;
            number_of_reads = num_of_read;
            uint64_t *z = &t0;
            struct timespec test_1, test_2;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }
            struct timespec start, end;
            while (number_of_reads-- > 0)
            {
                if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
                {
                    perror("clock_gettime start");
                    return 1;
                }
                asm volatile("mfence" ::: "memory");
                *f; 
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");

                *s;
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");

                *p;
                asm volatile("clflushopt (%0)"
                :
                : "r"(p)
                : "memory");
                asm volatile("mfence" ::: "memory");
                if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
                {
                    perror("clock_gettime end");
                    return 1;
                }
                res += (end.tv_nsec - start.tv_nsec);
            }
            res /= (num_of_read);
            if (res > 300)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
        fence_time /= (test_round * num_of_read);
    }
    assert(test_round > error_cnt);
    return sum_res / (test_round - error_cnt);
}

uint64_t get_three_timing_multicolumn_gather(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode)
{
    int error_cnt = 0;
    int test_round = 6;
    uint64_t t0 = 0, res = 0, fence_time = 0;
    uint64_t number_of_reads = 0;
    uint64_t sum_res = 0;
    third^= (1ul << (8));
    volatile size_t *f = (volatile size_t *)first;
    volatile size_t *s = (volatile size_t *)second;
    volatile size_t *p = (volatile size_t *)third;
    if (mode)
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0;
            number_of_reads = num_of_read;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }

            while (number_of_reads-- > 0)
            {
                asm volatile("clflushopt (%0)"
                :
                : "r"(p)
                : "memory");
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");
                t0 = rdtsc();
                *f; 
                *s;
                *p;
                res += rdtsc2() - t0;
            }

            res /= num_of_read;

            if (res >= MAX_LATENCY_SIZE)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
    }
    else
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0, res = 0;
            number_of_reads = num_of_read;
            uint64_t *z = &t0;
            struct timespec test_1, test_2;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }
            struct timespec start, end;
            while (number_of_reads-- > 0)
            {
                asm volatile("clflushopt (%0)"
                :
                : "r"(f)
                : "memory");
                asm volatile("clflushopt (%0)"
                :
                : "r"(s)
                : "memory");
                asm volatile("clflushopt (%0)"
                :
                : "r"(p)
                : "memory");

                if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
                {
                    perror("clock_gettime start");
                    return 1;
                }
                asm volatile("mfence" ::: "memory");
                *f; 
                *s;
                *p;
                asm volatile("mfence" ::: "memory");
                if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
                {
                    perror("clock_gettime end");
                    return 1;
                }
                res += (end.tv_nsec - start.tv_nsec);
            }
            res /= (num_of_read);
            if (res > 300)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), get_phys_addr(second), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
        fence_time /= (test_round * num_of_read);
    }
    assert(test_round > error_cnt);
    return sum_res / (test_round - error_cnt);
}

uint64_t get_three_timing_NOP(virtaddr_t first, virtaddr_t second, virtaddr_t third, uint64_t num_of_read, int mode, int no_nop)
{
    const int test_round = 6;
    uint64_t sum_res = 0;
    int valid_rounds = 0;

    std::vector<uint64_t> round_results;
    round_results.reserve(test_round);

    char *buffer = new char[no_nop + 1];
    for (int i = 0; i < no_nop; i++) {
        buffer[i] = 0x90;
    }
    buffer[no_nop] = 0xc3;
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(buffer);
    addr &= ~0xfff;
    
    if (mprotect(reinterpret_cast<void *>(addr), 4096, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        perror("mprotect");
        delete[] buffer;
        return 1;
    }
    
    typedef void (*func_t)();
    func_t func = reinterpret_cast<func_t>(buffer);

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        for (int round = 0; round < test_round; ++round) {
            uint64_t res = 0;
            uint64_t reads = num_of_read;
            
            volatile size_t *f = (volatile size_t *)first;
            volatile size_t *s = (volatile size_t *)second;
            volatile size_t *p = (volatile size_t *)third;
            
            for (int j = 0; j < 10; j++) {
                sched_yield();
            }

            if (mode) {
                uint64_t t0;
                while (reads >= UNROLL_FACTOR) {
                    for (int i = 0; i < UNROLL_FACTOR; ++i) {
                        asm volatile("clflushopt (%0)" : : "r"(f) : "memory");
                        asm volatile("clflushopt (%0)" : : "r"(s) : "memory");
                        asm volatile("clflushopt (%0)" : : "r"(p) : "memory");
                        asm volatile("mfence" ::: "memory");
                        
                        t0 = rdtsc();
                        *f;
                        *s;
                        func();
                        *p;
                        res += rdtsc2() - t0;
                    }
                    reads -= UNROLL_FACTOR;
                }
                
                while (reads-- > 0) {
                    asm volatile("clflushopt (%0)" : : "r"(f) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(s) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(p) : "memory");
                    asm volatile("mfence" ::: "memory");
                    
                    t0 = rdtsc();
                    *f;
                    *s;
                    func();
                    *p;
                    res += rdtsc2() - t0;
                }
            } else {
                struct timespec start, end;
                while (reads-- > 0) {
                    asm volatile("clflushopt (%0)" : : "r"(f) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(s) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(p) : "memory");
                    asm volatile("mfence" ::: "memory");
                    
                    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start) == -1) {
                        perror("clock_gettime start");
                        mprotect(reinterpret_cast<void *>(addr), 4096, PROT_READ | PROT_WRITE);
                        delete[] buffer;
                        return 1;
                    }
                    
                    *f;
                    *s;
                    func();
                    *p;
                    
                    if (clock_gettime(CLOCK_MONOTONIC_RAW, &end) == -1) {
                        perror("clock_gettime end");
                        mprotect(reinterpret_cast<void *>(addr), 4096, PROT_READ | PROT_WRITE);
                        delete[] buffer;
                        return 1;
                    }
                    
                    res += (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
                }
            }
            
            res /= num_of_read;
            
            const uint64_t latency_threshold = mode ? MAX_LATENCY_SIZE : 300;
            if (res < latency_threshold) {
                sum_res += res;
                valid_rounds++;
            } else {
                dbg_printf("[-][-] error round: %d, attempt: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           round, attempt, get_phys_addr(first), get_phys_addr(second), res);
            }
        }
        
        if (valid_rounds >= (test_round / 2)) {
            break;
        }
        
        if (valid_rounds == 0 && attempt < MAX_ATTEMPTS - 1) {
            sum_res = 0;
        }
    }
    
    mprotect(reinterpret_cast<void *>(addr), 4096, PROT_READ | PROT_WRITE);
    delete[] buffer;
    
    if (valid_rounds == 0) {
        return MAX_LATENCY_SIZE;
    }
    
    return sum_res / valid_rounds;
}

uint64_t get_single_timing(virtaddr_t first, uint64_t num_of_read, int mode)
{
    int error_cnt = 0;
    int test_round = 10;
    uint64_t t0 = 0, res = 0;
    uint64_t number_of_reads = 0;
    uint64_t sum_res = 0;

    if (mode)
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0;
            number_of_reads = num_of_read;

            volatile size_t *f = (volatile size_t *)first;

            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }

            while (number_of_reads-- > 0)
            {
                asm volatile("clflushopt (%0)"
                             :
                             : "r"(f)
                             : "memory");
                asm volatile("mfence" ::: "memory");
                t0 = rdtsc();
                *f;
                asm volatile("mfence" ::: "memory");
                res += rdtsc2() - t0;
            }

            res /= num_of_read;

            if (res >= MAX_LATENCY_SIZE)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx,  latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
    }
    else
    {
        for (int i = 0; i < test_round; i++)
        {
            t0 = 0;
            number_of_reads = num_of_read;

            volatile size_t *f = (volatile size_t *)first;
            for (int j = 0; j < 10; j++)
            {
                sched_yield();
            }

            while (number_of_reads-- > 0)
            {
                struct timespec start, end;
                asm volatile("clflushopt (%0)"
                             :
                             : "r"(f)
                             : "memory");
                if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
                {
                    perror("clock_gettime start");
                    return 1;
                }
                asm volatile("mfence" ::: "memory");
                t0 = start.tv_sec * 1000000000LL + start.tv_nsec;
                asm volatile("mfence" ::: "memory");
                *f;

                if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
                {
                    perror("clock_gettime end");
                    return 1;
                }

                res += end.tv_sec * 1000000000LL + end.tv_nsec - t0;
            }
            res /= num_of_read;
            if (res >= 300)
            {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, i: %d, first pa: 0x%lx, latency: %lu\n",
                           error_cnt, i, get_phys_addr(first), res);
            }
            else
            {
                sum_res += res;
            }

            if (error_cnt != 0)
            {
                i = 0;
                error_cnt = 0;
                sum_res = 0;
            }
        }
    }
    assert(test_round > error_cnt);
    return sum_res / (test_round - error_cnt);
}

uint64_t get_four_timing(virtaddr_t first, virtaddr_t second, virtaddr_t third, virtaddr_t fourth, uint64_t num_of_read, int mode)
{
    const int test_round = 6;
    int error_cnt = 0;
    uint64_t sum_res = 0;
    int valid_rounds = 0;

    std::vector<uint64_t> round_results;
    round_results.reserve(test_round);

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        for (int round = 0; round < test_round; ++round) {
            uint64_t t0 = 0, res = 0;
            uint64_t reads_remaining = num_of_read;

            volatile size_t *f = (volatile size_t *)first;
            volatile size_t *s = (volatile size_t *)second;
            volatile size_t *y = (volatile size_t *)third;
            volatile size_t *z = (volatile size_t *)fourth;

            for (int j = 0; j < 10; j++) {
                sched_yield();
            }

            if (mode) {
                while (reads_remaining >= UNROLL_FACTOR) {
                    for (int i = 0; i < UNROLL_FACTOR; ++i) {
                        asm volatile("clflushopt (%0)" : : "r"(f) : "memory");
                        asm volatile("clflushopt (%0)" : : "r"(s) : "memory");
                        asm volatile("clflushopt (%0)" : : "r"(y) : "memory");
                        asm volatile("clflushopt (%0)" : : "r"(z) : "memory");
                        asm volatile("mfence" ::: "memory");
                        
                        t0 = rdtsc();
                        *f;
                        *s;
                        *y;
                        *z;
                        res += rdtsc2() - t0;
                    }
                    reads_remaining -= UNROLL_FACTOR;
                }
                
                while (reads_remaining-- > 0) {
                    asm volatile("clflushopt (%0)" : : "r"(f) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(s) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(y) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(z) : "memory");
                    asm volatile("mfence" ::: "memory");
                    
                    t0 = rdtsc();
                    *f;
                    *s;
                    *y;
                    *z;
                    res += rdtsc2() - t0;
                }
            } else {
                struct timespec start, end;
                
                while (reads_remaining-- > 0) {
                    asm volatile("clflushopt (%0)" : : "r"(f) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(s) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(y) : "memory");
                    asm volatile("clflushopt (%0)" : : "r"(z) : "memory");
                    asm volatile("mfence" ::: "memory");
                    
                    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start) == -1) {
                        perror("clock_gettime start");
                        return MAX_LATENCY_SIZE;
                    }
                    
                    *f;
                    *s;
                    *y;
                    *z;
                    
                    if (clock_gettime(CLOCK_MONOTONIC_RAW, &end) == -1) {
                        perror("clock_gettime end");
                        return MAX_LATENCY_SIZE;
                    }
                    
                    res += (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
                }
            }
            
            res /= num_of_read;
            
            const uint64_t latency_threshold = mode ? MAX_LATENCY_SIZE : 300;
            
            if (res < latency_threshold) {
                sum_res += res;
                valid_rounds++;
                round_results.push_back(res);
            } else {
                error_cnt++;
                dbg_printf("[-][-] error_cnt: %d, round: %d, attempt: %d, first pa: 0x%lx, second pa: 0x%lx, latency: %lu\n",
                           error_cnt, round, attempt, get_phys_addr(first), get_phys_addr(second), res);
            }
        }
        
        if (valid_rounds >= (test_round / 2)) {
            break;
        }
        
        if (valid_rounds == 0 && attempt < MAX_ATTEMPTS - 1) {
            sum_res = 0;
            error_cnt = 0;
            round_results.clear();
        }
    }
    
    if (valid_rounds == 0) {
        return MAX_LATENCY_SIZE;
    }
    
    if (round_results.size() >= 3) {
        std::sort(round_results.begin(), round_results.end());
        sum_res = 0;
        for (size_t i = 1; i < round_results.size() - 1; ++i) {
            sum_res += round_results[i];
        }
        return sum_res / (round_results.size() - 2);
    }
    
    return sum_res / valid_rounds;
}
