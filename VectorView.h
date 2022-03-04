#pragma once

#include <vector>

template <typename T>
class VectorView {
public:
    VectorView(const std::vector<T>& d) : data(d), position(0), length(d.size()) {}
    VectorView(const std::vector<T>& d, size_t p) : data(d), position(p), length(d.size()) {}
    VectorView(const std::vector<T>& d, size_t p, size_t l) : data(d), position(p), length(l) {}

    VectorView slice(size_t p, size_t l) const {
        assert(p < length);
        assert((p + l) <= length);
        return VectorView(data, position + p, l);
    }
    VectorView slice(size_t p) const {
        assert(p < length);
        return VectorView(data, position + p, length - p);
    }

    const T& pull_front() {
        size_t index = position;
        position++;
        length--;
        return data[index];
    }
    const T& peak() {
        return data[position];
    }

    bool empty() const {
        return position >= length;
    }

    size_t size() const {
        return length;
    }
private:
    const std::vector<T>& data;
    size_t position;
    size_t length;
};
