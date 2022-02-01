#include "VMStack.h"

#include <iostream>

const size_t VMSTACK_PAGE_ADDR_BITS = 16;
const size_t VMSTACK_PAGE_SIZE = 1 << VMSTACK_PAGE_ADDR_BITS;

VMFixedStack::VMFixedStack(size_t bytes) : _data(bytes), _len(0) {}
VMFixedStack::~VMFixedStack() {}

void
VMFixedStack::reserve(size_t l) {
    _len += l;
    //std::cout << "reserve " << l << " -> " << _len << "\n";
}

void
VMFixedStack::unreserve(size_t l) {
    _len -= l;
    //std::cout << "unreserve " << l << " -> " << _len << "\n";
}

void
VMFixedStack::unreserve_to(size_t l) {
    _len = l;
}

size_t
VMFixedStack::size() {
    return _len;
}


VMDynamicStack::VMDynamicStack() : _len(0) {
}
VMDynamicStack::~VMDynamicStack() {
}

void
VMDynamicStack::reserve(size_t l) {
    _len += l;
    if (_len >= _pages.size() * VMSTACK_PAGE_SIZE) {
        _pages.emplace_back();
    }
}

void
VMDynamicStack::unreserve_to(size_t l) {
    _len = l;
}

size_t
VMDynamicStack::size() {
    return _len;
}
