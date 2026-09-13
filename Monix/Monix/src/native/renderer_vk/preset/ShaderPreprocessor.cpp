#include "ShaderPreprocessor.hpp"

#include "../core/FileSystem.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>

namespace monix::renderer_vk {
namespace {

struct ConditionalFrame {
    bool parentActive = true;
    bool branchActive = true;
    bool branchTaken = false;
    bool seenElse = false;
};

#ifndef MONIX_ANON_TRIM
#define MONIX_ANON_TRIM
std::string trim(std::string_view text) {
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return std::string(text.substr(first, last - first));
}
#endif

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string parseIncludePath(std::string_view directive) {
    const auto firstQuote = directive.find('"');
    if (firstQuote != std::string_view::npos) {
        const auto lastQuote = directive.find('"', firstQuote + 1);
        if (lastQuote != std::string_view::npos) {
            return std::string(directive.substr(firstQuote + 1, lastQuote - firstQuote - 1));
        }
    }
    const auto firstAngle = directive.find('<');
    if (firstAngle != std::string_view::npos) {
        const auto lastAngle = directive.find('>', firstAngle + 1);
        if (lastAngle != std::string_view::npos) {
            return std::string(directive.substr(firstAngle + 1, lastAngle - firstAngle - 1));
        }
    }
    return {};
}

std::string macroNameFromDefine(std::string_view rest) {
    auto trimmed = trim(rest);
    rest = trimmed;
    size_t end = 0;
    while (end < rest.size()) {
        const char c = rest[end];
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) break;
        ++end;
    }
    if (end == 0) return {};
    return std::string(rest.substr(0, end));
}

std::string macroValueFromDefine(std::string_view rest) {
    auto trimmed = trim(rest);
    rest = trimmed;
    size_t end = 0;
    while (end < rest.size()) {
        const char c = rest[end];
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) break;
        ++end;
    }
    while (end < rest.size() && std::isspace(static_cast<unsigned char>(rest[end]))) ++end;
    if (end >= rest.size()) return "1";
    return trim(rest.substr(end));
}

class ExpressionParser {
public:
    ExpressionParser(std::string_view text, const std::unordered_map<std::string, std::string>& macros)
        : text_(text), macros_(macros) {}

    int parse() {
        pos_ = 0;
        return parseOr();
    }

private:
    std::string_view text_;
    const std::unordered_map<std::string, std::string>& macros_;
    size_t pos_ = 0;

    void skipSpaces() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }

    bool consume(std::string_view token) {
        skipSpaces();
        if (text_.substr(pos_, token.size()) == token) {
            pos_ += token.size();
            return true;
        }
        return false;
    }

    int parseOr() {
        int lhs = parseAnd();
        while (consume("||")) lhs = (lhs || parseAnd()) ? 1 : 0;
        return lhs;
    }

    int parseAnd() {
        int lhs = parseEquality();
        while (consume("&&")) lhs = (lhs && parseEquality()) ? 1 : 0;
        return lhs;
    }

    int parseEquality() {
        int lhs = parseRelational();
        for (;;) {
            if (consume("==")) lhs = (lhs == parseRelational()) ? 1 : 0;
            else if (consume("!=")) lhs = (lhs != parseRelational()) ? 1 : 0;
            else return lhs;
        }
    }

    int parseRelational() {
        int lhs = parseUnary();
        for (;;) {
            if (consume(">=")) lhs = (lhs >= parseUnary()) ? 1 : 0;
            else if (consume("<=")) lhs = (lhs <= parseUnary()) ? 1 : 0;
            else if (consume(">")) lhs = (lhs > parseUnary()) ? 1 : 0;
            else if (consume("<")) lhs = (lhs < parseUnary()) ? 1 : 0;
            else return lhs;
        }
    }

    int parseUnary() {
        if (consume("!")) return parseUnary() ? 0 : 1;
        if (consume("-")) return -parseUnary();
        if (consume("+")) return parseUnary();
        return parsePrimary();
    }

    int parsePrimary() {
        skipSpaces();
        if (consume("(")) {
            int value = parseOr();
            consume(")");
            return value;
        }
        if (text_.substr(pos_, 7) == "defined") {
            pos_ += 7;
            skipSpaces();
            std::string name;
            if (consume("(")) {
                name = parseIdentifier();
                consume(")");
            } else {
                name = parseIdentifier();
            }
            return macros_.find(name) != macros_.end() ? 1 : 0;
        }
        if (pos_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[pos_])) || text_[pos_] == '-')) {
            const char* first = text_.data() + pos_;
            const char* last = text_.data() + text_.size();
            int value = 0;
            auto result = std::from_chars(first, last, value);
            if (result.ec == std::errc{}) {
                pos_ = static_cast<size_t>(result.ptr - text_.data());
                return value;
            }
        }
        const std::string id = parseIdentifier();
        if (id.empty()) return 0;
        auto macro = macros_.find(id);
        if (macro == macros_.end()) return 0;
        int value = 1;
        auto raw = trim(macro->second);
        auto result = std::from_chars(raw.data(), raw.data() + raw.size(), value);
        return result.ec == std::errc{} ? value : 1;
    }

    std::string parseIdentifier() {
        skipSpaces();
        size_t begin = pos_;
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) break;
            ++pos_;
        }
        return std::string(text_.substr(begin, pos_ - begin));
    }
};

