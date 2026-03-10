#include "TrmnlRegisterActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <WiFi.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "trmnl/TrmnlService.h"

void TrmnlRegisterActivity::onWifiSelectionComplete(const bool success) {
  if (!success) {
    {
      RenderLock lock(*this);
      state = FAILED;
      errorMessage = tr(STR_WIFI_CONN_FAILED);
    }
    requestUpdate();
    return;
  }

  {
    RenderLock lock(*this);
    state = REGISTERING;
    statusMessage = tr(STR_TRMNL_REGISTERING);
  }
  requestUpdate();

  performRegistration();
}

void TrmnlRegisterActivity::performRegistration() {
  const bool result = TrmnlService::registerDevice();

  {
    RenderLock lock(*this);
    if (result) {
      state = SUCCESS;
      statusMessage = tr(STR_TRMNL_REG_SUCCESS);
      // The config is updated by registerDevice and saved.
      // We can get the friendlyId from the static config.
      friendlyId = TrmnlService::getConfig().friendlyId;
    } else {
      state = FAILED;
      // TrmnlService doesn't have a detailed error string, so use a generic one.
      errorMessage = tr(STR_TRMNL_REG_FAILED);
    }
  }
  requestUpdate();
}

void TrmnlRegisterActivity::onEnter() {
  Activity::onEnter();

  // Check if already connected
  if (WiFi.status() == WL_CONNECTED) {
    onWifiSelectionComplete(true);
    return;
  }

  // Launch WiFi selection
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void TrmnlRegisterActivity::onExit() {
  Activity::onExit();

  // Turn off wifi
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
}

void TrmnlRegisterActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TRMNL_REGISTER_DEVICE));
  const auto height = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = (pageHeight - height) / 2;

  if (state == REGISTERING) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, statusMessage.c_str());
  } else if (state == SUCCESS) {
    renderer.drawCenteredText(UI_10_FONT_ID, top - height, tr(STR_TRMNL_REG_SUCCESS), true, EpdFontFamily::BOLD);
    if (!friendlyId.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, top, (std::string(tr(STR_TRMNL_FRIENDLY_ID)) + ": " + friendlyId).c_str());
    }
    renderer.drawCenteredText(UI_10_FONT_ID, top + height + 10, tr(STR_TRMNL_REG_READY));
  } else if (state == FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_TRMNL_REG_FAILED), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, top + height + 10, errorMessage.c_str());
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void TrmnlRegisterActivity::loop() {
  if (state == SUCCESS || state == FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      finish();
    }
  }
}