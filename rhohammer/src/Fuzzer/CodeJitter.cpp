#include <iostream>
#include <ctime>
#include <iomanip>

#include "Fuzzer/CodeJitter.hpp"
#include "Utilities/AsmPrimitives.hpp"
#include "Utilities/Pagemap.hpp"

#define MEASURE_TIME (1)

CodeJitter::CodeJitter()
    : flushing_strategy(FLUSHING_STRATEGY::EARLIEST_POSSIBLE),
      fencing_strategy(FENCING_STRATEGY::LATEST_POSSIBLE),
      total_activations(5000000)
{
#ifdef ENABLE_JITTING
  logger = new asmjit::StringLogger;
#endif
}

CodeJitter::~CodeJitter()
{
  cleanup();
}

void CodeJitter::cleanup()
{
#ifdef ENABLE_JITTING
  if (fn != nullptr)
  {
    runtime.release(fn);
    fn = nullptr;
  }
  if (fn_ref_sync != nullptr)
  {
    runtime.release(fn_ref_sync);
    fn_ref_sync = nullptr;
  }
  if (logger != nullptr)
  {
    delete logger;
    logger = nullptr;
  }
#endif
}

size_t CodeJitter::hammer_pattern(FuzzingParameterSet &fuzzing_parameters, bool verbose)
{
  if (fn == nullptr)
  {
    Logger::log_error("Skipping hammering pattern as pattern could not be created successfully.");
    return -1;
  }
  if (verbose)
    Logger::log_info("Hammering the last generated pattern.");

  assert(fn != nullptr && "jitting hammering code failed!");
  size_t total_sync_acts = 0;
  HammeringData data{};

  total_sync_acts = fn(&data);
  // while (true) {
  //   (void)fn();
  // }

  if (verbose)
  {
    Logger::log_data(format_string("#Total sync_acts: %d", total_sync_acts));
    const auto total_acts_pattern = fuzzing_parameters.get_total_acts_pattern();
    const auto pattern_rounds = fuzzing_parameters.get_hammering_total_num_activations() / total_acts_pattern;
    auto num_synced_refs = pattern_rounds;
    // as we sync after each pattern execution, #pattern reps equals to #synced REFs
    Logger::log_data(format_string("#Number of pattern reps while hammering: %d", pattern_rounds));
    Logger::log_data(format_string("avg_acts_per_sync: %d", total_sync_acts / num_synced_refs));
    Logger::log_data(format_string("Number of total synced REFs (est.): %d", num_synced_refs));
    // Print total ACTs + TSC delta.
    Logger::log_data(format_string("ACT DATA: %llu ACTs, %llu cycles", data.total_acts, data.tsc_delta));
  }

  return total_sync_acts;
}

size_t CodeJitter::get_next_sync_rows_idx()
{
  return sync_rows_idx++ % (sync_rows_size - 1);
};

