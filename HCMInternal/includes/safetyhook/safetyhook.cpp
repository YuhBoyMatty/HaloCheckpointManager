// DO NOT EDIT. This file is auto-generated by `amalgamate.py`.

#define NOMINMAX

#include <safetyhook.hpp>


//
// Source file: allocator.cpp
//

#include <algorithm>
#include <cassert>
#include <functional>
#include <limits>

#define NOMINMAX
#include <Windows.h>


namespace safetyhook {
constexpr auto align_up(uintptr_t address, size_t align) {
    return (address + align - 1) & ~(align - 1);
}

constexpr auto align_down(uintptr_t address, size_t align) {
    return address & ~(align - 1);
}

Allocation::Allocation(Allocation&& other) noexcept {
    *this = std::move(other);
}

Allocation& Allocation::operator=(Allocation&& other) noexcept {
    if (this != &other) {
        free();

        m_allocator = std::move(other.m_allocator);
        m_address = other.m_address;
        m_size = other.m_size;

        other.m_address = 0;
        other.m_size = 0;
    }

    return *this;
}

Allocation::~Allocation() {
    free();
}

void Allocation::free() {
    if (m_allocator && m_address != 0 && m_size != 0) {
        m_allocator->free(m_address, m_size);
        m_address = 0;
        m_size = 0;
        m_allocator.reset();
    }
}

Allocation::Allocation(std::shared_ptr<Allocator> allocator, uintptr_t address, size_t size) noexcept
    : m_allocator{std::move(allocator)}, m_address{address}, m_size{size} {
}

std::shared_ptr<Allocator> Allocator::global() {
    static std::weak_ptr<Allocator> global_allocator{};
    static std::mutex global_allocator_mutex{};

    std::scoped_lock lock{global_allocator_mutex};

    if (auto allocator = global_allocator.lock()) {
        return allocator;
    }

    auto allocator = Allocator::create();

    global_allocator = allocator;

    return allocator;
}

std::shared_ptr<Allocator> Allocator::create() {
    return std::shared_ptr<Allocator>{new Allocator{}};
}

std::expected<Allocation, Allocator::Error> Allocator::allocate(size_t size) {
    return allocate_near({}, size, std::numeric_limits<size_t>::max());
}

std::expected<Allocation, Allocator::Error> Allocator::allocate_near(
    const std::vector<uintptr_t>& desired_addresses, size_t size, size_t max_distance) {
    std::scoped_lock lock{m_mutex};
    return internal_allocate_near(desired_addresses, size, max_distance);
}

void Allocator::free(uintptr_t address, size_t size) {
    std::scoped_lock lock{m_mutex};
    return internal_free(address, size);
}

std::expected<Allocation, Allocator::Error> Allocator::internal_allocate_near(
    const std::vector<uintptr_t>& desired_addresses, size_t size, size_t max_distance) {
    // First search through our list of allocations for a free block that is large
    // enough.
    for (const auto& allocation : m_memory) {
        if (allocation->size < size) {
            continue;
        }

        for (auto node = allocation->freelist.get(); node != nullptr; node = node->next.get()) {
            // Enough room?
            if (node->end - node->start < size) {
                continue;
            }

            const auto address = node->start;

            // Close enough?
            if (!in_range(address, desired_addresses, max_distance)) {
                continue;
            }

            node->start += size;

            return Allocation{shared_from_this(), address, size};
        }
    }

    // If we didn't find a free block, we need to allocate a new one.
    SYSTEM_INFO si{};

    GetSystemInfo(&si);

    const auto allocation_size = align_up(size, si.dwAllocationGranularity);
    const auto allocation_address = allocate_nearby_memory(desired_addresses, allocation_size, max_distance);

    if (!allocation_address) {
        return std::unexpected{allocation_address.error()};
    }

    const auto& allocation = m_memory.emplace_back(new Memory);

    allocation->address = *allocation_address;
    allocation->size = allocation_size;
    allocation->freelist = std::make_unique<FreeNode>();
    allocation->freelist->start = *allocation_address + size;
    allocation->freelist->end = *allocation_address + allocation_size;

    return Allocation{shared_from_this(), *allocation_address, size};
}

void Allocator::internal_free(uintptr_t address, size_t size) {
    for (const auto& allocation : m_memory) {
        if (allocation->address > address || allocation->address + allocation->size < address) {
            continue;
        }

        // Find the right place for our new freenode.
        FreeNode* prev{};

        for (auto node = allocation->freelist.get(); node != nullptr; prev = node, node = node->next.get()) {
            if (node->start > address) {
                break;
            }
        }

        // Add new freenode.
        auto free_node = std::make_unique<FreeNode>();

        free_node->start = address;
        free_node->end = address + size;

        if (prev == nullptr) {
            free_node->next.swap(allocation->freelist);
            allocation->freelist.swap(free_node);
        } else {
            free_node->next.swap(prev->next);
            prev->next.swap(free_node);
        }

        combine_adjacent_freenodes(*allocation);
        break;
    }
}

void Allocator::combine_adjacent_freenodes(Memory& memory) {
    for (auto prev = memory.freelist.get(), node = prev; node != nullptr; node = node->next.get()) {
        if (prev->end == node->start) {
            prev->end = node->end;
            prev->next.swap(node->next);
            node->next.reset();
            node = prev;
        } else {
            prev = node;
        }
    }
}

std::expected<uintptr_t, Allocator::Error> Allocator::allocate_nearby_memory(
    const std::vector<uintptr_t>& desired_addresses, size_t size, size_t max_distance) {
    if (desired_addresses.empty()) {
        if (const auto result = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            result != nullptr) {
            return reinterpret_cast<uintptr_t>(result);
        }

        return std::unexpected{Error::BAD_VIRTUAL_ALLOC};
    }

    auto attempt_allocation = [&](uintptr_t p) -> uintptr_t {
        if (!in_range(p, desired_addresses, max_distance)) {
            return 0;
        }

        return reinterpret_cast<uintptr_t>(
            VirtualAlloc(reinterpret_cast<LPVOID>(p), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    };

    SYSTEM_INFO si{};

    GetSystemInfo(&si);

    auto desired_address = desired_addresses[0];
    auto search_start = std::numeric_limits<uintptr_t>::min();
    auto search_end = std::numeric_limits<uintptr_t>::max();

    if (desired_address > max_distance) {
        search_start = desired_address - max_distance;
    }

    if (std::numeric_limits<uintptr_t>::max() - desired_address > max_distance) {
        search_end = desired_address + max_distance;
    }

    search_start = std::max(search_start, reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress));
    search_end = std::min(search_end, reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress));
    desired_address = align_up(desired_address, si.dwAllocationGranularity);
    MEMORY_BASIC_INFORMATION mbi{};

    // Search backwards from the desired_address.
    for (auto p = desired_address; p > search_start && in_range(p, desired_addresses, max_distance);
         p = align_down(reinterpret_cast<uintptr_t>(mbi.AllocationBase) - 1, si.dwAllocationGranularity)) {
        if (VirtualQuery(reinterpret_cast<LPCVOID>(p), &mbi, sizeof(mbi)) == 0) {
            break;
        }

        if (mbi.State != MEM_FREE) {
            continue;
        }

        if (auto allocation_address = attempt_allocation(p); allocation_address != 0) {
            return allocation_address;
        }
    }

    // Search forwards from the desired_address.
    for (auto p = desired_address; p < search_end && in_range(p, desired_addresses, max_distance);
         p += mbi.RegionSize) {
        if (VirtualQuery(reinterpret_cast<LPCVOID>(p), &mbi, sizeof(mbi)) == 0) {
            break;
        }

        if (mbi.State != MEM_FREE) {
            continue;
        }

        if (auto allocation_address = attempt_allocation(p); allocation_address != 0) {
            return allocation_address;
        }
    }

    return std::unexpected{Error::NO_MEMORY_IN_RANGE};
}

bool Allocator::in_range(uintptr_t address, const std::vector<uintptr_t>& desired_addresses, size_t max_distance) {
    for (auto&& desired_address : desired_addresses) {
        auto delta = (address > desired_address) ? address - desired_address : desired_address - address;
        if (delta > max_distance) {
            return false;
        }
    }

    return true;
}

Allocator::Memory::~Memory() {
    VirtualFree(reinterpret_cast<LPVOID>(address), 0, MEM_RELEASE);
}
} // namespace safetyhook

