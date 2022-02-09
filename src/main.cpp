// vim: textwidth=100
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string>
#include <string_view>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <vector>
#include <memory>

#include <getopt.h>

/*
 * The test is dedicated for checking how memory access time depends on used working set size.
 * Double-linked list is built on a continuous block of memory, then nodes are randomly swapped.
 * During the test the list is traversed and each node's value is read from.
 */

constexpr std::size_t s_cache_line_size = 64;

inline std::uint64_t rdtsc() {
    std::uint64_t res;
    asm volatile ("rdtsc\n"
                  "shl $32, %%rdx\n"
                  "or %%rdx, %0\n"
                  : "=a" (res)
                  :
                  : "cc", "rdx");
    return res;
}

struct node {
    node* m_next;
    node* m_prev;
    std::uint32_t m_data;
    char m_padding[s_cache_line_size - sizeof(node*) * 2 - sizeof(m_data)];
};

static_assert(sizeof(node) == s_cache_line_size);

// Swap locations of two nodes in a double-linked list
void swap_nodes(node& n1, node& n2) {
    using std::swap;

    swap(n1.m_prev->m_next, n2.m_prev->m_next);
    swap(n1.m_prev, n2.m_prev);
    swap(n1.m_next->m_prev, n2.m_next->m_prev);
    swap(n1.m_next, n2.m_next);
}

// returns mean and median
std::pair<double, double> test(node* container, std::size_t elements_count) {
    if (elements_count < 2)
        return {};

    // build double-linked wheel from elements in the container
    container[0].m_next = container + 1;
    container[0].m_prev = container + elements_count - 1;
    container[elements_count-1].m_next = container;
    container[elements_count-1].m_prev = container + elements_count - 2;

    for (node* ptr = container + 1; ptr != container + elements_count - 1; ++ptr) {
        ptr->m_next = ptr + 1;
        ptr->m_prev = ptr - 1;
    }

    // randomize location of elements in the wheel keeping it's structure
    std::random_device rd;
    std::mt19937 r(rd());

    typedef std::uniform_int_distribution<std::size_t> ud_t;
    ud_t ud;

    for (auto ix = elements_count - 1; ix > 0; --ix)
        swap_nodes(container[ud(r, ud_t::param_type{0, ix - 1})], container[ix]);

    // Providing a not-to-be-optimized-out destination for traversing over the
    // wheel and read data into else a clever compiler can throw out the whole
    // testing cycle at all.
    static volatile std::uint32_t tmp_destination;

    const std::size_t experiments_count = 20;
    const std::size_t wheel_walking_count = 20;
    std::vector<double> samples(experiments_count);

    for (auto& sample : samples) {
        const auto hopes = elements_count * wheel_walking_count;
        node* p = &container[ud(r, ud_t::param_type{0, elements_count - 1})];
        const auto start = rdtsc();

        for (std::size_t i = 0; i < hopes; ++i) {
            tmp_destination = p->m_data;
            p = p->m_next;
        }

        sample = static_cast<double>(rdtsc() - start) / hopes;
    }

    sort(begin(samples), end(samples));
    return {
        std::accumulate(begin(samples), end(samples), 0.0) / samples.size(),
        samples[samples.size()/2]};
}

bool set_thread_affinity(unsigned short cpuid) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpuid, &cpu_set);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set) == 0;
}

double get_cpu_freq_ghz() {
    using fp_seconds_t =
        std::chrono::duration<double, std::chrono::seconds::period>;

    auto get_freq_hz = [](){
        auto time_pt = std::chrono::high_resolution_clock::now();
        std::uint64_t end_ts, start_ts = rdtsc();
        do {
            end_ts = rdtsc();
        } while (end_ts - start_ts < 1'000'000);
        return (end_ts - start_ts) /
            fp_seconds_t(std::chrono::high_resolution_clock::now() - time_pt).count();
    };

    double freq_prev, freq = get_freq_hz();
    do {
        freq_prev = freq;
        freq = get_freq_hz();
    } while (std::abs(freq - freq_prev) > 1000.0);

    return freq / 1'000'000'000.0;
}

