#include <basalt/wire/common/timings/NativeSampler.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
#define BASALT_NATIVE_SAMPLING 1
#include <cxxabi.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/thread_info.h>
#include <sys/ucontext.h>
#elif defined(BASALT_NATIVE_SAMPLING)
#include <dirent.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <ucontext.h>
#endif

namespace basalt::wire::common {

void nameCurrentThread(std::string_view name) {
#if defined(__APPLE__)
    const std::string copy{name};
    pthread_setname_np(copy.c_str());
#elif defined(BASALT_NATIVE_SAMPLING)
    // Linux keeps fifteen characters, and a longer name would be refused rather than cut.
    const std::string copy{name.substr(0, 15)};
    pthread_setname_np(pthread_self(), copy.c_str());
#else
    static_cast<void>(name);
#endif
}

std::string withoutArguments(std::string_view demangled) {
    constexpr std::string_view kAnonymous = "(anonymous namespace)";
    std::string out;
    int braces = 0;
    for (std::size_t i = 0; i < demangled.size();) {
        const std::string_view rest = demangled.substr(i);
        const char c = demangled[i];
        const bool afterOperator = out.ends_with("operator");
        if (rest.starts_with(kAnonymous)) {
            out += kAnonymous;
            i += kAnonymous.size();
        } else if (rest.starts_with("[abi:")) {
            const std::size_t end = rest.find(']');
            i += end == std::string_view::npos ? rest.size() : end + 1;
        } else if (c == '{') {
            ++braces;
            out += c;
            ++i;
        } else if (c == '}') {
            braces = std::max(braces - 1, 0);
            out += c;
            ++i;
        } else if (braces > 0) {
            out += c;
            ++i;
        } else if (c == '<' && !afterOperator) {
            int depth = 0;
            while (i < demangled.size()) {
                depth += demangled[i] == '<' ? 1 : demangled[i] == '>' ? -1 : 0;
                ++i;
                if (depth == 0) {
                    break;
                }
            }
        } else if (c == '(' && afterOperator && rest.starts_with("()")) {
            out += "()";
            i += 2;
        } else if (c == '(') {
            int depth = 0;
            while (i < demangled.size()) {
                depth += demangled[i] == '(' ? 1 : demangled[i] == ')' ? -1 : 0;
                ++i;
                if (depth == 0) {
                    break;
                }
            }
        } else {
            out += c;
            ++i;
        }
    }
    for (const std::string_view qualifier : {" const", " volatile", " noexcept", " &&", " &"}) {
        while (out.ends_with(qualifier)) {
            out.erase(out.size() - qualifier.size());
        }
    }
    // A template function's return type leads its name, and it is the only thing separated from it by a space.
    if (out.find("operator") == std::string::npos) {
        int open = 0;
        std::size_t split = std::string::npos;
        for (std::size_t i = 0; i < out.size(); ++i) {
            open += out[i] == '(' || out[i] == '{' ? 1 : out[i] == ')' || out[i] == '}' ? -1 : 0;
            if (out[i] == ' ' && open == 0) {
                split = i;
            }
        }
        if (split != std::string::npos) {
            out.erase(0, split + 1);
        }
    }
    return out;
}

#if defined(BASALT_NATIVE_SAMPLING)
namespace {
[[nodiscard]] std::string demangled(const char* name) {
    if (name == nullptr || *name == '\0') {
        return {};
    }
    int status = 0;
    char* text = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    std::string out = status == 0 && text != nullptr ? withoutArguments(text) : std::string{name};
    std::free(text);
    return out;
}

/// The Java machine is stripped of its local symbols, so a lookup would name whatever exported function came before;
/// its work is reported by the Java sampler, and here it is one honest frame.
[[nodiscard]] bool isJavaMachine(std::string_view path) { return path.find("libjvm") != std::string_view::npos; }

[[nodiscard]] std::string hex(std::uintptr_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%lx", static_cast<unsigned long>(value));
    return buffer;
}

[[nodiscard]] std::string baseName(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
} // namespace
#endif

// ---------------------------------------------------------------------------------------------------------------
// Symbols

#if defined(__APPLE__)
struct StackSymbols::Impl {
    std::unordered_map<std::uintptr_t, std::string> cache;
};

StackSymbols::StackSymbols() : impl_(std::make_unique<Impl>()) {}

StackSymbols::~StackSymbols() = default;

std::string StackSymbols::name(std::uintptr_t address, bool returnAddress) {
    const std::uintptr_t looked = returnAddress && address > 0 ? address - 1 : address;
    if (const auto found = impl_->cache.find(looked); found != impl_->cache.end()) {
        return found->second;
    }
    std::string text;
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(looked), &info) != 0) {
        if (info.dli_fname != nullptr && isJavaMachine(info.dli_fname)) {
            text = "libjvm (Java VM)";
        } else if (info.dli_sname != nullptr) {
            text = demangled(info.dli_sname);
            if (text == info.dli_sname && text.starts_with("_")) {
                text = demangled(info.dli_sname + 1);
            }
        } else if (info.dli_fname != nullptr) {
            text = baseName(info.dli_fname) + "+" + hex(looked - reinterpret_cast<std::uintptr_t>(info.dli_fbase));
        }
    }
    if (text.empty()) {
        text = hex(looked);
    }
    return impl_->cache.emplace(looked, std::move(text)).first->second;
}
#elif defined(BASALT_NATIVE_SAMPLING)
struct StackSymbols::Impl {
    struct Symbol {
        std::uint64_t value = 0;
        std::uint64_t size = 0;
        const char* name = nullptr;
    };

