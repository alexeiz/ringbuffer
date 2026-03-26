#include "ringbuffer/ringbuffer.hpp"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/program_options.hpp>
#include <scope_exit/scope_exit.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <utility>
#include <array>
#include <stdexcept>
#include <mutex>

#include <cstdint>
#include <cstdio>
#include <cassert>
#include <climits>
#if defined(_MSC_VER)
#include <intrin.h>
#include <io.h>
#define pipe(fds) _pipe(fds, 4096, 0)
#define read      _read
#define write     _write
#define close     _close
using pid_t = int;
#endif

#if defined(_WIN32)
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace
{
using namespace std;
using namespace std::literals;

namespace po = boost::program_options;
namespace ipc = boost::interprocess;

using namespace rb;

struct shm_guard
{
    shm_guard(char const * name)
        : name_{name}
    {
        ipc::shared_memory_object::remove(name_);
    }

    shm_guard(shm_guard const &) = delete;
    shm_guard & operator=(shm_guard const &) = delete;
    shm_guard(shm_guard &&) = delete;
    shm_guard & operator=(shm_guard &&) = delete;

    ~shm_guard() { ipc::shared_memory_object::remove(name_); }

    char const * name_;
};

class sync_pipe
{
public:
    sync_pipe()
    {
        if (pipe(pipe_) < 0)
        {
            perror(nullptr);
            exit(-1);
        }
    }

    void wait(int count = 1) const
    {
        for (int i = 0; i != count; ++i)
        {
            char a{};
            auto bytes = read(pipe_read_, &a, 1);
            assert(bytes == 1);
            (void) bytes;
        }
    }

    void signal(int count = 1) const
    {
        for (int i = 0; i != count; ++i)
        {
            char a = 'z';
            auto bytes = write(pipe_write_, &a, 1);
            assert(bytes == 1);
            (void) bytes;
        }
    }

private:
    int pipe_[2];
    int & pipe_read_ = pipe_[0];
    int & pipe_write_ = pipe_[1];
};

inline uint64_t now_tsc()
{
#if defined(__aarch64__)
    uint64_t tsc;
    __asm__ __volatile__("isb" ::: "memory");
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(tsc));
    return tsc;
#elif defined(__arm__)
    unsigned lo, hi;
    __asm__ __volatile__("isb" ::: "memory");
    __asm__ __volatile__("mrrc p15, 1, %0, %1, c14" : "=r"(lo), "=r"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#elif defined(__x86_64__) || defined(_M_X64) || defined(_M_IX86)
#if defined(_MSC_VER)
    return __rdtsc();
#else
    unsigned lo, hi;
    __asm__ __volatile__("lfence" ::: "memory");
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#endif
#else
#error "now_tsc() is not implemented for this architecture"
#endif
}

inline auto now_chrono() { return chrono::high_resolution_clock::now(); }

inline uint64_t get_timestamp_freq_khz()
{
#if defined(__aarch64__)
    uint64_t freq;
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq / 1000;
#elif defined(__arm__)
    unsigned freq;
    __asm__ __volatile__("mrc p15, 0, %0, c14, c0, 0" : "=r"(freq));
    return static_cast<uint64_t>(freq) / 1000;
#else
    ifstream sys_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");

    uint64_t cur_freq = 0;
    if (sys_file >> cur_freq)
        return cur_freq;

    return 0;
#endif
}

inline char const * timestamp_unit()
{
#if defined(__aarch64__) || defined(__arm__)
    return "ticks";
#else
    return "cycles";
#endif
}

// simple synchronized logger implementation
void log_impl() { clog << endl; }

template <typename A, typename... Args>
void log_impl(A && arg1, Args &&... args)
{
    clog << std::forward<A>(arg1);
    log_impl(std::forward<Args>(args)...);
}

mutex log_mutex;

template <typename... Args>
void log_msg(Args &&... args)
{
    lock_guard<mutex> _{log_mutex};
    log_impl(std::forward<Args>(args)...);
}

template <size_t Size>
struct data_item
{
    constexpr static size_t payload_size = (Size - sizeof(uint64_t) - sizeof(int)) / sizeof(int);

    data_item(unsigned n)
        : timestamp{now_tsc()}
        , seq{n}
    {}

    uint64_t timestamp;
    unsigned seq;
    array<int, payload_size> payload;
};

static_assert(sizeof(data_item<16>) == 16, "wrong data_item size");
static_assert(sizeof(data_item<32>) == 32, "wrong data_item size");
static_assert(sizeof(data_item<64>) == 64, "wrong data_item size");

// synchronize writer and reader processes
sync_pipe reader_sync;
sync_pipe writer_sync;

constexpr unsigned sentry(-1);  // special sequence number value to signal readers to stop
constexpr char const * ring_buffer_name = "ringbuffer_concur_test";
vector<pid_t> reader_pids;
vector<thread> reader_threads;

template <typename Item>
void run_reader_impl()
{
    reader_sync.wait();
    scope(exit) { writer_sync.signal(); };

    log_msg("reader ", getpid(), ":", this_thread::get_id(), " started");

    ring_buffer_reader<Item> rb{ring_buffer_name};
    int read_items = 0;
    int gaps = 0;
    int errors = 0;
    unsigned prev = sentry;
    uint64_t latency = 0;
    uint64_t latency_min = ULLONG_MAX;
    int latency_items = 0;
    auto start = now_chrono();

    for (; ++read_items;)
    {
        Item cur = rb.get();
        rb.next();

        if (rb.empty())
        {
            auto lat = now_tsc() - cur.timestamp;
            latency += lat;
            ++latency_items;
            if (latency_min > lat)
                latency_min = lat;
        }

        if (cur.seq == sentry)
            break;

        if (cur.seq != prev + 1 && prev != sentry)
        {
            if (cur.seq > prev)
                gaps += cur.seq - prev - 1;
            else
                ++errors;  // it's an error for the sequence number to go back
        }

        prev = cur.seq;
    }

    auto end = now_chrono();

    double items_sec =
        read_items / (double(chrono::duration_cast<chrono::nanoseconds>(end - start).count()) / 1000'000'000);
    double bytes_sec = items_sec * sizeof(Item);
    auto timestamp_khz = get_timestamp_freq_khz();
    auto avg_latency = latency / double(latency_items);

    // clang-format off
    if (timestamp_khz != 0)
        log_msg("reader ", getpid(), ":", this_thread::get_id(), "\n",
                "  gaps            : ", gaps, "\n",
                "  errors          : ", errors, "\n",
                "  throughput      : ", items_sec, " items/sec, ", bytes_sec, " bytes/sec", "\n",
                "  average latency : ", avg_latency, " ", timestamp_unit(), ", ", avg_latency / timestamp_khz * 1000, " usec", "\n",
                "  min latency     : ", latency_min, " ", timestamp_unit(), ", ", double(latency_min) / timestamp_khz * 1000, " usec");
    else
        log_msg("reader ", getpid(), ":", this_thread::get_id(), "\n",
                "  gaps            : ", gaps, "\n",
                "  errors          : ", errors, "\n",
                "  throughput      : ", items_sec, " items/sec, ", bytes_sec, " bytes/sec", "\n",
                "  average latency : ", avg_latency, " ", timestamp_unit(), ", ", avg_latency / timestamp_khz * 1000, " usec", "\n",
                "  min latency     : ", latency_min, " ", timestamp_unit(), ", ", double(latency_min) / timestamp_khz * 1000, " usec");
    // clang-format on
}

void run_reader(size_t item_size)
{
#define READER_IMPL_CASE(N)                                                                                            \
    case N:                                                                                                            \
        run_reader_impl<data_item<N>>();                                                                               \
        break;

    switch (item_size)
    {
        READER_IMPL_CASE(16);
        READER_IMPL_CASE(32);
        READER_IMPL_CASE(48);
        READER_IMPL_CASE(64);
        READER_IMPL_CASE(80);
        READER_IMPL_CASE(96);
        READER_IMPL_CASE(112);
        READER_IMPL_CASE(128);
        READER_IMPL_CASE(256);
        READER_IMPL_CASE(512);
        READER_IMPL_CASE(1024);
    default:
        throw runtime_error("wrong item size");
    }

#undef READER_IMPL_CASE
}

template <typename Item>
void run_writer_impl(unsigned readers, unsigned item_count, unsigned rb_size)
{
    shm_guard _{ring_buffer_name};
    ring_buffer<Item> rb{ring_buffer_name, rb_size};

    log_msg("writer started");
    reader_sync.signal(readers);
    this_thread::sleep_for(100ms);

    scope(exit)
    {
        writer_sync.wait(readers);
        log_msg("writer done");
    };

    log_msg("items to push: ", item_count);

    auto start = now_chrono();
    for (unsigned i = 0; i != item_count; ++i)
    {
        rb.emplace(i);

        [[maybe_unused]] int volatile delay = 0;
        delay = 1;
        delay = 1;
        delay = 1;
    }
    rb.push(sentry);  // signal to readers to stop
    auto end = now_chrono();

    double items_sec =
        item_count / (double(chrono::duration_cast<chrono::nanoseconds>(end - start).count()) / 1'000'000'000);
    double bytes_sec = items_sec * sizeof(Item);

    // clang-format off
    log_msg("writer throughput: ", items_sec, " items/sec\n",
            "                 : ", bytes_sec, " bytes/sec");
    // clang-format on
}

void run_writer(unsigned readers, unsigned item_size, unsigned item_count, unsigned rb_size)
{
#define WRITER_IMPL_CASE(N)                                                                                            \
    case N:                                                                                                            \
        run_writer_impl<data_item<N>>(readers, item_count, rb_size);                                                   \
        break;

    switch (item_size)
    {
        WRITER_IMPL_CASE(16);
        WRITER_IMPL_CASE(32);
        WRITER_IMPL_CASE(48);
        WRITER_IMPL_CASE(64);
        WRITER_IMPL_CASE(80);
        WRITER_IMPL_CASE(96);
        WRITER_IMPL_CASE(112);
        WRITER_IMPL_CASE(128);
        WRITER_IMPL_CASE(256);
        WRITER_IMPL_CASE(512);
        WRITER_IMPL_CASE(1024);
    default:
        throw runtime_error("wrong item size");
    }

#undef WRITER_IMPL_CASE
}

void create_reader_processes(unsigned readers, size_t item_size)
{
#if defined(_WIN32)
    throw std::runtime_error(
        "Multiprocessing tests are not supported on Windows yet. Please use --use-threads instead.");
#else
    for (unsigned i = 0; i != readers; ++i)
    {
        if (auto pid = fork())
        {
            // parent
            if (pid < 0)
            {
                perror(nullptr);
                exit(-1);
            }

            reader_pids.push_back(pid);
        }
        else
        {
            // child
            run_reader(item_size);
            exit(0);
        }
    }
#endif
}

void wait_reader_processes()
{
#if !defined(_WIN32)
    for (auto rid : reader_pids)
    {
        int status = 0;
        waitpid(rid, &status, 0);
    }
#endif
}

void create_reader_threads(unsigned readers, size_t item_size)
{
    for (unsigned i = 0; i != readers; ++i)
        reader_threads.emplace_back([=] { run_reader(item_size); });
}

void wait_reader_threads()
{
    for (auto && thr : reader_threads)
        thr.join();
}

void run_test(unsigned readers, size_t item_size, unsigned item_count, size_t rb_size, bool use_threads)
{
    log_msg("number of readers  : ", readers);
    log_msg("size of data item  : ", item_size);
    log_msg("size of ring buffer: ", rb_size);
    log_msg("use threads        : ", use_threads);

    if (use_threads)
        create_reader_threads(readers, item_size);
    else
        create_reader_processes(readers, item_size);

    run_writer(readers, item_size, item_count, rb_size);

    if (use_threads)
        wait_reader_threads();
    else
        wait_reader_processes();
}
}  // anonymous namespace

int main(int argc, char * argv[])
try
{
    std::ios::sync_with_stdio(false);

    auto readers = std::thread::hardware_concurrency() - 1;  // one writer and the rest are readers
    size_t item_size = 16;
    size_t item_count = 10'000'000;
    size_t rb_size = 0x10000;
    bool use_threads = false;

    // clang-format off
    po::options_description desc("options");
    desc.add_options()
        ("help,h", "display this help message")
        ("readers,r",
         po::value<unsigned>(&readers)->default_value(readers),
         "number of readers")
        ("item-size,i",
         po::value<size_t>(&item_size)->default_value(item_size),
         "size of the data item in bytes")
        ("item-count,I",
         po::value<size_t>(&item_count)->default_value(item_count),
         "number of items to push")
        ("rb-size,s",
         po::value<size_t>(&rb_size)->default_value(rb_size),
         "number of items in the ring buffer")
        ("use-threads,t", "use reader threads (default: separate processes)");
    // clang-format on

    po::variables_map opt;
    po::store(po::parse_command_line(argc, argv, desc), opt);
    po::notify(opt);

    if (opt.contains("help"))
    {
        cout << desc << endl;
        exit(0);
    }

    // parameter validation
    item_size = std::max(item_size, size_t(16));
    item_size = ((item_size - 1) / 16 + 1) * 16;  // align to 16 bytes

    // adjust to the power of 2
    if ((rb_size & (rb_size - 1)) != 0)
    {
#if defined(_MSC_VER)
        unsigned long leading_zero = 0;
        if (_BitScanReverse64(&leading_zero, rb_size))
            rb_size = 1ull << (leading_zero + 1);
#else
        rb_size = 1ul << (CHAR_BIT * sizeof(rb_size) - __builtin_clzl(rb_size));
#endif
    }

    if (opt.contains("use-threads"))
        use_threads = true;

    run_test(readers, item_size, item_count, rb_size, use_threads);
}
catch (exception & e)
{
    cerr << "error: " << e.what() << endl;
    exit(-1);
}
