/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Dependencies/glew.h"
#include "Dependencies/freeglut.h"
#include "TutorialGame.h"
#include "LevelOneGame.h"
#include "GameMode.h"
#include <cstring>
#include <iostream>
#include <memory>

namespace
{
    std::unique_ptr<GameMode> game;
    bool tutorialMode = false;
    int previousTick = 0;
    HWND gameWindow = nullptr;

    void RenderScene()
    {
        if (!game)
        {
            return;
        }

        game->Render();
        glutSwapBuffers();
    }

    void Tick(int)
    {
        if (!game)
        {
            return;
        }

        int now = glutGet(GLUT_ELAPSED_TIME);
        float dt = (now - previousTick) / 1000.f;
        previousTick = now;
        if (GetForegroundWindow() != gameWindow)
        {
            game->ClearKeys();
        }

        game->Update(dt);
        if (game->WantsQuit())
        {
            glutLeaveMainLoop();
            return;
        }

        glutPostRedisplay();
        glutTimerFunc(16, Tick, 0);
    }

    void Resize(int w, int h)
    {
        if (game)
        {
            game->Resize(w, h);
        }
    }

    void KeyDown(unsigned char key, int, int)
    {
        if (game)
        {
            game->KeyDown(key);
        }
    }

    void KeyUp(unsigned char key, int, int)
    {
        if (game)
        {
            game->KeyUp(key);
        }
    }

    void MouseInput(int button, int state, int, int)
    {
        if (!game || tutorialMode || button != GLUT_LEFT_BUTTON)
        {
            return;
        }
        if (state == GLUT_DOWN)
        {
            game->KeyDown(' ');
        }
        else
        {
            game->KeyUp(' ');
        }
    }

    // Release GPU/font resources while the window's OpenGL context is still alive.
    void Close()
    {
        game.reset();
    }
} // namespace

int main(int argc, char** argv)
{
    // Strip the game-specific option before passing arguments to FreeGLUT.
    for (int i = 1; i < argc;)
    {
        if (std::strcmp(argv[i], "--tutorial") == 0)
        {
            tutorialMode = true;
            for (int j = i; j < argc; ++j)
            {
                argv[j] = argv[j + 1];
            }
            --argc;
        }
        else
        {
            ++i;
        }
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    // Renderer uses compatibility drawing plus optional framebuffer post-processing.
    glutInitContextVersion(2, 1);
    glutInitWindowSize(1280, 800);
    glutCreateWindow(tutorialMode ? "SimpleGame - Village of Echoes / Tutorial"
                                  : "SimpleGame - Level 1 / Ashen Hunting Grounds");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    if (glewInit() != GLEW_OK || !GLEW_VERSION_2_1)
    {
        std::cerr << "OpenGL 2.1 compatibility support is required.\n";
        glutDestroyWindow(glutGetWindow());
        return 1;
    }

    gameWindow = WindowFromDC(wglGetCurrentDC());
    if (tutorialMode)
    {
        game.reset(new TutorialGame);
    }
    else
    {
        game.reset(new LevelOneGame);
    }
    glutDisplayFunc(RenderScene);
    glutReshapeFunc(Resize);
    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);
    glutMouseFunc(MouseInput);
    glutCloseFunc(Close);
    glutIgnoreKeyRepeat(1);
    game->Resize(1280, 800);
    previousTick = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, Tick, 0);
    glutMainLoop();
    return 0;
}
