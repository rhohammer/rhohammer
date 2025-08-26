
#include "../include/utility.h"
#include <array>
// Replace repeated random number generator initializations
// For example in find_virt_pair, find_virt_pair_3bits and other functions
double time_diff(struct timeval &start, struct timeval &end)
{
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec + 0.0) / 1e6;
}

bool find_virt_pair(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                    int bit_index, addrpair_t &virt_pair)
{
    static vector<physaddr_t> cached_phyaddrs;
    static std::mt19937 rng1(std::random_device{}());

    // Initialize cache and random engine
    if (cached_phyaddrs.empty())
    {
        cached_phyaddrs.reserve(physical_pages.size());
        for (const auto &pair : physical_pages)
        {
            cached_phyaddrs.push_back(pair.first);
        }
    }
    if (physical_pages.size() < 2)
        return false;

    std::uniform_int_distribution<size_t> dist(0, cached_phyaddrs.size() - 1);
    constexpr size_t kMaxAttempts = 100;

    // For page offset bits (< 12), we can directly manipulate virtual addresses
    if (bit_index < 12)
    {
        size_t offset = dist(rng1);
        physaddr_t phyaddr_one = cached_phyaddrs[offset];
        virt_pair.first = physical_pages[phyaddr_one];
        virt_pair.second = virt_pair.first ^ (1ul << bit_index);
        return true;
    }
    else
    {
        // For higher bits, we need to find physical addresses that differ in the specified bit
        for (size_t i = 0; i < kMaxAttempts; ++i)
        {
            size_t offset = dist(rng1);
            physaddr_t phyaddr_one = cached_phyaddrs[offset];
            physaddr_t phyaddr_two = phyaddr_one ^ (1ul << bit_index);
            if (auto it = physical_pages.find(phyaddr_two); it != physical_pages.end())
            {
                virt_pair.first = physical_pages[phyaddr_one];
                virt_pair.second = it->second;
                return true;
            }
        }
        dbg_printf("Unable to find pair for bit %d\n", bit_index);
        return false;
    }
}

bool find_virt_pair_3bits(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                          int bit1, int bit2, int bit3, addrpair_t &virt_pair)
{
    static vector<physaddr_t> phys_addrs;
    static std::unordered_set<physaddr_t> phys_addr_set;
    static std::mt19937 rng4(std::random_device{}());

    // Initialize physical address cache
    if (phys_addrs.empty())
    {
        phys_addrs.reserve(physical_pages.size());
        for (auto &pair : physical_pages)
        {
            phys_addrs.push_back(pair.first);
        }
        phys_addr_set = std::unordered_set<physaddr_t>(phys_addrs.begin(), phys_addrs.end());
    }

    // Pre-calculate masks for high bits (≥12) and low bits (<12)
    uint64_t high_mask = 0, low_mask = 0;
    for (int b : {bit1, bit2, bit3})
    {
        if (b >= 12)
            high_mask |= (1ULL << b);
        else
            low_mask |= (1ULL << b);
    }

    const int max_attempts = 100;
    std::uniform_int_distribution<size_t> dist(0, phys_addrs.size() - 1);

    // Try to find matching physical address pairs
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        size_t offset = dist(rng4);
        physaddr_t phy_one = phys_addrs[offset];
        physaddr_t phy_two = phy_one ^ high_mask;

        if (phys_addr_set.contains(phy_two))
        {
            virt_pair.first = physical_pages[phy_one];
            virt_pair.second = physical_pages[phy_two] ^ low_mask;
            return true;
        }
    }

    dbg_printf("Failed after %d attempts\n", max_attempts);
    return false;
}

