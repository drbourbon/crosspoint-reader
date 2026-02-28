#pragma once

#include <functional>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
#include "trmnl/TrmnlService.h"

/**
 * Submenu for TRMNL Service settings.
 */
class TrmnlSettingsActivity final : public Activity {
 public:
  explicit TrmnlSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TrmnlSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;

  size_t selectedIndex = 0;
  const std::function<void()> onBack;
  TrmnlService::Config config;
  std::string cachedMacAddress;

  void handleSelection();
};