void CodeJitter::jit_strict(FuzzingParameterSet &fuzzing_parameters,
                            FLUSHING_STRATEGY flushing,
                            FENCING_STRATEGY fencing,
                            int total_num_activations,
                            const std::vector<volatile char *> &aggressor_pairs,
                            DRAMAddr syn_rows,
                            size_t ref_threshold)
{

  // this is used by hammer_pattern but only for some stats calculations
  // this->pattern_sync_each_ref = false;
  this->flushing_strategy = flushing;
  this->fencing_strategy = fencing;
  this->total_activations = total_num_activations;
  [[maybe_unused]] const int num_acts_per_trefi = fuzzing_parameters.get_num_activations_per_t_refi();

  // some sanity checks
  if (fn != nullptr)
  {
    Logger::log_error(
        "Function pointer is not NULL, cannot continue jitting code without leaking memory. Did you forget to call cleanup() before?");
    exit(1);
  }

  asmjit::CodeHolder code;
  code.init(runtime.environment());
  code.setLogger(logger);
  asmjit::x86::Assembler assembler(&code);

  asmjit::Label for_begin = assembler.newLabel();
  asmjit::Label for_end = assembler.newLabel();

  assembler.push(asmjit::x86::r12);
  assembler.push(asmjit::x86::r13);
  assembler.push(asmjit::x86::r14);
  assembler.push(asmjit::x86::r15);

  // Move pointer to struct HammeringData to %r12.
  assembler.mov(asmjit::x86::r12, asmjit::x86::rdi);

  // ==== here start's the actual program ====================================================
  // ------- part 1: flush the row used for hammering ---------------------------------------------------------------------------------
  // flush the row pairs used in hammering
  // DRAMAddr for_flush = syn_rows;
  // for (size_t i = 0; i < SYNC_REF_NUM_AGGRS; i++)
  // {
  //   auto current = for_flush.to_virt();
  //   assembler.mov(asmjit::x86::rax, (uint64_t)current);
  //   assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
  //   assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
  //   for_flush.add_inplace(0, 1, 0);
  //   // std::cout << "current_aggr: " << current_aggr.get_row() << std::endl
  // }
  // for (auto *aggr : aggressor_pairs)
  // {
  //   auto cur_addr = (uint64_t)aggr;
  //   assembler.mov(asmjit::x86::rax, cur_addr);
  //   assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
  //   assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
  // }
  // assembler.mfence();
  // ------- part 2: perform hammering ---------------------------------------------------------------------------------

  // Time the start. Move full TSC to %r13.
  assembler.rdtscp();
  assembler.mov(asmjit::x86::r13d, asmjit::x86::edx);
  assembler.shl(asmjit::x86::r13, 32);
  assembler.or_(asmjit::x86::r13, asmjit::x86::rax);

  // Start counting ACTs now.

  // initialize variables
  assembler.mov(asmjit::x86::rsi, total_num_activations);
  assembler.mov(asmjit::x86::edx, 0); // num activations counter

  assembler.bind(for_begin);
  assembler.cmp(asmjit::x86::rsi, 0);
  assembler.jle(for_end);

  // a map to keep track of aggressors that have been accessed before and need a fence before their next access
  std::unordered_map<uint64_t, bool> accessed_before;

  size_t cnt_total_activations = 0;
  // std::cout << "ASMjit run" << std::endl;
  sync_ref_nonrepeating(syn_rows, ref_threshold, assembler);
  // hammer each aggressor once
  for (auto *aggr : aggressor_pairs)
  {
    auto cur_addr = (uint64_t)aggr;
    if (accessed_before[cur_addr])
    {
      // flush
      if (flushing == FLUSHING_STRATEGY::LATEST_POSSIBLE)
      {
        assembler.mov(asmjit::x86::rax, cur_addr);
        assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
        accessed_before[cur_addr] = false;
      }
      // fence to ensure flushing finished and defined order of aggressors is guaranteed
      if (fencing == FENCING_STRATEGY::LATEST_POSSIBLE)
      {
        // std::cout<<222<<std::endl;
        assembler.mfence();
        accessed_before[cur_addr] = false;
      }
    }

    // hammer
    assembler.mov(asmjit::x86::rax, cur_addr);
    // assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
    assembler.prefetchnta(asmjit::x86::ptr(asmjit::x86::rax)); // prefetchnta need a memory address as input, can not be a direct value
    accessed_before[cur_addr] = true;
    assembler.dec(asmjit::x86::rsi);
    assembler.inc(asmjit::x86::edx);
    cnt_total_activations++;

    // flush
    if (flushing == FLUSHING_STRATEGY::EARLIEST_POSSIBLE)
    {
      // std::cout<<111<<std::endl;
      // assembler.mov(asmjit::x86::rax, cur_addr);
      assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
    }
    // for prefetch instruction fencing
    for (int i = 0; i < 330; i++)
    {
      assembler.nop();
    }
    // if(cnt_total_activations%num_acts_per_trefi==0){
    //   //syn when meet act_per_trefi
    //   sync_ref_nonrepeating(syn_rows,ref_threshold, a);
    // }
  }

  // fences -> ensure that aggressors are not interleaved, i.e., we access aggressors always in same order
  if (fencing != FENCING_STRATEGY::OMIT_FENCING)
  {
    assembler.mfence();
  }

  assembler.jmp(for_begin);
  assembler.bind(for_end);

  // Move ACT count to %r14.
  assembler.mov(asmjit::x86::r14d, asmjit::x86::edx);
  // Time the end.
  assembler.rdtscp();
  assembler.shl(asmjit::x86::rdx, 32);
  assembler.or_(asmjit::x86::rdx, asmjit::x86::rax);
  assembler.sub(asmjit::x86::rdx, asmjit::x86::r13);
  // TSC delta is now in %rdx. Store it into the struct.

  // Store data into the struct pointed to by %r12.
  assembler.mov(asmjit::x86::ptr(asmjit::x86::r12, offsetof(HammeringData, tsc_delta)), asmjit::x86::rdx);
  assembler.mov(asmjit::x86::ptr(asmjit::x86::r12, offsetof(HammeringData, total_acts)), asmjit::x86::r14);

  // Move ACT count back to %edx.
  assembler.mov(asmjit::x86::edx, asmjit::x86::r14d);

  assembler.pop(asmjit::x86::r15);
  assembler.pop(asmjit::x86::r14);
  assembler.pop(asmjit::x86::r13);
  assembler.pop(asmjit::x86::r12);

  // now move our counter for no. of activations in the end of interval sync. to the 1st output register %eax
  assembler.mov(asmjit::x86::eax, asmjit::x86::edx);
  assembler.ret(); // this is ESSENTIAL otherwise execution of jitted code creates a segfault

  // add the generated code to the runtime.
  asmjit::Error err = runtime.add(&fn, &code);
  if (err)
    throw std::runtime_error("[-] Error occurred while jitting code. Aborting execution!");

  FILE *log_file = fopen("asmjit_output.log", "a");
  if (log_file)
  {
    fprintf(log_file, "\n\n=== New JIT Session ===\n");
    fprintf(log_file, "Time: %s\n", "20250116");
    if (logger != nullptr)
    {
      fprintf(log_file, "[DEBUG] asmjit logger content:\n%s\n", logger->data());
      Logger::log_info("Assembly code appended to asmjit_output.log file");
    }
    else
    {
      fprintf(log_file, "[DEBUG] asmjit logger is null\n");
      Logger::log_info("asmjit logger is null, skipping logger content");
    }
    fclose(log_file);
  }
  else
  {
    Logger::log_error("Cannot open log file");
  }
}

