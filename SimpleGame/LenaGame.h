#pragma once

#include "GameMode.h"
#include <memory>

class LenaGame : public GameMode
{
  public:
    explicit LenaGame(bool arcanaMode = false);
    ~LenaGame();
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
