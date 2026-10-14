#pragma once

#include <memory>
#include "GameMode.h"

// Small, self-contained tutorial. Owns world, quest state and prototype drawing.
class TutorialGame : public GameMode
{
  public:
    TutorialGame();
    ~TutorialGame() override;
    void Resize(int width, int height) override;
    void Update(float dt) override;
    void Render() override;
    void KeyDown(unsigned char key) override;
    void KeyUp(unsigned char key) override;
    void ClearKeys() override;
    bool WantsQuit() const override;
    WorldMonitor::Snapshot CaptureMonitor() const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
