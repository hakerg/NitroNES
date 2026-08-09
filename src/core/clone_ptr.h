#pragma once
#include <memory>

template <typename T>
class clone_ptr {
public:
    clone_ptr() = default;
    clone_ptr(std::nullptr_t) {}
    clone_ptr(std::unique_ptr<T> p) : ptr(std::move(p)) {}

    clone_ptr(const clone_ptr& other) : ptr(other.ptr ? other.ptr->clone() : nullptr) {}
    clone_ptr(clone_ptr&&) = default;

    clone_ptr& operator=(const clone_ptr& other) {
        if (this != &other)
            ptr = other.ptr ? other.ptr->clone() : nullptr;
        return *this;
    }
    clone_ptr& operator=(clone_ptr&&) = default;
    clone_ptr& operator=(std::unique_ptr<T> p) { ptr = std::move(p); return *this; }

    T* operator->() { return ptr.get(); }
    const T* operator->() const { return ptr.get(); }
    T& operator*() { return *ptr; }
    const T& operator*() const { return *ptr; }

    explicit operator bool() const { return ptr != nullptr; }

    T* get() { return ptr.get(); }
    const T* get() const { return ptr.get(); }

private:
    std::unique_ptr<T> ptr;
};
