#include "../include/common.h"
#include "../include/utility.h"
#include <unordered_set>

// Coarse grained Bank function detection
void detect_bankfun_coarsegrained(void *mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages, vector<int> &vec_bank_bits, vector<int> &vec_bank_bits_mult_fun,
                                  vector<vector<int>> &bank_fun, int g_approx_latency, int g_available_length, int g_num_reads)
{
    // Maximum access time for different virtual address pairs in a single round
    double check_time_max[ADDRESSLENGTH];

    // Minimum access time for different virtual address pairs in a single round
    double check_time_min[ADDRESSLENGTH];

    // Average access time for different virtual address pairs in a single round
    double check_time_average[ADDRESSLENGTH];

    int index = 0, error_count = 0, test_count = 10;
    double sum = 0, max = 0, min = 0, current = 0, current_2 = 0;
    int Sum_num = 0;
    // Virtual address pairs
    addrpair_t virt_pair[16];

    dbg_printf("[!] Detecting Bank fun ...start\n");
    int no_pair = 0;
    int diff = 0;
    // SBDR threshold, global variable, manually selected
    // g_approx_latency = 320;
    // Pure row bits detect
    std::unordered_set<int> Pure_row;
    vector<int> Pure_column;

    // In detect_bankfun_coarsegrained function
    for (index = g_available_length - 1; index >= 6; index--)
    {
        // Batch search for virtual address pairs
        bool found_all = true;
        for (test_count = 0; test_count < 16; test_count++)
        {
            if (!(find_virt_pair(mapping, physical_pages, index, virt_pair[test_count])))
            {
                dbg_printf("[-] Can not to find the virt addr pair!\n");
                found_all = false;
                break;
            }
        }
        int time = 0;
        // Only perform timing when all address pairs are found
        if (found_all)
        {
            for (test_count = 0; test_count < 16; test_count++)
            {
                time += get_timing(virt_pair[test_count].first, virt_pair[test_count].second, g_num_reads);
            }
            time /= 16;
            if (time > g_approx_latency)
            {
                Pure_row.insert(index);
            }
        }
    }
    if (Pure_row.size() == 0)
    {
        dbg_printf("[+] No pure bits find! \n");
    }
    else
    {
        dbg_printf("=== The Pure row bit ===\n");
        for (auto bits : Pure_row)
        {
            dbg_printf("%d ", bits);
        }
        dbg_printf("\n");
    }
    // Bank and row bits detection
    // Find bits
    // Control diff and index to be different, index for diff+1->max
    for (diff = g_available_length - 1; diff > 6; diff--)
    {
        // Skip the bits found
        if (Pure_row.find(diff) != Pure_row.end())
        {
            continue;
        }
        if (std::find(vec_bank_bits.begin(), vec_bank_bits.end(), diff) != vec_bank_bits.end())
        {
            // dbg_printf("Skip diff: %d\n", diff);
            continue;
        }
        else
        {
            for (index = diff - 1; index > 5; index--)
            {
                if (Pure_row.find(index) != Pure_row.end())
                {
                    continue;
                }
                if (std::find(vec_bank_bits.begin(), vec_bank_bits.end(), index) != vec_bank_bits.end())
                {
                    // dbg_printf("Skip diff: %d\n", index);
                    continue;
                }
                sum = 0;
                Sum_num = 0;
                error_count = 0;
                // Find 16 different address pairs, each pair differs in index and diff bits
                for (no_pair = 0; no_pair < 16; no_pair++)
                {
                    if (find_virt_pair_2bits(mapping, physical_pages, diff, index, virt_pair[no_pair]))
                    {
                        continue;
                    }
                    else
                    {
                        dbg_printf("[-] Bits %d test No. %d fail to find the virt pair", index, no_pair);
                    }
                }
                // Test start
                max = 0;
                min = 1e10;
                for (int test_count_perbit = 0; test_count_perbit < 16; test_count_perbit++)
                {
                    current_2 = get_timing(virt_pair[test_count_perbit].first, virt_pair[test_count_perbit].second, g_num_reads);
                    sum += current_2;
                    Sum_num++;
                    max = current_2 > max ? current_2 : max;
                    min = current_2 < min ? current_2 : min;
#ifdef DEBUG
                    dbg_printf("keep Bit%d diff, change Bit%d, find pairs %d :first pa: 0x%lx, second pa: 0x%lx, latency: %f\n", diff, index, no_pair,
                               get_phys_addr(virt_pair[test_count].first), get_phys_addr(virt_pair[test_count].second), current_2);
#endif
                }
                check_time_max[index] = max;
                check_time_min[index] = min;
                check_time_average[index] = sum / Sum_num;
                if (check_time_average[index] >= g_approx_latency)
                {

                    dbg_printf("[+] find fun pair: (%d, %d)\n", diff, index);
                    if (merge_element_vector2(bank_fun, diff, index))
                    {
                        vec_bank_bits.push_back(diff);
                        vec_bank_bits.push_back(index);
                        // dbg_printf("[+] Insert element successfully!\n");
                        continue;
                    }
                    else
                    {
                        dbg_printf("[-] Fail to insert!");
                    }
                }
#ifdef DEBUG
                dbg_printf("Row_test result for bit %d and bit %d : %f ~ %f, average: %f\n", diff, index, check_time_min[index], check_time_max[index], check_time_average[index]);
#endif
            }
        }
    }
    // Find the ignored bank bits and the functions without row bits
    dbg_printf("[!] start finding the left bank bits\n");
    vector<int> bank_bits_new;

    for (int i = 0; i < bank_fun.size(); i++)
    {
        if (bank_fun[i].size() >= 2)
        {
            // Find ignored bank bits
            for (index = 0; index < g_available_length; index++)
            {
                // If bit index has been found, go to next
                if (Pure_row.find(index) != Pure_row.end())
                {
                    continue;
                }
                if (std::find(vec_bank_bits.begin(), vec_bank_bits.end(), index) != vec_bank_bits.end())
                    continue;
                else
                {
                    sum = 0;
                    Sum_num = 0;
                    for (no_pair = 0; no_pair < 16; no_pair++)
                        if (find_virt_pair_3bits(mapping, physical_pages, index, bank_fun[i][0], bank_fun[i][1], virt_pair[no_pair]))
                        {
                            Sum_num++;
                            sum += get_timing(virt_pair[no_pair].first, virt_pair[no_pair].second, g_num_reads);
                        }
                        else
                        {
                            dbg_printf("[-] Bits %d test No. %d fail to find the virt pair\n", index, no_pair);
                        }
                    if (sum / Sum_num < g_approx_latency)
                    {
                        vec_bank_bits.push_back(index);
                        bank_bits_new.push_back(index);
                        vec_bank_bits_mult_fun.push_back(index);
                        dbg_printf("[+] Find the new bank bit(must not be row bit) : %d\n", index);
                    }
                    else
                    {
                        if (Pure_row.size() == 0 || Pure_row.find(index) == Pure_row.end())
                        {
                            Pure_column.push_back(index);
                            dbg_printf("[+] Find the Pure column bit : %d\n", index);
                        }
                    }
                }
            }
            // Sort from min to max
            std::sort(vec_bank_bits.begin(), vec_bank_bits.end());
            // Find new bank functions
            for (int j = 0; j < bank_bits_new.size(); j++)
            {
                for (int k = j + 1; k < bank_bits_new.size(); k++)
                {
                    sum = 0;
                    Sum_num = 0;
                    for (no_pair = 0; no_pair < 16; no_pair++)
                    {
                        if (find_virt_pair_4bits(mapping, physical_pages, bank_bits_new[j], bank_bits_new[k], bank_fun[i][0], bank_fun[i][1], virt_pair[no_pair]))
                        {
                            Sum_num++;
                            sum += get_timing(virt_pair[no_pair].first, virt_pair[no_pair].second, g_num_reads);
                        }
                        else
                        {
                            dbg_printf("[-] Bits %d test No. %d fail to find the virt pair", index, no_pair);
                        }
                    }
                    if ((sum / Sum_num) > g_approx_latency)
                    {
                        if (merge_element_vector2(bank_fun, bank_bits_new[j], bank_bits_new[k]))
                        {
                            vec_bank_bits_mult_fun.erase(
                                std::remove(vec_bank_bits_mult_fun.begin(), vec_bank_bits_mult_fun.end(), bank_bits_new[j]),
                                vec_bank_bits_mult_fun.end());

                            vec_bank_bits_mult_fun.erase(
                                std::remove(vec_bank_bits_mult_fun.begin(), vec_bank_bits_mult_fun.end(), bank_bits_new[k]),
                                vec_bank_bits_mult_fun.end());
#ifdef DEBUG
                            dbg_printf("[+] Insert element successfully!\n");
#endif
                        }
                        else
                        {
                            dbg_printf("[-] Fail to insert!\n");
                        }
                    }
                }
            }
            break;
        }
    }
}

