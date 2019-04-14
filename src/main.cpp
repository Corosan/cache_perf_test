#include <cstring>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>

/*
 * The test is dedicated for checking how memory access time depends on used working set size.
 * Double-linked list is built on a continuous block of memory, then nodes are randomly swpped.
 * During the test the list is traversed and each node's value is read from.
 */

const unsigned attempts = 10'000'000;
const std::size_t max_working_set_size = 256 * 1024 * 1024;

inline std::uint64_t rdtsc() {
    unsigned int lo, hi;
    asm volatile ( "rdtsc\n" : "=a" (lo), "=d" (hi) );
    return ((std::uint64_t)hi << 32) | lo;
}

struct node {
    node* m_next;
    node* m_prev;
    std::uint32_t m_data;
};

// Swap locations of two nodes in a double-linked list
void swap(node& n1, node& n2) {
    using std::swap;

    swap(n1.m_prev->m_next, n2.m_prev->m_next);
    swap(n1.m_prev, n2.m_prev);
    swap(n1.m_next->m_prev, n2.m_next->m_prev);
    swap(n1.m_next, n2.m_next);
}

double test(node* container, std::size_t elements_count) {
    if (elements_count < 2)
        return 0.0;

    {
        // build double-linked list from elements in the container
        container[0].m_next = container + 1;
        container[0].m_prev = container + elements_count - 1;
        container[elements_count-1].m_next = container;
        container[elements_count-1].m_prev = container + elements_count - 2;

        for (node* ptr = container + 1; ptr != container + elements_count - 1; ++ptr) {
            ptr->m_next = ptr + 1;
            ptr->m_prev = ptr - 1;
        }
    }

    {
        // randomize elements location in the list
        std::random_device rd;
        std::mt19937 r(rd());

        typedef std::uniform_int_distribution<std::size_t> ud_t;
        ud_t ud;

        for (auto ix = elements_count - 1; ix > 0; --ix)
            swap(container[ud(r, ud_t::param_type{0, ix - 1})], container[ix]);
    }

    // hope reading into this variable will not be optimized out
    static volatile std::uint32_t tmp_destination;

    auto start = rdtsc(); //std::chrono::steady_clock::now();

    node* p = container;
    for (unsigned i = 0; i < attempts; ++i) {
        tmp_destination = p->m_data;
        p = p->m_next;
    }

    return static_cast<double>(rdtsc() - start) / attempts; //std::chrono::steady_clock::now() - start;
}

int main(int argc, char* argv[]) {
    auto container_size = max_working_set_size / sizeof(node);
    node* container = new node[container_size];
    std::memset(container, 0, sizeof(node) * container_size);

    std::cout.precision(3);
    std::cout << std::fixed;

    for (std::size_t working_set_size = 16; working_set_size <= max_working_set_size; working_set_size <<= 1) {
        auto elements = working_set_size / sizeof(node);
        if (elements > 0)
            std::cout
                << std::setw(12) << working_set_size
                << std::setw(12) << test(container, elements) << std::endl;
    }

    delete[] container;
    return 0;
}
