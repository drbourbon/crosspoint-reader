#pragma once

#include <functional>

#include "activities/ActivityWithSubactivity.h"
#include "util/ButtonNavigator.h"
#include "trmnl/TrmnlService.h"

/**
 * Submenu for TRMNL Service settings.
 */
class TrmnlSettingsActivity final : public ActivityWithSubactivity {
 public:
  explicit TrmnlSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::function<void()>& onBack)
      : ActivityWithSubactivity("TrmnlSettings", renderer, mappedInput), onBack(onBack) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(Activity::RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;

  size_t selectedIndex = 0;
  const std::function<void()> onBack;
  TrmnlService::Config config;

  void handleSelection();
};