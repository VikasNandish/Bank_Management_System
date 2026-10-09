// Menu-driven console front end. All business logic lives in the services (src/*.cpp);
// this file only reads input and prints output.
#include <iostream>
#include <string>
#include "AccountManager.h"
#include "AuditLogger.h"
#include "AuthService.h"
#include "Storage.h"
#include "TransactionService.h"
#include "Util.h"
#include "Validator.h"

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace bms;

namespace {

bool stdinIsTty() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt << std::flush;
    return static_cast<bool>(std::getline(std::cin, out));
}

// BMS-SR-002: PIN characters are shown as '*' when typed at a terminal.
bool readPin(const std::string& prompt, std::string& pin) {
    pin.clear();
    std::cout << prompt << std::flush;
    if (!stdinIsTty()) return static_cast<bool>(std::getline(std::cin, pin));
#ifdef _WIN32
    int c;
    while ((c = _getch()) != '\r' && c != '\n') {
        if (c == 8) {
            if (!pin.empty()) { pin.pop_back(); std::cout << "\b \b" << std::flush; }
        } else {
            pin.push_back(static_cast<char>(c));
            std::cout << '*' << std::flush;
        }
    }
#else
    termios oldt{}, newt{};
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != '\n' && c != '\r') {
        if (c == 127 || c == 8) {
            if (!pin.empty()) { pin.pop_back(); std::cout << "\b \b" << std::flush; }
        } else {
            pin.push_back(c);
            std::cout << '*' << std::flush;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
    std::cout << "\n";
    return true;
}

bool parseId(const std::string& s, int& id) {
    if (s.empty() || s.size() > 9) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    id = std::stoi(s);
    return true;
}

const char* depositMessage(TransactionService::Result r) {
    switch (r) {
        case TransactionService::Result::Success: return "Deposit successful.";
        case TransactionService::Result::InvalidAmount: return "Invalid amount.";
        case TransactionService::Result::UnknownAccount: return "Account not found.";
        default: return "Deposit failed (storage error).";
    }
}

const char* withdrawMessage(TransactionService::Result r) {
    switch (r) {
        case TransactionService::Result::Success: return "Withdrawal successful.";
        case TransactionService::Result::InvalidAmount: return "Invalid amount.";
        case TransactionService::Result::InsufficientFunds: return "Insufficient balance.";
        case TransactionService::Result::LimitExceeded: return "Amount exceeds the per-transaction withdrawal limit.";
        case TransactionService::Result::UnknownAccount: return "Account not found.";
        default: return "Withdrawal failed (storage error).";
    }
}

void createAccountFlow(AccountManager& accounts) {
    std::string name, pin;
    if (!readLine("Full name: ", name)) return;
    if (!readPin("Choose a 4-6 digit PIN: ", pin)) return;
    int id = 0;
    switch (accounts.createAccount(name, pin, id)) {
        case AccountManager::CreateResult::Success:
            std::cout << "Account created. Your account number is " << id << "\n"; break;
        case AccountManager::CreateResult::InvalidName:
            std::cout << "Invalid name (1-50 characters, no '|').\n"; break;
        case AccountManager::CreateResult::InvalidPin:
            std::cout << "Invalid PIN (must be 4-6 digits).\n"; break;
        case AccountManager::CreateResult::SaveFailed:
            std::cout << "Could not save the account.\n"; break;
    }
}

void sessionMenu(int id, AccountManager& accounts, TransactionService& txn) {
    std::string choice, text;
    while (true) {
        std::cout << "\n--- Account " << id << " ---\n1. Balance\n2. Deposit\n3. Withdraw\n4. Logout\n";
        if (!readLine("Choice: ", choice)) return;
        if (choice == "1") {
            long long bal = 0;
            if (accounts.getBalance(id, bal)) std::cout << "Balance: " << formatMoney(bal) << "\n";
        } else if (choice == "2" || choice == "3") {
            if (!readLine("Amount: ", text)) return;
            long long cents = 0;
            if (!parseAmountCents(text, cents)) { std::cout << "Invalid amount.\n"; continue; }
            auto r = (choice == "2") ? txn.deposit(id, cents) : txn.withdraw(id, cents);
            std::cout << ((choice == "2") ? depositMessage(r) : withdrawMessage(r)) << "\n";
        } else if (choice == "4") {
            std::cout << "Logged out.\n";
            return;
        } else {
            std::cout << "Invalid choice.\n";
        }
    }
}

void loginFlow(AccountManager& accounts, AuthService& auth, TransactionService& txn) {
    std::string idText, pin;
    int id = 0;
    if (!readLine("Account number: ", idText)) return;
    if (!parseId(idText, id)) { std::cout << "Invalid account number.\n"; return; }
    if (!readPin("PIN: ", pin)) return;
    switch (auth.login(id, pin)) {
        case AuthService::LoginResult::Success: sessionMenu(id, accounts, txn); break;
        case AuthService::LoginResult::InvalidCredentials: std::cout << "Incorrect account number or PIN.\n"; break;
        case AuthService::LoginResult::AccountLocked: std::cout << "Account is locked. Contact an administrator.\n"; break;
        case AuthService::LoginResult::UnknownAccount: std::cout << "Incorrect account number or PIN.\n"; break;
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dataDir = (argc > 1) ? argv[1] : "data";
    Storage storage(dataDir);
    AuditLogger audit(dataDir + "/audit.log");
    AccountManager accounts(storage);
    if (!accounts.load()) {
        std::cerr << "Error: account data file is corrupt. Refusing to start so data is not overwritten.\n";
        return 1;
    }
    AuthService auth(accounts, audit);
    TransactionService txn(accounts, storage, audit);

    std::string choice;
    while (true) {
        std::cout << "\n=== Bank Management System ===\n1. Create account\n2. Login\n3. Exit\n";
        if (!readLine("Choice: ", choice)) break;
        if (choice == "1") createAccountFlow(accounts);
        else if (choice == "2") loginFlow(accounts, auth, txn);
        else if (choice == "3") break;
        else std::cout << "Invalid choice.\n";
    }
    std::cout << "Goodbye.\n";
    return 0;
}