[[maybe_unused]] void CodeJitter::wait_for_user_input()
{
  // TODO: move this into a helper class
  do
  {
    std::cout << '\n'
              << "Are you sure you want to hammer innocent rows? "
                 "Press any key to continue..."
              << std::endl;
  } while (std::cin.get() != '\n');
}

#pragma GCC push_options
#pragma GCC optimize("unroll-loops")

void CodeJitter::sync_ref_unjitted(const std::vector<volatile char *> &sync_rows,
                                   synchronization_stats &sync_stats,
                                   size_t ref_threshold,
                                   [[maybe_unused]] size_t sync_rounds_max) const
{
  sync_stats.num_sync_rounds++;
  // std::cout<<"The value of REFthreshold: "<<ref_threshold<<std::endl;
  //[[maybe_unused]] const size_t sync_rows_max = sync_rows.size();
  // std::cout<<"Sync_row.size:  "<<sync_rows.size()<<std::endl;
  size_t sync_cnt = 0;
  uint64_t after;
  uint64_t before = rdtscp();
  do
  {
    clflushopt(sync_rows[sync_cnt]);
    *sync_rows[sync_cnt++];
    after = rdtscp();
    if ((after - before) > ref_threshold)
      break;
    before = after;
  } while (sync_cnt < 256);
  sync_stats.num_sync_acts += sync_cnt;
}

