#ifndef CODEJITTER
#define CODEJITTER

#include <unordered_map>
#include <vector>
#include <iostream>
#include "Utilities/Enums.hpp"
#include "Fuzzer/FuzzingParameterSet.hpp"

#ifdef ENABLE_JITTING
#include <asmjit/asmjit.h>
#endif

#ifdef ENABLE_JSON
#include <nlohmann/json.hpp>
#endif
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <unistd.h>

// Performance counter wrapper class
class PerfCounter
{
public:
  PerfCounter(uint32_t type, uint64_t config)
  {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(pe));

    pe.config = config;

    pe.type = type;

    // Structure size
    pe.size = sizeof(struct perf_event_attr);

    // Count instructions
    pe.disabled = 1;
    // Disable counter at initialization
    pe.exclude_kernel = 0;
    // Include kernel instructions
    pe.exclude_hv = 1;
    // Exclude hypervisor layer
    pe.read_format = 0;

    // Use syscall
    fd = syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd == -1)
    {
      // Print detailed error information
      std::cerr << "perf_event_open failed: type=" << type
                << ", config=" << config
                << ", error=" << strerror(errno) << std::endl;
      throw std::runtime_error("perf_event_open failed");
    }
  }

  ~PerfCounter()
  {
    if (fd != -1)
      close(fd);
  }

  void start()
  {
    if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) == -1)
    {
      throw std::runtime_error("PERF_EVENT_IOC_RESET failed: " + std::string(strerror(errno)));
    }
    if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) == -1)
    {
      throw std::runtime_error("PERF_EVENT_IOC_ENABLE failed: " + std::string(strerror(errno)));
    }
  }

  uint64_t stop()
  {
    if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0) == -1)
    {
      throw std::runtime_error("PERF_EVENT_IOC_DISABLE failed: " + std::string(strerror(errno)));
    }
    uint64_t count = 0;
    if (read(fd, &count, sizeof(count)) == -1)
    {
      throw std::runtime_error("read perf counter failed: " + std::string(strerror(errno)));
    }
    return count;
  }

private:
  int fd = -1; // Initialize to invalid value
};

struct synchronization_stats
{
  // how often we accessed sync dummies
  size_t num_sync_acts;
  // how often we started the synchronization procedure
  size_t num_sync_rounds;
};

// NOTE: The members of this struct should not be re-arranged, as they are written to from Assembly.
struct RefSyncData
{
  // first sync: immediately after the function starts
  uint32_t first_sync_act_count{0};
  uint32_t first_sync_tsc_delta{0};
  // second sync: REF-to-REF, after first sync
  uint32_t second_sync_act_count{0};
  uint32_t second_sync_tsc_delta{0};
  // last sync: after configurable number of aggressors
  uint32_t last_sync_act_count{0};
  uint32_t last_sync_tsc_delta{0};
};

struct HammeringData
{
  uint64_t tsc_delta{0};
  uint64_t total_acts{0};
};

class CodeJitter
{
private:
#ifdef ENABLE_JITTING
  /// runtime for JIT code execution, can be reused by cleaning the function ptr (see cleanup method)
  asmjit::JitRuntime runtime;

  /// a logger that keeps track of the generated ASM instructions - useful for debugging
  asmjit::StringLogger *logger = nullptr;
#endif

  const uint64_t REFRESH_THRESHOLD_CYCLES_LOW = 500;
  const uint64_t REFRESH_THRESHOLD_CYCLES_HIGH = 900;

  /// a function pointer to a function that takes no input (void) and returns an integer
  int (*fn)(HammeringData *) = nullptr;
  size_t (*fn_ref_sync)(RefSyncData *) = nullptr;

public:
  FLUSHING_STRATEGY flushing_strategy;

  FENCING_STRATEGY fencing_strategy;

  int total_activations;

  size_t sync_rows_idx = 0;

  static constexpr size_t SYNC_REF_NUM_AGGRS = 256;

  size_t sync_rows_size;

  /// constructor
  CodeJitter();

  /// destructor
  ~CodeJitter();

  /// generates the jitted function and assigns the function pointer fn to it
  void jit_strict(FuzzingParameterSet &fuzzing_parameters,
                  FLUSHING_STRATEGY flushing,
                  FENCING_STRATEGY fencing,
                  int total_num_activations,
                  const std::vector<volatile char *> &aggressor_pairs,
                  DRAMAddr sync_rows,
                  size_t ref_threshold);

  /// does the hammering if the function was previously created successfully, otherwise does nothing
  size_t hammer_pattern(FuzzingParameterSet &fuzzing_parameters, bool verbose);

  /// cleans this instance associated function pointer that points to the function that was jitted at runtime;
  /// cleaning up is required to release memory before jit_strict can be called again
  void cleanup();

  void jit_ref_sync(
      FLUSHING_STRATEGY flushing,
      FENCING_STRATEGY fencing,
      std::vector<volatile char *> const &aggressors,
      DRAMAddr sync_ref_initial_aggr,
      size_t sync_ref_threshold);

  size_t run_ref_sync(RefSyncData *ref_sync_data)
  {
    if (fn_ref_sync == nullptr)
    {
      Logger::log_error("Cannot run fn_ref_sync as it is NULL.");
      exit(1);
    }
    return (*fn_ref_sync)(ref_sync_data);
  }
  static void sync_ref_nonrepeating(DRAMAddr initial_aggressor, size_t sync_ref_threshold, asmjit::x86::Assembler &assembler);

  size_t get_next_sync_rows_idx();

#ifdef ENABLE_JITTING
  void sync_ref(const std::vector<volatile char *> &sync_rows,
                asmjit::x86::Assembler &assembler,
                size_t num_timed_accesses);
#endif
  void hammer_pattern_unjitted(FuzzingParameterSet &fuzzing_parameters,
                               bool verbose,
                               FLUSHING_STRATEGY flushing,
                               FENCING_STRATEGY fencing,
                               int total_num_activations,
                               const std::vector<volatile char *> &aggressor_pairs,
                               const std::vector<volatile char *> &sync_rows,
                               size_t ref_threshold);

  void sync_ref_unjitted(const std::vector<volatile char *> &sync_rows,
                         synchronization_stats &sync_stats,
                         size_t ref_threshold, size_t sync_rounds_max) const;

  [[maybe_unused]] static void wait_for_user_input();
};

#ifdef ENABLE_JSON

void to_json(nlohmann::json &j, const CodeJitter &p);

void from_json(const nlohmann::json &j, CodeJitter &p);

#endif

#endif /* CODEJITTER */
