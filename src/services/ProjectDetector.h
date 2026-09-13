#pragma once

#include <string>
#include <vector>
#include <functional>

namespace archive::services {

struct ProjectPattern {
    std::string name;
    std::string type;
    std::vector<std::string> indicators;

    ProjectPattern(std::string name_, std::string type_, std::vector<std::string> indicators_)
        : name(std::move(name_))
        , type(std::move(type_))
        , indicators(std::move(indicators_))
    {}
};

class ProjectDetector {
public:
    using PatternList = std::vector<ProjectPattern>;

    ProjectDetector();

    void register_pattern(ProjectPattern pattern);
    void register_patterns(std::vector<ProjectPattern> patterns);

    struct DetectionResult {
        bool is_project = false;
        std::string project_type;
        std::string confidence;
    };

    DetectionResult detect(const std::string& path) const;
    std::vector<ProjectPattern> get_all_patterns() const;

private:
    std::vector<ProjectPattern> patterns_;

    void register_defaults();
    bool has_indicator(const std::string& path, const std::string& indicator) const;
};

} // namespace archive::services
