#pragma once

#include <activities/Activity.h>
#include <GfxRenderer.h>
#include <MappedInputManager.h>
#include <string>

class TrmnlRegisterActivity : public Activity {
 public:
  TrmnlRegisterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TrmnlRegister", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;

 protected:
  void render(RenderLock&&) override;

 private:
  enum State {
    REGISTERING,
    SUCCESS,
    FAILED,
  };

  void onWifiSelectionComplete(bool success);
  void performRegistration();

  State state = REGISTERING;
  std::string statusMessage;
  std::string errorMessage;
  std::string friendlyId;
};