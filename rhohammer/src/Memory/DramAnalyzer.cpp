#include "Memory/DramAnalyzer.hpp"
#include "Memory/DRAMAddr.hpp"
#include "Utilities/CustomRandom.hpp"
#include "Utilities/Helper.hpp"
#include "Fuzzer/CodeJitter.hpp"
#include <cassert>
#include <unordered_set>
#include <iostream>
#include "Memory/Memory.hpp"
void DramAnalyzer::set_sync_ref_threshold(uint64_t ref_threshold)
{
  this->ref_threshold = ref_threshold;
}

void DramAnalyzer::find_threshold()
{
  assert(threshold == (size_t)-1 && "find_threshold() has not been called yet.");
  Logger::log_info("Generating histogram data to find bank conflict threshold.");
  constexpr size_t HISTOGRAM_MAX_VALUE = 4096;
  constexpr size_t HISTOGRAM_ENTRIES = 16384;

  std::vector<size_t> histogram(HISTOGRAM_MAX_VALUE, 0);
  size_t num_entries = 0;

  while (num_entries < HISTOGRAM_ENTRIES)
  {
    auto a1 = get_random_address();
    auto a2 = get_random_address();
    auto time = (size_t)measure_time(a1, a2);
    if (time < histogram.size())
    {
      histogram[time]++;
      num_entries++;
    }
  }

  // Find threshold such that HISTOGRAM_ENTRIES / NUM_BANKS times are above it.
  threshold = HISTOGRAM_MAX_VALUE - 1;
  size_t num_entries_above_threshold = 0;
  while (num_entries_above_threshold < HISTOGRAM_ENTRIES / NUM_BANKS)
  {
    num_entries_above_threshold += histogram[threshold];
    threshold--;
    assert(threshold > 0);
  }

  Logger::log_info(format_string("Found bank conflict threshold to be %zu.", threshold));
}

size_t DramAnalyzer::find_sync_ref_threshold()
{
  Logger::log_info("Finding sync REF threshold using using jitted code.");
  CodeJitter jitter;

  // Idea: Start with a threshold that is definitely too high, wait until there are no overruns (i.e., # ACTs is the
  // maximum, meaning no REF is detected) in any of the three sync attempts.

  // Prepare aggressors.
  constexpr size_t NUM_AGGRS = 32;
  std::vector<volatile char *> aggressors;
  for (size_t i = 0; i < NUM_AGGRS; i++)
  {
    aggressors.push_back((volatile char *)DRAMAddr(0, 2 * i, 0).to_virt());
  }

  // Prepare REF sync address.
  DRAMAddr initial_sync_addr(1, 0, 0);
  // NOTE: This needs to be in the same rank, but a different bank w.r.t. the aggressors.

  for (size_t sync_ref_threshold = 4000; sync_ref_threshold >= 800; sync_ref_threshold -= 200)
  {
    size_t total_synced_refs = 0;
    size_t missed_refs = 0;

    jitter.jit_ref_sync(FLUSHING_STRATEGY::EARLIEST_POSSIBLE, FENCING_STRATEGY::OMIT_FENCING,
                        aggressors, initial_sync_addr, sync_ref_threshold);

    // 32 iterations.
    for (size_t i = 0; i < 32; i++)
    {
      RefSyncData data;
      jitter.run_ref_sync(&data);
      if (data.first_sync_act_count == CodeJitter::SYNC_REF_NUM_AGGRS)
      {
        missed_refs++;
      }
      if (data.second_sync_act_count == CodeJitter::SYNC_REF_NUM_AGGRS)
      {
        missed_refs++;
      }
      if (data.last_sync_act_count == CodeJitter::SYNC_REF_NUM_AGGRS)
      {
        missed_refs++;
      }
      total_synced_refs += 3;
    }

    jitter.cleanup();

    Logger::log_data(format_string("sync_ref_threshold = %zu, missed_refs = %zu", sync_ref_threshold, missed_refs));

    // Allow one missed REF due to noise.
    if (missed_refs <= 1)
    {
      // Add a margin for safety.
      sync_ref_threshold -= 100;
      Logger::log_info(format_string("Choosing sync_ref_threshold = %zu.", sync_ref_threshold));
      return sync_ref_threshold;
    }
  }

  Logger::log_error("Error: Could not determine sync_ref_threshold.");
  exit(EXIT_FAILURE);
}

