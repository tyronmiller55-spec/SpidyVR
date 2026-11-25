// The session's diagnostics (diag_log.hpp).
#include "spidy/diag_log.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <windows.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#ifndef SPIDY_VERSION
#define SPIDY_VERSION ""
#endif
#ifndef SPIDY_BUILD_COMMIT
#define SPIDY_BUILD_COMMIT ""
#endif
#ifndef SPIDY_BUILD_STAMP
#define SPIDY_BUILD_STAMP __DATE__ " " __TIME__
#endif
using namespace spidy;

namespace {
SRWLOCK lock = SRWLOCK_INIT;
diag_log::Ring<diag_log::mainLines> mainRing;
diag_log::Ring<diag_log::aimLines> aimRing;
diag_log::Ring<diag_log::headLines> headRing;
ULONGLONG firstTick{};
std::atomic<bool> on{};
diag_log::Throttle throttle;
FILE* durableFile{};
char durablePathBuf[260]{};
void (*publishHook)();
} // namespace

void diag_log::enable(bool wanted) {
    on.store(wanted, std::memory_order_relaxed);
}

bool diag_log::enabled() {
    return on.load(std::memory_order_relaxed);
}

void diag_log::setSlow(bool slow) {
    AcquireSRWLockExclusive(&lock);
    throttle.slow(slow);
    ReleaseSRWLockExclusive(&lock);
}

void diag_log::setLevel(int level) {
    AcquireSRWLockExclusive(&lock);
    throttle.level(level);
    ReleaseSRWLockExclusive(&lock);
}

uint64_t diag_log::droppedLines() {
    AcquireSRWLockShared(&lock);
    const uint64_t n = throttle.dropped;
    ReleaseSRWLockShared(&lock);
    return n;
}

void diag_log::write(const char* text) {
    if (!text || !on.load(std::memory_order_relaxed))
        return;
    char line[lineChars];
    AcquireSRWLockExclusive(&lock);
    const ULONGLONG now = GetTickCount64();
    if (!firstTick)
        firstTick = now;
    const double seconds = static_cast<double>(now - firstTick) / 1000.0;
    if (!throttle.allow(text, seconds)) {
        ReleaseSRWLockExclusive(&lock);
        return;
    }
    _snprintf_s(line, sizeof(line), _TRUNCATE, "[%8.3f] %s", seconds, text);
    if (const auto channel = channelOf(text); channel == Channel::aim)
        aimRing.push(line);
    else if (channel == Channel::head)
        headRing.push(line);
    else
        mainRing.push(line);
    ReleaseSRWLockExclusive(&lock);
}

void diag_log::writef(const char* format, ...) {
    char text[lineChars];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(text, sizeof(text), _TRUNCATE, format, args);
    va_end(args);
    write(text);
}

void diag_log::copyOut(Data& out) {
    AcquireSRWLockShared(&lock);
    if (out.main.written != mainRing.written)
        out.main = mainRing;
    if (out.aim.written != aimRing.written)
        out.aim = aimRing;
    if (out.head.written != headRing.written)
        out.head = headRing;
    ReleaseSRWLockShared(&lock);
}

const char* diag_log::build() {
    static const std::string id = buildId(SPIDY_VERSION, SPIDY_BUILD_COMMIT, SPIDY_BUILD_STAMP);
    return id.c_str();
}

namespace {
void flushDurableFile() {
    if (!durableFile)
        return;
    fflush(durableFile);
#ifdef _WIN32
    FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(durableFile))));
#else
    fsync(fileno(durableFile));
#endif
}

void durableAnchor() {}

void defaultDurablePath(char* out, std::size_t n) {
#ifdef _WIN32
    char module[MAX_PATH]{};
    HMODULE handle{};
    const auto* here = reinterpret_cast<LPCSTR>(reinterpret_cast<const void*>(&durableAnchor));
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           here, &handle) &&
        GetModuleFileNameA(handle, module, MAX_PATH)) {
        char* slash = std::strrchr(module, '\\');
        if (!slash)
            slash = std::strrchr(module, '/');
        if (slash) {
            slash[1] = 0;
            _snprintf_s(out, n, _TRUNCATE, "%sspidy-vr-start.log", module);
            return;
        }
    }
    _snprintf_s(out, n, _TRUNCATE, "spidy-vr-start.log");
#else
    std::snprintf(out, n, "/tmp/spidy-vr-start.log");
#endif
}

void openDurable(const char* path) {
    if (durableFile) {
        fclose(durableFile);
        durableFile = nullptr;
    }
    durablePathBuf[0] = 0;
    if (!path || !path[0])
        return;
#ifdef _WIN32
    if (fopen_s(&durableFile, path, "wb") != 0)
        durableFile = nullptr;
#else
    durableFile = std::fopen(path, "wb");
#endif
    if (!durableFile)
        return;
#ifdef _WIN32
    _snprintf_s(durablePathBuf, sizeof(durablePathBuf), _TRUNCATE, "%s", path);
#else
    std::snprintf(durablePathBuf, sizeof(durablePathBuf), "%s", path);
#endif
}

void forceRing(const char* text) {
    char line[diag_log::lineChars];
    AcquireSRWLockExclusive(&lock);
    const ULONGLONG now = GetTickCount64();
    if (!firstTick)
        firstTick = now;
    const double seconds = static_cast<double>(now - firstTick) / 1000.0;
    _snprintf_s(line, sizeof(line), _TRUNCATE, "[%8.3f] %s", seconds, text);
    if (const auto channel = diag_log::channelOf(text); channel == diag_log::Channel::aim)
        aimRing.push(line);
    else if (channel == diag_log::Channel::head)
        headRing.push(line);
    else
        mainRing.push(line);
    ReleaseSRWLockExclusive(&lock);
}
} // namespace

void diag_log::durableClose() {
    if (durableFile) {
        flushDurableFile();
        fclose(durableFile);
        durableFile = nullptr;
    }
}

void diag_log::durableBegin(const char* path) {
    char chosen[260]{};
    if (path && path[0]) {
#ifdef _WIN32
        _snprintf_s(chosen, sizeof(chosen), _TRUNCATE, "%s", path);
#else
        std::snprintf(chosen, sizeof(chosen), "%s", path);
#endif
    } else {
        defaultDurablePath(chosen, sizeof(chosen));
    }
    openDurable(chosen);
    if (!durableFile)
        return;
    char header[lineChars];
    _snprintf_s(header, sizeof(header), _TRUNCATE, "spidy vr-start log: %s", build());
    std::fputs(header, durableFile);
    std::fputc('\n', durableFile);
    flushDurableFile();
}

void diag_log::durableBegin() {
    durableBegin(nullptr);
}

void diag_log::durable(const char* text) {
    if (!text)
        return;
    if (!durableFile)
        durableBegin();
    const ULONGLONG now = GetTickCount64();
    if (!firstTick)
        firstTick = now;
    const double seconds = static_cast<double>(now - firstTick) / 1000.0;
    if (durableFile) {
        std::fprintf(durableFile, "[%8.3f] %s\n", seconds, text);
        flushDurableFile();
    }
    forceRing(text);
    if (publishHook)
        publishHook();
}

void diag_log::durablef(const char* format, ...) {
    char text[lineChars];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(text, sizeof(text), _TRUNCATE, format, args);
    va_end(args);
    durable(text);
}

const char* diag_log::durablePath() {
    return durablePathBuf[0] ? durablePathBuf : "";
}

void diag_log::setPublishHook(void (*fn)()) {
    publishHook = fn;
}
