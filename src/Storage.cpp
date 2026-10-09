#include "Storage.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace bms {

namespace {

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, delim)) parts.push_back(cur);
    if (!line.empty() && line.back() == delim) parts.push_back("");
    return parts;
}

bool toLL(const std::string& s, long long& out) {
    try {
        std::size_t pos = 0;
        out = std::stoll(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

}  // namespace

Storage::Storage(std::string dataDir) : dir_(std::move(dataDir)) {
    std::error_code ec;
    fs::create_directories(dir_, ec);
}

std::string Storage::accountsPath() const { return (fs::path(dir_) / "accounts.dat").string(); }
std::string Storage::transactionsPath() const { return (fs::path(dir_) / "transactions.dat").string(); }

bool Storage::saveAccounts(const std::vector<Account>& accounts) const {
    const std::string finalPath = accountsPath();
    const std::string tmpPath = finalPath + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out) return false;
        for (const auto& a : accounts) {
            out << a.id << '|' << a.name << '|' << a.pinHash << '|' << a.balanceCents << '|'
                << a.failedAttempts << '|' << (a.locked ? 1 : 0) << '\n';
        }
        out.flush();
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(tmpPath, finalPath, ec);
    if (ec) {  // Windows cannot rename over an existing file on some setups
        fs::remove(finalPath, ec);
        fs::rename(tmpPath, finalPath, ec);
    }
    return !ec;
}

bool Storage::loadAccounts(std::vector<Account>& out) const {
    out.clear();
    std::ifstream in(accountsPath());
    if (!in) return true;  // first run: nothing saved yet

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line, '|');
        if (p.size() != 6) { out.clear(); return false; }
        Account a;
        long long id, bal, fails, locked;
        if (!toLL(p[0], id) || !toLL(p[3], bal) || !toLL(p[4], fails) || !toLL(p[5], locked) ||
            p[2].size() != 64 || bal < 0 || fails < 0 || (locked != 0 && locked != 1)) {
            out.clear();
            return false;
        }
        a.id = static_cast<int>(id);
        a.name = p[1];
        a.pinHash = p[2];
        a.balanceCents = bal;
        a.failedAttempts = static_cast<int>(fails);
        a.locked = (locked == 1);
        out.push_back(a);
    }
    return true;
}

bool Storage::appendTransaction(const Transaction& t) const {
    std::ofstream out(transactionsPath(), std::ios::app);
    if (!out) return false;
    out << t.timestamp << '|' << t.accountId << '|' << txnTypeToString(t.type) << '|'
        << t.amountCents << '|' << t.balanceAfterCents << '\n';
    out.flush();
    return static_cast<bool>(out);
}

bool Storage::loadTransactions(std::vector<Transaction>& out) const {
    out.clear();
    std::ifstream in(transactionsPath());
    if (!in) return true;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line, '|');
        long long id, amt, bal;
        if (p.size() != 5 || !toLL(p[1], id) || !toLL(p[3], amt) || !toLL(p[4], bal) ||
            (p[2] != "DEPOSIT" && p[2] != "WITHDRAWAL")) {
            out.clear();
            return false;
        }
        Transaction t;
        t.timestamp = p[0];
        t.accountId = static_cast<int>(id);
        t.type = (p[2] == "DEPOSIT") ? TxnType::Deposit : TxnType::Withdrawal;
        t.amountCents = amt;
        t.balanceAfterCents = bal;
        out.push_back(t);
    }
    return true;
}

}  // namespace bms
