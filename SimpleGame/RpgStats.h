#pragma once

#include <algorithm>

struct RpgStats
{
    int level = 1;
    int experience = 0;
    int totalExperience = 0;
    int strength = 3;
    int vitality = 3;
    int agility = 3;
    int abilityPoints = 0;
    int spentPoints = 0;
    int health = 100;
    int potions = 3;
    int gold = 0;

    int NextLevelExperience() const
    {
        return level * 50;
    }

    int MaxHealth() const
    {
        return 60 + vitality * 10 + level * 10;
    }

    int Attack() const
    {
        return 6 + strength * 2 + level * 2;
    }

    int Defense() const
    {
        return level + vitality / 2;
    }

    float AttackCooldown() const
    {
        return std::max(.20f, .62f - agility * .025f);
    }

    int GainExperience(int amount)
    {
        experience += amount;
        totalExperience += amount;
        int gainedLevels = 0;

        while (level < 50 && experience >= NextLevelExperience())
        {
            experience -= NextLevelExperience();
            ++level;
            abilityPoints += 3;
            ++gainedLevels;
            health = MaxHealth();
        }

        if (level == 50)
        {
            experience = std::min(experience, NextLevelExperience());
        }

        return gainedLevels;
    }

    bool Allocate(int attribute)
    {
        if (abilityPoints <= 0 || attribute < 0 || attribute > 2)
        {
            return false;
        }

        const int previousMax = MaxHealth();
        if (attribute == 0)
        {
            ++strength;
        }
        if (attribute == 1)
        {
            ++vitality;
        }
        if (attribute == 2)
        {
            ++agility;
        }
        --abilityPoints;
        ++spentPoints;
        health += MaxHealth() - previousMax;
        return true;
    }
};
