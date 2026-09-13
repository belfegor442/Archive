#include "../../transitions/TransitionType.hpp"
#include "../../transitions/Transition.hpp"
#include "../../transitions/ScanlineTransition.hpp"
#include "../../transitions/TransitionManager.hpp"

#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>

using namespace monix;

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
  printf("  %-40s ", name);

#define PASS() \
  { printf("[PASS]\n"); testsPassed++; }

#define FAIL(msg) \
  { printf("[FAIL] %s\n", msg); testsFailed++; return; }

#define ASSERT_TRUE(expr) \
  do { if (!(expr)) { FAIL(#expr); } } while(0)

#define ASSERT_FALSE(expr) \
  do { if ((expr)) { FAIL(#expr); } } while(0)

#define ASSERT_EQ(a, b) \
  do { if ((a) != (b)) { FAIL(#a " != " #b); } } while(0)

#define ASSERT_NEAR(a, b, eps) \
  do { if (std::abs((a) - (b)) > (eps)) { FAIL(#a " !~= " #b); } } while(0)

void test_transition_type_names() {
  TEST("TransitionType names");
  ASSERT_EQ(std::string(TransitionTypeName(TransitionType::None)), "None");
  ASSERT_EQ(std::string(TransitionTypeName(TransitionType::Scanline)), "Scanline");
  ASSERT_EQ(std::string(TransitionTypeName(TransitionType::Glitch)), "Glitch");
  ASSERT_EQ(std::string(TransitionTypeName(TransitionType::Fade)), "Fade");
  PASS();
}

void test_scanline_transition_basic() {
  TEST("ScanlineTransition basic lifecycle");
  ScanlineTransition tr;
  ASSERT_FALSE(tr.isActive());
  ASSERT_EQ(tr.type(), TransitionType::Scanline);
  ASSERT_EQ(std::string(tr.name()), "Scanline");
  ASSERT_NEAR(tr.progress(), 0.0f, 0.001f);

  TransitionConfig cfg;
  tr.start(220.0, cfg);
  ASSERT_TRUE(tr.isActive());
  ASSERT_NEAR(tr.progress(), 0.0f, 0.001f);

  tr.reset();
  ASSERT_FALSE(tr.isActive());
  PASS();
}

void test_scanline_transition_progress() {
  TEST("ScanlineTransition progress over time");
  ScanlineTransition tr;
  TransitionConfig cfg;
  tr.start(100.0, cfg);

  bool done = false;
  for (int i = 0; i < 10 && !done; i++) {
    done = tr.update(15.0);
  }
  ASSERT_TRUE(done);
  ASSERT_NEAR(tr.progress(), 1.0f, 0.01f);
  ASSERT_FALSE(tr.isActive());
  PASS();
}

void test_scanline_transition_clamping() {
  TEST("ScanlineTransition progress clamped to [0,1]");
  ScanlineTransition tr;
  TransitionConfig cfg;
  tr.start(50.0, cfg);

  for (int i = 0; i < 100; i++) tr.update(10.0);
  ASSERT_NEAR(tr.progress(), 1.0f, 0.001f);
  PASS();
}

void test_scanline_gpu_state() {
  TEST("ScanlineTransition GPU state");
  ScanlineTransition tr;
  TransitionConfig cfg;
  cfg.intensity = 0.9;
  cfg.scanlineWidth = 0.02;
  cfg.distortion = 0.005;
  cfg.noiseAmount = 0.03;
  cfg.flickerAmount = 0.04;
  tr.start(200.0, cfg);

  TransitionState s = tr.gpuState();
  ASSERT_NEAR(s.intensity, 0.9f, 0.001f);
  ASSERT_NEAR(s.scanlineWidth, 0.02f, 0.001f);
  ASSERT_NEAR(s.distortion, 0.005f, 0.001f);
  ASSERT_NEAR(s.noiseAmount, 0.03f, 0.001f);
  ASSERT_NEAR(s.flickerAmount, 0.04f, 0.001f);
  PASS();
}

void test_manager_basic() {
  TEST("TransitionManager basic state");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = true;
  mgr.setConfig(cfg);

  ASSERT_TRUE(mgr.enabled());
  ASSERT_FALSE(mgr.isTransitioning());
  ASSERT_NEAR(mgr.currentProgress(), 1.0f, 0.001f);
  PASS();
}

void test_manager_request_transition() {
  TEST("TransitionManager request transition");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = true;
  cfg.durationMs = 100.0;
  mgr.setConfig(cfg);

  bool started = mgr.requestTransition(Tab::Log, Tab::Settings, TransitionType::Scanline);
  ASSERT_TRUE(started);
  ASSERT_TRUE(mgr.isTransitioning());
  ASSERT_NEAR(mgr.currentProgress(), 0.0f, 0.01f);
  PASS();
}

void test_manager_same_tab_no_transition() {
  TEST("TransitionManager same tab no transition");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = true;
  mgr.setConfig(cfg);

  bool started = mgr.requestTransition(Tab::Log, Tab::Log, TransitionType::Scanline);
  ASSERT_FALSE(started);
  PASS();
}

void test_manager_disabled_no_transition() {
  TEST("TransitionManager disabled no transition");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = false;
  mgr.setConfig(cfg);

  bool started = mgr.requestTransition(Tab::Log, Tab::Settings, TransitionType::Scanline);
  ASSERT_FALSE(started);
  PASS();
}

void test_manager_update() {
  TEST("TransitionManager update completes");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = true;
  cfg.durationMs = 50.0;
  mgr.setConfig(cfg);

  mgr.requestTransition(Tab::Log, Tab::Tasks, TransitionType::Scanline);

  bool completed = false;
  for (int i = 0; i < 20 && !completed; i++) {
    completed = mgr.update(10.0);
  }
  ASSERT_TRUE(completed);
  ASSERT_FALSE(mgr.isTransitioning());
  PASS();
}

void test_manager_cancel() {
  TEST("TransitionManager cancel");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = true;
  cfg.durationMs = 500.0;
  mgr.setConfig(cfg);

  mgr.requestTransition(Tab::Log, Tab::Settings, TransitionType::Scanline);
  ASSERT_TRUE(mgr.isTransitioning());

  mgr.cancelCurrent();
  ASSERT_FALSE(mgr.isTransitioning());
  PASS();
}

void test_manager_replace_transition() {
  TEST("TransitionManager replace transition");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = true;
  cfg.durationMs = 500.0;
  mgr.setConfig(cfg);

  mgr.requestTransition(Tab::Log, Tab::Settings, TransitionType::Scanline);
  ASSERT_TRUE(mgr.isTransitioning());

  mgr.requestTransition(Tab::Settings, Tab::Tasks, TransitionType::Scanline);
  ASSERT_TRUE(mgr.isTransitioning());
  ASSERT_EQ(static_cast<int>(mgr.pendingTargetTab()), static_cast<int>(Tab::Tasks));
  PASS();
}

void test_manager_gpu_state() {
  TEST("TransitionManager GPU state");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = true;
  cfg.durationMs = 100.0;
  cfg.intensity = 0.75;
  mgr.setConfig(cfg);

  mgr.requestTransition(Tab::Log, Tab::Hardware, TransitionType::Scanline);

  TransitionState s = mgr.gpuState();
  ASSERT_NEAR(s.progress, 0.0f, 0.01f);
  ASSERT_NEAR(s.intensity, 0.75f, 0.001f);
  PASS();
}

void test_manager_event_callback() {
  TEST("TransitionManager event callback");
  TransitionManager mgr;
  TransitionConfig cfg;
  cfg.enabled = true;
  cfg.durationMs = 100.0;
  mgr.setConfig(cfg);

  int eventCount = 0;
  TransitionEvent::Type lastType = TransitionEvent::Type::Started;

  mgr.setEventCallback([&](const TransitionEvent& e) {
    eventCount++;
    lastType = e.type;
  });

  mgr.requestTransition(Tab::Log, Tab::Settings, TransitionType::Scanline);
  ASSERT_EQ(eventCount, 1);
  ASSERT_EQ(static_cast<int>(lastType), static_cast<int>(TransitionEvent::Type::Started));

  for (int i = 0; i < 20; i++) mgr.update(10.0);
  ASSERT_EQ(eventCount, 2);
  ASSERT_EQ(static_cast<int>(lastType), static_cast<int>(TransitionEvent::Type::Completed));
  PASS();
}

int main() {
  printf("=== Transition System Tests ===\n\n");

  test_transition_type_names();
  test_scanline_transition_basic();
  test_scanline_transition_progress();
  test_scanline_transition_clamping();
  test_scanline_gpu_state();
  test_manager_basic();
  test_manager_request_transition();
  test_manager_same_tab_no_transition();
  test_manager_disabled_no_transition();
  test_manager_update();
  test_manager_cancel();
  test_manager_replace_transition();
  test_manager_gpu_state();
  test_manager_event_callback();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
