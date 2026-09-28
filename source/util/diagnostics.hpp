#pragma once
#include <string>

namespace diagnostic {
std::string current_report();
std::string save(const std::string& report, bool* saved = nullptr);
}
