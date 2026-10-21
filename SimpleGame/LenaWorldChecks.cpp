#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "LenaWorld.h"
#include "ArcanaWorld.h"
#include "WorldGeometry.h"
#include <Windows.h>
#include <fstream>
#include <iostream>

bool Lena::World::SelfTest()
{
    int failed = 0;
    auto check = [&](bool condition, const char* name)
    {
        std::cout << "[LenaCheck] " << (condition ? "PASS " : "FAIL ") << name << '\n';
        failed += condition ? 0 : 1;
    };

    World world;
    check(world.Population(Species::Human) == 84 && world.Population(Species::Wolf) == 7 &&
              world.Population(Species::Deer) == 32 && world.people.size() == 128,
          "seed_population");
    for (auto id : world.people)
    {
        check(world.Walkable(world.Find(id)->Position()), "connected_spawn");
    }
    auto* lia = world.Find(world.liaId);
    auto* edwin = world.Find(world.edwinId);
    auto* darian = world.Find(world.darianId);
    auto* selin = world.Find(world.selinId);
    edwin->SetPosition({0, 3});
    darian->SetPosition({0, 3.5f});
    world.Damage(*darian, *edwin, 100);
    auto deathCount = world.facts.size();
    world.Damage(*darian, *edwin, 100);
    check(edwin->state.dead && world.Population(Species::Human) == 83 &&
              world.facts.size() == deathCount,
          "death_once");
    world.Observe(*lia);
    check(lia->state.known.empty(), "no_remote_death_knowledge");
    lia->SetPosition({0, 3});
    world.Observe(*lia);
    check(lia->state.report != 0 && lia->state.grief > 0, "local_witness_memory");
    world.SetAction(*lia, Action::Report);
    world.Act(*lia, .1f);
    double security = world.town.security;
    check(world.facts.back().reported, "report_requires_arrival");
    lia->state.report = world.facts.back().id;
    world.Act(*lia, .1f);
    check(world.town.security == security, "report_effect_once");

    Person* bandit = nullptr;
    Person* wolf = nullptr;
    Person* deer = nullptr;
    Person* merchant = nullptr;
    for (auto id : world.people)
    {
        auto* p = world.Find(id);
        if (p->role == Role::Bandit)
        {
            bandit = p;
        }
        if (p->role == Role::Merchant)
        {
            merchant = p;
        }
        if (p->species == Species::Wolf)
        {
            wolf = p;
        }
        if (p->species == Species::Deer)
        {
            deer = p;
        }
    }
    bandit->state.surrendered = true;
    world.Damage(*darian, *bandit, 100);
    check(!bandit->state.dead && bandit->state.health == 100, "accepted_surrender_blocks_attack");
    check(!world.CanKill(*darian, *lia), "human_kill_gate_closed");

    wolf->SetPosition({0, 3});
    deer->SetPosition({0, 3.5f});
    deer->state.health = 10;
    wolf->state.hunger = 80;
    world.SetAction(*wolf, Action::Hunt, deer->GetId());
    wolf->state.held = 3;
    check(world.CanKill(*wolf, *deer), "wolf_kill_gate");
    world.Damage(*wolf, *deer, 100, true);
    float food = deer->state.corpseFood;
    world.SetAction(*wolf, Action::Eat, deer->GetId());
    world.Act(*wolf, 1);
    check(deer->state.dead && deer->state.corpseFood < food && wolf->state.hunger < 80,
          "predation_transfers_finite_food");
    world.Act(*wolf, 100);
    check(deer->state.corpseFood == 0, "corpse_food_nonnegative");

    lia->state.health = 50;
    selin->SetPosition({0, 3.5f});
    float medicine = selin->state.medicine;
    world.SetAction(*selin, Action::Help, lia->GetId());
    world.Act(*selin, .1f);
    check(lia->state.health == 75 && selin->state.medicine == medicine - 1,
          "npc_heal_resource_cost");
    merchant->SetPosition({0, 3.5f});
    merchant->state.food = 1;
    lia->state.coins = 2;
    lia->state.food = 0;
    lia->state.cooldown = 0;
    float coins = merchant->state.coins;
    world.SetAction(*lia, Action::Trade, merchant->GetId());
    world.Act(*lia, .1f);
    check(lia->state.food == 1 && lia->state.coins == 0 && merchant->state.food == 0 &&
              merchant->state.coins == coins + 2,
          "npc_trade_conservation");

    world.Damage(world.Player(), *lia, 100);
    world.Choose(1);
    check(world.prologue == 0, "dead_lia_trust_disabled");
    world.Choose(2);
    world.Interact();
    check(world.prologue == 1 && world.waterClue, "dead_lia_story_fallback");

    wchar_t tempDirectory[MAX_PATH]{};
    wchar_t file[MAX_PATH]{};
    DWORD size = GetTempPathW(MAX_PATH, tempDirectory);
    if (size == 0 || size >= MAX_PATH || !GetTempFileNameW(tempDirectory, L"len", 0, file))
    {
        check(false, "temporary_save_path");
        return false;
    }
    check(world.Save(file), "save_snapshot");
    World restored;
    check(restored.Load(file), "load_snapshot");
    check(restored.Population(Species::Human) == world.Population(Species::Human) &&
              restored.Population(Species::Deer) == world.Population(Species::Deer) &&
              restored.facts.size() == world.facts.size() && restored.waterClue,
          "restore_deaths_facts_choices");
    double oldFood = restored.town.food;
    {
        std::ofstream invalid(file, std::ios::binary | std::ios::trunc);
        invalid << "LENA_SAVE_1\nnan broken";
    }
    check(!restored.Load(file) && restored.town.food == oldFood, "bad_save_atomic_rejection");
    DeleteFileW(file);
    DeleteFileW((std::wstring(file) + L".tmp").c_str());
    std::cout << "[LenaCheck] failures=" << failed << '\n';
    return failed == 0;
}