bool find_virt_pair_5bits(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                          int bit1, int bit2, int bit3, int bit4, int bit5, addrpair_t &virt_pair)
{
    // Use static cache to accelerate lookups
    static vector<physaddr_t> phys_addrs;
    static std::unordered_set<physaddr_t> phys_addr_set;
    static std::mt19937 rng5(std::random_device{}());

    // Initialize physical address cache
    if (phys_addrs.empty())
    {
        phys_addrs.reserve(physical_pages.size());
        for (auto &pair : physical_pages)
        {
            phys_addrs.push_back(pair.first);
        }
        phys_addr_set = std::unordered_set<physaddr_t>(phys_addrs.begin(), phys_addrs.end());
    }

    // Pre-calculate high and low bit masks
    uint64_t high_mask = 0, low_mask = 0;
    for (int b : {bit1, bit2, bit3, bit4, bit5})
    {
        if (b >= 12)
            high_mask |= (1ULL << b);
        else
            low_mask |= (1ULL << b);
    }

    // Set maximum attempts and use uniform distribution
    const int max_attempts = 100;
    std::uniform_int_distribution<size_t> dist(0, phys_addrs.size() - 1);

    // Try to find matching physical address pairs
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        size_t offset = dist(rng5);
        physaddr_t phy_one = phys_addrs[offset];
        physaddr_t phy_two = phy_one ^ high_mask;

        // Check if matching physical address exists
        if (phys_addr_set.contains(phy_two))
        {
            virt_pair.first = physical_pages[phy_one];
            virt_pair.second = physical_pages[phy_two] ^ low_mask;
            return true;
        }
    }

    dbg_printf("Unable to find an address pair for 5 bits\n");
    return false;
}

bool find_virt_pair_4bits(void *memory_mapping,
                          unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                          int bit1, int bit2, int bit3, int bit4,
                          addrpair_t &virt_pair)
{
    static vector<physaddr_t> phys_addrs;
    static std::unordered_set<physaddr_t> phys_addr_set;
    static std::mt19937 rng4(std::random_device{}());

    // Initialize physical address cache
    if (phys_addrs.empty())
    {
        phys_addrs.reserve(physical_pages.size());
        for (auto &pair : physical_pages)
        {
            phys_addrs.push_back(pair.first);
        }
        phys_addr_set = std::unordered_set<physaddr_t>(phys_addrs.begin(), phys_addrs.end());
    }

    // Pre-calculate masks
    uint64_t high_mask = 0, low_mask = 0;
    for (int b : {bit1, bit2, bit3, bit4})
    {
        if (b >= 12)
            high_mask |= (1ULL << b);
        else
            low_mask |= (1ULL << b);
    }

    const int max_attempts = 100;
    std::uniform_int_distribution<size_t> dist(0, phys_addrs.size() - 1);

    // Search for matching address pairs
    for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
        size_t offset = dist(rng4);
        physaddr_t phy_one = phys_addrs[offset];
        physaddr_t phy_two = phy_one ^ high_mask;

        if (phys_addr_set.contains(phy_two))
        {
            virt_pair.first = physical_pages[phy_one];
            virt_pair.second = physical_pages[phy_two] ^ low_mask;
            return true;
        }
    }

    dbg_printf("Failed after %d attempts\n", max_attempts);
    return false;
}

