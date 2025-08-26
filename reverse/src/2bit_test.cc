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


// Detect Bank functions
void bankfun_2bit_test(void *mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages, vector<int> &vec_row_bits)
{
    std::vector<std::vector<int>> bank_fun;

    // Maximum access time for different virtual address pairs in a single round
    double check_time[ADDRESSLENGTH];

    // Minimum access time for different virtual address pairs in a single round
    double check_time_lowest[ADDRESSLENGTH];

    // Average access time for different virtual address pairs in a single round
    double check_time_average[ADDRESSLENGTH];

    int index = 0, error_count = 0, test_count = 10;
    double sum = 0, max = 0, min = 0, current = 0, current_2 = 0;
    // Virtual address pairs
    addrpair_t virt_pair[16];
    addrpair_t virt_pair_2;

    dbg_printf("[!] Detecting Bank fun ...start\n");

    int diff = 0;
    
    // Control diff and index to be different
    for (diff = 0; diff < g_available_length; diff++)
    {
        for (index = 0; index < g_available_length; index++)
        {
            sum = 0;
            int Sum_num = 0;
            error_count = 0;
            int no_pair = 0;
            max = 0;
            min = 1e10;
            // Find 16 different address pairs, each pair differs in index and diff bits
            for (no_pair = 0; no_pair < 16; no_pair++)
            {
                if (find_virt_pair_2bits(mapping, physical_pages, diff, index, virt_pair[no_pair]))
                {
                    continue;
                }
                else
                {
                    dbg_printf("Bits %d test No. %d fail to find the virt pair", index, no_pair);
                }
            }
            // test start
            for (int test_count_perbit = 0; test_count_perbit < 16; test_count_perbit++)
            {
                current_2 = get_timing(virt_pair[test_count_perbit].first, virt_pair[test_count_perbit].second, g_num_reads);
                sum += current_2;
                Sum_num++;
                if (current_2 > max)
                {
                    check_time[index] = current_2;
                    max=current_2;
                }
                if (current_2 < min){
                    check_time_lowest[index] = current_2;
                    min=current_2;
                }   
                dbg_printf("keep Bit%d diff, change Bit%d, find pairs %d :first pa: 0x%lx, second pa: 0x%lx, latency: %f\n", diff, index, no_pair,
                           get_phys_addr(virt_pair[test_count].first), get_phys_addr(virt_pair[test_count].second), current_2);
            }
            check_time_average[index] = sum / Sum_num;
            dbg_printf("Row_test result for bit %d and bit %d : %f ~ %f, average: %f\n", diff, index, check_time_lowest[index], check_time[index], check_time_average[index]);
        }
        // SBDR threshold, global variable, manually selected
        // g_approx_latency = 320;
        bool isbankbit = false;
        for (index = diff; index < g_available_length; ++index)
        {
            // if (check_time_average[index] >= g_approx_latency)
            if (check_time_average[index] >= g_approx_latency)
            {
                isbankbit = true;
                dbg_printf("[+] find fun pair: (%d, %d)\n", diff, index);
                if (merge_element_vector2(bank_fun, diff, index))
                {
                    dbg_printf("[+] Insert element successfully!");
                }
                else
                {
                    dbg_printf("[-] Fail to insert!");
                }
            }
        }
        if (isbankbit)
        {
            dbg_printf("bit %d is Bank bit\n", diff);
            vec_row_bits.push_back(diff);
        }
    }

    if (vec_row_bits.size() == 0)
    {
        dbg_printf("[-] Error. none of bank bits has been detected. abort.");
        abort();
    }
    else
    {
        // Output the obtained information
        dbg_printf(" APPROXY_LATENCY: %d\n", g_approx_latency);
        dbg_printf("=== BANK bits:\n");
        for (int i = 0; i < vec_row_bits.size() - 1; i++)
        {
            dbg_printf("%d, ", vec_row_bits[i]);
        }
        dbg_printf("%d\n", vec_row_bits[vec_row_bits.size() - 1]);
        dbg_printf("[+] the finnally %zu bank function is :\n", bank_fun.size());
        for (auto &row : bank_fun)
        {
            std::sort(row.begin(), row.end());
            dbg_printf("( ");
            for (int i = 0; i < row.size() - 1; i++)
            {
                dbg_printf("%d, ", row[i]);
            }
            dbg_printf("%d )\n", row[row.size() - 1]);
        }
    }
}



int main()
{
    if (SUCCESS != read_config("./output/config/config.ini"))
    {
        return -1;
    }

    // Set random number seed
    srand(time(NULL));

    struct timeval start = {0};
    struct timeval t1 = {0};

    // Size of allocated memory region
    uint64_t mapping_size = 0;
    // Starting address of allocated memory region
    void *mapping = NULL;

    MEASURE_TIME_COST_START(start)

    MEASURE_TIME_COST_START(t1)
    setup_mapping(&mapping_size, &mapping, 0.7);
    dbg_printf("[+] mapping: %p, mapping_size: 0x%lx\n", mapping, mapping_size);

    // Key: physical address; Value: virtual address
    unordered_map<physaddr_t, virtaddr_t> physical_pages;
    store_phy_pages(mapping, mapping_size, physical_pages);
    MEASURE_TIME_COST_END("setup_mapping and store_phy_pages", t1)

    // row bits
    MEASURE_TIME_COST_START(t1)

    // Store row bits
    vector<int> vec_row_bits;
    g_approx_latency = find_threshold(mapping, physical_pages, mapping_size, g_num_reads, g_banks_number_total);
    bankfun_2bit_test(mapping, physical_pages, vec_row_bits);
    MEASURE_TIME_COST_END("bankfun_2bit_test", t1)

  
    munmap(mapping, mapping_size);
    dbg_printf("[+] done!\n");
    fflush(stdout);
}