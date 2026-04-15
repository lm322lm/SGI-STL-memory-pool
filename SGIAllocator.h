#ifndef SGI_ALLOCATOR_H_
#define SGI_ALLOCATOR_H_

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

namespace sgi_stl {

// ==================== 一级配置器 ====================
// 大于 128 字节的内存请求直接使用 malloc/free
// 提供 OOM handler 机制：内存不足时循环调用用户设置的处理函数
class MallocAlloc {
private:
    static void (*&oom_handler())() {
        static void (*handler)() = nullptr;
        return handler;
    }

    static void* oom_malloc(std::size_t n) {
        void (*my_handler)();
        void* result;
        for (;;) {
            my_handler = oom_handler();
            if (!my_handler) throw std::bad_alloc();
            (*my_handler)();
            result = std::malloc(n);
            if (result) return result;
        }
    }

    static void* oom_realloc(void* p, std::size_t n) {
        void (*my_handler)();
        void* result;
        for (;;) {
            my_handler = oom_handler();
            if (!my_handler) throw std::bad_alloc();
            (*my_handler)();
            result = std::realloc(p, n);
            if (result) return result;
        }
    }
public:
    static void* allocate(std::size_t n) {
        void* result = std::malloc(n);
        if (!result) result = oom_malloc(n);
        return result;
    }

    static void deallocate(void* p, std::size_t) noexcept {
        std::free(p);
    }

    static void* reallocate(void* p, std::size_t, std::size_t new_sz) {
        void* result = std::realloc(p, new_sz);
        if (!result) result = oom_realloc(p, new_sz);
        return result;
    }

    static void (*set_malloc_handler(void (*f)()))() {
        void (*old)() = oom_handler();
        oom_handler() = f;
        return old;
    }
};

// ==================== 二级配置器（内存池） ====================
// 小于等于 128 字节的请求走内存池 + free_list
// 8 字节对齐，16 条 free_list 分别管理 8, 16, 24, ..., 128 字节的小块
class PoolAlloc {
private:
    enum : std::size_t {
        ALIGN = 8,
        MAX_BYTES = 128,
        NFREELISTS = MAX_BYTES / ALIGN
    };

    union FreeNode {
        union FreeNode* next;
        char data[1];
    };

    static std::size_t round_up(std::size_t bytes) noexcept {
        return (bytes + ALIGN - 1) & ~(ALIGN - 1);
    }

    static std::size_t freelist_index(std::size_t bytes) noexcept {
        return (bytes + ALIGN - 1) / ALIGN - 1;
    }
    // 16 条 free_list
    static FreeNode* volatile* free_list_base() {
        static FreeNode* volatile lists[NFREELISTS] = {};
        return lists;
    }

    // 内存池边界
    static char*& start_free() { static char* p = nullptr; return p; }
    static char*& end_free()   { static char* p = nullptr; return p; }
    static std::size_t& heap_size() { static std::size_t s = 0; return s; }

    // 从内存池中取出一大块，切分后补充 free_list
    static void* refill(std::size_t n) {
        int nobjs = 20;
        char* chunk = chunk_alloc(n, nobjs);
        if (nobjs == 1) return chunk;

        FreeNode* volatile* my_list = free_list_base() + freelist_index(n);
        FreeNode* result = reinterpret_cast<FreeNode*>(chunk);
        FreeNode* cur;
        FreeNode* nxt;

        *my_list = nxt = reinterpret_cast<FreeNode*>(chunk + n);
        for (int i = 1; ; ++i) {
            cur = nxt;
            nxt = reinterpret_cast<FreeNode*>(reinterpret_cast<char*>(nxt) + n);
            if (i == nobjs - 1) { cur->next = nullptr; break; }
            else                { cur->next = nxt; }
        }
        return result;
    }
    // 内存池核心：从池中分配 nobjs 个大小为 size 的块
    static char* chunk_alloc(std::size_t size, int& nobjs) {
        char* result;
        std::size_t total = size * static_cast<std::size_t>(nobjs);
        char*& sf = start_free();
        char*& ef = end_free();
        std::size_t& hs = heap_size();
        std::size_t left = static_cast<std::size_t>(ef - sf);

        if (left >= total) {                    // a) 足够
            result = sf;
            sf += total;
            return result;
        }
        if (left >= size) {                     // b) 至少够1个
            nobjs = static_cast<int>(left / size);
            total = size * static_cast<std::size_t>(nobjs);
            result = sf;
            sf += total;
            return result;
        }
        // c) 一个都不够，向系统申请
        std::size_t to_get = 2 * total + round_up(hs >> 4);
        if (left > 0) {                         // 零头挂回 free_list
            FreeNode* volatile* fl = free_list_base() + freelist_index(left);
            reinterpret_cast<FreeNode*>(sf)->next = *fl;
            *fl = reinterpret_cast<FreeNode*>(sf);
        }
        sf = static_cast<char*>(std::malloc(to_get));
        if (!sf) {
            // 从更大的 free_list 借一块应急
            for (std::size_t i = size + ALIGN; i <= MAX_BYTES; i += ALIGN) {
                FreeNode* volatile* fl = free_list_base() + freelist_index(i);
                FreeNode* p = *fl;
                if (p) {
                    *fl = p->next;
                    sf = reinterpret_cast<char*>(p);
                    ef = sf + i;
                    return chunk_alloc(size, nobjs);
                }
            }
            // 最后退回一级配置器（会抛 bad_alloc）
            sf = static_cast<char*>(MallocAlloc::allocate(to_get));
        }
        hs += to_get;
        ef = sf + to_get;
        return chunk_alloc(size, nobjs);
    }

public:
    static void* allocate(std::size_t n) {
        if (n > MAX_BYTES) return MallocAlloc::allocate(n);
        FreeNode* volatile* my_list = free_list_base() + freelist_index(n);
        FreeNode* result = *my_list;
        if (!result) return refill(round_up(n));
        *my_list = result->next;
        return result;
    }

    static void deallocate(void* p, std::size_t n) {
        if (n > MAX_BYTES) { MallocAlloc::deallocate(p, n); return; }
        FreeNode* q = static_cast<FreeNode*>(p);
        FreeNode* volatile* my_list = free_list_base() + freelist_index(n);
        q->next = *my_list;
        *my_list = q;
    }
};
// ==================== STL 标准 Allocator 适配器 ====================
template <class T>
class SGIAllocator {
public:
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <class U> struct rebind { using other = SGIAllocator<U>; };

    SGIAllocator() noexcept = default;
    template <class U> SGIAllocator(const SGIAllocator<U>&) noexcept {}

    [[nodiscard]] pointer allocate(size_type n) {
        if (n == 0) return nullptr;
        return static_cast<pointer>(PoolAlloc::allocate(n * sizeof(T)));
    }

    void deallocate(pointer p, size_type n) noexcept {
        if (p) PoolAlloc::deallocate(static_cast<void*>(p), n * sizeof(T));
    }

    template <class U, class... Args>
    void construct(U* p, Args&&... args) {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U* p) { p->~U(); }

    size_type max_size() const noexcept {
        return static_cast<size_type>(-1) / sizeof(T);
    }

    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;
};

template <class T, class U>
bool operator==(const SGIAllocator<T>&, const SGIAllocator<U>&) noexcept { return true; }

template <class T, class U>
bool operator!=(const SGIAllocator<T>&, const SGIAllocator<U>&) noexcept { return false; }

} // namespace sgi_stl

#endif // SGI_ALLOCATOR_H_
