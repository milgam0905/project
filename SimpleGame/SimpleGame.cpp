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
#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
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
    constexpr double UpdateInterval = .016;
    double simulationDebt = 0;
    bool drawable = false, redrawRequested = true;
    bool spaceDown = false, mouseDown = false;

    void RenderScene(GLFWwindow* window)
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
            glfwSwapBuffers(window);
        }
        RenderStats::EndFrame();
    }

    void Tick(double dt)
    {
        Profiler::Gauge("loop.update_dt_ms", dt * 1000.0);
        Profiler::Gauge("loop.requested_timer_ms", UpdateInterval * 1000.0);
        {
            Profiler::Scope profile("game.update");
            Profiler::Count("game.update_calls");
            // Preserve normal slow-frame time; bounded fixed steps avoid collision tunnelling.
            simulationDebt += std::max(0.0, dt);
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
    }

    void Resize(GLFWwindow*, int width, int height)
    {
        drawable = width > 0 && height > 0;
        if (game && drawable)
        {
            game->Resize(width, height);
            redrawRequested = true;
        }
    }

    void Refresh(GLFWwindow*)
    {
        redrawRequested = true;
    }

    void Focus(GLFWwindow*, int focused)
    {
        if (!focused && game)
        {
            spaceDown = mouseDown = false;
            game->ClearKeys();
        }
    }

    void SetAttack(bool& source, bool down)
    {
        const bool wasDown = spaceDown || mouseDown;
        source = down;
        const bool isDown = spaceDown || mouseDown;
        if (isDown && !wasDown)
        {
            Profiler::Scope profile("input.key_down");
            game->KeyDown(' ');
        }
        else if (!isDown && wasDown)
        {
            game->KeyUp(' ');
        }
    }

    void KeyInput(GLFWwindow*, int key, int, int action, int modifiers)
    {
        if (!game || action == GLFW_REPEAT ||
            (action == GLFW_PRESS &&
             (modifiers & (GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER))))
        {
            return;
        }
        if (key == GLFW_KEY_SPACE)
        {
            SetAttack(spaceDown, action == GLFW_PRESS);
            return;
        }
        int code = 0;
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        {
            code = 'a' + key - GLFW_KEY_A;
        }
        else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        {
            code = '0' + key - GLFW_KEY_0;
        }
        else if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
        {
            code = '0' + key - GLFW_KEY_KP_0;
        }
        else if (key == GLFW_KEY_ESCAPE)
        {
            code = 27;
        }
        if (!code)
        {
            return;
        }
        if (action == GLFW_PRESS)
        {
            Profiler::Scope profile("input.key_down");
            game->KeyDown(static_cast<unsigned char>(code));
        }
        else
        {
            game->KeyUp(static_cast<unsigned char>(code));
        }
    }

    void MouseInput(GLFWwindow*, int button, int action, int)
    {
        if (game && !tutorialMode && button == GLFW_MOUSE_BUTTON_LEFT)
        {
            SetAttack(mouseDown, action == GLFW_PRESS);
        }
    }

    void RunLoop(GLFWwindow* window)
    {
        double previousTick = glfwGetTime();
        double nextTick = previousTick + UpdateInterval;
        while (!glfwWindowShouldClose(window))
        {
            const double remaining = nextTick - glfwGetTime();
            if (remaining > 0 && !redrawRequested)
            {
                Profiler::Scope wait("loop.event_wait");
                glfwWaitEventsTimeout(remaining);
            }
            else
            {
                glfwPollEvents();
            }
            if (glfwWindowShouldClose(window) || game->WantsQuit())
            {
                break;
            }
            const double now = glfwGetTime();
            if (now >= nextTick)
            {
                Tick(now - previousTick);
                previousTick = now;
                // The old timer scheduled its next wake after the update completed.
                nextTick = glfwGetTime() + UpdateInterval;
                redrawRequested = true;
            }
            if (game->WantsQuit())
            {
                break;
            }
            if (drawable && redrawRequested)
            {
                RenderScene(window);
            }
            redrawRequested = false;
        }
    }

    // Release GPU/font resources while the window's OpenGL context is still alive.
    void Close()
    {
        {
            Profiler::Scope profile("lifecycle.shutdown");
            WorldMonitor::Stop();
            game.reset();
            RenderQueue::Shutdown();
        }
        Profiler::Shutdown();
    }

    void WindowError(int code, const char* description)
    {
        std::cerr << "[GLFW] " << code << ": " << description << '\n';
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

    // GLFW does not parse argv; retain the game's existing launch options.
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
        }
        ++i;
    }

    glfwSetErrorCallback(WindowError);
    if (!glfwInit())
    {
        return 1;
    }
    // Renderer uses compatibility drawing plus optional framebuffer post-processing.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    GLFWwindow* window =
        glfwCreateWindow(1280, 800, u8"\uC544\uB974\uCE74\uB098 : ARCANA", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    // Preserve the driver/default swap interval rather than forcing a new VSync policy.
    if (glewInit() != GLEW_OK || !GLEW_VERSION_2_1)
    {
        std::cerr << "OpenGL 2.1 compatibility support is required.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    Profiler::Initialize(profiling,
                         arcanaMode     ? "arcana"
                         : lenaMode     ? "lena"
                         : tutorialMode ? "tutorial"
                                        : "level_one");
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
        MessageBoxA(glfwGetWin32Window(window),
                    error.what(),
                    "ARCANA - Content error",
                    MB_OK | MB_ICONERROR);
        Close();
        glfwDestroyWindow(window);
        glfwTerminate();
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
    glfwSetFramebufferSizeCallback(window, Resize);
    glfwSetWindowRefreshCallback(window, Refresh);
    glfwSetWindowFocusCallback(window, Focus);
    glfwSetKeyCallback(window, KeyInput);
    glfwSetMouseButtonCallback(window, MouseInput);
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    Resize(window, width, height);
    RunLoop(window);
    Close();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