void DramAnalyzer::find_bank_conflicts()
{
  size_t nr_banks_cur = 0;
  auto num_banks = (size_t)NUM_BANKS;
  int remaining_tries = num_banks * 1024; // experimentally determined, may be unprecise
  while (nr_banks_cur < num_banks && remaining_tries > 0)
  {
  reset:
    remaining_tries--;
    auto a1 = get_random_address();
    auto a2 = get_random_address();
    auto ret1 = measure_time(a1, a2);
    auto ret2 = measure_time(a1, a2);

    if ((ret1 > threshold) && (ret2 > threshold))
    {
      bool all_banks_set = true;
      for (size_t i = 0; i < NUM_BANKS; i++)
      {
        if (banks.at(i).empty())
        {
          all_banks_set = false;
        }
        else
        {
          auto bank = banks.at(i); 
          ret1 = measure_time(a1, bank[0]);
          ret2 = measure_time(a2, bank[0]);
          if ((ret1 > threshold) || (ret2 > threshold))
          {
            // possibly noise if only exactly one is true,
            // i.e., (ret1 > threshold) or (ret2 > threshold)
            goto reset;
          }
        }
      }

      // stop if we already determined addresses for each bank
      if (all_banks_set)
        return;

      // store addresses found for each bank
      assert(banks.at(nr_banks_cur).empty() && "Bank not empty");
      banks.at(nr_banks_cur).push_back(a1);
      banks.at(nr_banks_cur).push_back(a2);
      nr_banks_cur++;
    }
    if (remaining_tries == 0)
    {
      Logger::log_error(format_string(
          "Could not find conflicting address sets. Is the number of banks (%zu) defined correctly?",
          NUM_BANKS));
      exit(1);
    }
  }

  Logger::log_info("Found bank conflicts.");
}

void DramAnalyzer::find_targets(std::vector<volatile char *> &target_bank)
{
  // create an unordered set of the addresses in the target bank for a quick lookup
  // std::unordered_set<volatile char*> tmp; tmp.insert(target_bank.begin(), target_bank.end());
  std::unordered_set<volatile char *> tmp(target_bank.begin(), target_bank.end());
  target_bank.clear();
  size_t num_repetitions = 5;
  while (tmp.size() < 10)
  {
    auto a1 = start_address + (dist(cr.gen) % (MEM_SIZE / 64)) * 64;
    if (tmp.count(a1) > 0)
      continue;
    uint64_t cumulative_times = 0;
    for (size_t i = 0; i < num_repetitions; i++)
    {
      for (const auto &addr : tmp)
      {
        cumulative_times += measure_time(a1, addr);
      }
    }
    cumulative_times /= num_repetitions;
    if ((cumulative_times / tmp.size()) > BK_CONF_THRESH)
    {
      tmp.insert(a1);
      target_bank.push_back(a1);
    }
  }
}

DramAnalyzer::DramAnalyzer(volatile char *target)
    : start_address(target)
{
  cr = CustomRandom();
  dist = std::uniform_int_distribution<>(0, std::numeric_limits<int>::max());
  banks = std::vector<std::vector<volatile char *>>(NUM_BANKS, std::vector<volatile char *>());
}

size_t DramAnalyzer::count_acts_per_ref()
{
  auto exp_cfg = ExperimentConfig();
  exp_cfg.exec_mode = execution_mode::ALTERNATING;
  exp_cfg.num_measurement_reps = 10'000;
  exp_cfg.num_measurement_rounds = 10;
  exp_cfg.num_accesses_per_round = 2;
  exp_cfg.num_sync_rows = 32;
  exp_cfg.row_distance = 4;
  // exp_cfg.min_ref_thresh = 1000;
  exp_cfg.min_ref_thresh = 1260;
  exp_cfg.row_origin_same_bg = true;
  exp_cfg.row_origin_same_bk = true;
  return count_acts_per_ref(exp_cfg);
}

std::vector<uint64_t> DramAnalyzer::get_nth_highest_values(size_t N, std::vector<uint64_t> &values)
{
  // compute frequencies
  std::map<uint64_t, std::size_t> freq;
  for (const auto &word : values)
  {
    freq[word]++;
  }

  // map -> vector
  std::vector<std::pair<uint64_t, uint64_t>> freq_as_vec(freq.begin(), freq.end());

  // partially sort the histogram based on the frequency count
  std::partial_sort(freq_as_vec.begin(), freq_as_vec.end(), freq_as_vec.begin() + N,
                    [](const auto &lhs, const auto &rhs)
                    {
                      return lhs.second > rhs.second;
                    });

  // build the result vector containing the top-N values only
  std::vector<uint64_t> result;
  for (size_t i = 0; i < N; ++i)
  {
    // std::cout << freq_as_vec[i].first << " " << freq_as_vec[i].second << "x" << std::endl;
    result.push_back(freq_as_vec[i].first);
  }

  return result;
}