bool isActive(const std::vector<ConditionalFrame>& stack) {
    return stack.empty() || stack.back().branchActive;
}

std::string includeGuardName(const std::filesystem::path& includePath) {
    auto stem = includePath.stem().string();
    std::string guard;
    for (char c : stem) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            guard += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        } else {
            guard += '_';
        }
    }
    guard += "_INC";
    return guard;
}

std::string findIncludeGuard(const std::string& source) {
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line)) {
        auto trimmed = line;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
            trimmed.erase(trimmed.begin());
        if (trimmed.rfind("#ifndef ", 0) == 0) {
            auto macro = trimmed.substr(8);
            while (!macro.empty() && (macro.back() == ' ' || macro.back() == '\t' || macro.back() == '\r'))
                macro.pop_back();
            return macro;
        }
        if (trimmed.rfind("#pragma", 0) == 0 || trimmed.rfind("#version", 0) == 0)
            break;
    }
    return "";
}

}  // namespace

ShaderPreprocessor::ShaderPreprocessor(IncludeResolver resolver) : resolver_(std::move(resolver)) {}

Result<PreprocessResult> ShaderPreprocessor::preprocessFile(
    const std::filesystem::path& path,
    const std::unordered_map<std::string, std::string>& initialDefines) const {
    // Try direct path first, then fall back to include resolver
    auto text = FileSystem::readText(path);
    std::filesystem::path resolvedPath = FileSystem::normalize(path);
    if (!text) {
        // Try resolving the relative path through include resolver roots
        // The path may have wrong relative prefix (e.g. ../stock.slang when stock.slang is in CRT/)
        auto resolved = resolver_.resolve(path.parent_path(), path.filename().string());
        if (resolved) {
            text = FileSystem::readText(resolved.value());
            if (text) {
                resolvedPath = resolved.value();
            }
        }
        if (!text) return text.status();
    }

    PreprocessResult result;
    result.rootFile = FileSystem::normalize(path);
    result.macros = initialDefines;
    std::unordered_set<std::string> stack;
    auto preprocessed = preprocessText(text.value(), result.rootFile, result, stack, result.macros);
    if (!preprocessed) return preprocessed.status();
    result.source = preprocessed.value();
    return result;
}

