#include "AuthService.h"
#include "Hasher.h"

namespace bms {

AuthService::AuthService(AccountManager& accounts, AuditLogger& audit)
    : accounts_(accounts), audit_(audit) {}

AuthService::LoginResult AuthService::login(int accountId, const std::string& pin) {
    const std::string idText = std::to_string(accountId);
    Account* acc = accounts_.find(accountId);

    if (!acc) {
        audit_.log(idText, "LOGIN", "FAILED_UNKNOWN_ACCOUNT");
        return LoginResult::UnknownAccount;
    }
    if (acc->locked) {
        audit_.log(idText, "LOGIN", "REJECTED_LOCKED");
        return LoginResult::AccountLocked;
    }

    if (hashPin(pin, accountId) == acc->pinHash) {
        acc->failedAttempts = 0;
        accounts_.save();
        audit_.log(idText, "LOGIN", "SUCCESS");
        return LoginResult::Success;
    }

    acc->failedAttempts++;
    if (acc->failedAttempts >= kMaxFailedAttempts) acc->locked = true;
    accounts_.save();
    audit_.log(idText, "LOGIN", acc->locked ? "FAILED_ACCOUNT_LOCKED" : "FAILED_BAD_PIN");
    return LoginResult::InvalidCredentials;
}

bool AuthService::adminUnlock(int accountId) {
    Account* acc = accounts_.find(accountId);
    if (!acc) return false;
    acc->locked = false;
    acc->failedAttempts = 0;
    accounts_.save();
    audit_.log(std::to_string(accountId), "ADMIN_UNLOCK", "SUCCESS");
    return true;
}

}  // namespace bms
