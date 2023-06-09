#include "ringbuffer.hpp"
#include "scope_exit.hpp"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <utility>
#include <tuple>
#include <array>
#include <stdexcept>
#include <fstream>
#include <mutex>

#include <cstdio>
#include <cassert>
#include <climits>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

namespace
{

using namespace std;
using namespace std::literals;

namespace po = boost::program_options;
namespace ipc = boost::interprocess;

struct shm_guard
{
    shm_guard(char const * name)
        : name_{name}
    { ipc::shared_memory_object::remove(name_); }

    ~shm_guard()
    { ipc::shared_memory_object::remove(name_); }

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

    void wait(int count = 1)
    {
        for (int i = 0; i != count; ++i)
        {
            char a;
            auto bytes = read(pipe_read_, &a, 1);
            assert(bytes == 1); (void) bytes;
        }
    }

    void signal(int count = 1)
    {
        for (int i = 0; i != count; ++i)
        {
            char a = 'z';
            auto bytes = write(pipe_write_, &a, 1);
            assert(bytes == 1); (void) bytes;
        }
    }

private:
    int pipe_[2];
    int & pipe_read_ = pipe_[0];
    int & pipe_write_ = pipe_[1];
};


inline
unsigned long now_rdtsc()
{
    union
    {
        struct
        {
            unsigned lo;
            unsigned hi;
        } regs;

        unsigned long tsc;
    } ts;

    asm volatile("rdtsc" : "=a" (ts.regs.lo), "=d" (ts.regs.hi));
    return ts.tsc;
}


inline
auto now_chrono()
{
    return chrono::high_resolution_clock::now();
}


/*
inline
unsigned long now_rdtscp()
{
    union
    {
        struct
        {
            unsigned lo;
            unsigned hi;
        } regs;

        unsigned long tsc;
    } ts;

    asm volatile("rdtscp" : "=a" (ts.regs.lo), "=c" (ts.regs.hi) :: "%rdx");
    return ts.tsc;
}

inline
unsigned get_cpufreq_khz()
{
    ifstream sys_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    sys_file.exceptions(ios::badbit | ios::failbit);

    unsigned cur_freq = 1;
    sys_file >> cur_freq;
    return cur_freq;
}
*/


// simple synchronized logger implementation
void log_impl()
{
    clog << endl;
}

template <typename A, typename ...Args>
void log_impl(A && arg1, Args && ...args)
{
    clog << std::forward<A>(arg1);
    log_impl(std::forward<Args>(args)...);
}

mutex log_mutex;

template <typename ...Args>
void log_msg(Args && ...args)
{
    lock_guard<mutex> _{log_mutex};
    log_impl(std::forward<Args>(args)...);
}


template <std::size_t Size>
struct data_item
{
    constexpr static size_t payload_size = (Size - sizeof(unsigned long) - sizeof(int)) / sizeof(int);

    data_item(int n)
        : timestamp{now_rdtsc()}
        , seq{n}
    {}

