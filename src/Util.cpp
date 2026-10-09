#include "Util.h"
#include <cstdio>
#include <ctime>

namespace bms {

std::string nowTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

std::string formatMoney(long long cents) {
    long long whole = cents / 100;
    long long frac = cents % 100;
    if (frac < 0) frac = -frac;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%lld.%02lld", whole, frac);
    return buf;
}

}  // namespace bms
