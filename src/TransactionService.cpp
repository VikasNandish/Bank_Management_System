#include "TransactionService.h"
#include "Util.h"
#include "Validator.h"

namespace bms {

TransactionService::TransactionService(AccountManager& accounts, Storage& storage, AuditLogger& audit)
    : accounts_(accounts), storage_(storage), audit_(audit) {}

TransactionService::Result TransactionService::deposit(int accountId, long long amountCents) {
    return apply(accountId, amountCents, TxnType::Deposit);
}

TransactionService::Result TransactionService::withdraw(int accountId, long long amountCents) {
    return apply(accountId, amountCents, TxnType::Withdrawal);
}

TransactionService::Result TransactionService::apply(int accountId, long long amountCents, TxnType type) {
    const std::string idText = std::to_string(accountId);
    const std::string event = txnTypeToString(type);

    Account* acc = accounts_.find(accountId);
    if (!acc) {
        audit_.log(idText, event, "FAILED_UNKNOWN_ACCOUNT");
        return Result::UnknownAccount;
    }
    // Validate BEFORE touching the stored balance (BMS-SR-004).
    if (amountCents <= 0 || amountCents > kMaxAmountCents) {
        audit_.log(idText, event, "FAILED_INVALID_AMOUNT");
        return Result::InvalidAmount;
    }

    long long newBalance = acc->balanceCents;
    if (type == TxnType::Deposit) {
        if (acc->balanceCents + amountCents > kMaxAmountCents * 10) {  // overflow guard
            audit_.log(idText, event, "FAILED_INVALID_AMOUNT");
            return Result::InvalidAmount;
        }
        newBalance += amountCents;
    } else {
        if (amountCents > kWithdrawalLimitCents) {
            audit_.log(idText, event, "FAILED_LIMIT_EXCEEDED");
            return Result::LimitExceeded;
        }
        if (amountCents > acc->balanceCents) {
            audit_.log(idText, event, "FAILED_INSUFFICIENT_FUNDS");
            return Result::InsufficientFunds;
        }
        newBalance -= amountCents;
    }

    const long long oldBalance = acc->balanceCents;
    acc->balanceCents = newBalance;
    if (!accounts_.save()) {
        acc->balanceCents = oldBalance;  // roll back so memory matches file
        audit_.log(idText, event, "FAILED_STORAGE_ERROR");
        return Result::StorageError;
    }

    Transaction t;
    t.timestamp = nowTimestamp();
    t.accountId = accountId;
    t.type = type;
    t.amountCents = amountCents;
    t.balanceAfterCents = newBalance;
    if (!storage_.appendTransaction(t)) {
        audit_.log(idText, event, "FAILED_STORAGE_ERROR");
        return Result::StorageError;
    }

    audit_.log(idText, event, "SUCCESS");
    return Result::Success;
}

}  // namespace bms
