// src/activities/calendar/CalendarActivity.h
#pragma once

#include <Arduino.h>
#include <functional>
#include "activities/Activity.h"
#include "GfxRenderer.h"
#include "MappedInputManager.h"

class CalendarActivity : public Activity {
public:
    CalendarActivity(GfxRenderer& renderer, MappedInputManager& inputManager, std::function<void()> onBack);
    void onEnter() override;
    void loop() override;
    void onExit() override;

private:
    GfxRenderer& renderer;
    MappedInputManager& inputManager;
    std::function<void()> onBack;
    bool loaded = false;

    void fetchAndRenderCalendar();
    void saveSleepImage();
};
