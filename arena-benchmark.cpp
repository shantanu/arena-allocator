#include <iostream>
#include <vector>
#include <array>
#include <memory>
#include <cstdint>


void* operator new(size_t n)
{
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc{};
    std::cout << "Allocated " << n << " bytes\n";
    return p;
}

void operator delete(void* p) noexcept
{
    std::cout << "Freeing memory\n";
    return std::free(p);
}

template<size_t N>
class Arena
{
private:
    static constexpr size_t alignment = alignof(std::max_align_t);
    alignas(alignment) std::array<std::byte, N> d_buffer;
    std::byte* d_next;

public:
    Arena () noexcept :d_next{&d_buffer[0]}  {}
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    ~Arena() noexcept {
        std::cout << "freeing arena with used= " << used() << std::endl; 
    }

    size_t used() const noexcept {
        return static_cast<size_t>(d_next - &d_buffer[0]);
    }

    std::byte* allocate(size_t n) {

        const size_t aligned_n = align_up(n);
        const size_t bytes_free = N - used();

        std::cout << static_cast<size_t>(d_next - &d_buffer[0]) << std::endl;
        std::cout << "Allocating in arena " << n << " bytes" << std::endl;

        std::cout << "aligned_n=" << aligned_n << " bytes_free=" << bytes_free << std::endl; 
        if (bytes_free >= aligned_n)
        {
            std::cout << "Found space in buffer" << std::endl;
            std::byte* ret = d_next;
            d_next += aligned_n;
            std::cout << "Now used = " << used() << std::endl;
            return ret;
        }
        // default to the global allocator
        return static_cast<std::byte*>(::operator new(n));
    }

    void deallocate(std::byte* p, size_t n) noexcept {
        std::cout << "Deallocating in arena " << n << " bytes" << std::endl;
        if (pointer_in_buffer(p)) {
            if (p + n == d_next) {
                std::cout << "just moving pointer back" << std::endl;
                d_next = p;
                std::cout << "now used = " << used() << std::endl;
                return;
            }
        }
        else {
            return ::operator delete(p);
        }
    }


private:
    static constexpr size_t align_up(size_t n) noexcept {
        return (n + alignment-1) & ~(alignment-1);
    }

    bool pointer_in_buffer(const std::byte* p) const noexcept {
        return std::uintptr_t(p) >= std::uintptr_t(&d_buffer[0]) && 
                std::uintptr_t(p) < std::uintptr_t(&d_buffer[0]) + N;
    }
};

template <class T, size_t N>
class ShortAlloc
{
public:
    using value_type = T;
    using arena_type = Arena<N>;

     template <class U> struct rebind {typedef ShortAlloc<U, N> other;};

    ShortAlloc(arena_type& arena) noexcept :d_arena{&arena} {} 
    
    ShortAlloc(const ShortAlloc&) = default;
    ShortAlloc& operator=(const ShortAlloc&) = default;


    template<class U>
    ShortAlloc(const ShortAlloc<U, N>& other) noexcept
        : d_arena{other.d_arena} 
    {}


    value_type* allocate(std::size_t n)
    {
        std::cout << "allocating in ShortAlloc " << n << " value_types" << std::endl; 
        return reinterpret_cast<value_type*>(d_arena->allocate(n*sizeof(value_type)));
    }

    void deallocate(value_type* p, std::size_t n) noexcept 
    {
        std::cout << "freeing in ShortAlloc " << n << " value_types" << std::endl;
        return d_arena->deallocate(reinterpret_cast<std::byte*>(p), n*sizeof(value_type));
    }

private:
    arena_type* d_arena;
};

template <class T, class U, size_t N, size_t M>
bool
operator==(ShortAlloc<T, N> const& lhs, ShortAlloc<U, M> const& rhs) noexcept
{
    return N == M && lhs.d_arena == rhs.arena;
}

template <class T, class U, size_t N, size_t M>
bool
operator!=(ShortAlloc<T, N> const& lhs, ShortAlloc<U, M> const& rhs) noexcept
{
    return !(N == M && lhs.d_arena == rhs.d_arena);
}


template<size_t N>
struct TestClass {
    std::array<std::byte, N> data;

    TestClass() noexcept {}
    TestClass(const TestClass&) {}
    TestClass& operator=(const TestClass&) {}
    TestClass(TestClass&&) noexcept {}
    TestClass& operator=(TestClass&&) noexcept {}
};

constexpr size_t N_ITER = 100;

constexpr size_t DATA_SIZE = 1024;
using MyTestClass = TestClass<DATA_SIZE>;
constexpr size_t ARENA_SIZE = DATA_SIZE*128;

int main() {

    using MyAlloc = ShortAlloc<MyTestClass, ARENA_SIZE>;

    MyAlloc::arena_type arena{};
    std::vector<MyTestClass, MyAlloc> shortVec {arena};

    // std::cout << "Created shortVec\n";
    for (int i = 0; i < N_ITER; i++) {
        shortVec.emplace_back();
    }


    std::vector<MyTestClass> vec;
    for (int i = 0; i < N_ITER; i++) {
      vec.emplace_back();
    }

}
