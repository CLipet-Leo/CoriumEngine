#pragma once

// Lecture/écriture de scène au format texte (sous-ensemble strict de YAML, cf. doc/SceneFormat.md).
// Meshes et matériaux sont référencés par nom ; le chargement les enregistre dans le renderer.
namespace SceneSerializer
{
    // Charge dans une Scene neuve qui remplace scene ; scene est inchangée en cas d'échec.
    // error reçoit le motif (avec numéro de ligne pour une erreur de syntaxe).
    // Champs et composants inconnus : ignorés, avertissement en sortie debug.
    CORIUM_API bool Load(const std::wstring& path, Scene& scene, IRenderer& renderer, std::string* error = nullptr);

    // Écriture déterministe : même scène = même fichier, octet pour octet
    CORIUM_API bool Save(const std::wstring& path, Scene& scene, const IRenderer& renderer, std::string* error = nullptr);

    // Debug : charger → sauver → recharger → resauver doit donner deux textes identiques
    CORIUM_API void SelfCheck(const std::wstring& path, IRenderer& renderer);
}