#pragma GCC pop_options
#pragma GCC push_options
#pragma GCC optimize("unroll-loops")
void CodeJitter::hammer_pattern_unjitted(FuzzingParameterSet &fuzzing_parameters,
                                         bool verbose,
                                         [[maybe_unused]] FLUSHING_STRATEGY flushing,
                                         [[maybe_unused]] FENCING_STRATEGY fencing,
                                         int total_num_activations,
                                         const std::vector<volatile char *> &aggressor_pairs,
                                         const std::vector<volatile char *> &sync_rows,
                                         size_t ref_threshold)
{

  if (verbose)
  {
    Logger::log_debug("CodeJitter::hammer_pattern_unjitted stats:");
    Logger::log_data(format_string("#aggressor pairs: %lu", aggressor_pairs.size()));
    Logger::log_data(format_string("#sync rows: %lu", sync_rows.size()));
    Logger::log_data(format_string("num_acts_per_trefi: %d\n", fuzzing_parameters.get_num_activations_per_t_refi()));
  }

  // if (flushing != FLUSHING_STRATEGY::EARLIEST_POSSIBLE)
  //   throw std::runtime_error("[-] FLUSHING_STRATEGY must be EARLIEST_POSSIBLE");
  // if (fencing != FENCING_STRATEGY::OMIT_FENCING)
  //   throw std::runtime_error(" [-] FENCING_STRATEGY must be OMIT_FENCING");

  // Initialize counter (using L1D miss as an example)
  // Intel L1D cache replacement event code example
  // PerfCounter l1d_misses(PERF_TYPE_HW_CACHE, (PERF_COUNT_HW_CACHE_L1D |
  //                                             (PERF_COUNT_HW_CACHE_OP_READ << 8) |
  //                                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)));
  total_num_activations *= MULTI_BANK;
  // int total_activations = total_num_activations;
  //  flush all sync rows but keep array holding addresses cached
  for (size_t i = 0; i < sync_rows.size(); ++i)
  {
    *sync_rows[i];
    clflushopt(sync_rows[i]);
  }
  // flush all aggressor rows but keep array holding addresses cached
  for (size_t i = 0; i < aggressor_pairs.size(); ++i)
  {
    *aggressor_pairs[i];
    clflushopt(aggressor_pairs[i]);
    //    auto paddr = pagemap::vaddr2paddr((uint64_t)aggressor_pairs[i]);
    //    std::cout << std::setfill('0') << std::setw(4) << std::dec << i
    //              << std::hex << "  0x" << (uint64_t)aggressor_pairs[i]
    //              << std::hex << "  0x" << paddr
    //              << "\n";
  }

  // make sure flushing finished before we start
  sfence();

  [[maybe_unused]] const int num_acts_per_trefi = fuzzing_parameters.get_num_activations_per_t_refi();
  const size_t NUM_AGG_PAIRS = aggressor_pairs.size();
  synchronization_stats sync_stats{.num_sync_acts = 0, .num_sync_rounds = 0};

  std::vector<uint64_t> before_sync_tscs;
  std::vector<uint64_t> after_sync_tscs;
  before_sync_tscs.reserve(128 * 1024);
  after_sync_tscs.reserve(128 * 1024);

  lfence();
  size_t agg_idx = 0;
  const size_t sync_rounds_max_original = (num_acts_per_trefi / 2);
  size_t sync_rounds_max = sync_rounds_max_original;
  // PerfCounter cycles(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  // PerfCounter l1d_misses(PERF_TYPE_HW_CACHE, (PERF_COUNT_HW_CACHE_L1D |
  //                                             (PERF_COUNT_HW_CACHE_OP_READ << 8) |
  //                                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)));
  // l1d_misses.start();
  while (total_num_activations > 0)
  {
    // Use hardware random number and timestamp to create chaos state
    uint64_t rand_state;
    asm volatile("rdrand %0" : "=r"(rand_state));
    uint64_t tsc = rdtscp();

    // Create 12 different execution paths
    void *jump_targets[] = {
        &&path0, &&path1, &&path2, &&path3,
        &&path4, &&path5, &&path6, &&path7,
        &&path8, &&path9, &&path10, &&path11};

    // Use multiple random sources to generate jump index
    size_t idx = ((rand_state ^ tsc) ^ ((rand_state >> 32) * (tsc >> 32))) & 15;
    idx = (idx >= 12) ? (idx - 4) : idx; // Ensure index is within 0-11 range

    // Select different sync parameters based on different paths
    [[maybe_unused]] size_t current_sync_rounds;
    switch (idx)
    {
    case 0:
      current_sync_rounds = sync_rounds_max / 4;
      break;
    case 1:
      current_sync_rounds = sync_rounds_max / 3;
      break;
    case 2:
      current_sync_rounds = sync_rounds_max / 2;
      break;
    case 3:
      current_sync_rounds = sync_rounds_max * 3 / 4;
      break;
    case 4:
      current_sync_rounds = sync_rounds_max;
      break;
    case 5:
      current_sync_rounds = sync_rounds_max * 5 / 4;
      break;
    case 6:
      current_sync_rounds = sync_rounds_max * 3 / 2;
      break;
    case 7:
      current_sync_rounds = sync_rounds_max * 7 / 4;
      break;
    case 8:
      current_sync_rounds = sync_rounds_max * 2;
      break;
    case 9:
      current_sync_rounds = sync_rounds_max * 5 / 2;
      break;
    case 10:
      current_sync_rounds = sync_rounds_max * 3;
      break;
    case 11:
      current_sync_rounds = sync_rounds_max * 4;
      break;
    }

    // Execute sync operation, must be performed before each hammer
    sync_ref_unjitted(sync_rows, sync_stats, ref_threshold, sync_rounds_max);

    // Indirect jump to hammer
    goto hammer;

  // Define path labels (not directly used, but kept to maintain code structure)
  path0:
  path1:
  path2:
  path3:
  path4:
  path5:
  path6:
  path7:
  path8:
  path9:
  path10:
  path11:

  hammer:
    // attack_begin
    // mode_1:b1_flushnta_obf_0nop
    for (; agg_idx < NUM_AGG_PAIRS; agg_idx++)
    {
      asm volatile("prefetchnta (%0)" : : "r"(aggressor_pairs[agg_idx]) : "memory");
      asm volatile("clflushopt (%0)" : : "r"(aggressor_pairs[agg_idx]) : "memory");
      // nop_begin
      // BEGIN NOP INSERT
      lfence();
      // END NOP INSERT
      // nop_end
    }
    // attack_end
    //  Use randomness and bit operations to update counter
    uint64_t new_rand;
    asm volatile("rdrand %0" : "=r"(new_rand));
    agg_idx = ((agg_idx >= NUM_AGG_PAIRS) || (new_rand & 1)) ? ((tsc & 1) ? 0 : (agg_idx % NUM_AGG_PAIRS)) : agg_idx;
    // agg_idx=0;
    total_num_activations -= NUM_AGG_PAIRS;
  }
  // const uint64_t misses = l1d_misses.stop();
  // std::cout << "[***] L1D Misses count: " << misses << ", Aggr num :" << NUM_AGG_PAIRS << ", Miss rate: " << double(misses - sync_stats.num_sync_acts) / double(20000000 * 2) << std::endl;

  // for (size_t i = 1; i < before_sync_tscs.size(); i++)
  // {
  //   auto hammer = before_sync_tscs[i] - after_sync_tscs[i - 1];
  //   auto sync = after_sync_tscs[i] - before_sync_tscs[i];
  //   // Logger::log_data(format_string("HAMMER = %7zu | SYNC = %7zu | TOTAL = %7zu", hammer, sync, hammer + sync));
  //   std::cout << "hammer=" << hammer << " | sync=" << sync << " | total=" << hammer + sync << std::endl;
  // }
}
#pragma GCC pop_options