    struct Module {
        std::uintptr_t bias = 0;
        std::uintptr_t low = 0;
        std::uintptr_t high = 0;
        std::string path;
        bool loaded = false;
        void* map = nullptr;
        std::size_t mapSize = 0;
        std::vector<Symbol> symbols;
    };

    std::vector<Module> modules;
    std::unordered_map<std::uintptr_t, std::string> cache;

    Impl() {
        dl_iterate_phdr(
            [](dl_phdr_info* info, std::size_t, void* data) {
                auto* self = static_cast<Impl*>(data);
                Module module;
                module.bias = info->dlpi_addr;
                module.low = ~static_cast<std::uintptr_t>(0);
                for (int i = 0; i < info->dlpi_phnum; ++i) {
                    const ElfW(Phdr)& header = info->dlpi_phdr[i];
                    if (header.p_type == PT_LOAD) {
                        module.low = std::min<std::uintptr_t>(module.low, info->dlpi_addr + header.p_vaddr);
                        module.high =
                            std::max<std::uintptr_t>(module.high, info->dlpi_addr + header.p_vaddr + header.p_memsz);
                    }
                }
                module.path =
                    info->dlpi_name != nullptr && *info->dlpi_name != '\0' ? info->dlpi_name : "/proc/self/exe";
                if (module.low < module.high) {
                    self->modules.push_back(std::move(module));
                }
                return 0;
            },
            this);
    }

    ~Impl() {
        for (Module& module : modules) {
            if (module.map != nullptr) {
                munmap(module.map, module.mapSize);
            }
        }
    }