int main(int argc, char* argv[]) {
    using namespace std::string_view_literals;

    std::size_t max_working_set_size = 128;
    unsigned short cpuid = 1;
    bool go_parse = true;
    const char* prog_name = argv[0];
    const struct option long_opts[] = {{"help", 0, nullptr, 'h'},
        {"cpu", 1, nullptr, 'c'}, {"working-set", 1, nullptr, 'w'}, {}};

    if (auto p = std::strrchr(prog_name, '/'))
        prog_name = p + 1;

    while (go_parse)
        switch (getopt_long(argc, argv, "c:w:h", long_opts, nullptr)) {
        case 'h':
            std::cout << prog_name << " [OPTIONS]\n\n"
                "The program allows to measure average access time to memory depending on\n"
                "used working set size.\n\n"
                "Options:\n"
                "  -h, --help          - this help message\n"
                "  -c, --cpu N         - bind to cpu N (default: 1)\n"
                "  -w, --working-set N - max working set size in Mb (default: 128)"sv
                << std::endl;
            return 0;
        case 'w': {
            std::istringstream is{optarg};
            is >> max_working_set_size;
            if (is.bad() || is.fail()) {
                std::cerr << "unable to convert max working set size into an acceptable number"sv << std::endl;
                return 1;
            }
            break;
        }
        case 'c': {
            std::istringstream is{optarg};
            is >> cpuid;
            if (is.bad() || is.fail()) {
                std::cerr << "unable to convert cpuid into an acceptable number"sv << std::endl;
                return 1;
            }
            break;
        }
        case -1:
            if (optind < argc) {
                std::cerr << "unexpected positional parameters at command line"sv << std::endl;
                return 1;
            }
            go_parse = false;
            break;
        case '?':
            return 1;
        }

    if (! set_thread_affinity(cpuid)) {
        std::cerr << "unable to bound to cpuid "sv << cpuid << std::endl;
        return 2;
    }

    max_working_set_size *= 1024*1024ul;
    const auto cpu_freq = get_cpu_freq_ghz();
    const int print_col_size = 16;
    auto container_size = max_working_set_size / sizeof(node);
    std::unique_ptr<node[]> container{new node[container_size]};

    std::cout.precision(3);
    std::cout << std::fixed;

    auto run_and_print_test = [cpu_freq](std::size_t working_set_size, node* container){
            auto elements = working_set_size / sizeof(node);
            if (elements) {
                auto res = test(container, elements);
                std::cout
                    << std::setw(print_col_size) << elements * sizeof(node)
                    << std::setw(print_col_size) << res.first
                    << std::setw(print_col_size) << res.second
                    << std::setw(print_col_size) << res.first / cpu_freq
                    << std::setw(print_col_size) << res.second / cpu_freq
                    << std::endl;
            }
        };

    std::cout <<
        "tsc frequency, Ghz       : " << cpu_freq << "\n"
        "working set size, Mb     : " << max_working_set_size / 1024 / 1024 << "\n"
        "single data item size, b : " << sizeof(node) << "\n"
        "--------------------------------------\n" <<
        std::setw(print_col_size) << "working set"sv <<
        std::setw(print_col_size) << "mean, cycles"sv <<
        std::setw(print_col_size) << "median, cycles"sv <<
        std::setw(print_col_size) << "mean, ns"sv <<
        std::setw(print_col_size) << "median, ns"sv << '\n';

    for (std::size_t ws_size = s_cache_line_size * 2; ws_size <= max_working_set_size; ws_size <<= 1) {
        run_and_print_test(ws_size, container.get());
        auto next_ws_size = ws_size * 3 / 2;
        if (next_ws_size <= max_working_set_size)
            run_and_print_test(next_ws_size, container.get());
    }

    return 0;
}