#ifdef ENABLE_JITTING
void CodeJitter::sync_ref(const std::vector<volatile char *> &sync_rows,
                          asmjit::x86::Assembler &assembler,
                          size_t num_timed_accesses)
{
  asmjit::Label wbegin = assembler.newLabel();
  asmjit::Label wend = assembler.newLabel();

  assembler.bind(wbegin);

  assembler.push(asmjit::x86::edx);
  assembler.rdtscp(); // result of rdtscp is in [edx:eax]
                      //  assembler.lfence();
  // discard upper 32 bits and store lower 32 bits in ebx to compare later
  assembler.mov(asmjit::x86::ebx, asmjit::x86::eax);
  assembler.pop(asmjit::x86::edx);

  for (size_t i = 0; i < num_timed_accesses; i++)
  {
    assembler.mov(asmjit::x86::rax, (uint64_t)sync_rows[get_next_sync_rows_idx()]);
    assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax)); // access
    assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));            // flush
    // update counter that counts the number of ACTs in the trailing synchronization
    assembler.inc(asmjit::x86::edx);
  }

  assembler.lfence();
  assembler.push(asmjit::x86::edx);
  assembler.rdtscp(); // result: edx:eax
  assembler.pop(asmjit::x86::edx);

  // if ((after - before) > REFRESH_THRESHOLD_CYCLES) break;
  assembler.sub(asmjit::x86::eax, asmjit::x86::ebx);
  assembler.cmp(asmjit::x86::eax, (uint64_t)REFRESH_THRESHOLD_CYCLES_LOW);

  // depending on the cmp's outcome...
  assembler.jg(wend);    // ... jump out of the loop
  assembler.jmp(wbegin); // ... or jump back to the loop's beginning
  assembler.bind(wend);
}
#endif

#ifdef ENABLE_JSON
void to_json(nlohmann::json &j, const CodeJitter &p)
{
  j = {
      {"flushing_strategy", to_string(p.flushing_strategy)},
      {"fencing_strategy", to_string(p.fencing_strategy)},
      {"total_activations", p.total_activations},
  };
}
#endif

