// Count ordinary allocations in this executable only. This observes the actual
// shared fixture helper; no wall-clock deadline or production allocator changes.
#include "OriginalInfluenceFixture.h"

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
std::atomic<std::size_t> ordinary_allocations{0};

void* allocate(std::size_t size) {
    ordinary_allocations.fetch_add(1,std::memory_order_relaxed);
    if(void* memory=std::malloc(size ? size:1))return memory;
    throw std::bad_alloc();
}

void require(bool condition,const char* message) {
    if(!condition) {
        std::fputs(message,stderr);std::fputc('\n',stderr);std::exit(1);
    }
}
}

// Pair every replaced ordinary allocation form with free, including sized
// deletes selected by a compiler or standard library. Aligned allocation and
// deallocation retain their matching standard-library implementations.
void* operator new(std::size_t size) {return allocate(size);}
void* operator new[](std::size_t size) {return allocate(size);}
void operator delete(void* memory) noexcept {std::free(memory);}
void operator delete[](void* memory) noexcept {std::free(memory);}
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}

int main() {
    namespace fixture=oe_influence_fixture;
    constexpr const char* literal=
        "A successful fixture assertion must preserve all its checks without "
        "constructing this long diagnostic as a temporary heap string. "
        "This message deliberately exceeds every ordinary short-string buffer.";
    // Volatile keeps the predicate a runtime input in optimized builds.
    volatile bool accepted=true;
    const auto before=ordinary_allocations.load(std::memory_order_relaxed);
    fixture::check(accepted,literal);
    const auto after=ordinary_allocations.load(std::memory_order_relaxed);
    require(after==before,"Successful long-literal fixture check allocated");

    bool literal_threw=false;
    try {fixture::check(false,literal);}
    catch(const std::runtime_error& error) {
        literal_threw=true;
        require(std::strcmp(error.what(),literal)==0,"Literal failure text changed");
    }
    require(literal_threw,"False literal fixture check did not throw runtime_error");

    // The original function signature remains available, including callers
    // that explicitly bind its address and messages assembled dynamically.
    using OriginalCheck=void(*)(bool,const std::string&);
    const auto original=static_cast<OriginalCheck>(&fixture::check);
    const std::string dynamic=std::string("Dynamic fixture detail: ")+literal+"\nnext line";
    original(true,dynamic);
    bool dynamic_threw=false;
    try {original(false,dynamic);}
    catch(const std::runtime_error& error) {
        dynamic_threw=true;
        require(dynamic==error.what(),"Dynamic failure text changed");
    }
    require(dynamic_threw,"False dynamic fixture check did not throw runtime_error");

    // Exercise the test-only hook pairings as well; ASan must see no allocator
    // mismatch. Over-aligned objects use the untouched aligned operators.
    void* ordinary=::operator new(17);::operator delete(ordinary,std::size_t{17});
    void* array=::operator new[](31);::operator delete[](array,std::size_t{31});
    struct alignas(64) Aligned {char bytes[64]{};};
    auto* aligned=new Aligned;
    require(reinterpret_cast<std::uintptr_t>(aligned)%alignof(Aligned)==0,
        "Standard aligned allocation lost its alignment");
    delete aligned;
    std::puts("Shared fixture checks preserve failures and allocate zero for successful literals");
    return 0;
}
