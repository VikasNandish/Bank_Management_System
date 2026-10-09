#include "Validator.h"
#include <cctype>

namespace bms {

namespace {
bool allDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}
}  // namespace

bool parseAmountCents(const std::string& text, long long& outCents) {
    std::string whole = text;
    std::string frac;
    std::size_t dot = text.find('.');
    if (dot != std::string::npos) {
        whole = text.substr(0, dot);
        frac = text.substr(dot + 1);
        if (frac.empty() || frac.size() > 2) return false;  // "5." or "1.234"
        if (!allDigits(frac)) return false;
    }
    if (!allDigits(whole)) return false;  // rejects "", "-5", "abc", "1e3", ".5"
    if (whole.size() > 9) return false;   // keeps value below overflow and max limit

    long long cents = std::stoll(whole) * 100;
    if (frac.size() == 1) cents += (frac[0] - '0') * 10;
    if (frac.size() == 2) cents += (frac[0] - '0') * 10 + (frac[1] - '0');

    if (cents <= 0 || cents > kMaxAmountCents) return false;
    outCents = cents;
    return true;
}

bool isValidPin(const std::string& pin) {
    return pin.size() >= 4 && pin.size() <= 6 && allDigits(pin);
}

bool isValidName(const std::string& name) {
    if (name.empty() || name.size() > 50) return false;
    bool hasNonSpace = false;
    for (char c : name) {
        if (c == '|' || c == '\n' || c == '\r') return false;
        if (c != ' ' && c != '\t') hasNonSpace = true;
    }
    return hasNonSpace;
}

}  // namespace bms