bool Lena::World::ArcanaSelfTest()
{
    int failed = 0;
    auto check = [&](bool condition, const char* name)
    {
        std::cout << "[ArcanaCheck] " << (condition ? "PASS " : "FAIL ") << name << '\n';
        failed += condition ? 0 : 1;
    };
    World world(true);
    const auto originalPosition = world.Player().Position();
    for (float dt : {1.f / 60, 1.f / 120})
    {
        for (RenderPoint input : {RenderPoint{-1, 0},
                                  RenderPoint{1, 0},
                                  RenderPoint{0, -1},
                                  RenderPoint{0, 1},
                                  RenderPoint{-1, -1},
                                  RenderPoint{1, -1},
                                  RenderPoint{-1, 1},
                                  RenderPoint{1, 1}})
        {
            const auto start = world.Safe(world.square);
            world.Player().SetPosition(start);
            const float length = std::hypot(input.x, input.y);
            RenderPoint screenStep{input.x / length * 170 * dt, input.y / length * 170 * dt};
            auto delta = WorldGeometry::UnprojectDelta(screenStep);
            bool moved = world.Move(world.Player(),
                                    {start.x + delta.x, start.y + delta.y},
                                    std::hypot(delta.x, delta.y));
            auto end = world.Player().Position();
            auto actual = WorldGeometry::Project({end.x - start.x, end.y - start.y});
            check(moved && std::abs(actual.x - screenStep.x) < .001f &&
                      std::abs(actual.y - screenStep.y) < .001f,
                  "screen_aligned_eight_direction_movement");
        }
    }
    world.Player().SetPosition(originalPosition);
    check(world.Population(Species::Human) == 120 && world.Population(Species::Deer) == 28 &&
              world.Population(Species::Wolf) == 12,
          "db_population_without_double_counting");
    Person* beast = nullptr;
    Person* serdin = nullptr;
    for (auto id : world.people)
    {
        auto* p = world.Find(id);
        check(world.Walkable(p->Position()), "connected_spawn");
        if (p->key == "beast_01")
        {
            beast = p;
        }
        if (p->key == "serdin_01")
        {
            serdin = p;
        }
        if (p->profile == "herbivore")
        {
            world.Decide(*p);
            check(p->state.action != Action::Attack && p->state.action != Action::Kill,
                  "herbivore_profile_gate");
        }
    }
    check(beast && serdin, "required_seed_ids");
    if (!beast || !serdin)
    {
        return false;
    }
    world.Interact();
    world.Interact();
    world.Player().SetPosition(serdin->Position());
    world.Interact();
    world.Player().SetPosition(world.Safe(world.arcana->forge));
    world.Interact();
    world.Interact();
    float maximum = world.Player().state.manaMax;
    float current = world.Player().state.manaCurrent;
    world.arcana->Cast();
    check(world.arcana->State().step == 6 && world.arcana->State().combustion &&
              world.Player().state.manaMax == maximum && world.Player().state.manaCurrent < current,
          "knowledge_changes_efficiency_not_capacity");
    world.Player().SetPosition(world.Safe(world.arcana->outpost));
    world.Interact();
    check(world.arcana->State().step == 7, "opening_quest_sequence");

    world.Damage(world.Player(), *beast, 1000);
    world.Player().SetPosition(beast->Position());
    world.RebuildIndex();
    current = world.Player().state.manaCurrent;
    world.Interact();
    float grown = world.Player().state.manaMax;
    check(beast->state.dead && beast->state.absorbed && grown > maximum &&
              world.Player().state.manaCurrent == current,
          "credited_beast_capacity_only");
    world.Interact();
    check(world.Player().state.manaMax == grown, "absorption_not_repeatable");

    wchar_t directory[MAX_PATH]{};
    wchar_t file[MAX_PATH]{};
    DWORD length = GetTempPathW(MAX_PATH, directory);
    if (!length || length >= MAX_PATH || !GetTempFileNameW(directory, L"arc", 0, file))
    {
        check(false, "temporary_save_path");
        return false;
    }
    check(world.Save(file), "save_arcana_snapshot");
    World restored(true);
    check(restored.Load(file), "restore_arcana_snapshot");
    check(restored.arcana->State().step == 7 && restored.arcana->State().boarKills == 1 &&
              restored.Player().state.manaMax == grown,
          "restore_quests_growth_and_deaths");
    restored.Interact();
    check(restored.Player().state.manaMax == grown, "reload_does_not_duplicate_absorption");
    World legacy;
    check(!legacy.Load(file), "legacy_save_isolation");
    DeleteFileW(file);
    DeleteFileW((std::wstring(file) + L".tmp").c_str());
    std::cout << "[ArcanaCheck] failures=" << failed << '\n';
    return failed == 0;
}
