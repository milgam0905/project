#pragma once

#include <memory>

// Small, self-contained tutorial. Owns world, quest state and prototype drawing.
class TutorialGame
{
public:
    TutorialGame();
    ~TutorialGame();
    void Resize(int width, int height);
    void Update(float dt);
    void Render();
    void KeyDown(unsigned char key);
    void KeyUp(unsigned char key);
    void ClearKeys();
    bool WantsQuit() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