    unsigned long            timestamp;
    int                      seq;
    array<int, payload_size> payload;
};

static_assert(sizeof(data_item<16>) == 16, "wrong data_item size");
static_assert(sizeof(data_item<32>) == 32, "wrong data_item size");
static_assert(sizeof(data_item<64>) == 64, "wrong data_item size");


// synchronize writer and reader processes
sync_pipe reader_sync;
sync_pipe writer_sync;

char const *   ring_buffer_name = "ringbuffer_concur_test";
vector<pid_t>  reader_pids;
vector<thread> reader_threads;


template <typename Item>
void run_reader_impl()
{
    reader_sync.wait();
    scope (exit) { writer_sync.signal(); };

    log_msg("reader ", getpid(), ":", this_thread::get_id(), " started");

    tsq::ring_buffer_reader<Item> rb{ring_buffer_name};
    int read_items = 0;
    int gaps = 0;
    int errors = 0;
    int prev = -1;
    unsigned long latency = 0;
    unsigned long latency_min = ULONG_MAX;
    int latency_items = 0;
    auto start = now_chrono();

    for (; ++read_items;)
    {
        Item cur = rb.get();
        rb.next();

        if (rb.empty())
        {
            auto lat = now_rdtsc() - cur.timestamp;
            latency += lat;
            ++latency_items;
            if (latency_min > lat)
                latency_min  = lat;
        }

        if (cur.seq == -1)
            break;

        if (cur.seq != prev + 1 && prev != -1)
        {
            if (cur.seq > prev)
                gaps += cur.seq - prev - 1;
            else
                ++errors;  // it's an error for the sequence number to go back
        }

        prev = cur.seq;
    }

    auto end = now_chrono();

    double items_sec = read_items / (double(chrono::duration_cast<chrono::nanoseconds>(end - start).count()) / 1000'000'000);
    double bytes_sec = items_sec * sizeof(Item);

    log_msg("reader ", getpid(), ":", this_thread::get_id(), "\n",
            "  gaps           : ", gaps, "\n",
            "  errors         : ", errors, "\n",
            "  throughput     : ", items_sec, " items/sec, ", bytes_sec, " bytes/sec\n",
            "  average latency: ", latency / double(latency_items), " cycles\n",
            "  min latency    : ", latency_min, " cycles");
}

void run_reader(size_t item_size)
{
#define READER_IMPL_CASE(N)                            \
    case N: run_reader_impl<data_item<N>>(); break;    \

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
void run_writer_impl(unsigned readers, unsigned rb_size)
{
    shm_guard _{ring_buffer_name};
    tsq::ring_buffer<Item> rb{ring_buffer_name, rb_size};

    log_msg("writer started");
    reader_sync.signal(readers);
    this_thread::sleep_for(100ms);

    scope (exit) {
        writer_sync.wait(readers);
        log_msg("writer done");
    };

    int const items = 1000'000'000;
    log_msg("items to push: ", items);

    auto start = now_chrono();
    for (int i = 0; i != items; ++i)
    {
        rb.emplace(i);

        int volatile __attribute__((unused)) delay;
        delay = 1;
        delay = 1;
        delay = 1;
    }
    rb.push(-1);  // signal to readers to stop
    auto end = now_chrono();

    double items_sec = items / (double(chrono::duration_cast<chrono::nanoseconds>(end - start).count()) / 1000'000'000);
    double bytes_sec = items_sec * sizeof(Item);

    log_msg("writer throughput: ", items_sec, " items/sec\n",
            "                 : ", bytes_sec, " bytes/sec");
}

void run_writer(unsigned readers, unsigned item_size, unsigned rb_size)
{
#define WRITER_IMPL_CASE(N)                                             \
    case N: run_writer_impl<data_item<N>>(readers, rb_size); break;     \

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
    for (unsigned i = 0; i != readers; ++i)
    {
        if (auto pid = fork())
        {
            // parent
            if  (pid < 0)
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
}

void wait_reader_processes()
{
    for (auto rid : reader_pids)
    {
        int status;
        waitpid(rid, &status, 0);
    }
}

void create_reader_threads(unsigned readers, size_t item_size)
{
    for (unsigned i = 0; i != readers; ++i)
        reader_threads.emplace_back([=]{ run_reader(item_size); });
}

void wait_reader_threads()
{
    for (auto && thr : reader_threads)
        thr.join();
}

void run_test(unsigned readers, size_t item_size, size_t rb_size, bool use_threads)
{
    log_msg("number of readers  : ", readers);
    log_msg("size of data item  : ",item_size);
    log_msg("size of ring buffer: ", rb_size);
    log_msg("use threads        : ", use_threads);

    if (use_threads)
        create_reader_threads(readers, item_size);
    else
        create_reader_processes(readers, item_size);

    run_writer(readers, item_size, rb_size);

    if (use_threads)
        wait_reader_threads();
    else
        wait_reader_processes();
}

}  // anonymous namespace


int main(int argc, char * argv[])
try
{
    cin.sync_with_stdio(false);
    cout.sync_with_stdio(false);

    auto readers = std::thread::hardware_concurrency() - 1;  // one writer and the rest are readers
    size_t item_size = 16;
    size_t rb_size = 0x10000;
    bool use_threads = false;

    po::options_description desc("options");
    desc.add_options()
        ("help,h", "display this help message")
        ("readers,r",
         po::value<unsigned>(&readers)->default_value(readers),
         "number of readers")
        ("item-size,i",
         po::value<size_t>(&item_size)->default_value(item_size),
         "size of the data item in bytes")
        ("rb-size,s",
         po::value<size_t>(&rb_size)->default_value(rb_size),
         "number of items in the ring buffer")
        ("use-threads,t", "use reader threads (default: separate processes)");

    po::variables_map opt;
    po::store(po::parse_command_line(argc, argv, desc), opt);
    po::notify(opt);

    if (opt.count("help"))
    {
        cout << desc << endl;
        exit(0);
    }

    // parameter validation
    if (item_size < 16)
        item_size = 16;
    item_size = ((item_size - 1) / 16 + 1) * 16;  // align to 16 bytes

    // adjust to the power of 2
    if ((rb_size & (rb_size - 1)) != 0)
        rb_size = 1ul << (CHAR_BIT * sizeof(rb_size) - __builtin_clzl(rb_size));

    if (opt.count("use-threads"))
        use_threads = true;

    run_test(readers, item_size, rb_size, use_threads);
}
catch (exception & e)
{
    cerr << "error: " << e.what() << endl;
    exit(-1);
}
