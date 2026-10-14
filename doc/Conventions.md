# Conventions mathématiques et de rendu — CoriumEngine

Référence unique. Tout code (C++, HLSL, futurs backends) s'y conforme ; un changement ici
implique de migrer tout le code concerné dans le même commit.

## Repère
- **Main gauche**, Y vers le haut, +Z vers l'avant (direction de vue d'une caméra sans rotation).
- +X à droite.

## Vecteurs et matrices
- **Vecteurs ligne** : un point se transforme par `v * M`.
- Composition lue de gauche à droite : `world = scale * rotation * translation`,
  `wvp = world * view * proj`, `world_enfant = local_enfant * world_parent`.
- Translation dans la dernière ligne (`m[3][0..2]`).
- Stockage CPU **row-major, non transposé** (`Mat4` comme `XMFLOAT4X4`).
- Upload GPU : la matrice est **transposée** au moment de l'écriture dans le constant buffer ;
  les shaders gardent le packing HLSL par défaut (column_major) et écrivent `mul(v, M)`.
  Aucune autre transposition ailleurs.

## Rotations et angles
- Quaternion `(x, y, z, w)`, `w` scalaire en dernier, unitaire.
- Euler (inspecteur, saisie) : `x = pitch`, `y = yaw`, `z = roll`, en **degrés** ;
  appliqués dans l'ordre **roll (Z) → pitch (X) → yaw (Y)** (équivalent `XMMatrixRotationRollPitchYaw`).
- Angles en **degrés** dans les composants et l'UI (`fovY`, `spotAngle`, Euler),
  en **radians** dans l'API de la librairie de maths.

## Caméra et projection
- Vue : `LookTo` main gauche (eye, forward = +Z local, up = +Y local), à partir de la matrice
  **world** de l'entité (la hiérarchie s'applique aux caméras et lumières).
- Projection perspective main gauche, `fovY` vertical.
- Clip space : x, y ∈ [-1, 1], Y vers le haut ; **profondeur ∈ [0, 1]**, proche = 0.
- Depth buffer : clear à 1.0, test `LESS` (pas de reverse-Z).
- La projection est indépendante du backend : l'inversion Y de Vulkan se fait par
  viewport à hauteur négative (côté RHI), jamais dans la matrice.

## Rasterisation
- Face avant = **sens horaire** à l'écran (`FrontCounterClockwise = FALSE`), culling `Back` par défaut.

## Layout GPU
- `Vec3` = 3 floats (12 octets), aucun `alignas` : les structures de constant buffer suivent
  le packing HLSL (pas de champ qui chevauche une frontière de 16 octets, padding explicite).
- Chaque structure de `ShaderTypes.h` porte des `static_assert` sur `sizeof`/`offsetof`.
