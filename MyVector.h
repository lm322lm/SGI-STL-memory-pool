#ifndef MY_VECTOR_H_
#define MY_VECTOR_H_

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <memory>
#include <algorithm>
#include <initializer_list>

// 简易 vector 实现，支持自定义 Allocator
template <class T, class Alloc = std::allocator<T>>
class MyVector {
public:
    using value_type      = T;
    using allocator_type  = Alloc;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using iterator        = T*;
    using const_iterator  = const T*;

private:
    using AllocTraits = std::allocator_traits<Alloc>;

    pointer _start = nullptr;
    pointer _finish = nullptr;
    pointer _end_of_storage = nullptr;
    allocator_type _alloc;

    // 销毁 [first, last) 范围内的对象
    void destroy_range(pointer first, pointer last) {
        for (; first != last; ++first) {
            AllocTraits::destroy(_alloc, first);
        }
    }

    // 释放全部内存
    void deallocate_all() {
        if (_start) {
            _alloc.deallocate(_start, capacity());
        }
    }

    // 扩容：2倍增长策略
    void expand(size_type min_cap) {
        size_type old_cap = capacity();
        size_type new_cap = old_cap ? old_cap * 2 : 1;
        if (new_cap < min_cap) new_cap = min_cap;

        pointer new_start = AllocTraits::allocate(_alloc, new_cap);
        pointer new_finish = new_start;

        // 移动旧元素到新空间
        for (pointer p = _start; p != _finish; ++p, ++new_finish) {
            AllocTraits::construct(_alloc, new_finish, std::move(*p));
        }

        // 销毁旧元素并释放旧空间
        destroy_range(_start, _finish);
        deallocate_all();

        _start = new_start;
        _finish = new_finish;
        _end_of_storage = new_start + new_cap;
    }

public:
    // ---------- 构造 / 析构 ----------
    MyVector() noexcept = default;

    explicit MyVector(size_type n, const T& val = T()) {
        _start = AllocTraits::allocate(_alloc, n);
        _end_of_storage = _start + n;
        _finish = _start;
        for (size_type i = 0; i < n; ++i, ++_finish) {
            AllocTraits::construct(_alloc, _finish, val);
        }
    }

    MyVector(std::initializer_list<T> il) {
        size_type n = il.size();
        _start = AllocTraits::allocate(_alloc, n);
        _end_of_storage = _start + n;
        _finish = _start;
        for (auto& v : il) {
            AllocTraits::construct(_alloc, _finish, v);
            ++_finish;
        }
    }

    // 拷贝构造
    MyVector(const MyVector& other) {
        size_type n = other.size();
        _start = AllocTraits::allocate(_alloc, n);
        _end_of_storage = _start + n;
        _finish = _start;
        for (const_pointer p = other._start; p != other._finish; ++p, ++_finish) {
            AllocTraits::construct(_alloc, _finish, *p);
        }
    }
    // 移动构造
    MyVector(MyVector&& other) noexcept
        : _start(other._start), _finish(other._finish),
          _end_of_storage(other._end_of_storage), _alloc(std::move(other._alloc)) {
        other._start = other._finish = other._end_of_storage = nullptr;
    }

    // 拷贝赋值
    MyVector& operator=(const MyVector& other) {
        if (this != &other) {
            MyVector tmp(other);
            swap(tmp);
        }
        return *this;
    }

    // 移动赋值
    MyVector& operator=(MyVector&& other) noexcept {
        if (this != &other) {
            destroy_range(_start, _finish);
            deallocate_all();
            _start = other._start;
            _finish = other._finish;
            _end_of_storage = other._end_of_storage;
            other._start = other._finish = other._end_of_storage = nullptr;
        }
        return *this;
    }

    ~MyVector() {
        destroy_range(_start, _finish);
        deallocate_all();
    }

    // ---------- 容量 ----------
    size_type size()     const noexcept { return static_cast<size_type>(_finish - _start); }
    size_type capacity() const noexcept { return static_cast<size_type>(_end_of_storage - _start); }
    bool      empty()    const noexcept { return _start == _finish; }

    void reserve(size_type n) {
        if (n > capacity()) expand(n);
    }

    // ---------- 元素访问 ----------
    reference       operator[](size_type i)       { return _start[i]; }
    const_reference operator[](size_type i) const { return _start[i]; }

    reference       front()       { return *_start; }
    const_reference front() const { return *_start; }
    reference       back()        { return *(_finish - 1); }
    const_reference back()  const { return *(_finish - 1); }
    pointer         data()        noexcept { return _start; }
    const_pointer   data()  const noexcept { return _start; }
    // ---------- 迭代器 ----------
    iterator       begin()        noexcept { return _start; }
    const_iterator begin()  const noexcept { return _start; }
    iterator       end()          noexcept { return _finish; }
    const_iterator end()    const noexcept { return _finish; }

    // ---------- 修改器 ----------
    void push_back(const T& val) {
        if (_finish == _end_of_storage) expand(size() + 1);
        AllocTraits::construct(_alloc, _finish, val);
        ++_finish;
    }

    void push_back(T&& val) {
        if (_finish == _end_of_storage) expand(size() + 1);
        AllocTraits::construct(_alloc, _finish, std::move(val));
        ++_finish;
    }

    template <class... Args>
    void emplace_back(Args&&... args) {
        if (_finish == _end_of_storage) expand(size() + 1);
        AllocTraits::construct(_alloc, _finish, std::forward<Args>(args)...);
        ++_finish;
    }

    void pop_back() {
        --_finish;
        AllocTraits::destroy(_alloc, _finish);
    }

    void clear() {
        destroy_range(_start, _finish);
        _finish = _start;
    }

    void swap(MyVector& other) noexcept {
        std::swap(_start, other._start);
        std::swap(_finish, other._finish);
        std::swap(_end_of_storage, other._end_of_storage);
        std::swap(_alloc, other._alloc);
    }
};

#endif // MY_VECTOR_H_