    /// Reads the function symbols of one file; a stripped one keeps only what the dynamic table exports.
    static void load(Module& module) {
        module.loaded = true;
        const int fd = open(module.path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return;
        }
        struct stat status{};
        if (fstat(fd, &status) != 0 || status.st_size < static_cast<off_t>(sizeof(ElfW(Ehdr)))) {
            close(fd);
            return;
        }
        void* map = mmap(nullptr, static_cast<std::size_t>(status.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (map == MAP_FAILED) {
            return;
        }
        module.map = map;
        module.mapSize = static_cast<std::size_t>(status.st_size);
        const auto* base = static_cast<const unsigned char*>(map);
        const auto* header = static_cast<const ElfW(Ehdr)*>(map);
        if (std::memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 || header->e_ident[EI_CLASS] != ELFCLASS64 ||
            header->e_shoff == 0 ||
            header->e_shoff + static_cast<std::uint64_t>(header->e_shnum) * sizeof(ElfW(Shdr)) > module.mapSize) {
            return;
        }
        const auto* sections = static_cast<const ElfW(Shdr)*>(static_cast<const void*>(base + header->e_shoff));
        const ElfW(Shdr)* table = nullptr;
        for (const std::uint32_t wanted :
             {static_cast<std::uint32_t>(SHT_SYMTAB), static_cast<std::uint32_t>(SHT_DYNSYM)}) {
            for (int i = 0; i < header->e_shnum && table == nullptr; ++i) {
                if (sections[i].sh_type == wanted) {
                    table = &sections[i];
                }
            }
            if (table != nullptr) {
                break;
            }
        }
        if (table == nullptr || table->sh_link >= header->e_shnum) {
            return;
        }
        const ElfW(Shdr)& strings = sections[table->sh_link];
        if (table->sh_offset + table->sh_size > module.mapSize ||
            strings.sh_offset + strings.sh_size > module.mapSize) {
            return;
        }
        const auto* symbols = static_cast<const ElfW(Sym)*>(static_cast<const void*>(base + table->sh_offset));
        const char* names = reinterpret_cast<const char*>(base + strings.sh_offset);
        const std::size_t count = table->sh_size / sizeof(ElfW(Sym));
        for (std::size_t i = 0; i < count; ++i) {
            const ElfW(Sym)& symbol = symbols[i];
            if (ELF64_ST_TYPE(symbol.st_info) != STT_FUNC || symbol.st_value == 0 ||
                symbol.st_name >= strings.sh_size) {
                continue;
            }
            module.symbols.push_back({symbol.st_value, symbol.st_size, names + symbol.st_name});
        }
        std::ranges::sort(module.symbols, {}, &Symbol::value);
    }
};

StackSymbols::StackSymbols() : impl_(std::make_unique<Impl>()) {}

StackSymbols::~StackSymbols() = default;

std::string StackSymbols::name(std::uintptr_t address, bool returnAddress) {
    const std::uintptr_t looked = returnAddress && address > 0 ? address - 1 : address;
    if (const auto found = impl_->cache.find(looked); found != impl_->cache.end()) {
        return found->second;
    }
    std::string text;
    for (Impl::Module& module : impl_->modules) {
        if (looked < module.low || looked >= module.high) {
            continue;
        }
        if (!module.loaded) {
            Impl::load(module);
        }
        if (isJavaMachine(module.path)) {
            text = "libjvm (Java VM)";
            break;
        }
        const std::uint64_t offset = looked - module.bias;
        auto after = std::ranges::upper_bound(module.symbols, offset, {}, &Impl::Symbol::value);
        if (after != module.symbols.begin()) {
            const Impl::Symbol& symbol = *(after - 1);
            if (symbol.size == 0 || offset < symbol.value + symbol.size) {
                text = demangled(symbol.name);
            }
        }
        if (text.empty()) {
            text = baseName(module.path == "/proc/self/exe" ? std::string{"basalt"} : module.path) + "+" +
                   hex(static_cast<std::uintptr_t>(offset));
        }
        break;
    }
    if (text.empty()) {
        text = hex(looked);
    }
    return impl_->cache.emplace(looked, std::move(text)).first->second;
}
#else
struct StackSymbols::Impl {};

StackSymbols::StackSymbols() = default;

StackSymbols::~StackSymbols() = default;

std::string StackSymbols::name(std::uintptr_t address, bool) { return "0x" + std::to_string(address); }
#endif

// ---------------------------------------------------------------------------------------------------------------
// Sampling

#if defined(BASALT_NATIVE_SAMPLING)
namespace {
constexpr std::chrono::milliseconds kInterval{10};
constexpr std::chrono::milliseconds kIdlePoll{50};
constexpr std::int64_t kWindowMillis = 10'000;
constexpr std::size_t kMaxWindows = 180;
constexpr std::size_t kMaxStacks = 40'000;
constexpr int kMaxFrames = 96;
constexpr std::int64_t kParkNanos = 20'000'000;

/// What the handler leaves for the sampling thread: the registers that start the walk, and who may go on.
struct Slot {
    std::atomic<std::uint64_t> target{0};
    /// 0 armed, 1 the target is parked and its registers are filled in, 2 let it go.
    std::atomic<int> state{0};
    std::uintptr_t pc = 0;
    std::uintptr_t fp = 0;
    std::uintptr_t sp = 0;
};

Slot gSlot;

[[nodiscard]] std::uint64_t currentThreadId() noexcept {
#if defined(__APPLE__)
    std::uint64_t id = 0;
    pthread_threadid_np(nullptr, &id);
    return id;
#else
    return static_cast<std::uint64_t>(syscall(SYS_gettid));
#endif
}

[[nodiscard]] std::int64_t monotonicNanos() noexcept {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::int64_t>(now.tv_sec) * 1'000'000'000 + now.tv_nsec;
}

void captureRegisters(void* context) noexcept {
    const auto* uc = static_cast<const ::ucontext_t*>(context);
#if defined(__APPLE__) && defined(__aarch64__)
    gSlot.pc = static_cast<std::uintptr_t>(__darwin_arm_thread_state64_get_pc(uc->uc_mcontext->__ss));
    gSlot.fp = static_cast<std::uintptr_t>(__darwin_arm_thread_state64_get_fp(uc->uc_mcontext->__ss));
    gSlot.sp = static_cast<std::uintptr_t>(__darwin_arm_thread_state64_get_sp(uc->uc_mcontext->__ss));
#elif defined(__APPLE__) && defined(__x86_64__)
    gSlot.pc = static_cast<std::uintptr_t>(uc->uc_mcontext->__ss.__rip);
    gSlot.fp = static_cast<std::uintptr_t>(uc->uc_mcontext->__ss.__rbp);
    gSlot.sp = static_cast<std::uintptr_t>(uc->uc_mcontext->__ss.__rsp);
#elif defined(__linux__) && defined(__x86_64__)
    gSlot.pc = static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
    gSlot.fp = static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RBP]);
    gSlot.sp = static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RSP]);