//
// Source file: easy.cpp
//


namespace safetyhook {
InlineHook create_inline(void* target, void* destination) {
    return create_inline(reinterpret_cast<uintptr_t>(target), reinterpret_cast<uintptr_t>(destination));
}

InlineHook create_inline(uintptr_t target, uintptr_t destination) {
    if (auto hook = InlineHook::create(target, destination)) {
        return std::move(*hook);
    } else {
        return {};
    }
}

MidHook create_mid(void* target, MidHookFn destination) {
    return create_mid(reinterpret_cast<uintptr_t>(target), destination);
}

MidHook create_mid(uintptr_t target, MidHookFn destination) {
    if (auto hook = MidHook::create(target, destination)) {
        return std::move(*hook);
    } else {
        return {};
    }
}
} // namespace safetyhook

//
// Source file: inline_hook.cpp
//

#include <iterator>

#include <Windows.h>

#if __has_include(<Zydis/Zydis.h>)
#include <Zydis/Zydis.h>
#elif __has_include(<Zydis.h>)
#include <Zydis.h>
#else
#error "Zydis not found"
#endif



namespace safetyhook {
class UnprotectMemory {
public:
    UnprotectMemory(uintptr_t address, size_t size) : m_address{address}, m_size{size} {
        VirtualProtect(reinterpret_cast<LPVOID>(m_address), m_size, PAGE_EXECUTE_READWRITE, &m_protect);
    }

