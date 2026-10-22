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
            const int expected = input.y > 0   ? (input.x > 0   ? 1
                                                  : input.x < 0 ? 7
                                                                : 0)
                                 : input.y < 0 ? (input.x > 0   ? 3
                                                  : input.x < 0 ? 5
                                                                : 4)
                                               : (input.x > 0 ? 2 : 6);
            check(WorldGeometry::FacingDirection(world.Player().visualDirection) == expected,
                  "sprite_facing_matches_screen_movement");
        }
    }
    world.Player().SetPosition(originalPosition);
    auto snapshot = world.Capture(false);
    check(world.Population(Species::Human) == 100 && world.Population(Species::Deer) == 28 &&
              world.people.size() == 230 && snapshot.settlements.size() == 5,
          "five_villages_and_fixed_capacity");
    int monsterCount = 0;
    for (const auto& entity : snapshot.entities)
    {
        monsterCount += entity.kind == "monster";
    }
    check(monsterCount == 100, "one_hundred_monsters_excludes_wildlife");
    auto slice = world.Capture(false, 0, 16, false);
    check(slice.entities.size() == 16 && slice.settlements.empty(), "bounded_monitor_entity_slice");
    bool safeStart = true;
    for (auto id : world.people)
    {
        const auto* p = world.Find(id);
        if (p->beast)
        {
            safeStart = safeStart && Distance(p->Position(), world.canal) > 26 &&
                        world.arcana->Location(p->Position(), 14) < 0;
        }
    }
    check(safeStart, "monster_spawns_avoid_start_and_villages");
    for (const auto& village : snapshot.settlements)
    {
        check(village.metrics.at("population") == 20, "twenty_residents_per_village");
    }
    for (int v = 1; v < 5; ++v)
    {
        auto start = Arcana::Villages()[0].center;
        auto goal = Arcana::Villages()[v].center;
        world.Player().SetPosition(start);
        auto screen = WorldGeometry::Project({goal.x - start.x, goal.y - start.y});
        float seconds = std::hypot(screen.x, screen.y) / 170;
        int steps = static_cast<int>(std::ceil(seconds * 60));
        bool clear = true;
        for (int i = 1; i <= steps; ++i)
        {
            float t = static_cast<float>(i) / steps;
            RenderPoint target{start.x + (goal.x - start.x) * t, start.y + (goal.y - start.y) * t};
            clear = world.Move(world.Player(), target, 1) && clear;
        }
        check(clear && seconds >= 19 && seconds <= 21 &&
                  Distance(world.Player().Position(), goal) < .01f,
              "connected_twenty_second_town_road");
    }
    world.Player().SetPosition(originalPosition);
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

    world.Damage(world.Player(), *beast, Arcana::BasicAttackDamage);
    world.Damage(world.Player(), *beast, Arcana::BasicAttackDamage);
    world.SetAction(*beast, Action::Rest);
    world.Act(*beast, .55f);
    check(!beast->state.dead && beast->state.health == 60,
          "two_orb_hits_and_no_combat_regeneration");
    world.Damage(world.Player(), *beast, Arcana::BasicAttackDamage);
    check(beast->state.dead, "third_orb_hit_defeats_basic_monster");
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

    Person* listener = nullptr;
    Person* resident = nullptr;
    for (auto id : world.people)
    {
        auto* p = world.Find(id);
        if (p->settlement == 1 && !p->courier)
        {
            listener = p;
        }
        if (p->settlement == 2 && !p->courier)
        {
            resident = p;
        }
    }
    if (!listener || !resident)
    {
        check(false, "rumor_and_permanent_death_test_actors");
        return false;
    }
    auto rumor = world.facts.back().id;
    serdin->state.known.insert(rumor);
    auto serdinStart = serdin->Position();
    serdin->SetPosition(Arcana::Villages()[0].center);
    listener->SetPosition(Arcana::Villages()[1].center);
    world.arcana->Share(*serdin, *listener);
    check(!listener->state.known.count(rumor), "rumor_cannot_cross_villages_remotely");
    for (int i = 0; i < 1600 && Distance(serdin->Position(), listener->Position()) >= 1; ++i)
    {
        world.Move(*serdin, listener->Position(), 2.4f / 60);
    }
    world.arcana->Share(*serdin, *listener);
    check(listener->state.known.count(rumor) != 0, "rumor_arrives_after_physical_travel");
    serdin->SetPosition(serdinStart);
    auto previousSpawn = beast->Position();
    world.Damage(world.Player(), *resident, 1000);
    world.Player().SetPosition(originalPosition);
    world.minutes += .1;
    world.arcana->Update(1.f / 60);
    check(!beast->state.dead && beast->state.generation == 1 &&
              Distance(beast->Position(), previousSpawn) >= 12 &&
              Distance(beast->Position(), world.Player().Position()) >= 12,
          "new_monster_generation_respawns_elsewhere");
    check(resident->state.dead, "npc_death_does_not_respawn");
    auto unabsorbedDeath = beast->Position();
    world.Damage(world.Player(), *beast, 1000);
    world.minutes += .1;
    world.arcana->Update(1.f / 60);
    check(beast->state.generation == 2 && !world.arcana->State().remnants.empty() &&
              Distance(world.arcana->State().remnants.front().position, unabsorbedDeath) < .01f,
          "respawn_preserves_unabsorbed_mana_at_death_site");
    auto mood = world.Capture(false);
    check(mood.settlements.size() == 5 && mood.settlements[2].metrics.at("resident_deaths") == 1 &&
              mood.settlements[2].metrics.count("mood_calm") == 1,
          "per_village_death_and_mood_statistics");

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
    check(!restored.arcana->State().remnants.empty(), "restore_unabsorbed_remnant");
    if (!restored.arcana->State().remnants.empty())
    {
        restored.Player().SetPosition(restored.arcana->State().remnants.front().position);
        restored.Interact();
        check(restored.Player().state.manaMax > grown && restored.arcana->State().remnants.empty(),
              "restored_remnant_absorbs_exactly_once");
    }
    World legacy;
    check(!legacy.Load(file), "legacy_save_isolation");
    DeleteFileW(file);
    DeleteFileW((std::wstring(file) + L".tmp").c_str());
    std::cout << "[ArcanaCheck] failures=" << failed << '\n';
    return failed == 0;
}