#elif defined(__linux__) && defined(__aarch64__)
    gSlot.pc = static_cast<std::uintptr_t>(uc->uc_mcontext.pc);
    gSlot.fp = static_cast<std::uintptr_t>(uc->uc_mcontext.regs[29]);
    gSlot.sp = static_cast<std::uintptr_t>(uc->uc_mcontext.sp);
#else
    static_cast<void>(uc);
    gSlot.pc = gSlot.fp = gSlot.sp = 0;
#endif
}

/// Runs on the sampled thread, so it only stores registers and waits.
void onSignal(int, siginfo_t*, void* context) {
    const int savedErrno = errno;
    if (gSlot.target.load(std::memory_order_acquire) == currentThreadId()) {
        captureRegisters(context);
        int armed = 0;
        if (gSlot.state.compare_exchange_strong(armed, 1, std::memory_order_acq_rel)) {
            const std::int64_t deadline = monotonicNanos() + kParkNanos;
            while (gSlot.state.load(std::memory_order_acquire) != 2 && monotonicNanos() < deadline) {
                sched_yield();
            }
        }
    }
    errno = savedErrno;
}

struct Target {
    std::uint64_t tid = 0;
    std::string name;
#if defined(__APPLE__)
    pthread_t thread = nullptr;
#endif
};

struct Bounds {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
};

#if defined(__linux__)
/// The mapping that holds `sp`, which is the thread's stack: reading inside it cannot fault.
[[nodiscard]] Bounds stackAround(std::uintptr_t sp) {
    std::FILE* maps = std::fopen("/proc/self/maps", "r");
    Bounds found;
    if (maps == nullptr) {
        return found;
    }
    char line[512];
    while (std::fgets(line, sizeof(line), maps) != nullptr) {
        unsigned long low = 0;
        unsigned long high = 0;
        if (std::sscanf(line, "%lx-%lx", &low, &high) == 2 && sp >= low && sp < high) {
            found = {low, high};
            break;
        }
    }
    std::fclose(maps);
    return found;
}
#endif
} // namespace

struct NativeSampler::State {
    struct Counts {
        std::int64_t hits = 0;
        std::vector<std::int64_t> windows;
    };

    std::mutex mutex;
    std::map<std::string, std::map<std::vector<std::uintptr_t>, Counts>> threads;
    std::int64_t rounds = 0;
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point lastRoundAt;
    bool running = false;

    std::thread worker;
    std::atomic<bool> quit{false};
    std::unordered_map<std::uint64_t, Bounds> bounds;
    bool handlerInstalled = false;
};

