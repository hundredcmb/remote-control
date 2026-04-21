#ifndef BASE_TIMESTAMP_H
#define BASE_TIMESTAMP_H

#include <ctime>
#include <string>
#include <cstdio>
#include <cstdint>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else

#include <sys/time.h>

#endif

namespace lsy {

class Timestamp {
public:
    Timestamp()
        : microSecondsSinceEpoch_(0) {
    }

    explicit Timestamp(int64_t microSecondsSinceEpochArg)
        : microSecondsSinceEpoch_(microSecondsSinceEpochArg) {
    }

    void swap(Timestamp &that) {
        std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
    }

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] std::string
    toFormattedString(bool showMicroseconds = true) const;

    [[nodiscard]] bool valid() const { return microSecondsSinceEpoch_ > 0; }

    [[nodiscard]] int64_t
    microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    [[nodiscard]] time_t secondsSinceEpoch() const {
        return static_cast<time_t>(microSecondsSinceEpoch_ /
                                   kMicroSecondsPerSecond);
    }

    static Timestamp now();

    static Timestamp invalid() {
        return {};
    }

    static Timestamp fromUnixTime(time_t t) {
        return fromUnixTime(t, 0);
    }

    static Timestamp fromUnixTime(time_t t, int microseconds) {
        return Timestamp(
            static_cast<int64_t>(t) * kMicroSecondsPerSecond + microseconds);
    }

    static constexpr int kMicroSecondsPerSecond = 1000 * 1000;

private:
    int64_t microSecondsSinceEpoch_;
};

inline bool operator<(Timestamp lhs, Timestamp rhs) {
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs) {
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline bool operator!=(Timestamp lhs, Timestamp rhs) {
    return !(lhs == rhs);
}

inline bool operator>(Timestamp lhs, Timestamp rhs) {
    return rhs < lhs;
}

inline bool operator<=(Timestamp lhs, Timestamp rhs) {
    return !(rhs < lhs);
}

inline bool operator>=(Timestamp lhs, Timestamp rhs) {
    return !(lhs < rhs);
}

inline double timeDifference(Timestamp high, Timestamp low) {
    int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
    return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

inline Timestamp addTime(Timestamp timestamp, double seconds) {
    auto delta = static_cast<int64_t>(seconds *
                                      Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

inline std::string Timestamp::toString() const {
    char buf[32] = {0};
    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf), "%lld.%06lld",
             static_cast<long long>(seconds),
             static_cast<long long>(microseconds));
    return {buf};
}

inline std::string Timestamp::toFormattedString(bool showMicroseconds) const {
    char buf[64] = {0};
    time_t seconds = secondsSinceEpoch();
    tm tm_time{};

#ifdef _WIN32
    localtime_s(&tm_time, &seconds);
#else
    localtime_r(&seconds, &tm_time);
#endif

    if (showMicroseconds) {
        int microseconds = static_cast<int>(microSecondsSinceEpoch_ %
                                            kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%06d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
                 microseconds);
    } else {
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return {buf};
}

inline Timestamp Timestamp::now() {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    uint64_t ticks = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    int64_t microSecondsSinceEpoch = static_cast<int64_t>(ticks / 10 - 11644473600000000LL);
#else
    ::timeval tv{};
    gettimeofday(&tv, nullptr);
    int64_t seconds = tv.tv_sec;
    int64_t microSecondsSinceEpoch =
        seconds * kMicroSecondsPerSecond + tv.tv_usec;
#endif
    return Timestamp(microSecondsSinceEpoch);
}

} // namespace lsy

#endif // BASE_TIMESTAMP_H