Result<std::string> ShaderPreprocessor::preprocessText(const std::string& source,
                                                       const std::filesystem::path& path,
                                                       PreprocessResult& result,
                                                       std::unordered_set<std::string>& includeStack,
                                                       std::unordered_map<std::string, std::string>& macros,
                                                       bool eraseGuards) const {
    const auto normalized = FileSystem::normalize(path).string();
    if (includeStack.find(normalized) != includeStack.end()) {
        return Status::failure("Recursive include detected: " + normalized);
    }
    includeStack.insert(normalized);
    result.dependencies.push_back(FileSystem::normalize(path));

    std::ostringstream output;
    std::vector<ConditionalFrame> conditionals;
    std::istringstream input(source);
    std::string line;
    int lineNumber = 0;
    bool insideStage = false;
    std::unordered_map<std::string, std::string> commonMacros;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto trimmed = trim(line);
        if (!startsWith(trimmed, "#")) {
            if (isActive(conditionals)) output << line << "\n";
            continue;
        }

        if (startsWith(trimmed, "#pragma")) {
            if (!insideStage &&
                (trimmed.find("stage vertex") != std::string::npos ||
                 trimmed.find("stage fragment") != std::string::npos)) {
                insideStage = true;
                commonMacros = macros;
            } else if (insideStage &&
                       (trimmed.find("stage vertex") != std::string::npos ||
                        trimmed.find("stage fragment") != std::string::npos)) {
                macros = commonMacros;
            }
        }

        if (startsWith(trimmed, "#include")) {
            if (!isActive(conditionals)) continue;
            const auto includePath = parseIncludePath(trimmed);
            if (includePath.empty()) {
                return Status::failure("Malformed include in " + path.string() + ":" + std::to_string(lineNumber));
            }
            auto resolved = resolver_.resolve(path, includePath);
            if (!resolved) return resolved.status();
            auto includedText = FileSystem::readText(resolved.value());
            if (!includedText) return includedText.status();
            auto resolvedStr = resolved.value().string();
            std::replace(resolvedStr.begin(), resolvedStr.end(), '\\', '/');
            auto pathStr = path.string();
            std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
            output << "#line 1 \"" << resolvedStr << "\"\n";
            auto included = preprocessText(includedText.value(), resolved.value(), result, includeStack, macros, false);
            if (!included) return included.status();
            includeStack.erase(FileSystem::normalize(resolved.value()).string());
            output << included.value();
            output << "#line " << (lineNumber + 1) << " \"" << pathStr << "\"\n";
            continue;
        }

        if (startsWith(trimmed, "#define")) {
            if (isActive(conditionals)) {
                const auto rest = trim(std::string_view(trimmed).substr(7));
                const auto name = macroNameFromDefine(rest);
                if (!name.empty()) {
                    macros[name] = macroValueFromDefine(rest);
                }
                output << line << "\n";
            }
            continue;
        }

        if (startsWith(trimmed, "#undef")) {
            if (isActive(conditionals)) {
                const auto name = trim(std::string_view(trimmed).substr(6));
                macros.erase(name);
                output << line << "\n";
            }
            continue;
        }

        if (startsWith(trimmed, "#ifdef") || startsWith(trimmed, "#ifndef") || startsWith(trimmed, "#if")) {
            const bool parent = isActive(conditionals);
            bool active = false;
            if (startsWith(trimmed, "#ifdef")) {
                const auto name = trim(std::string_view(trimmed).substr(6));
                active = macros.find(name) != macros.end();
            } else if (startsWith(trimmed, "#ifndef")) {
                const auto name = trim(std::string_view(trimmed).substr(7));
                active = macros.find(name) == macros.end();
            } else {
                const auto expr = trim(std::string_view(trimmed).substr(3));
                active = ExpressionParser(expr, macros).parse() != 0;
            }
            conditionals.push_back(ConditionalFrame{parent, parent && active, active, false});
            continue;
        }

        if (startsWith(trimmed, "#elif")) {
            if (conditionals.empty()) return Status::failure("#elif without #if in " + path.string());
            auto& frame = conditionals.back();
            if (frame.seenElse) return Status::failure("#elif after #else in " + path.string());
            const auto expr = trim(std::string_view(trimmed).substr(5));
            const bool active = !frame.branchTaken && ExpressionParser(expr, macros).parse() != 0;
            frame.branchActive = frame.parentActive && active;
            frame.branchTaken = frame.branchTaken || active;
            continue;
        }

        if (startsWith(trimmed, "#else")) {
            if (conditionals.empty()) return Status::failure("#else without #if in " + path.string());
            auto& frame = conditionals.back();
            frame.seenElse = true;
            const bool active = !frame.branchTaken;
            frame.branchActive = frame.parentActive && active;
            frame.branchTaken = true;
            continue;
        }

        if (startsWith(trimmed, "#endif")) {
            if (conditionals.empty()) return Status::failure("#endif without #if in " + path.string());
            conditionals.pop_back();
            continue;
        }

        if (isActive(conditionals)) output << line << "\n";
    }

    includeStack.erase(normalized);
    if (!conditionals.empty()) {
        return Status::failure("Unclosed preprocessor conditional in " + path.string());
    }
    return output.str();
}

}  // namespace monix::renderer_vk