bool find_virt_pair_2bits(void *memory_mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages,
                          int x_index, int y_index, addrpair_t &virt_pair)
{
    // Use static cache and hash table to accelerate lookups
    static vector<physaddr_t> phys_addrs;
    static std::unordered_set<physaddr_t> phys_addr_set;
    static bool initialized = false;

    // One-time initialization to avoid repeated calculations
    if (!initialized)
    {
        phys_addrs.reserve(physical_pages.size());
        for (auto &pair : physical_pages)
            phys_addrs.push_back(pair.first);
        phys_addr_set = std::unordered_set<physaddr_t>(phys_addrs.begin(), phys_addrs.end());
        initialized = true;
    }

    const int max_attempts = 100; // Limit maximum attempts
    const size_t total_pages = phys_addrs.size();
    if (total_pages < 2)
        return false;

    // Handle special case for page offset bits (<12)
    auto handle_low_bits = [&](int bit1, int bit2)
    {
        const size_t offset = rand() % total_pages;
        physaddr_t phy_addr = phys_addrs[offset];
        virtaddr_t virt_base = physical_pages[phy_addr];
        virt_pair.first = virt_base;
        virt_pair.second = virt_base ^ (1ULL << bit1) ^ (1ULL << bit2);
        return true;
    };

    // Dynamically select search strategy
    if (x_index < 12 && y_index < 12)
    {
        return handle_low_bits(x_index, y_index);
    }

    // Handle special case for identical bits
    if (x_index >= 12 && x_index == y_index)
    {
        const size_t offset = rand() % total_pages;
        physaddr_t phy_addr = phys_addrs[offset];
        virtaddr_t virt_addr = physical_pages[phy_addr];
        virt_pair = {virt_addr, virt_addr};
        return true;
    }

    // Optimize mask calculation using bit operations
    const uint64_t mask_x = (x_index < 12) ? 0 : (1ULL << x_index);
    const uint64_t mask_y = (y_index < 12) ? 0 : (1ULL << y_index);
    const uint64_t combined_mask = mask_x | mask_y;

    // Use batch processing to find matches
    constexpr int batch_size = 16; // Adjust based on CPU cache size
    for (int i = 0; i < max_attempts; i += batch_size)
    {
        std::array<physaddr_t, batch_size> candidates;
        std::array<physaddr_t, batch_size> targets;

        // Generate candidate addresses in batch
        for (int j = 0; j < batch_size && i + j < max_attempts; ++j)
        {
            const size_t offset = rand() % total_pages;
            candidates[j] = phys_addrs[offset];
            targets[j] = candidates[j] ^ combined_mask;
        }

        // Check matches in batch
        for (int j = 0; j < batch_size && i + j < max_attempts; ++j)
        {
            if (phys_addr_set.count(targets[j]))
            {
                // Found a match, process result
                virtaddr_t virt_one = physical_pages[candidates[j]];
                virtaddr_t virt_two = physical_pages[targets[j]];

                // Apply low bit corrections
                if (x_index < 12)
                    virt_two ^= (1ULL << x_index);
                if (y_index < 12)
                    virt_two ^= (1ULL << y_index);

                virt_pair = {virt_one, virt_two};
                return true;
            }
        }
    }
    // Fallback mechanism: try low 12 bits modification
    if (x_index >= 12 && y_index >= 12)
    {
        return handle_low_bits(x_index % 12, y_index % 12);
    }

    return false;
}

// Merge new function pairs into bank_fun
bool merge_element_vector2(vector<vector<int>> &matrix, int target1, int target2)
{
    for (auto &row : matrix)
    {
        bool found1 = false, found2 = false;
        for (int num : row)
        {
            if (num == target1)
                found1 = true;
            if (num == target2)
                found2 = true;
            if (found1 && found2)
                break; // Exit loop early
        }
        if (found1 && found2)
        {
            return true;
        }
        else if (found1)
        {
            row.push_back(target2);
            dbg_printf("Insert %d\n", target2);
            return true;
        }
        else if (found2)
        {
            row.push_back(target1);
            dbg_printf("Insert %d\n", target1);
            return true;
        }
    }
    // If not found in any row, add a new row
    matrix.push_back({target1, target2});
    dbg_printf("Insert (%d, %d)\n", target1, target2);
    return true;
}