// Fine grained bank function detection (find bits in 2 functions)
void detect_bankfun_finegrained(void *mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages, vector<int> &vec_bank_bits, vector<int> &vec_bank_bits_mult_fun,
                                vector<vector<int>> &bank_fun, int g_approx_latency, int g_available_length, int g_num_reads)
{

    dbg_printf("[!] Detecting Bank bit in 2 fun...start\n");
    addrpair_t pair[16];
    vector<int> vec_bank_bits_2_fun(vec_bank_bits_mult_fun);
    if (vec_bank_bits_mult_fun.size() <= 0)
    {
        dbg_printf("[+] No bit in more than 1 bank fun!\n");
    }
    else
    {
        // 2_fun check
        for (int index = 0; index < vec_bank_bits_mult_fun.size(); index++)
        {
            dbg_printf("[!] the bit for detecting whether in 2_fun include: %d\n", vec_bank_bits_mult_fun[index]);
        }
        vector<int> list;
        int SBDR[2];
        SBDR[0] = bank_fun[0][0];
        SBDR[1] = bank_fun[0][bank_fun[0].size() - 1];
        for (int i = 0; i < bank_fun.size(); i++)
        {
            list.push_back(bank_fun[i][0]);
        }
        for (int i = 0; i < list.size(); i++)
            for (int j = i + 1; j < list.size(); j++)
                for (int index = 0; index < vec_bank_bits_2_fun.size(); index++)
                {
#ifdef DEBUG
                    dbg_printf("Judging if the bit %d is in fun %d and fun %d\n", vec_bank_bits_2_fun[index], i, j);
#endif
                    int sum = 0, Sum_num = 0;
                    for (int no_pair = 0; no_pair < 16; no_pair++)
                    {
                        if (find_virt_pair_5bits(mapping, physical_pages, vec_bank_bits_2_fun[index], list[i], list[j], SBDR[0], SBDR[1], pair[no_pair]))
                        {
                            sum += get_timing(pair[no_pair].first, pair[no_pair].second, g_num_reads);
                            Sum_num++;
                        }
                        else
                        {
                            dbg_printf("[-] Bits %d test No. %d fail to find the virt pair", vec_bank_bits_2_fun[index], no_pair);
                        }
                    }
                    if ((sum / Sum_num) > g_approx_latency)
                    {
                        dbg_printf("[+] The bit %d is in fun %d and fun %d\n", vec_bank_bits_2_fun[index], i, j);
                        bank_fun[i].push_back(vec_bank_bits_2_fun[index]);
                        bank_fun[j].push_back(vec_bank_bits_2_fun[index]);
                    }
                }
    }

    // all_bankfun check
    // for (int index = 0; index < vec_bank_bits_mult_fun.size(); index++)
    // {
    //     if(vec_bank_bits_mult_fun[index])
    //     dbg_printf("[!] tDetecting Bank bit in all exiting fun(subchannel) include: %d\n", vec_bank_bits_mult_fun[index]);
    // }
    

    // Print the final message
    if (vec_bank_bits.size() == 0)
    {
        dbg_printf("[-] Error. none of bank bits has been detected. abort.");
        abort();
    }
    else
    {
        // Output the obtained information
        // dbg_printf(" APPROXY_LATENCY_SELECT: %d\n", g_approx_latency);
        dbg_printf("=== BANK bits ===\n");
        for (int i = 0; i < vec_bank_bits.size() - 1; i++)
        {
            if ((i == vec_bank_bits.size() - 1) || (vec_bank_bits[i] != vec_bank_bits[i + 1]))
                dbg_printf("%d, ", vec_bank_bits[i]);
        }
        dbg_printf("%d\n", vec_bank_bits[vec_bank_bits.size() - 1]);
        dbg_printf("=== The %zu bank function ===\n", bank_fun.size());
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


// In merge_bankfun function
// Use bit operations to optimize condition checks
void merge_bankfun(void *mapping, unordered_map<physaddr_t, virtaddr_t> &physical_pages, vector<vector<int>> &bank_fun, int g_banks_number_total, int g_approx_latency, int g_num_reads)
{
    dbg_printf("[!] Start to merge the surplus fun\n");
    int bank_size = bank_fun.size();
    addrpair_t virt_pair;
    // Use bit mask to track processed functions
    uint64_t processed_mask = 0;

    for (int detected = 0; detected < bank_size; detected++)
    {
        if (processed_mask & (1ULL << detected))
            continue;

        for (int front = 0; front < bank_size - 1; front++)
        {
            if ((front == detected) || (processed_mask & (1ULL << front)))
                continue;

            for (int behind = front + 1; behind < bank_size; behind++)
            {
                if ((behind == detected) || (processed_mask & (1ULL << behind)))
                    continue;

                int sum = 0, Sum_num = 0;
                for (int num_addr = 0; num_addr < 16; num_addr++)
                {
                    if (find_virt_pair_4bits(mapping, physical_pages, bank_fun[detected][bank_fun[detected].size() - 1], bank_fun[detected][bank_fun[detected].size() - 2], bank_fun[front][bank_fun[front].size() - 1],
                                             bank_fun[behind][bank_fun[behind].size() - 1], virt_pair))
                    {
                        sum += get_timing(virt_pair.first, virt_pair.second, g_num_reads);
                        Sum_num++;
                    }
                    else
                    {
                        dbg_printf("[-] Merge test : whether fun %d in fun %d and fun %d fail to find the virt pair\n", detected, front, behind);
                    }
                }
                // For debug
                // dbg_printf("Merge test : whether fun %d in fun %d and fun %d ,latency: %d\n",detected,front,behind,sum/Sum_num);
                if ((sum / Sum_num) > g_approx_latency)
                {
                    dbg_printf("[+]Fun %d is in fun %d and fun %d !!!\n", detected, front, behind);
                }
            }
        }

        // Mark as processed
        processed_mask |= (1ULL << detected);
    }

    if (1UL << bank_size != g_banks_number_total)
    {
        dbg_printf("[-] Fail to merge the surplus fun!\n");
    }
    else
    {
        dbg_printf("=== The finnally %zu bank function ===\n", bank_fun.size());
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