    ~UnprotectMemory() { VirtualProtect(reinterpret_cast<LPVOID>(m_address), m_size, m_protect, &m_protect); }

private:
    uintptr_t m_address{};
    size_t m_size{};
    DWORD m_protect{};
};

#pragma pack(push, 1)
struct JmpE9 {
    uint8_t opcode{0xE9};
    uint32_t offset{0};
};

#if defined(_M_X64)
struct JmpFF {
    uint8_t opcode0{0xFF};
    uint8_t opcode1{0x25};
    uint32_t offset{0};
};

struct TrampolineEpilogueE9 {
    JmpE9 jmp_to_original{};
    JmpFF jmp_to_destination{};
    uint64_t destination_address{};
};

struct TrampolineEpilogueFF {
    JmpFF jmp_to_original{};
    uint64_t original_address{};
};
#elif defined(_M_IX86)
struct TrampolineEpilogueE9 {
    JmpE9 jmp_to_original{};
    JmpE9 jmp_to_destination{};
};
#endif
#pragma pack(pop)

#ifdef _M_X64
static auto make_jmp_ff(uintptr_t src, uintptr_t dst, uintptr_t data) {
    JmpFF jmp{};

    jmp.offset = static_cast<uint32_t>(data - src - sizeof(jmp));
    *reinterpret_cast<uintptr_t*>(data) = dst;

    return jmp;
}

static void emit_jmp_ff(uintptr_t src, uintptr_t dst, uintptr_t data, size_t size = sizeof(JmpFF)) {
    if (size < sizeof(JmpFF)) {
        return;
    }

    UnprotectMemory unprotect{src, size};

    if (size > sizeof(JmpFF)) {
        std::fill_n(reinterpret_cast<uint8_t*>(src), size, static_cast<uint8_t>(0x90));
    }

    *reinterpret_cast<JmpFF*>(src) = make_jmp_ff(src, dst, data);
}
#endif

constexpr auto make_jmp_e9(uintptr_t src, uintptr_t dst) {
    JmpE9 jmp{};

    jmp.offset = static_cast<uint32_t>(dst - src - sizeof(jmp));

    return jmp;
}

static void emit_jmp_e9(uintptr_t src, uintptr_t dst, size_t size = sizeof(JmpE9)) {
    if (size < sizeof(JmpE9)) {
        return;
    }

    UnprotectMemory unprotect{src, size};

    if (size > sizeof(JmpE9)) {
        std::fill_n(reinterpret_cast<uint8_t*>(src), size, static_cast<uint8_t>(0x90));
    }

    *reinterpret_cast<JmpE9*>(src) = make_jmp_e9(src, dst);
}

static bool decode(ZydisDecodedInstruction* ix, uintptr_t ip) {
    ZydisDecoder decoder{};
    ZyanStatus status{};

#if defined(_M_X64)
    status = ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
#elif defined(_M_IX86)
    status = ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
#else
#error "Unsupported architecture"
#endif

    if (!ZYAN_SUCCESS(status)) {
        return false;
    }

    return ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(&decoder, nullptr, reinterpret_cast<const void*>(ip), 15, ix));
}

