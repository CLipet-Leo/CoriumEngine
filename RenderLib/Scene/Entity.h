#pragma once

// Un EntityID est juste un entier — simple et rapide
using EntityID = uint32_t;
constexpr EntityID NULL_ENTITY = UINT32_MAX;

// Identité persistante : sérialisée et stable d'un chargement à l'autre (l'EntityID ne l'est pas)
using EntityUUID = uint64_t;
constexpr EntityUUID NULL_UUID = 0;

struct Entity
{
    EntityID    id = NULL_ENTITY;
    EntityUUID  uuid = NULL_UUID;
    std::string name = "Entity";
    bool        active = true;

    bool IsValid() const { return id != NULL_ENTITY; }
};
