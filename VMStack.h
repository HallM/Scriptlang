#pragma once

#include <cstdlib>
#include <vector>


extern const size_t VMSTACK_PAGE_ADDR_BITS;
extern const size_t VMSTACK_PAGE_SIZE;

struct VMStackPage {
public:
    VMStackPage() {
        stack = malloc(VMSTACK_PAGE_SIZE);
    }
    VMStackPage(size_t size) {
        stack = malloc(size);
    }
    ~VMStackPage() {
        free(stack);
    }
    void* stack;
};

class VMFixedStack {
public:
    VMFixedStack(size_t bytes);
    ~VMFixedStack();

    void reserve(size_t l);
    void unreserve(size_t l);
    void unreserve_to(size_t l);
    size_t size();

    template <typename T>
    T* at(size_t address) {
        return reinterpret_cast<T*>((char*)_data.stack + address);
    }

private:
    VMStackPage _data;
    size_t _len;
};

class VMDynamicStack {
public:
    VMDynamicStack();
    ~VMDynamicStack();

    void reserve(size_t l);
    void unreserve_to(size_t l);
    size_t size();

    template <typename T>
    T* at(size_t address) {
        // extremely dangerous... but no marshalling required.
        size_t page = address >> VMSTACK_PAGE_ADDR_BITS;
        size_t offset = address & (VMSTACK_PAGE_SIZE - 1);
        return reinterpret_cast<T*>((char*)_pages[page].stack + offset);
    }

private:
    std::vector<VMStackPage> _pages;
    size_t _len;
};
