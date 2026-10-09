#include "AuditLogger.h"
#include <filesystem>
#include <fstream>
#include "Util.h"

namespace bms {

AuditLogger::AuditLogger(std::string path) : path_(std::move(path)) {
    std::error_code ec;
    std::filesystem::path parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
}

void AuditLogger::log(const std::string& accountId, const std::string& event,
                      const std::string& outcome) const {
    std::ofstream out(path_, std::ios::app);
    if (out) out << nowTimestamp() << '|' << accountId << '|' << event << '|' << outcome << '\n';
}

std::vector<std::string> AuditLogger::readAll() const {
    std::vector<std::string> lines;
    std::ifstream in(path_);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

}  // namespace bms
