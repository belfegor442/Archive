#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::ruleengine {

enum class RuleOperator : std::uint8_t {
  Equals,
  NotEquals,
  Contains,
  GreaterThan,
  LessThan,
  InSet,
  Derived
};

const char* RuleOperatorName(RuleOperator op);

enum class RuleAction : std::uint8_t {
  GenerateAnalysis,
  IncreasePriority,
  TagEvent,
  LinkCorrelation,
  NoOp
};

const char* RuleActionName(RuleAction action);

struct RuleCondition {
  std::string field;
  RuleOperator op = RuleOperator::Equals;
  std::string value;
  std::vector<std::string> value_set;

  bool matches(const std::string& actual_value) const;
  bool matchesNumeric(double actual, double threshold) const;
  bool isValid() const;
};

struct RuleEffect {
  RuleAction action = RuleAction::GenerateAnalysis;
  std::string analysis_type;
  std::string tag;
  std::string priority_override;
  std::string correlation_id;

  bool isValid() const;
};

struct Rule {
  std::string rule_id;
  std::string name;
  std::string description;
  std::vector<RuleCondition> conditions;
  std::vector<RuleEffect> effects;
  bool enabled = true;
  std::uint32_t cooldown_ms = 0;
  std::int64_t last_fired_ms = 0;
  std::size_t fire_count = 0;

  bool isValid() const;
  bool shouldFire(std::int64_t now_ms) const;
  void recordFire(std::int64_t now_ms);
};

struct ObservedEvent {
  std::string event_id;
  std::string event_type;
  std::string source;
  std::string actor;
  std::string severity;
  std::string device_type;
  std::string device_trust;
  std::string session_id;
  std::string activity_id;
  std::string payload;
  std::int64_t timestamp_ms = 0;
  std::unordered_map<std::string, std::string> fields;

  std::string getField(const std::string& name) const;
  bool isValid() const;
};

struct AnalysisEvent {
  std::string event_id;
  std::string source_event_id;
  std::string analysis_type;
  std::string description;
  std::string priority;
  std::string correlation_id;
  std::vector<std::string> tags;
  std::int64_t generated_ms = 0;

  bool isValid() const;
};

struct RuleEvaluationResult {
  bool matched = false;
  std::vector<AnalysisEvent> generated;
  std::size_t rules_evaluated = 0;
  std::size_t rules_matched = 0;

  std::string summary() const;
};

class RuleEngine {
public:
  RuleEngine();
  ~RuleEngine();

  RuleEngine(const RuleEngine&) = delete;
  RuleEngine& operator=(const RuleEngine&) = delete;

  void addRule(const Rule& rule);
  bool removeRule(const std::string& rule_id);
  void enableRule(const std::string& rule_id, bool enabled);

  RuleEvaluationResult evaluate(const ObservedEvent& event, std::int64_t now_ms = 0);

  std::size_t ruleCount() const;
  std::size_t activeRuleCount() const;
  std::size_t totalEvaluations() const;
  std::size_t totalMatches() const;

  void clear();
  std::vector<Rule> allRules() const;

private:
  bool evaluateCondition(const RuleCondition& cond, const ObservedEvent& event) const;
  AnalysisEvent createAnalysis(const Rule& rule, const ObservedEvent& event, const RuleEffect& effect) const;

  mutable std::mutex mu_;
  std::vector<Rule> rules_;
  std::atomic<std::size_t> total_evaluations_{0};
  std::atomic<std::size_t> total_matches_{0};
};

}  // namespace monix::collectors::ruleengine
