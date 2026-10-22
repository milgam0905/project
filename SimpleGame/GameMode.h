#pragma once
#include "WorldMonitor.h"

class GameMode
{
  public:
    virtual ~GameMode() = default;
    virtual void Resize(int width, int height) = 0;
    virtual void Update(float dt) = 0;
    virtual void Render() = 0;
    virtual void KeyDown(unsigned char key) = 0;
    virtual void KeyUp(unsigned char key) = 0;
    virtual void ClearKeys() = 0;
    virtual bool WantsQuit() const = 0;
    virtual WorldMonitor::Snapshot CaptureMonitor() const = 0;

    virtual bool MonitorReady() const
    {
        return true;
    }

    virtual void PrepareMonitor()
    {
    }
};
