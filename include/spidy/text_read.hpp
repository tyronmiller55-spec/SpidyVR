#pragma once
// Reading a whole text file in chunks, whatever its size, up to a safety cap.
//
// November 17: the game read only the first 4096 bytes of spidy_vr.ini (game_xr.cpp, readTabOnly) while the
// defaults alone had grown to 4982 bytes. Every key past the cut was silently left at its default in game, the
// movement defaults' revision key among them, so that migration ran again at every launch, and a change in the
// SPIDY VR tab wrote the cut keys back at their defaults. The settings files are now read whole: in chunks, so
// no size is assumed, with a cap (kSettingsLimit) only so a wrong or runaway file cannot take the memory.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace spidy::text_read {

// Far above any settings file Spidy writes (about 5 KB in November 2026); a file past it is not settings.
inline constexpr size_t kSettingsLimit = size_t{4} << 20;
inline constexpr size_t kChunk = 16384;

struct Result {
    std::string text;
    bool failed{};    // a read failed part way (text has what came before)
    bool truncated{}; // the file went on past the limit (text has the limit's worth)
};

// read(buffer, capacity) gives the bytes it put in buffer: 0 at the end, a negative number on a failure.
template <class Read>
Result readAll(Read&& read, size_t limit = kSettingsLimit) {
    Result result;
    char chunk[kChunk];
    for (;;) {
        const size_t room = limit - result.text.size();
        if (!room) {
            // Is there more? One byte says so.
            char extra{};
            result.truncated = read(&extra, size_t{1}) > 0;
            break;
        }
        const long long got = read(chunk, room < kChunk ? room : kChunk);
        if (got < 0) {
            result.failed = true;
            break;
        }
        if (got == 0)
            break;
        result.text.append(chunk, static_cast<size_t>(got));
    }
    return result;
}

// A file opened by the C library (tests, tools): the same reading.
inline Result readAll(std::FILE* file, size_t limit = kSettingsLimit) {
    if (!file)
        return {{}, true, false};
    return readAll(
        [file](char* buffer, size_t capacity) -> long long {
            const size_t got = std::fread(buffer, 1, capacity, file);
            if (!got && std::ferror(file))
                return -1;
            return static_cast<long long>(got);
        },
        limit);
}

} // namespace spidy::text_read
