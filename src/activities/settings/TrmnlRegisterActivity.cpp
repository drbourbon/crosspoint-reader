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
  std::string message;
  const bool result = TrmnlService::registerDevice(message);

  {
    RenderLock lock(*this);
    if (result) {
      state = SUCCESS;
      statusMessage = message;
      if (statusMessage.empty()) {
        statusMessage = tr(STR_TRMNL_REG_SUCCESS);
      }
      // The config is updated by registerDevice and saved.
      // We can get the friendlyId from the static config.
      friendlyId = TrmnlService::getConfig().friendlyId;
    } else {
      state = FAILED;
      errorMessage = message;
      if (errorMessage.empty()) {
        errorMessage = tr(STR_TRMNL_REG_FAILED);
      }
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
  } else if (state == SUCCESS) {  // Multi-line success message
    const int padding = 20;
    const int maxWidth = pageWidth - 2 * padding;
    auto statusLines = renderer.wrappedText(UI_10_FONT_ID, statusMessage.c_str(), maxWidth, 5, EpdFontFamily::BOLD);
    const int statusBlockHeight = statusLines.size() * height;
    const bool hasFriendlyId = !friendlyId.empty();
    const int friendlyIdHeight = hasFriendlyId ? height : 0;
    const int readyHeight = height;
    const int spacing = 10;
    int totalHeight = statusBlockHeight + readyHeight + spacing;
    if (hasFriendlyId) {
      totalHeight += friendlyIdHeight + spacing;
    }
    int currentY = (pageHeight - totalHeight) / 2;
    for (const auto& line : statusLines) {
      renderer.drawCenteredText(UI_10_FONT_ID, currentY, line.c_str(), true, EpdFontFamily::BOLD);
      currentY += height;
    }
    if (!friendlyId.empty()) {
      currentY += spacing;
      renderer.drawCenteredText(UI_10_FONT_ID, currentY,
                                (std::string(tr(STR_TRMNL_FRIENDLY_ID)) + ": " + friendlyId).c_str());
      currentY += height;
    }
    currentY += spacing;
    renderer.drawCenteredText(UI_10_FONT_ID, currentY, tr(STR_TRMNL_REG_READY));
  } else if (state == FAILED) {  // Multi-line error message
    const int padding = 20;
    const int maxWidth = pageWidth - 2 * padding;
    auto errorLines = renderer.wrappedText(UI_10_FONT_ID, errorMessage.c_str(), maxWidth, 2);
    const int totalHeight = height + 10 + errorLines.size() * height;
    int currentY = (pageHeight - totalHeight) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, currentY, tr(STR_TRMNL_REG_FAILED), true, EpdFontFamily::BOLD);
    currentY += height + 10;
    for (const auto& line : errorLines) {
      renderer.drawCenteredText(UI_10_FONT_ID, currentY, line.c_str());
      currentY += height;
    }
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