// Measure access time between two memory addresses
uint64_t get_timing(virtaddr_t first, virtaddr_t second,
                    uint64_t num_of_read)
{
    volatile size_t *f = (volatile size_t *)first;
    volatile size_t *s = (volatile size_t *)second;

    uint64_t sum_res = 0;
    int valid_rounds = 0;
    const int max_rounds = 6;

    // Pre-allocate memory to avoid allocation in loop
    std::vector<uint64_t> round_results;
    round_results.reserve(max_rounds);

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        for (int round = 0; round < max_rounds; ++round)
        {
            uint64_t res = 0;
            uint64_t reads = num_of_read;

            // Batch flush cache lines
            flush_addresses(f, s);
            if (TIMING_MODE)
            { // Mode 1: RDTSCP timing
                uint64_t start;
                // Unroll loop to reduce branch prediction overhead
                while (reads >= UNROLL_FACTOR)
                {
                    for (int i = 0; i < UNROLL_FACTOR; ++i)
                    {
                        flush_addresses(f, s);
                        start = rdtsc();
                        *f;
                        *s; // Memory access operations
                        res += (rdtsc2() - start);
                    }
                    reads -= UNROLL_FACTOR;
                }
                // Handle remaining iterations
                while (reads-- > 0)
                {
                    flush_addresses(f, s);
                    start = rdtsc();
                    *f;
                    *s; // Memory access operations
                    res += (rdtsc2() - start);
                }
            }
            else
            { // Mode 2: clock_gettime timing
                struct timespec start, end;
                while (reads-- > 0)
                {
                    flush_addresses(f, s);
                    mfence();
                    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
                    *f;
                    *s; // Memory access operations
                    // mfence();
                    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
                    mfence();
                    res += (end.tv_sec - start.tv_sec) * 1e9 +
                           (end.tv_nsec - start.tv_nsec);
                }
            }

            // Dynamic threshold calculation (example values need calibration)
            const uint64_t latency_threshold = TIMING_MODE ? MAX_LATENCY_SIZE : MAX_LATENCY_SIZE_CLOCK;
            if (res / num_of_read < latency_threshold)
            {
                sum_res += res;
                valid_rounds++;
            }
            else if (++attempt >= MAX_ATTEMPTS)
            {
                break;
            }
        }

        if (valid_rounds >= (max_rounds / 2))
            break; // Success condition
    }

    return valid_rounds ? (sum_res / (valid_rounds * num_of_read)) : UINT64_MAX;
}

// Get the size of the physical memory in the system
uint64_t get_phys_mem_size()
{
    struct sysinfo info;
    sysinfo(&info);
    return (size_t)info.totalram * (size_t)info.mem_unit;
}

// Get the page frame number for a virtual address
physaddr_t get_page_frame_num(int pagemap, virtaddr_t virtual_address)
{
    uint64_t value;
    int got = pread(pagemap, &value, 8, (virtual_address / PAGE_SIZE) * 8);
    assert(got == 8);
    physaddr_t page_frame_number = value & ((1ul << 54) - 1);
    return page_frame_number;
}

// Get the physical address corresponding to virtual_addr
physaddr_t get_phys_addr(virtaddr_t virtual_addr)
{
    int fd = open("/proc/self/pagemap", O_RDONLY);
    assert(fd >= 0);
    off_t pos = lseek(fd, (virtual_addr / PAGE_SIZE) * 8, SEEK_SET);
    assert(pos >= 0);
    uint64_t value;
    int got = read(fd, &value, 8);
    assert(got == 8);
    int rc = close(fd);
    assert(rc == 0);

    physaddr_t frame_num = value & ((1ul << 54) - 1);
    return (frame_num * PAGE_SIZE) | (virtual_addr & (PAGE_SIZE - 1));
}

// Extract a specific bit from a physical address
uint8_t bit(physaddr_t pa, int bit)
{
    return (pa >> bit) & 1;
}

