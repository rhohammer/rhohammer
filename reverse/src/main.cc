#include "../include/common.h"
#include "../include/utility.h"
// read from config file
int g_banks_number_total = 32;      // Total number of banks
int g_available_length = 34;        // Number of memory address bits
uint64_t g_num_reads = 50;
// updated at runtime
int g_approx_latency = TIMING_MODE? 320:120; // Threshold to distinguish high and low latency

/// Functions moved from dramdig.cc
int handle(char *line, ssize_t llen)
{
    if (!line || llen <= 0)
    {
        return -1;
    }

    // Use fixed-size arrays allocated on stack to avoid VLA
    char cmd[256] = {0};
    char arg[256] = {0};
    
    // Use safer sscanf with limited reading length
    if (sscanf(line, "%255s %255s", cmd, arg) < 2) {
        return SUCCESS; // Incomplete parsing but not considered an error
    }
    
    // Use hash table or lookup table to optimize string comparison
    static const struct {
        const char* name;
        int* value;
        int base;
    } config_map[] = {
        {"num_of_read", (int*)&g_num_reads, 10},
        {"banks_number_total", &g_banks_number_total, 10},
        {"availablelength", &g_available_length, 10}
    };
    
    for (size_t i = 0; i < sizeof(config_map)/sizeof(config_map[0]); i++) {
        if (!strcasecmp(cmd, config_map[i].name)) {
            *config_map[i].value = strtol(arg, NULL, config_map[i].base);
            return SUCCESS;
        }
    }

    return SUCCESS;
}

// Read file content line by line, separated by newlines, and set global variables
int read_config(const char *fname)
{
    if (!fname)
    {
        return FAILED;
    }

    // Use buffered reading to improve performance
    FILE *f = fopen(fname, "r");
    if (!f)
    {
        return FAILED;
    }
    
    // Pre-allocate fixed-size buffer
    char buffer[4096];
    char *line = buffer;
    size_t max_line_len = sizeof(buffer);
    int ret = SUCCESS;
    
    // Use fgets instead of getline to reduce memory allocation
    while (fgets(line, max_line_len, f) != NULL)
    {
        size_t len = strlen(line);
        // Remove newline character
        if (len > 0 && line[len-1] == '\n')
            line[len-1] = '\0';
            
        if (FAILED == (ret = handle(line, len)))
        {
            break;
        }
    }

    dbg_printf(
        "BANKS_NUMBER_TOTAL: %d\n"
        "AVAILABLE_LENGTH: %d\n"
        "NUM_OF_READS: %lu\n",
        g_banks_number_total, g_available_length, g_num_reads);
    
    fclose(f);
    return ret;
}

int main()
{
    struct timeval start = {0};
    // struct timeval t1 = {0};
    MEASURE_TIME_COST_START(start)
    // MEASURE_TIME_COST_START(t1)
    
    // Use relative path directly, avoid using realpath
    if (SUCCESS != read_config("./output/config/config.ini"))
    {
        return -1;
    }

    // Use better random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Pre-allocate memory size
    uint64_t mapping_size = 0;
    void *mapping = NULL;

    // Use more efficient memory allocation strategy
    setup_mapping(&mapping_size, &mapping, 0.7);
    dbg_printf("[+] mapping: %p, mapping_size: 0x%lx\n", mapping, mapping_size);

    // Pre-allocate container size to reduce reallocation
    unordered_map<physaddr_t, virtaddr_t> physical_pages;
    physical_pages.reserve(mapping_size / PAGE_SIZE);
    store_phy_pages(mapping, mapping_size, physical_pages);

    // Pre-allocate vector capacity
    vector<int> vec_bank_bits;
    vec_bank_bits.reserve(g_available_length);
    vector<vector<int>> vec_bank_fun;
    vec_bank_fun.reserve(log2(g_banks_number_total));
    vector<int> vec_bank_bits_2fun;
    vec_bank_bits_2fun.reserve(g_available_length);
    
    dbg_printf("[+] mapping size: %ld\n", mapping_size);
    
    // Use parallel computation for threshold
    g_approx_latency = find_threshold(mapping, physical_pages, mapping_size, g_num_reads, g_banks_number_total);
    dbg_printf("[+] Row conflicts threshold: %d\n", g_approx_latency);

        // Consider using task parallelization
        detect_bankfun_coarsegrained(mapping, physical_pages, vec_bank_bits, vec_bank_bits_2fun, vec_bank_fun,
                                     g_approx_latency, g_available_length, g_num_reads);
        detect_bankfun_finegrained(mapping, physical_pages, vec_bank_bits, vec_bank_bits_2fun, vec_bank_fun,
                                   g_approx_latency, g_available_length, g_num_reads);
        if (1UL << vec_bank_fun.size() != g_banks_number_total)
            merge_bankfun(mapping, physical_pages, vec_bank_fun, g_banks_number_total, g_approx_latency, g_num_reads);

    // Ensure resources are released before program ends
    munmap(mapping, mapping_size);
    dbg_printf("[+] done!\n");
    MEASURE_TIME_COST_END("cost: ", start)
    fflush(stdout);
    return 0;
}
