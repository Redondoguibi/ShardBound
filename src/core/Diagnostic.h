#pragma once
#include <stdexcept>
#include <string>
#include <utility>
namespace game {
struct Diagnostic {
    std::string code, message, file, mod, resource;
    unsigned line = 0, column = 0;
};
class DiagnosticError : public std::runtime_error {
public:
    explicit DiagnosticError(Diagnostic value)
        : std::runtime_error(value.code + " " + value.file + ":" + std::to_string(value.line) + ":" + std::to_string(value.column) + " " + value.message), diagnostic(std::move(value)) {}
    Diagnostic diagnostic;
};
}
