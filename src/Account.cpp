#include "Account.h"

namespace bms {

std::string txnTypeToString(TxnType t) {
    return t == TxnType::Deposit ? "DEPOSIT" : "WITHDRAWAL";
}

}  // namespace bms