std::expected<InlineHook, InlineHook::Error> InlineHook::create(void* target, void* destination) {
    return create(Allocator::global(), target, destination);
}

std::expected<InlineHook, InlineHook::Error> InlineHook::create(uintptr_t target, uintptr_t destination) {
    return create(Allocator::global(), target, destination);
}

std::expected<InlineHook, InlineHook::Error> InlineHook::create(
    const std::shared_ptr<Allocator>& allocator, void* target, void* destination) {
    return create(allocator, reinterpret_cast<uintptr_t>(target), reinterpret_cast<uintptr_t>(destination));
}

std::expected<InlineHook, InlineHook::Error> InlineHook::create(
    const std::shared_ptr<Allocator>& allocator, uintptr_t target, uintptr_t destination) {
    InlineHook hook{};

    if (const auto setup_result = hook.setup(allocator, target, destination); !setup_result) {
        return std::unexpected{setup_result.error()};
    }

    return hook;
}

InlineHook::InlineHook(InlineHook&& other) noexcept {
    *this = std::move(other);
}

InlineHook& InlineHook::operator=(InlineHook&& other) noexcept {
    if (this != &other) {
        destroy();

        std::scoped_lock lock{m_mutex, other.m_mutex};

        m_target = other.m_target;
        m_destination = other.m_destination;
        m_trampoline = std::move(other.m_trampoline);
        m_trampoline_size = other.m_trampoline_size;
        m_original_bytes = std::move(other.m_original_bytes);

        other.m_target = 0;
        other.m_destination = 0;
        other.m_trampoline_size = 0;
    }

    return *this;
}

InlineHook::~InlineHook() {
    destroy();
}

void InlineHook::reset() {
    *this = {};
}

std::expected<void, InlineHook::Error> InlineHook::setup(
    const std::shared_ptr<Allocator>& allocator, uintptr_t target, uintptr_t destination) {
    m_target = target;
    m_destination = destination;

    if (const auto e9_result = e9_hook(allocator); !e9_result) {
#ifdef _M_X64
        if (const auto ff_result = ff_hook(allocator); !ff_result) {
            return ff_result;
        }
#else
        return e9_result;
#endif
    }

    return {};
}