size_t DramAnalyzer::count_acts_per_ref([[maybe_unused]] const ExperimentConfig &exp_cfg)
{
  Logger::log_info("Determining the number of activations per REF(sb|ab) interval...");

  size_t num_tries = 0;
  if (false)
  {
  retry:
    Logger::log_info(format_string("Trying it again.. try %d", num_tries));
    num_tries++;
  }

  // measurement parameters
  const size_t NUM_ADDRS = 2;
  size_t NUM_REPS = 5'000'000;

  std::vector<uint64_t> timing_values;
  timing_values.resize(NUM_REPS, 0);

  std::vector<volatile char *> addrs;
  for (size_t v = 0; v < NUM_ADDRS; ++v)
  {
    addrs.push_back((volatile char *)DRAMAddr(1, 0, 0, 0, v * 2, 0).to_virt());
  }

  uint64_t tmp_before;
  uint64_t tmp_after;
  uint64_t cur;
  uint64_t cnt_higher_th = 0;
  uint64_t counted_reps = 0;

  // FILE* f2 = fopen("times2.txt", "w");
  sched_yield();
  for (size_t i = 0; i < NUM_REPS; i++)
  {
    sfence();
    tmp_before = rdtscp();
    lfence();
    for (size_t k = 0; k < NUM_ADDRS; ++k)
    {
      *addrs[k];
      clflushopt(addrs[k]);
    }
    lfence();
    tmp_after = rdtscp();
    for (size_t k = 0; k < NUM_ADDRS; ++k)
    {
      // clflushopt(addrs[k]);
    }
    cur = (tmp_after - tmp_before);
    timing_values[i] = cur;
    // fprintf(f2, "%ld\n", cur);
  }
  // fclose(f2);

  //
  // STEP 1: Figure out the REF threshold by taking the average of the two
  // peaks we can observe in timing accessing two same-<bg, bk> addresses.
  //
  auto min_distance = 150; // cycles
  auto vec = get_nth_highest_values(5, timing_values);
  uint64_t highest = vec[0];
  uint64_t second_highest;
  bool second_highest_found = false;
  for (std::size_t i = 1; i < vec.size(); ++i)
  {
    auto candidate = vec[i];
    if (candidate < (highest - min_distance) || candidate > (highest + min_distance))
    {
      second_highest = vec[i];
      second_highest_found = true;
    }
    else
    {
      highest = (highest + vec[i]) / 2;
    }
    if (second_highest_found)
    {
      if (candidate < (second_highest - min_distance) || candidate > (second_highest + min_distance))
      {
        second_highest = (second_highest + vec[i]) / 2;
      }
      else
      {
        break;
      }
    }
  }
  ref_threshold = (highest + second_highest) / 2;
  std::cout << std::dec << highest << " | " << second_highest << " => " << ref_threshold << std::endl;

  // sometimes the measurement leads to weird/very high results, in this case
  // we just repeat
  if (ref_threshold > 1500)
    goto retry;

  //
  // STEP 2: Use the threshold to determine the number of activations we can do
  // in a REF interval, i.e., between two consecutive REF commands.
  //

  std::vector<uint64_t> act_cnt;
  act_cnt.resize(NUM_REPS, 0);

  // FILE* f2 = fopen("times3.txt", "w");
  cnt_higher_th = 0;
  counted_reps = 0;

  sched_yield();
  for (size_t i = 0; i < NUM_REPS; i++)
  {
    sfence();
    tmp_before = rdtscp();
    lfence();
    for (size_t k = 0; k < NUM_ADDRS; ++k)
    {
      *addrs[k];
      clflushopt(addrs[k]);
    }
    lfence();
    tmp_after = rdtscp();
    if ((tmp_after - tmp_before) > ref_threshold)
    {
      act_cnt[cnt_higher_th] = (counted_reps * NUM_ADDRS);
      cnt_higher_th++;
      // fprintf(f2, "%ld, %ld\n", counted_reps*NUM_ADDRS, cur);
      counted_reps = 0;
    }
    else
    {
      counted_reps++;
    }
  }
  // fclose(f2);

  act_cnt.resize(cnt_higher_th - 1);
  auto acts_per_ref = get_nth_highest_values(5, act_cnt);
  for (std::size_t i = 0; i < acts_per_ref.size(); ++i)
  {
    if (acts_per_ref[i] > 10)
      return acts_per_ref[i];
  }

  Logger::log_error("Could not determine reasonable ACTs/REF value. Using default (30).");
  Logger::log_data(format_string("REF threshold: %ld", ref_threshold));
  Logger::log_data(format_string("ACTs/REF (best): %ld", acts_per_ref[0]));

  return 30;
}

size_t DramAnalyzer::get_ref_threshold() const
{
  return ref_threshold;
}

