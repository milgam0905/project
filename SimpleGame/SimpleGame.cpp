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
#include "LenaGame.h"
#include "LenaWorld.h"
#include "GameMode.h"
#include "RenderStats.h"
#include "RenderQueue.h"
#include "Profiler.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <algorithm>

namespace
{
    std::unique_ptr<GameMode> game;
    bool tutorialMode = false;
    bool lenaMode = true;
    bool arcanaMode = true;
    bool profiling = true;
    bool monitoring = true;
    std::string monitorWorld;
    int previousTick = 0;
    double simulationDebt = 0;
    HWND gameWindow = nullptr;

    void RenderScene()
    {
        if (!game)
        {
            return;
        }

        RenderStats::BeginFrame();
        {
            Profiler::Scope cpu("render.frame_submit");
            Profiler::GpuScope gpu("render.frame");
            game->Render();
            RenderQueue::Flush();
        }
        {
            Profiler::Scope swap("render.swap_wait");
            glutSwapBuffers();
        }
        RenderStats::EndFrame();
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

        Profiler::Gauge("loop.update_dt_ms", dt * 1000.0);
        Profiler::Gauge("loop.requested_timer_ms", 16);
        {
            Profiler::Scope profile("game.update");
            Profiler::Count("game.update_calls");
            // Preserve normal slow-frame time; bounded fixed steps avoid collision tunnelling.
            simulationDebt += std::max(0.0, static_cast<double>(dt));
            if (simulationDebt > 2.0)
            {
                Profiler::Count("loop.long_gap_clamps");
                Profiler::Gauge("loop.last_discarded_gap_ms", (simulationDebt - 2.0) * 1000);
                simulationDebt = 2.0;
            }
            constexpr double step = 1.0 / 60.0;
            int steps = 0;
            while (simulationDebt >= step && steps < 8)
            {
                game->Update(static_cast<float>(step));
                simulationDebt -= step;
                ++steps;
            }
            Profiler::Count("loop.simulation_substeps", steps);
            Profiler::Gauge("loop.simulation_debt_ms", simulationDebt * 1000);
        }
        game->PrepareMonitor();
        if (game->MonitorReady() && WorldMonitor::IsDue())
        {
            WorldMonitor::Publish(game->CaptureMonitor());
        }
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
            Profiler::Scope profile("input.key_down");
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
        if (!game)
        {
            return;
        }
        {
            Profiler::Scope profile("lifecycle.shutdown");
            WorldMonitor::Stop();
            game.reset();
            RenderQueue::Shutdown();
        }
        Profiler::Shutdown();
    }
} // namespace

int main(int argc, char** argv)
{
    // Explicit user-run headless checks; never run during ordinary startup.
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--arcana-self-test") == 0)
        {
            try
            {
                return Lena::World::ArcanaSelfTest() ? 0 : 1;
            }
            catch (const std::exception& error)
            {
                std::cerr << "[ArcanaCheck] " << error.what() << '\n';
                return 1;
            }
        }
        if (std::strcmp(argv[i], "--lena-self-test") == 0)
        {
            return Lena::World::SelfTest() ? 0 : 1;
        }
    }

    // Strip the game-specific option before passing arguments to FreeGLUT.
    for (int i = 1; i < argc;)
    {
        if (std::strcmp(argv[i], "--tutorial") == 0 || std::strcmp(argv[i], "--village") == 0 ||
            std::strcmp(argv[i], "--level-one") == 0 || std::strcmp(argv[i], "--lena") == 0 ||
            std::strcmp(argv[i], "--arcana") == 0 || std::strcmp(argv[i], "--no-profile") == 0 ||
            std::strcmp(argv[i], "--no-monitor") == 0 ||
            std::strncmp(argv[i], "--world-id=", 11) == 0)
        {
            if (std::strcmp(argv[i], "--tutorial") == 0 || std::strcmp(argv[i], "--village") == 0)
            {
                tutorialMode = true;
                lenaMode = false;
                arcanaMode = false;
            }
            else if (std::strcmp(argv[i], "--level-one") == 0)
            {
                tutorialMode = false;
                lenaMode = false;
                arcanaMode = false;
            }
            else if (std::strcmp(argv[i], "--lena") == 0)
            {
                lenaMode = true;
                tutorialMode = false;
                arcanaMode = false;
            }
            else if (std::strcmp(argv[i], "--arcana") == 0)
            {
                lenaMode = true;
                tutorialMode = false;
                arcanaMode = true;
            }
            else if (std::strcmp(argv[i], "--no-monitor") == 0)
            {
                monitoring = false;
            }
            else if (std::strncmp(argv[i], "--world-id=", 11) == 0)
            {
                monitorWorld = argv[i] + 11;
                if (monitorWorld.empty() || monitorWorld.size() > 64 ||
                    monitorWorld.find_first_not_of(
                        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") !=
                        std::string::npos)
                {
                    std::cerr
                        << "Invalid --world-id (use 1-64 ASCII letters, digits, _, . or -).\n";
                    return 1;
                }
            }
            else
            {
                profiling = false;
            }
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
    glutCreateWindow("ARCANA");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    if (glewInit() != GLEW_OK || !GLEW_VERSION_2_1)
    {
        std::cerr << "OpenGL 2.1 compatibility support is required.\n";
        glutDestroyWindow(glutGetWindow());
        return 1;
    }

    Profiler::Initialize(profiling,
                         arcanaMode     ? "arcana"
                         : lenaMode     ? "lena"
                         : tutorialMode ? "tutorial"
                                        : "level_one");
    gameWindow = WindowFromDC(wglGetCurrentDC());
    SetWindowTextW(gameWindow, L"\uc544\ub974\uce74\ub098 : ARCANA");
    try
    {
        if (lenaMode)
        {
            game.reset(new LenaGame(arcanaMode));
        }
        else if (tutorialMode)
        {
            game.reset(new TutorialGame);
        }
        else
        {
            game.reset(new LevelOneGame);
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "[ARCANA] Initialization failed: " << error.what() << '\n';
        MessageBoxA(gameWindow, error.what(), "ARCANA - Content error", MB_OK | MB_ICONERROR);
        Profiler::Shutdown();
        glutDestroyWindow(glutGetWindow());
        return 1;
    }
    if (monitoring)
    {
        try
        {
            WorldMonitor::Start(monitorWorld.empty() ? (arcanaMode     ? "WORLD_ARCANA_BELHEIM"
                                                        : lenaMode     ? "WORLD_AERIA_LENA"
                                                        : tutorialMode ? "village"
                                                                       : "level-one")
                                                     : monitorWorld);
            std::cout
                << "[Monitor] Local receiver: http://127.0.0.1:8765 (start monitor/server.py separately)\n";
        }
        catch (const std::exception& error)
        {
            std::cerr << "[Monitor] Disabled: " << error.what() << '\n';
        }
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
    if (game && wglGetCurrentContext())
    {
        Close();
    }
    return 0;
}