std::expected<void, InlineHook::Error> InlineHook::e9_hook(const std::shared_ptr<Allocator>& allocator) {
    m_original_bytes.clear();
    m_trampoline_size = sizeof(TrampolineEpilogueE9);

    std::vector<uintptr_t> desired_addresses{m_target};
    ZydisDecodedInstruction ix{};

    for (uintptr_t ip = m_target; ip < m_target + sizeof(JmpE9); ip += ix.length) {
        if (!decode(&ix, ip)) {
            return std::unexpected{Error::failed_to_decode_instruction(ip)};
        }

        m_trampoline_size += ix.length;
        m_original_bytes.insert(
            m_original_bytes.end(), reinterpret_cast<uint8_t*>(ip), reinterpret_cast<uint8_t*>(ip) + ix.length);

        const auto is_relative = (ix.attributes & ZYDIS_ATTRIB_IS_RELATIVE) != 0;

        if (is_relative) {
            if (ix.raw.disp.size == 32) {
                const auto target_address = ip + ix.length + static_cast<int32_t>(ix.raw.disp.value);
                desired_addresses.emplace_back(target_address);
            } else if (ix.raw.imm[0].size == 32) {
                const auto target_address = ip + ix.length + static_cast<int32_t>(ix.raw.imm[0].value.s);
                desired_addresses.emplace_back(target_address);
            } else if (ix.meta.category == ZYDIS_CATEGORY_COND_BR && ix.meta.branch_type == ZYDIS_BRANCH_TYPE_SHORT) {
                const auto target_address = ip + ix.length + static_cast<int32_t>(ix.raw.imm[0].value.s);
                desired_addresses.emplace_back(target_address);
                m_trampoline_size += 4; // near conditional branches are 4 bytes larger.
            } else if (ix.meta.category == ZYDIS_CATEGORY_UNCOND_BR && ix.meta.branch_type == ZYDIS_BRANCH_TYPE_SHORT) {
                const auto target_address = ip + ix.length + static_cast<int32_t>(ix.raw.imm[0].value.s);
                desired_addresses.emplace_back(target_address);
                m_trampoline_size += 3; // near unconditional branches are 3 bytes larger.
            } else {
                return std::unexpected{Error::unsupported_instruction_in_trampoline(ip)};
            }
        }
    }

    auto trampoline_allocation = allocator->allocate_near(desired_addresses, m_trampoline_size);

    if (!trampoline_allocation) {
        return std::unexpected{Error::bad_allocation(trampoline_allocation.error())};
    }

    m_trampoline = std::move(*trampoline_allocation);

    for (uintptr_t ip = m_target, tramp_ip = m_trampoline.address(); ip < m_target + m_original_bytes.size();
         ip += ix.length) {
        if (!decode(&ix, ip)) {
            m_trampoline.free();
            return std::unexpected{Error::failed_to_decode_instruction(ip)};
        }

        const auto is_relative = (ix.attributes & ZYDIS_ATTRIB_IS_RELATIVE) != 0;

        if (is_relative && ix.raw.disp.size == 32) {
            std::copy_n(reinterpret_cast<uint8_t*>(ip), ix.length, reinterpret_cast<uint8_t*>(tramp_ip));
            const auto target_address = ip + ix.length + static_cast<int32_t>(ix.raw.disp.value);
            const auto new_disp = static_cast<int32_t>(target_address - (tramp_ip + ix.length));
            *reinterpret_cast<int32_t*>(tramp_ip + ix.raw.disp.offset) = new_disp;
            tramp_ip += ix.length;
        } else if (is_relative && ix.raw.imm[0].size == 32) {
            std::copy_n(reinterpret_cast<uint8_t*>(ip), ix.length, reinterpret_cast<uint8_t*>(tramp_ip));
            const auto target_address = ip + ix.length + static_cast<int32_t>(ix.raw.imm[0].value.s);
            const auto new_disp = static_cast<int32_t>(target_address - (tramp_ip + ix.length));
            *reinterpret_cast<int32_t*>(tramp_ip + ix.raw.imm[0].offset) = new_disp;
            tramp_ip += ix.length;
        } else if (ix.meta.category == ZYDIS_CATEGORY_COND_BR && ix.meta.branch_type == ZYDIS_BRANCH_TYPE_SHORT) {
            const auto target_address = ip + ix.length + static_cast<int32_t>(ix.raw.imm[0].value.s);
            auto new_disp = static_cast<int32_t>(target_address - (tramp_ip + 6));

            // Handle the case where the target is now in the trampoline.
            if (target_address < m_target + m_original_bytes.size()) {
                new_disp = static_cast<int32_t>(ix.raw.imm[0].value.s);
            }

            *reinterpret_cast<uint8_t*>(tramp_ip) = 0x0F;
            *reinterpret_cast<uint8_t*>(tramp_ip + 1) = 0x10 + ix.opcode;
            *reinterpret_cast<int32_t*>(tramp_ip + 2) = new_disp;
            tramp_ip += 6;
        } else if (ix.meta.category == ZYDIS_CATEGORY_UNCOND_BR && ix.meta.branch_type == ZYDIS_BRANCH_TYPE_SHORT) {
            const auto target_address = ip + ix.length + static_cast<int32_t>(ix.raw.imm[0].value.s);
            auto new_disp = static_cast<int32_t>(target_address - (tramp_ip + 5));

            // Handle the case where the target is now in the trampoline.
            if (target_address < m_target + m_original_bytes.size()) {
                new_disp = static_cast<int32_t>(ix.raw.imm[0].value.s);
            }

            *reinterpret_cast<uint8_t*>(tramp_ip) = 0xE9;
            *reinterpret_cast<int32_t*>(tramp_ip + 1) = new_disp;
            tramp_ip += 5;
        } else {
            std::copy_n(reinterpret_cast<uint8_t*>(ip), ix.length, reinterpret_cast<uint8_t*>(tramp_ip));
            tramp_ip += ix.length;
        }
    }

    auto trampoline_epilogue = reinterpret_cast<TrampolineEpilogueE9*>(
        m_trampoline.address() + m_trampoline_size - sizeof(TrampolineEpilogueE9));

    // jmp from trampoline to original.
    auto src = reinterpret_cast<uintptr_t>(&trampoline_epilogue->jmp_to_original);
    auto dst = m_target + m_original_bytes.size();
    emit_jmp_e9(src, dst);

    // jmp from trampoline to destination.
    src = reinterpret_cast<uintptr_t>(&trampoline_epilogue->jmp_to_destination);
    dst = m_destination;

#ifdef _M_X64
    auto data = reinterpret_cast<uintptr_t>(&trampoline_epilogue->destination_address);
    emit_jmp_ff(src, dst, data);
#else
    emit_jmp_e9(src, dst);
#endif

    // jmp from original to trampoline.
    ThreadFreezer freezer{};

    src = m_target;
    dst = reinterpret_cast<uintptr_t>(&trampoline_epilogue->jmp_to_destination);
    emit_jmp_e9(src, dst, m_original_bytes.size());

    for (size_t i = 0; i < m_original_bytes.size(); ++i) {
        freezer.fix_ip(m_target + i, m_trampoline.address() + i);
    }

    return {};
}