size_t inline DramAnalyzer::measure_time(volatile char *a1, volatile char *a2)
{
  int error_cnt = 0;
  int test_round = 6;
  uint64_t res = 0;
  uint64_t number_of_reads = 0;
  size_t sum_res = 0;
  for (int i = 0; i < test_round; i++)
  {
    res = 0;
    number_of_reads = 500;
    volatile size_t *f = (volatile size_t *)a1;
    volatile size_t *s = (volatile size_t *)a2;
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
      asm volatile("clflushopt (%0)"
                   :
                   : "r"(s)
                   : "memory");
      asm volatile("mfence" ::: "memory");
      if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
      {
        perror("clock_gettime start");
        return 1;
      }
      *f;
      *s;
      if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
      {
        perror("clock_gettime end");
        return 1;
      }
      asm volatile("mfence" ::: "memory");
      res += (end.tv_nsec - start.tv_nsec);
    }
    res /= (500);
    if (res > 300)
    {
      error_cnt++;
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
  return sum_res / (test_round - error_cnt);
}

bool DramAnalyzer::check_sync_ref_threshold(size_t sync_ref_threshold)
{
  Logger::log_info(format_string("Checking sync REF threshold (%zu cycles) using using jitted code.", sync_ref_threshold));
  CodeJitter jitter;

  // The second sync should be independent of the number of aggressors. Take the average of the averages to check this.
  size_t second_sync_avg_min = -1;
  size_t second_sync_avg_max = 0;

  for (size_t num_aggrs = 0; num_aggrs <= 40; num_aggrs += 5)
  {
    // Prepare aggressors.
    std::vector<volatile char *> aggressors;
    for (size_t i = 0; i < num_aggrs; i++)
    {
      aggressors.push_back((volatile char *)DRAMAddr(0, 2 * i, 0).to_virt());
    }

    // Prepare REF sync address.
    DRAMAddr initial_sync_addr(1, 0, 0);
    // NOTE: This needs to be in the same rank, but a different bank w.r.t. the aggressors.

    jitter.jit_ref_sync(FLUSHING_STRATEGY::EARLIEST_POSSIBLE, FENCING_STRATEGY::OMIT_FENCING,
                        aggressors, initial_sync_addr, sync_ref_threshold);

    size_t total_runs = 0;
    size_t missed_refs = 0;

    size_t second_tsc_sum = 0;
    size_t last_tsc_sum = 0;

    uint32_t second_tsc_min = -1;
    uint32_t second_tsc_max = 0;
    uint32_t last_tsc_min = -1;
    uint32_t last_tsc_max = 0;

    // 32 iterations.
    for (size_t i = 0; i < 32; i++)
    {
      RefSyncData data;
      jitter.run_ref_sync(&data);
      if (data.first_sync_act_count == CodeJitter::SYNC_REF_NUM_AGGRS)
      {
        missed_refs++;
      }
      if (data.second_sync_act_count == CodeJitter::SYNC_REF_NUM_AGGRS)
      {
        missed_refs++;
      }
      if (data.last_sync_act_count == CodeJitter::SYNC_REF_NUM_AGGRS)
      {
        missed_refs++;
      }

      total_runs++;
      second_tsc_sum += data.second_sync_tsc_delta;
      last_tsc_sum += data.last_sync_tsc_delta;
      second_tsc_min = std::min(second_tsc_min, data.second_sync_tsc_delta);
      second_tsc_max = std::max(second_tsc_max, data.second_sync_tsc_delta);
      last_tsc_min = std::min(last_tsc_min, data.last_sync_tsc_delta);
      last_tsc_max = std::max(last_tsc_max, data.last_sync_tsc_delta);
    }

    jitter.cleanup();

    auto second_tsc_avg = second_tsc_sum / total_runs;
    auto last_tsc_avg = last_tsc_sum / total_runs;

    second_sync_avg_min = std::min(second_sync_avg_min, second_tsc_avg);
    second_sync_avg_max = std::max(second_sync_avg_max, second_tsc_avg);

    Logger::log_data(format_string(
        "num aggressors = %2zu, missed REFs = %2zu, second sync cycles (min:avg:max) = %5u:%5u:%5u, last sync cycles (avg) = %5u:%5u:%5u",
        num_aggrs, missed_refs, second_tsc_min, second_tsc_avg, second_tsc_max, last_tsc_min, last_tsc_avg, last_tsc_max));

    // Abort if there is more than one missed REF.
    if (missed_refs > 1)
    {
      Logger::log_error(format_string("Error: Too many missed REFs (%zu)!", missed_refs));
      // exit(EXIT_FAILURE);
      return false;
    }
  }

  Logger::log_info(format_string("Second sync cycle averages are between %zu and %zu.", second_sync_avg_min, second_sync_avg_max));
  if (second_sync_avg_max > 1.2 * second_sync_avg_min)
  {
    Logger::log_error("Second sync cycle averages are spread too widely.");
    // exit(EXIT_FAILURE);
    return false;
  }
  return true;
}

volatile char *DramAnalyzer::get_random_address() const
{
  static std::random_device rd;
  static std::default_random_engine gen(rd());
  static std::uniform_int_distribution<size_t> dist(0, MEM_SIZE - 1);
  return start_address + dist(gen);
}