#ifdef ENABLE_JSON
void from_json(const nlohmann::json &j, CodeJitter &p)
{
  from_string(j.at("flushing_strategy"), p.flushing_strategy);
  from_string(j.at("fencing_strategy"), p.fencing_strategy);
  j.at("total_activations").get_to(p.total_activations);
}
#endif

void CodeJitter::jit_ref_sync(
    FLUSHING_STRATEGY flushing,
    FENCING_STRATEGY fencing,
    std::vector<volatile char *> const &aggressors,
    DRAMAddr sync_ref_initial_aggr,
    size_t sync_ref_threshold)
{
  if (flushing != FLUSHING_STRATEGY::EARLIEST_POSSIBLE || fencing != FENCING_STRATEGY::OMIT_FENCING)
  {
    Logger::log_error("jit_ref_sync() implicitly assumes FLUSHING_STRATEGY::EARLIEST_POSSIBLE and FENCING_STRATEGY::OMIT_FENCING");
    exit(1);
  }

  if (fn_ref_sync != nullptr)
  {
    Logger::log_error("Function pointer is not NULL, cannot continue jitting code without leaking memory. Did you forget to call cleanup() before?");
    exit(1);
  }

  // Initialize assembler.
  asmjit::CodeHolder code;
  code.init(runtime.environment());
  asmjit::x86::Assembler assembler(&code);

  // PRE: %rdi (first register) contains a pointer to a struct RefSyncData, used to return the results.

  // FIRST SYNC: Initially synchronize with REF.

  // Store time stamp into %r8d.
  assembler.rdtscp();
  assembler.mov(asmjit::x86::r8d, asmjit::x86::eax);

  // Initialize ACT count.
  assembler.mov(asmjit::x86::edx, 0);
  sync_ref_nonrepeating(sync_ref_initial_aggr, sync_ref_threshold, assembler);
  // Move ACT count to %r9d to store it for later use.
  assembler.mov(asmjit::x86::r9d, asmjit::x86::edx);

  // Obtain time stamp.
  assembler.rdtscp(); // clobbers %eax, %edx.
  // Keep new timestamp in %eax (for later use), also copy it to %edx.
  assembler.mov(asmjit::x86::edx, asmjit::x86::eax);

  // Subtract initial timestamp (in %r8d) from current timestamp (in %edx) to get TSC delta (in %edx).
  assembler.sub(asmjit::x86::edx, asmjit::x86::r8d);
  // Move new timestamp (in %eax) to %r8d for use after next sync.
  assembler.mov(asmjit::x86::r8d, asmjit::x86::eax);

  // Store time stamp delta (from %edx) and ACT count (from %r9d) into struct. Store as 32 bit.
  assembler.mov(asmjit::x86::ptr(asmjit::x86::rdi, offsetof(RefSyncData, first_sync_tsc_delta)), asmjit::x86::edx);
  assembler.mov(asmjit::x86::ptr(asmjit::x86::rdi, offsetof(RefSyncData, first_sync_act_count)), asmjit::x86::r9d);

  // SECOND SYNC: Synchronize REF to REF.

  // Previous timestamp is still in %r8d.
  // Initialize ACT count.
  assembler.mov(asmjit::x86::edx, 0);
  sync_ref_nonrepeating(sync_ref_initial_aggr, sync_ref_threshold, assembler);
  // Move ACT count to %r9d to store it for later use.
  assembler.mov(asmjit::x86::r9d, asmjit::x86::edx);

  // Obtain time stamp.
  assembler.rdtscp(); // clobbers %eax, %edx.

  // Subtract initial timestamp (in %r8d) from current timestamp (in %eax) to get TSC delta (in %eax).
  assembler.sub(asmjit::x86::eax, asmjit::x86::r8d);

  // Store time stamp delta (from %edx) and ACT count (from %r9d) into struct. Store as 32 bit.
  assembler.mov(asmjit::x86::ptr(asmjit::x86::rdi, offsetof(RefSyncData, second_sync_tsc_delta)), asmjit::x86::eax);
  assembler.mov(asmjit::x86::ptr(asmjit::x86::rdi, offsetof(RefSyncData, second_sync_act_count)), asmjit::x86::r9d);

  // AGGRESSOR ACTIVATIONS
  // Access each given agressor once, and clflush it.
  for (auto *aggressor : aggressors)
  {
    assembler.mov(asmjit::x86::rax, (uint64_t)aggressor);
    assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
    assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
  }

  // LAST SYNC

  // Store time stamp into %r8d.
  assembler.rdtscp();
  assembler.mov(asmjit::x86::r8d, asmjit::x86::eax);

  // Initialize ACT count.
  assembler.mov(asmjit::x86::edx, 0);
  sync_ref_nonrepeating(sync_ref_initial_aggr, sync_ref_threshold, assembler);
  // Move ACT count to %r9d to store it for later use.
  assembler.mov(asmjit::x86::r9d, asmjit::x86::edx);

  // Obtain time stamp.
  assembler.rdtscp(); // clobbers %eax, %edx.
  // Subtract initial timestamp (in %r8d) from current timestamp (in %eax) to get TSC delta (in %eax).
  assembler.sub(asmjit::x86::eax, asmjit::x86::r8d);

  // Store time stamp delta (from %edx) and ACT count (from %r9d) into struct. Store as 32 bit.
  assembler.mov(asmjit::x86::ptr(asmjit::x86::rdi, offsetof(RefSyncData, last_sync_tsc_delta)), asmjit::x86::eax);
  assembler.mov(asmjit::x86::ptr(asmjit::x86::rdi, offsetof(RefSyncData, last_sync_act_count)), asmjit::x86::r9d);

  // return 0;
  assembler.mov(asmjit::x86::rax, 0);
  assembler.ret();

  // Add the generated code to the runtime.
  asmjit::Error err = runtime.add(&fn_ref_sync, &code);
  if (err)
    throw std::runtime_error("[-] Error occurred while jitting code. Aborting execution!");
}