#ifdef _M_X64
std::expected<void, InlineHook::Error> InlineHook::ff_hook(const std::shared_ptr<Allocator>& allocator) {
    m_original_bytes.clear();
    m_trampoline_size = sizeof(TrampolineEpilogueFF);
    ZydisDecodedInstruction ix{};

    for (auto ip = m_target; ip < m_target + sizeof(JmpFF) + sizeof(uintptr_t); ip += ix.length) {
        if (!decode(&ix, ip)) {
            return std::unexpected{Error::failed_to_decode_instruction(ip)};
        }

        // We can't support any instruction that is IP relative here because
        // ff_hook should only be called if e9_hook failed indicating that
        // we're likely outside the +- 2GB range.
        if (ix.attributes & ZYDIS_ATTRIB_IS_RELATIVE) {
            return std::unexpected{Error::ip_relative_instruction_out_of_range(ip)};
        }

        m_original_bytes.insert(
            m_original_bytes.end(), reinterpret_cast<uint8_t*>(ip), reinterpret_cast<uint8_t*>(ip + ix.length));
        m_trampoline_size += ix.length;
    }

    auto trampoline_allocation = allocator->allocate(m_trampoline_size);

    if (!trampoline_allocation) {
        return std::unexpected{Error::bad_allocation(trampoline_allocation.error())};
    }

    m_trampoline = std::move(*trampoline_allocation);

    std::copy(m_original_bytes.begin(), m_original_bytes.end(), reinterpret_cast<uint8_t*>(m_trampoline.address()));

    const auto trampoline_epilogue = reinterpret_cast<TrampolineEpilogueFF*>(
        m_trampoline.address() + m_trampoline_size - sizeof(TrampolineEpilogueFF));

    // jmp from trampoline to original.
    auto src = reinterpret_cast<uintptr_t>(&trampoline_epilogue->jmp_to_original);
    auto dst = m_target + m_original_bytes.size();
    auto data = reinterpret_cast<uintptr_t>(&trampoline_epilogue->original_address);
    emit_jmp_ff(src, dst, data);

    // jmp from original to trampoline.
    ThreadFreezer freezer{};

    src = m_target;
    dst = m_destination;
    data = src + sizeof(JmpFF);
    emit_jmp_ff(src, dst, data, m_original_bytes.size());

    for (size_t i = 0; i < m_original_bytes.size(); ++i) {
        freezer.fix_ip(m_target + i, m_trampoline.address() + i);
    }

    return {};
}
#endif