namespace {
[[nodiscard]] std::vector<Target> busyThreads() {
    std::vector<Target> found;
    const std::uint64_t self = currentThreadId();
#if defined(__APPLE__)
    thread_act_array_t list = nullptr;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &list, &count) != KERN_SUCCESS) {
        return found;
    }
    for (mach_msg_type_number_t i = 0; i < count; ++i) {
        thread_basic_info_data_t basic{};
        mach_msg_type_number_t basicCount = THREAD_BASIC_INFO_COUNT;
        thread_identifier_info_data_t identity{};
        mach_msg_type_number_t identityCount = THREAD_IDENTIFIER_INFO_COUNT;
        const bool ok = thread_info(list[i], THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&basic), &basicCount) ==
                            KERN_SUCCESS &&
                        thread_info(list[i], THREAD_IDENTIFIER_INFO, reinterpret_cast<thread_info_t>(&identity),
                                    &identityCount) == KERN_SUCCESS;
        if (ok && basic.run_state == TH_STATE_RUNNING && identity.thread_id != self) {
            Target target;
            target.tid = identity.thread_id;
            target.thread = pthread_from_mach_thread_np(list[i]);
            if (target.thread != nullptr) {
                char name[64] = {};
                pthread_getname_np(target.thread, name, sizeof(name));
                target.name = name;
                found.push_back(std::move(target));
            }
        }
        mach_port_deallocate(mach_task_self(), list[i]);
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(list), count * sizeof(thread_t));
#else
    DIR* directory = opendir("/proc/self/task");
    if (directory == nullptr) {
        return found;
    }
    while (const dirent* entry = readdir(directory)) {
        char* end = nullptr;
        const unsigned long tid = std::strtoul(entry->d_name, &end, 10);
        if (end == entry->d_name || *end != '\0' || tid == self) {
            continue;
        }
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/self/task/%lu/stat", tid);
        std::FILE* stat = std::fopen(path, "r");
        if (stat == nullptr) {
            continue;
        }
        char text[512] = {};
        const std::size_t length = std::fread(text, 1, sizeof(text) - 1, stat);
        std::fclose(stat);
        const std::string_view line{text, length};
        const std::size_t opening = line.find('(');
        const std::size_t closing = line.rfind(')');
        if (opening == std::string_view::npos || closing == std::string_view::npos || closing + 2 >= line.size()) {
            continue;
        }
        if (line[closing + 2] == 'R') {
            Target target;
            target.tid = tid;
            target.name = std::string{line.substr(opening + 1, closing - opening - 1)};
            found.push_back(std::move(target));
        }
    }
    closedir(directory);
#endif
    return found;
}

/// Parks `target`, walks its frame pointers while it waits, and lets it go. Empty when it could not be reached.
[[nodiscard]] std::vector<std::uintptr_t> capture(const Target& target,
                                                  [[maybe_unused]] std::unordered_map<std::uint64_t, Bounds>& bounds) {
    std::vector<std::uintptr_t> frames;
    gSlot.state.store(0, std::memory_order_release);
    gSlot.target.store(target.tid, std::memory_order_release);
#if defined(__APPLE__)
    const int sent = pthread_kill(target.thread, SIGPROF);
#else
    const int sent = static_cast<int>(syscall(SYS_tgkill, getpid(), static_cast<pid_t>(target.tid), SIGPROF));
#endif
    if (sent == 0) {
        const std::int64_t deadline = monotonicNanos() + 2'000'000;
        while (gSlot.state.load(std::memory_order_acquire) != 1 && monotonicNanos() < deadline) {
            sched_yield();
        }
    }
    if (sent == 0 && gSlot.state.load(std::memory_order_acquire) == 1) {
        Bounds stack;
#if defined(__APPLE__)
        const auto top = reinterpret_cast<std::uintptr_t>(pthread_get_stackaddr_np(target.thread));
        stack = {top - pthread_get_stacksize_np(target.thread), top};
#else
        Bounds& cached = bounds[target.tid];
        if (gSlot.sp < cached.low || gSlot.sp >= cached.high) {
            cached = stackAround(gSlot.sp);
        }
        stack = cached;
#endif
        if (gSlot.sp >= stack.low && gSlot.sp < stack.high) {
            frames.push_back(gSlot.pc);
            std::uintptr_t fp = gSlot.fp;
            for (int i = 0; i < kMaxFrames; ++i) {
                if (fp < gSlot.sp || fp % sizeof(std::uintptr_t) != 0 || fp + 2 * sizeof(std::uintptr_t) > stack.high) {
                    break;
                }
                const auto* record = reinterpret_cast<const std::uintptr_t*>(fp);
                const std::uintptr_t next = record[0];
                const std::uintptr_t returnTo = record[1];
                if (returnTo < 4096) {
                    break;
                }
                frames.push_back(returnTo);
                if (next <= fp) {
                    break;
                }
                fp = next;
            }
        }
    }
    gSlot.target.store(0, std::memory_order_release);
    gSlot.state.store(2, std::memory_order_release);
    return frames;
}
} // namespace

