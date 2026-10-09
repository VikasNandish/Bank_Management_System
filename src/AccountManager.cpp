#include "AccountManager.h"
#include "Hasher.h"
#include "Validator.h"

namespace bms {

namespace {
constexpr int kFirstAccountId = 1001;
}

AccountManager::AccountManager(Storage& storage) : storage_(storage) {}

bool AccountManager::load() { return storage_.loadAccounts(accounts_); }

bool AccountManager::save() const { return storage_.saveAccounts(accounts_); }

AccountManager::CreateResult AccountManager::createAccount(const std::string& name,
                                                           const std::string& pin, int& outId) {
    if (!isValidName(name)) return CreateResult::InvalidName;
    if (!isValidPin(pin)) return CreateResult::InvalidPin;

    int nextId = kFirstAccountId;
    for (const auto& a : accounts_) {
        if (a.id >= nextId) nextId = a.id + 1;
    }

    Account acc;
    acc.id = nextId;
    acc.name = name;
    acc.pinHash = hashPin(pin, nextId);
    accounts_.push_back(acc);

    if (!save()) {
        accounts_.pop_back();  // keep memory and file consistent
        return CreateResult::SaveFailed;
    }
    outId = nextId;
    return CreateResult::Success;
}

Account* AccountManager::find(int id) {
    for (auto& a : accounts_) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

const Account* AccountManager::find(int id) const {
    for (const auto& a : accounts_) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

bool AccountManager::getBalance(int id, long long& outCents) const {
    const Account* a = find(id);
    if (!a) return false;
    outCents = a->balanceCents;
    return true;
}

std::size_t AccountManager::count() const { return accounts_.size(); }

}  // namespace bms
