#pragma once

// Un EntityID est juste un entier — simple et rapide
using EntityID = uint32_t;
constexpr EntityID NULL_ENTITY = UINT32_MAX;

struct Entity
{
    EntityID    id = NULL_ENTITY;
    std::string name = "Entity";
    bool        active = true;

    bool IsValid() const { return id != NULL_ENTITY; }
};
