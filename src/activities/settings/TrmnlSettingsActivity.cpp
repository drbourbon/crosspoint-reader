#include "TrmnlSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int MENU_ITEMS = 2;
// Nota: si presume che questi ID di stringa vengano aggiunti a I18nKeys.h
const StrId menuNames[MENU_ITEMS] = {StrId::STR_TRMNL_ENABLED, StrId::STR_TRMNL_SERVER_URL};
}  // namespace

void TrmnlSettingsActivity::onEnter() {
  LOG_DBG("TRMNL", "onEnter");
  ActivityWithSubactivity::onEnter();

  LOG_DBG("TRMNL", "getting config");
  config = TrmnlService::getConfig();

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[64];
  snprintf(macStr, sizeof(macStr), "%s %02x-%02x-%02x-%02x-%02x-%02x", tr(STR_MAC_ADDRESS), mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
  cachedMacAddress = std::string(macStr);

  selectedIndex = 0;
  requestUpdate();
  LOG_DBG("TRMNL", "onEnter done");
}

void TrmnlSettingsActivity::onExit() { ActivityWithSubactivity::onExit(); }

void TrmnlSettingsActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  // Handle navigation
  buttonNavigator.onNext([this] {
    selectedIndex = (selectedIndex + 1) % MENU_ITEMS;
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = (selectedIndex + MENU_ITEMS - 1) % MENU_ITEMS;
    requestUpdate();
  });
}

void TrmnlSettingsActivity::handleSelection() {
  LOG_DBG("TRMNL", "handleSelection %d", selectedIndex);

  if (selectedIndex == 0) {
    // Toggle enabled
    config.enabled = !config.enabled;
    TrmnlService::setConfig(config);
    TrmnlService::saveConfig();
    requestUpdate();
  } else if (selectedIndex == 1) {
    // Server URL
    exitActivity();
    enterNewActivity(new KeyboardEntryActivity(
        renderer, mappedInput, tr(STR_TRMNL_SERVER_URL), config.serverUrl,
        255,    // maxLength
        false,  // not password
        [this](const std::string& url) {
          config.serverUrl = url;
          TrmnlService::setConfig(config);
          TrmnlService::saveConfig();
          exitActivity();
          requestUpdate();
        },
        [this]() {
          exitActivity();
          requestUpdate();
        }));
  }
}

void TrmnlSettingsActivity::render(Activity::RenderLock&&) {
  LOG_DBG("TRMNL", "render start");
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TRMNL_SETTINGS));

  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    cachedMacAddress.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(MENU_ITEMS),
      static_cast<int>(selectedIndex), [](int index) { return std::string(I18N.get(menuNames[index])); }, nullptr,
      nullptr,
      [this](int index) {
        LOG_DBG("TRMNL", "render item %d", index);
        if (index == 0) {
          return config.enabled ? std::string(tr(STR_STATE_ON)) : std::string(tr(STR_STATE_OFF));
        } else if (index == 1) {
          return config.serverUrl.empty() ? std::string(tr(STR_NOT_SET)) : config.serverUrl;
        }
        return std::string("");
      },
      true);

  // Draw help text at bottom
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
  LOG_DBG("TRMNL", "render done");
}