NativeSampler::NativeSampler(const Timings& timings) : timings_(&timings), state_(std::make_unique<State>()) {}

NativeSampler::~NativeSampler() { stop(); }

bool NativeSampler::supported() noexcept { return true; }

void NativeSampler::start() {
    if (state_->worker.joinable()) {
        return;
    }
    if (!state_->handlerInstalled) {
        struct sigaction action{};
        action.sa_sigaction = &onSignal;
        action.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGPROF, &action, nullptr) != 0) {
            return;
        }
        state_->handlerInstalled = true;
    }
    state_->quit.store(false);
    State* state = state_.get();
    const Timings* timings = timings_;
    state->worker = std::thread{[state, timings] {
        nameCurrentThread("basalt-sampler");
        while (!state->quit.load(std::memory_order_relaxed)) {
            if (!timings->enabled()) {
                {
                    const std::lock_guard<std::mutex> lock{state->mutex};
                    state->running = false;
                }
                std::this_thread::sleep_for(kIdlePoll);
                continue;
            }
            const auto now = std::chrono::steady_clock::now();
            std::size_t window = 0;
            {
                const std::lock_guard<std::mutex> lock{state->mutex};
                if (!state->running) {
                    state->running = true;
                    state->threads.clear();
                    state->rounds = 0;
                    state->startedAt = now;
                }
                state->lastRoundAt = now;
                state->rounds++;
                window = std::min<std::size_t>(
                    static_cast<std::size_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - state->startedAt).count() /
                        kWindowMillis),
                    kMaxWindows - 1);
            }
            for (const Target& target : busyThreads()) {
                std::vector<std::uintptr_t> frames = capture(target, state->bounds);
                if (frames.empty()) {
                    continue;
                }
                const std::lock_guard<std::mutex> lock{state->mutex};
                auto& stacks =
                    state->threads[target.name.empty() ? "thread-" + std::to_string(target.tid) : target.name];
                if (stacks.size() >= kMaxStacks && !stacks.contains(frames)) {
                    continue;
                }
                State::Counts& counts = stacks[std::move(frames)];
                counts.hits++;
                if (counts.windows.size() <= window) {
                    counts.windows.resize(window + 1, 0);
                }
                counts.windows[window]++;
            }
            std::this_thread::sleep_for(kInterval);
        }
    }};
}

void NativeSampler::stop() {
    if (state_ == nullptr || !state_->worker.joinable()) {
        return;
    }
    state_->quit.store(true);
    state_->worker.join();
}

namespace {
/// Frames every thread starts with, which say nothing about the code and would bury it a dozen levels down.
[[nodiscard]] bool isScaffolding(const std::string& name) {
    for (const std::string_view prefix :
         {"thread_start", "pthread_start", "start_thread", "clone", "void* std::__1::__thread_proxy",
          "void std::__1::__thread_execute", "std::__1::__invoke", "void std::__1::__invoke", "std::__invoke",
          "std::__1::__function::", "std::__function::", "std::__1::__thread_proxy", "std::thread::_State_impl",
          "std::thread::_Invoker", "execute_native_thread_routine", "std::__1::__thread_proxy",
          "std::__1::__thread_execute", "std::__1::function"}) {
        if (name.starts_with(prefix)) {
            return true;
        }
    }
    return name.find("__invoke_result_impl") != std::string::npos ||
           name.find("__invoke_void_return_wrapper") != std::string::npos;
}

/// A thread that was running when signalled but sits in one of these is only passing through a wait.
[[nodiscard]] bool isWaiting(const std::string& name) {
    const std::string_view stripped = std::string_view{name}.substr(std::min(name.find_first_not_of('_'), name.size()));
    for (const std::string_view waiting : {"__semwait_signal",
                                           "__psynch_cvwait",
                                           "__psynch_mutexwait",
                                           "nanosleep",
                                           "usleep",
                                           "clock_nanosleep",
                                           "kevent",
                                           "kevent64",
                                           "select",
                                           "poll",
                                           "ppoll",
                                           "epoll_wait",
                                           "epoll_pwait",
                                           "futex",
                                           "pthread_cond_wait",
                                           "pthread_cond_timedwait",
                                           "mach_msg",
                                           "mach_msg_trap",
                                           "mach_msg2_trap",
                                           "__ulock_wait",
                                           "__ulock_wait2",
                                           "sigtimedwait",
                                           "sigwait",
                                           "__wait4",
                                           "read",
                                           "recv",
                                           "recvfrom",
                                           "accept",
                                           "__sigwait"}) {
        if (stripped == waiting.substr(std::min(waiting.find_first_not_of('_'), waiting.size()))) {
            return true;
        }
    }
    return false;
}

struct TreeNode {
    std::int64_t hits = 0;
    std::int64_t self = 0;
    std::vector<std::int64_t> windows;
    std::map<std::string, std::unique_ptr<TreeNode>> children;

