#pragma once

#include <vector>
#include <random>
#include <algorithm>
#include <functional>

struct HareGenome {
    float reproduction_threshold = 1.5f;
    float movement_aggression = 0.5f; // 0 = random movement, 1 = always seek plants
    float weight = 1.0f; // 0.5 = light/fast, 1.5 = heavy/slow
    float fear = 0.5f; // 0 = fearless (takes risks), 1 = fearful (avoids danger)
    float movement_efficiency = 1.0f; // 0.5 = inefficient/high cost, 1.5 = efficient/low cost
    bool can_burrow = false; // Can burrow to hide

    HareGenome() = default;
    HareGenome(float thresh, float aggression, float w, float f) : reproduction_threshold(thresh), movement_aggression(aggression), weight(w), fear(f) {}

    HareGenome mutate(std::mt19937& gen) const {
        std::normal_distribution<float> dist(0.0f, 0.1f); // Small mutations
        HareGenome child = *this;
        child.reproduction_threshold += dist(gen);
        child.reproduction_threshold = std::clamp(child.reproduction_threshold, 1.0f, 2.0f);
        child.movement_aggression += dist(gen);
        child.movement_aggression = std::clamp(child.movement_aggression, 0.0f, 1.0f);
        child.weight += dist(gen);
        child.weight = std::clamp(child.weight, 0.5f, 1.5f);
        child.fear += dist(gen);
        child.fear = std::clamp(child.fear, 0.0f, 1.0f);
        child.movement_efficiency += dist(gen);
        child.movement_efficiency = std::clamp(child.movement_efficiency, 0.5f, 1.5f);
        // Burrow trait: rare mutation
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(gen) < 0.01f) {
            child.can_burrow = !child.can_burrow;
        }
        return child;
    }

    bool operator<(const HareGenome& other) const {
        if (reproduction_threshold != other.reproduction_threshold) {
            return reproduction_threshold < other.reproduction_threshold;
        }
        if (movement_aggression != other.movement_aggression) {
            return movement_aggression < other.movement_aggression;
        }
        if (weight != other.weight) {
            return weight < other.weight;
        }
        if (fear != other.fear) {
            return fear < other.fear;
        }
        if (movement_efficiency != other.movement_efficiency) {
            return movement_efficiency < other.movement_efficiency;
        }
        return can_burrow < other.can_burrow;
    }
};

struct FoxGenome {
    float reproduction_threshold = 4.0f;
    float hunting_aggression = 0.5f; // 0 = passive, 1 = aggressive
    float weight = 1.0f; // 0.5 = light/fast, 1.5 = heavy/slow
    float movement_efficiency = 1.0f; // 0.5 = inefficient/high cost, 1.5 = efficient/low cost

    FoxGenome() = default;
    FoxGenome(float thresh, float aggression, float w, float eff) : reproduction_threshold(thresh), hunting_aggression(aggression), weight(w), movement_efficiency(eff) {}

    FoxGenome mutate(std::mt19937& gen) const {
        std::normal_distribution<float> dist(0.0f, 0.1f); // Small mutations
        FoxGenome child = *this;
        child.reproduction_threshold += dist(gen);
        child.reproduction_threshold = std::min(std::max(child.reproduction_threshold, 2.0f), 6.0f);
        child.hunting_aggression += dist(gen);
        child.hunting_aggression = std::min(std::max(child.hunting_aggression, 0.0f), 1.0f);
        child.weight += dist(gen);
        child.weight = std::min(std::max(child.weight, 0.5f), 1.5f);
        child.movement_efficiency += dist(gen);
        child.movement_efficiency = std::min(std::max(child.movement_efficiency, 0.5f), 1.5f);
        return child;
    }

    bool operator<(const FoxGenome& other) const {
        if (reproduction_threshold != other.reproduction_threshold) {
            return reproduction_threshold < other.reproduction_threshold;
        }
        if (hunting_aggression != other.hunting_aggression) {
            return hunting_aggression < other.hunting_aggression;
        }
        if (weight != other.weight) {
            return weight < other.weight;
        }
        return movement_efficiency < other.movement_efficiency;
    }
};

struct WolfGenome {
    float reproduction_threshold = 6.0f;
    float hunting_aggression = 0.5f; // 0 = passive, 1 = aggressive
    float weight = 1.0f; // 0.5 = light/fast, 1.5 = heavy/slow
    float movement_efficiency = 1.0f; // 0.5 = inefficient/high cost, 1.5 = efficient/low cost

    WolfGenome() = default;
    WolfGenome(float thresh, float aggression, float w, float eff) : reproduction_threshold(thresh), hunting_aggression(aggression), weight(w), movement_efficiency(eff) {}

    WolfGenome mutate(std::mt19937& gen) const {
        std::normal_distribution<float> dist(0.0f, 0.1f); // Small mutations
        WolfGenome child = *this;
        child.reproduction_threshold += dist(gen);
        child.reproduction_threshold = std::clamp(child.reproduction_threshold, 5.0f, 7.0f);
        child.hunting_aggression += dist(gen);
        child.hunting_aggression = std::clamp(child.hunting_aggression, 0.0f, 1.0f);
        child.weight += dist(gen);
        child.weight = std::clamp(child.weight, 0.5f, 1.5f);
        child.movement_efficiency += dist(gen);
        child.movement_efficiency = std::clamp(child.movement_efficiency, 0.5f, 1.5f);
        return child;
    }

    bool operator<(const WolfGenome& other) const {
        if (reproduction_threshold != other.reproduction_threshold) {
            return reproduction_threshold < other.reproduction_threshold;
        }
        if (hunting_aggression != other.hunting_aggression) {
            return hunting_aggression < other.hunting_aggression;
        }
        if (weight != other.weight) {
            return weight < other.weight;
        }
        return movement_efficiency < other.movement_efficiency;
    }
};