// Allocate memory: parameters are memory size, memory start address, and fraction of total memory
void setup_mapping(uint64_t *mapping_size, void **mapping, double fraction)
{
    *mapping_size = static_cast<uint64_t>((static_cast<double>(get_phys_mem_size()) * fraction));
    *mapping = mmap(NULL, *mapping_size, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(*mapping != (void *)-1);

    dbg_printf("[!] Initializing large memory mapping ...");
    for (uint64_t index = 0; index < *mapping_size; index += PAGE_SIZE)
    {
        // Initialize values in the allocated memory region
        uint64_t *temporary = reinterpret_cast<uint64_t *>(static_cast<uint8_t *>(*mapping) + index);
        temporary[0] = index; // set page value to index
    }
    dbg_printf("done\n");
}

// Initialize virtual addresses by traversing all pages and reading physical addresses from /proc/self/pagemap
void store_phy_pages(void *memory_mapping, uint64_t memory_mapping_size,
                     unordered_map<physaddr_t, virtaddr_t> &physical_pages)
{
    int pagemap = open("/proc/self/pagemap", O_RDONLY);
    assert(pagemap >= 0);
#ifdef DEBUG
    std::ofstream outfile("phy_virt.txt");
#endif
    dbg_printf("[!] Translating virtual addresses -> physical addresses...");
    for (uint64_t offset = 0; offset * PAGE_SIZE < memory_mapping_size; offset++)
    {
        virtaddr_t virtual_address = (virtaddr_t)memory_mapping + offset * PAGE_SIZE;
        physaddr_t page_frame_number = get_page_frame_num(pagemap, virtual_address);
        physaddr_t physical_address = page_frame_number * PAGE_SIZE;
        physical_pages[physical_address] = virtual_address;
#ifdef DEBUG
        outfile << std::hex;
        outfile << "< physical_address: 0x" << physical_address << ", virtual_address: 0x" << virtual_address << " >" << std::endl;
#endif
    }
    dbg_printf("done\n");
}

// Get a random address quickly from the memory mapping
virtaddr_t get_random_address_fast(void *start, uint64_t mapping_size)
{
    static std::random_device rd;
    static std::default_random_engine gen(rd());
    static std::uniform_int_distribution<size_t> dist(0, mapping_size);
    return (virtaddr_t)start + dist(gen);
}

// Find threshold based on probability density distribution
int find_threshold(void *start, unordered_map<physaddr_t, virtaddr_t> &physical_pages, uint64_t mapping_size, uint64_t g_num_reads, int g_banks_number_total)
{
    // Log the start of histogram data generation
    dbg_printf("[!] Generating histogram data to find bank conflict threshold\n");
    int threshold1, threshold2 = 0;
    // Define histogram maximum value and sample count
    constexpr size_t HISTOGRAM_MAX_VALUE = TIMING_MODE ? MAX_LATENCY_SIZE : MAX_LATENCY_SIZE_CLOCK;
    size_t HISTOGRAM_ENTRIES = g_banks_number_total * NUM_OF_THRESHOLD;

    // Initialize histogram with all values set to 0
    std::vector<size_t> histogram(HISTOGRAM_MAX_VALUE, 0);
    size_t num_entries = 0;

    // Collect histogram data
    while (num_entries < HISTOGRAM_ENTRIES)
    {
        // Generate random address pairs
        auto a1 = get_random_address_fast(start, mapping_size);
        auto a2 = get_random_address_fast(start, mapping_size);
        // Measure access time
        auto time = (size_t)get_timing(a1, a2, g_num_reads);
        // Update histogram if time is within range
        if (time < histogram.size())
        {
            histogram[time]++;
            num_entries++;
        }
    }

    // Calculate threshold
    threshold1 = HISTOGRAM_MAX_VALUE - 1;
    threshold2 = threshold1;
    size_t num_entries_above_threshold = 0;
    while (num_entries_above_threshold < (HISTOGRAM_ENTRIES / g_banks_number_total / 2))
    {
        num_entries_above_threshold += histogram[threshold1];
        threshold1--;
        threshold2--;
        assert(threshold1 > 0); // Ensure threshold doesn't go below 0
    }
    while (num_entries_above_threshold < double((g_banks_number_total - 1) * HISTOGRAM_ENTRIES / g_banks_number_total / 2))
    {
        num_entries_above_threshold += histogram[threshold2];
        threshold2--;
        assert(threshold2 > 0); // Ensure threshold doesn't go below 0
    }
    // For debugging
#ifdef DEBUG
    std::ofstream outfile("./threshold.csv");
    outfile << "latency,num" << std::endl;
    for (int i = 0; i <= HISTOGRAM_MAX_VALUE; i++)
    {
        outfile << i << ',' << histogram[i] << std::endl;
    }
    outfile << "latency,num" << std::endl;
    outfile <<(threshold1 + threshold2) / 2;
    outfile.close();
#endif
    return (threshold1 + threshold2) / 2;
}
