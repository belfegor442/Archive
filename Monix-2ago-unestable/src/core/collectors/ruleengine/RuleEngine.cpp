#include "RuleEngine.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace monix::collectors::ruleengine {

const char* RuleOperatorName(RuleOperator op) {
  switch (op) {
    case RuleOperator::Equals:      return "Equals";
    case RuleOperator::NotEquals:   return "NotEquals";
    case RuleOperator::Contains:    return "Contains";
    case RuleOperator::GreaterThan: return "GreaterThan";
    case RuleOperator::LessThan:    return "LessThan";
    case RuleOperator::InSet:       return "InSet";
    case RuleOperator::Derived:     return "Derived";
  }
  return "Unknown";
}

const char* RuleActionName(RuleAction action) {
  switch (action) {
    case RuleAction::GenerateAnalysis: return "GenerateAnalysis";
    case RuleAction::IncreasePriority: return "IncreasePriority";
    case RuleAction::TagEvent:         return "TagEvent";
    case RuleAction::LinkCorrelation:  return "LinkCorrelation";
    case RuleAction::NoOp:             return "NoOp";
  }
  return "Unknown";
}

bool RuleCondition::matches(const std::string& actual_value) const {
  switch (op) {
    case RuleOperator::Equals:
      return actual_value == value;
    case RuleOperator::NotEquals:
      return actual_value != value;
    case RuleOperator::Contains:
      return actual_value.find(value) != std::string::npos;
    case RuleOperator::GreaterThan:
    case RuleOperator::LessThan: {
      try {
        double actual = std::stod(actual_value);
        double threshold = std::stod(value);
        if (op == RuleOperator::GreaterThan) return actual > threshold;
        return actual < threshold;
      } catch (...) { return false; }
    }
    case RuleOperator::InSet:
      return std::find(value_set.begin(), value_set.end(), actual_value) != value_set.end();
    case RuleOperator::Derived:
      return !actual_value.empty();
  }
  return false;
}

bool RuleCondition::matchesNumeric(double actual, double threshold) const {
  if (op == RuleOperator::GreaterThan) return actual > threshold;
  if (op == RuleOperator::LessThan) return actual < threshold;
  return false;
}

bool RuleCondition::isValid() const {
  return !field.empty();
}

bool RuleEffect::isValid() const {
  if (action == RuleAction::GenerateAnalysis) return !analysis_type.empty();
  return true;
}

bool Rule::isValid() const {
  if (rule_id.empty() || name.empty()) return false;
  if (effects.empty()) return false;
  for (const auto& e : effects) {
    if (!e.isValid()) return false;
  }
  return true;
}

bool Rule::shouldFire(std::int64_t now_ms) const {
  if (!enabled) return false;
  if (cooldown_ms == 0) return true;
  if (last_fired_ms == 0) return true;
  return (now_ms - last_fired_ms) >= static_cast<std::int64_t>(cooldown_ms);
}

void Rule::recordFire(std::int64_t now_ms) {
  last_fired_ms = now_ms;
  fire_count++;
}

std::string ObservedEvent::getField(const std::string& name) const {
  if (name == "event_type") return event_type;
  if (name == "source") return source;
  if (name == "actor") return actor;
  if (name == "severity") return severity;
  if (name == "device_type") return device_type;
  if (name == "device_trust") return device_trust;
  if (name == "session_id") return session_id;
  if (name == "activity_id") return activity_id;
  if (name == "payload") return payload;
  auto it = fields.find(name);
  if (it != fields.end()) return it->second;
  return "";
}

bool ObservedEvent::isValid() const {
  return !event_id.empty();
}

bool AnalysisEvent::isValid() const {
  return !event_id.empty() && !source_event_id.empty() && !analysis_type.empty();
}

std::string RuleEvaluationResult::summary() const {
  return "evaluated=" + std::to_string(rules_evaluated) +
    " matched=" + std::to_string(rules_matched) +
    " generated=" + std::to_string(generated.size());
}

RuleEngine::RuleEngine() {}
RuleEngine::~RuleEngine() {}

void RuleEngine::addRule(const Rule& rule) {
  std::lock_guard<std::mutex> lock(mu_);
  rules_.push_back(rule);
}

bool RuleEngine::removeRule(const std::string& rule_id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = std::remove_if(rules_.begin(), rules_.end(),
    [&](const Rule& r) { return r.rule_id == rule_id; });
  if (it != rules_.end()) {
    rules_.erase(it, rules_.end());
    return true;
  }
  return false;
}

void RuleEngine::enableRule(const std::string& rule_id, bool enabled) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& r : rules_) {
    if (r.rule_id == rule_id) {
      r.enabled = enabled;
      return;
    }
  }
}

RuleEvaluationResult RuleEngine::evaluate(const ObservedEvent& event, std::int64_t now_ms) {
  std::lock_guard<std::mutex> lock(mu_);
  RuleEvaluationResult result;
  total_evaluations_++;

  if (now_ms == 0) {
    now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }

  for (auto& rule : rules_) {
    result.rules_evaluated++;
    if (!rule.shouldFire(now_ms)) continue;

    bool all_match = true;
    for (const auto& cond : rule.conditions) {
      std::string actual = event.getField(cond.field);
      if (!cond.matches(actual)) {
        all_match = false;
        break;
      }
    }

    if (all_match) {
      rule.recordFire(now_ms);
      result.rules_matched++;
      total_matches_++;

      for (const auto& effect : rule.effects) {
        if (effect.action == RuleAction::GenerateAnalysis) {
          result.generated.push_back(createAnalysis(rule, event, effect));
        }
      }
    }
  }

  return result;
}

std::size_t RuleEngine::ruleCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return rules_.size();
}

std::size_t RuleEngine::activeRuleCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t count = 0;
  for (const auto& r : rules_) {
    if (r.enabled) count++;
  }
  return count;
}

std::size_t RuleEngine::totalEvaluations() const {
  return total_evaluations_;
}

std::size_t RuleEngine::totalMatches() const {
  return total_matches_;
}

void RuleEngine::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  rules_.clear();
  total_evaluations_ = 0;
  total_matches_ = 0;
}

std::vector<Rule> RuleEngine::allRules() const {
  std::lock_guard<std::mutex> lock(mu_);
  return rules_;
}

bool RuleEngine::evaluateCondition(const RuleCondition& cond, const ObservedEvent& event) const {
  std::string actual = event.getField(cond.field);
  return cond.matches(actual);
}

AnalysisEvent RuleEngine::createAnalysis(const Rule& rule, const ObservedEvent& event, const RuleEffect& effect) const {
  AnalysisEvent analysis;
  analysis.event_id = "analysis_" + rule.rule_id + "_" + event.event_id;
  analysis.source_event_id = event.event_id;
  analysis.analysis_type = effect.analysis_type;
  analysis.description = "Rule [" + rule.name + "] triggered on " + event.event_id;
  analysis.priority = effect.priority_override.empty() ? "MEDIUM" : effect.priority_override;
  analysis.correlation_id = effect.correlation_id.empty() ? event.activity_id : effect.correlation_id;
  analysis.generated_ms = event.timestamp_ms;
  if (!effect.tag.empty()) {
    analysis.tags.push_back(effect.tag);
  }
  return analysis;
}

}  // namespace monix::collectors::ruleengine
