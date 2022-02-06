#include <cstring>
#include <cstddef>
#include <cstdint>
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

    // Providing an not-to-be-optimized-out destination for traversing over the
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

int main(int argc, char* argv[]) {
    using namespace std::string_view_literals;

    unsigned short cpuid = 1;

    for (int i = 1; i < argc; ++i) {
        if ("-c"sv == argv[i] && i + 1 < argc) {
            std::istringstream is{argv[++i]};
            is >> cpuid;
            if (is.bad() || is.fail()) {
                std::cerr << "unable to convert cpuid into an acceptable number"sv << std::endl;
                return 1;
            }
        } else {
            std::cerr << "unexpected parameters at command line"sv << std::endl;
            return 1;
        }
    }

    if (! set_thread_affinity(cpuid))
        std::cerr << "unable to bound to cpuid "sv << cpuid << std::endl;
    else
        std::cout << "bound to cpuid=" << cpuid << '\n';

    const std::size_t max_working_set_size = 128*1024*1024ul;
    const int print_col_size = 12;
    auto container_size = max_working_set_size / sizeof(node);
    std::unique_ptr<node[]> container{new node[container_size]};

    std::cout.precision(3);
    std::cout << std::fixed;

    auto run_and_print_test = [](std::size_t working_set_size, node* container){
            auto elements = working_set_size / sizeof(node);
            if (elements) {
                auto res = test(container, elements);
                std::cout
                    << std::setw(print_col_size) << elements * sizeof(node)
                    << std::setw(print_col_size) << res.first
                    << std::setw(print_col_size) << res.second
                    << std::endl;
            }
        };

    std::cout
        << "trying randomly accessed wheel with nodes of size="sv << sizeof(node) << "\n\n"
        << std::setw(print_col_size) << "working set"sv
        << std::setw(print_col_size) << "mean"sv
        << std::setw(print_col_size) << "median"sv << '\n';

    for (std::size_t working_set_size = 16; working_set_size <= max_working_set_size; working_set_size <<= 1) {
        run_and_print_test(working_set_size, container.get());
        run_and_print_test(((working_set_size << 1) + working_set_size) >> 1, container.get());
    }

    return 0;
}