// This function accesses a list of rows starting from initial_aggressors. It measures the access time between
// aggressors until REF is detected. Then it flushes all aggressors using clflush and hands control back.
void CodeJitter::sync_ref_nonrepeating(DRAMAddr inital_aggressor, size_t sync_ref_threshold, asmjit::x86::Assembler &assembler)
{
  asmjit::Label out = assembler.newLabel();
  // asmjit::Label flush_out = assembler.newLabel();
  // PRE: %edx is an in-out argument containing the number of ACTs done for synchronization.
  assembler.xor_(asmjit::x86::r11, asmjit::x86::r11);
  // Move ACT count from %edx to %r10d.
  assembler.mov(asmjit::x86::r10d, asmjit::x86::edx);

  // %ebx always contains the previous access timestamp.
  assembler.rdtscp(); // Returns result in [edx:eax]. We discard the upper 32 bits.
  assembler.mov(asmjit::x86::ebx, asmjit::x86::eax);

  // Weird row increment to hopefully not trigger the prefetcher.
  // constexpr size_t AGGR_ROW_INCREMENT = 1;
  std::mt19937 gen;
  std::random_device rd;
  gen = std::mt19937(rd());
  auto current_aggr = inital_aggressor;
  for (size_t i = 0; i < SYNC_REF_NUM_AGGRS; i++)
  {
    auto current = current_aggr.to_virt();
    assembler.mov(asmjit::x86::rax, (uint64_t)current);
    assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
    assembler.lfence();
    assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
    int random_data = Range<int>(1, 4).get_random_number(gen);
    current_aggr.add_inplace(0, random_data, 0);
    // std::cout << "current_aggr: " << current_aggr.get_row ()<< std::endl;
    //  Increment %r10, which counts the number of ACTs.
    assembler.inc(asmjit::x86::r10d);
    assembler.inc(asmjit::x86::r11);

    // Measure time.
    assembler.rdtscp();

    // %edx = %eax (current timestamp) - %ebx (previous timestamp).
    assembler.mov(asmjit::x86::edx, asmjit::x86::eax);
    assembler.sub(asmjit::x86::edx, asmjit::x86::ebx);

    // Ignore first 4 iterations for warmup.
    if (i >= 4)
    {
      // if (%edx > sync_ref_threshold) { break; }
      assembler.cmp(asmjit::x86::edx, sync_ref_threshold);
      assembler.jg(out);
    }

    // else { %ebx = %eax; }
    assembler.mov(asmjit::x86::ebx, asmjit::x86::eax);
  }

  assembler.bind(out);

  // Move ACT count from %r10d back to %edx.
  assembler.mov(asmjit::x86::edx, asmjit::x86::r10d);
}