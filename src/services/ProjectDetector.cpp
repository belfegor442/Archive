#include "ProjectDetector.h"

#include <filesystem>

namespace archive::services {

ProjectDetector::ProjectDetector() {
    register_defaults();
}

void ProjectDetector::register_pattern(ProjectPattern pattern) {
    patterns_.push_back(std::move(pattern));
}

void ProjectDetector::register_patterns(std::vector<ProjectPattern> patterns) {
    for (auto& p : patterns) {
        patterns_.push_back(std::move(p));
    }
}

ProjectDetector::DetectionResult ProjectDetector::detect(const std::string& path) const {
    DetectionResult result;

    if (!std::filesystem::is_directory(path)) return result;

    std::string best_type;
    int best_score = 0;

    for (const auto& pattern : patterns_) {
        int score = 0;
        for (const auto& indicator : pattern.indicators) {
            if (has_indicator(path, indicator)) {
                score++;
            }
        }
        if (score > best_score) {
            best_score = score;
            best_type = pattern.type;
        }
    }

    if (best_score > 0) {
        result.is_project = true;
        result.project_type = best_type;
        if (best_score >= 3)
            result.confidence = "high";
        else if (best_score >= 2)
            result.confidence = "medium";
        else
            result.confidence = "low";
    }

    return result;
}

std::vector<ProjectPattern> ProjectDetector::get_all_patterns() const {
    return patterns_;
}

bool ProjectDetector::has_indicator(const std::string& path, const std::string& indicator) const {
    return std::filesystem::exists(path + "/" + indicator);
}

void ProjectDetector::register_defaults() {
    patterns_.emplace_back("C++", "cpp", std::vector<std::string>{
        "CMakeLists.txt", "Makefile", "*.cpp", "*.h", "*.hpp", "meson.build"
    });

    patterns_.emplace_back("C", "c", std::vector<std::string>{
        "CMakeLists.txt", "Makefile", "*.c", "*.h"
    });

    patterns_.emplace_back("Rust", "rust", std::vector<std::string>{
        "Cargo.toml", "src/main.rs", "src/lib.rs"
    });

    patterns_.emplace_back("Python", "python", std::vector<std::string>{
        "setup.py", "pyproject.toml", "requirements.txt", "Pipfile",
        "__init__.py", "manage.py"
    });

    patterns_.emplace_back("Node.js", "nodejs", std::vector<std::string>{
        "package.json", "tsconfig.json", ".eslintrc", "yarn.lock", "pnpm-lock.yaml"
    });

    patterns_.emplace_back("Go", "go", std::vector<std::string>{
        "go.mod", "go.sum", "main.go"
    });

    patterns_.emplace_back("Java", "java", std::vector<std::string>{
        "pom.xml", "build.gradle", "build.gradle.kts", "src/main/java"
    });

    patterns_.emplace_back("C#", "csharp", std::vector<std::string>{
        "*.csproj", "*.sln", "src/", "Program.cs"
    });

    patterns_.emplace_back("Swift", "swift", std::vector<std::string>{
        "Package.swift", "*.xcodeproj", "Sources/"
    });

    patterns_.emplace_back("Kotlin", "kotlin", std::vector<std::string>{
        "build.gradle.kts", "src/main/kotlin"
    });

    patterns_.emplace_back("Web", "web", std::vector<std::string>{
        "index.html", "package.json", "webpack.config.js", "vite.config.js"
    });

    patterns_.emplace_back("Flutter", "flutter", std::vector<std::string>{
        "pubspec.yaml", "lib/main.dart", "android/", "ios/"
    });

    patterns_.emplace_back("Docker", "docker", std::vector<std::string>{
        "Dockerfile", "docker-compose.yml", "docker-compose.yaml", ".dockerignore"
    });

    patterns_.emplace_back("Arduino", "arduino", std::vector<std::string>{
        "*.ino", "platformio.ini"
    });

    patterns_.emplace_back("LaTeX", "latex", std::vector<std::string>{
        "*.tex", "*.bib", "Makefile", "latexmkrc"
    });
}

} // namespace archive::services