    void add(const std::vector<std::int64_t>& by, std::int64_t count) {
        hits += count;
        if (windows.size() < by.size()) {
            windows.resize(by.size(), 0);
        }
        for (std::size_t i = 0; i < by.size(); ++i) {
            windows[i] += by[i];
        }
    }
};

void quote(std::string& out, const std::string& text) {
    out += '"';
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            out += ' ';
        } else {
            out += c;
        }
    }
    out += '"';
}

[[nodiscard]] std::string millis(double value) {
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    return buffer;
}

void writeSeries(std::string& out, const std::vector<std::int64_t>& windows, double scale) {
    out += '[';
    for (std::size_t i = 0; i < windows.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        out += scale == 1.0 ? std::to_string(windows[i]) : millis(static_cast<double>(windows[i]) * scale);
    }
    out += ']';
}

void writeChildren(std::string& out, const TreeNode& parent, double perSample) {
    bool comma = false;
    for (const auto& [name, node] : parent.children) {
        if (comma) {
            out += ',';
        }
        comma = true;
        out += "{\"name\":";
        quote(out, name);
        out += ",\"calls\":" + std::to_string(node->hits) +
               ",\"total_millis\":" + millis(static_cast<double>(node->hits) * perSample) +
               ",\"self_millis\":" + millis(static_cast<double>(node->self) * perSample) +
               ",\"max_millis\":0,\"window_calls\":";
        writeSeries(out, node->windows, 1.0);
        out += ",\"window_millis\":";
        writeSeries(out, node->windows, perSample);
        out += ",\"children\":[";
        writeChildren(out, *node, perSample);
        out += "]}";
    }
}
} // namespace

std::vector<std::string> NativeSampler::threadsJson() const {
    std::vector<std::string> out;
    const std::lock_guard<std::mutex> lock{state_->mutex};
    if (state_->rounds == 0) {
        return out;
    }
    const double elapsed = std::max<double>(
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(state_->lastRoundAt - state_->startedAt).count()),
        1.0);
    const double perSample = elapsed / static_cast<double>(state_->rounds);
    StackSymbols symbols;
    for (const auto& [thread, stacks] : state_->threads) {
        TreeNode root;
        for (const auto& [frames, counts] : stacks) {
            if (isWaiting(symbols.name(frames.front(), false))) {
                continue;
            }
            root.add(counts.windows, counts.hits);
            TreeNode* node = &root;
            for (std::size_t i = frames.size(); i-- > 0;) {
                std::string label = symbols.name(frames[i], i > 0);
                if (isScaffolding(label)) {
                    continue;
                }
                std::unique_ptr<TreeNode>& child = node->children[std::move(label)];
                if (child == nullptr) {
                    child = std::make_unique<TreeNode>();
                }
                child->add(counts.windows, counts.hits);
                node = child.get();
            }
            node->self += counts.hits;
        }
        std::string json = "{\"name\":";
        quote(json, "Native " + thread);
        const std::string total = millis(static_cast<double>(root.hits) * perSample);
        json += ",\"total_millis\":" + total + ",\"root\":{\"name\":\"root\",\"calls\":0,\"total_millis\":" + total +
                ",\"self_millis\":0,\"max_millis\":0,\"window_calls\":[],\"window_millis\":[],\"children\":[";
        writeChildren(json, root, perSample);
        json += "]}}";
        out.push_back(std::move(json));
    }
    return out;
}
#else
struct NativeSampler::State {};

NativeSampler::NativeSampler(const Timings& timings) : timings_(&timings), state_(std::make_unique<State>()) {}

NativeSampler::~NativeSampler() = default;

bool NativeSampler::supported() noexcept { return false; }

void NativeSampler::start() {}

void NativeSampler::stop() {}

std::vector<std::string> NativeSampler::threadsJson() const { return {}; }
#endif
} // namespace basalt::wire::common
