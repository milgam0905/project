#pragma once

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
};