void InlineHook::destroy() {
    std::scoped_lock lock{m_mutex};

    if (!m_trampoline) {
        return;
    }

    ThreadFreezer freezer{};
    UnprotectMemory unprotect{m_target, m_original_bytes.size()};

    std::copy(m_original_bytes.begin(), m_original_bytes.end(), reinterpret_cast<uint8_t*>(m_target));

    for (size_t i = 0; i < m_trampoline_size; ++i) {
        freezer.fix_ip(m_trampoline.address() + i, m_target + i);
    }

    m_trampoline.free();
}
} // namespace safetyhook

//
// Source file: mid_hook.cpp
//

#include <algorithm>



namespace safetyhook {

#ifdef _M_X64
constexpr uint8_t asm_data[] = {0x54, 0x55, 0x50, 0x53, 0x51, 0x52, 0x56, 0x57, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52,
    0x41, 0x53, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x9C, 0x48, 0x8D, 0x0C, 0x24, 0x48, 0x89, 0xE3, 0x48,
    0x83, 0xEC, 0x30, 0x48, 0x83, 0xE4, 0xF0, 0xFF, 0x15, 0x22, 0x00, 0x00, 0x00, 0x48, 0x89, 0xDC, 0x9D, 0x41, 0x5F,
    0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5F, 0x5E, 0x5A, 0x59, 0x5B,
    0x58, 0x5D, 0x5C, 0xFF, 0x25, 0x08, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
#else
constexpr uint8_t asm_data[] = {0x54, 0x55, 0x50, 0x53, 0x51, 0x52, 0x56, 0x57, 0x9C, 0x54, 0xFF, 0x15, 0x00, 0x00,
    0x00, 0x00, 0x83, 0xC4, 0x04, 0x9D, 0x5F, 0x5E, 0x5A, 0x59, 0x5B, 0x58, 0x5D, 0x5C, 0xFF, 0x25, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif

std::expected<MidHook, MidHook::Error> MidHook::create(void* target, MidHookFn destination) {
    return create(Allocator::global(), target, destination);
}

std::expected<MidHook, MidHook::Error> MidHook::create(uintptr_t target, MidHookFn destination) {
    return create(Allocator::global(), target, destination);
}

std::expected<MidHook, MidHook::Error> MidHook::create(
    const std::shared_ptr<Allocator>& allocator, void* target, MidHookFn destination) {
    return create(allocator, reinterpret_cast<uintptr_t>(target), destination);
}

std::expected<MidHook, MidHook::Error> MidHook::create(
    const std::shared_ptr<Allocator>& allocator, uintptr_t target, MidHookFn destination) {
    MidHook hook{};

    if (const auto setup_result = hook.setup(allocator, target, destination); !setup_result) {
        return std::unexpected{setup_result.error()};
    }

    return hook;
}

MidHook::MidHook(MidHook&& other) noexcept {
    *this = std::move(other);
}

MidHook& MidHook::operator=(MidHook&& other) noexcept {
    if (this != &other) {
        m_hook = std::move(other.m_hook);
        m_target = other.m_target;
        m_stub = std::move(other.m_stub);
        m_destination = other.m_destination;

        other.m_target = 0;
        other.m_destination = nullptr;
    }

    return *this;
}

void MidHook::reset() {
    *this = {};
}

std::expected<void, MidHook::Error> MidHook::setup(
    const std::shared_ptr<Allocator>& allocator, uintptr_t target, MidHookFn destination) {
    m_target = target;
    m_destination = destination;

    auto stub_allocation = allocator->allocate(sizeof(asm_data));

    if (!stub_allocation) {
        return std::unexpected{Error::bad_allocation(stub_allocation.error())};
    }

    m_stub = std::move(*stub_allocation);

    std::copy_n(asm_data, sizeof(asm_data), reinterpret_cast<uint8_t*>(m_stub.address()));

#ifdef _M_X64
    *reinterpret_cast<MidHookFn*>(m_stub.address() + sizeof(asm_data) - 16) = m_destination;
#else
    *reinterpret_cast<MidHookFn*>(m_stub.address() + sizeof(asm_data) - 8) = m_destination;

    // 32-bit has some relocations we need to fix up as well.
    *reinterpret_cast<uintptr_t*>(m_stub.address() + 0xA + 2) = m_stub.address() + sizeof(asm_data) - 8;
    *reinterpret_cast<uintptr_t*>(m_stub.address() + 0x1C + 2) = m_stub.address() + sizeof(asm_data) - 4;
#endif

    auto hook_result = InlineHook::create(allocator, m_target, m_stub.address());

    if (!hook_result) {
        m_stub.free();
        return std::unexpected{Error::bad_inline_hook(hook_result.error())};
    }

    m_hook = std::move(*hook_result);

#ifdef _M_X64
    *reinterpret_cast<uintptr_t*>(m_stub.address() + sizeof(asm_data) - 8) = m_hook.trampoline().address();
#else
    *reinterpret_cast<uintptr_t*>(m_stub.address() + sizeof(asm_data) - 4) = m_hook.trampoline().address();
#endif

    return {};
}
} // namespace safetyhook

//
// Source file: thread_freezer.cpp
//

#include <algorithm>

#include <Windows.h>
#include <winternl.h>


#pragma comment(lib, "ntdll")

extern "C" {
NTSTATUS
NTAPI
NtGetNextThread(HANDLE ProcessHandle, HANDLE ThreadHandle, ACCESS_MASK DesiredAccess, ULONG HandleAttributes,
    ULONG Flags, PHANDLE NewThreadHandle);
}

namespace safetyhook {
ThreadFreezer::ThreadFreezer() {
    size_t num_threads_frozen{};

    do {
        num_threads_frozen = m_frozen_threads.size();
        HANDLE thread{};

        while (true) {
            const auto status = NtGetNextThread(GetCurrentProcess(), thread,
                THREAD_QUERY_LIMITED_INFORMATION | THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, 0,
                0, &thread);

            if (!NT_SUCCESS(status)) {
                break;
            }

            const auto thread_id = GetThreadId(thread);
            const auto already_frozen = std::any_of(m_frozen_threads.begin(), m_frozen_threads.end(),
                [=](const auto& thread) { return thread.thread_id == thread_id; });

            // Don't freeze ourselves or threads we already froze.
            if (thread_id == 0 || thread_id == GetCurrentThreadId() || already_frozen) {
                CloseHandle(thread);
                continue;
            }

            auto thread_ctx = CONTEXT{};

            thread_ctx.ContextFlags = CONTEXT_FULL;

            if (SuspendThread(thread) == static_cast<DWORD>(-1) || GetThreadContext(thread, &thread_ctx) == FALSE) {
                CloseHandle(thread);
                continue;
            }

            m_frozen_threads.push_back({thread_id, thread, thread_ctx});
        }
    } while (num_threads_frozen != m_frozen_threads.size());
}

ThreadFreezer::~ThreadFreezer() {
    for (auto& thread : m_frozen_threads) {
        SetThreadContext(thread.handle, &thread.ctx);
        ResumeThread(thread.handle);
        CloseHandle(thread.handle);
    }
}

void ThreadFreezer::fix_ip(uintptr_t old_ip, uintptr_t new_ip) {
    for (auto& thread : m_frozen_threads) {
#ifdef _M_X64
        auto ip = thread.ctx.Rip;
#else
        auto ip = thread.ctx.Eip;
#endif

        if (ip == old_ip) {
            ip = new_ip;
        }

#ifdef _M_X64
        thread.ctx.Rip = ip;
#else
        thread.ctx.Eip = ip;
#endif
    }
}
} // namespace safetyhook