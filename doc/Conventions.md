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
- Vulkan : viewport à hauteur négative (`y = hauteur`, `height = -hauteur`), qui donne la même image
  qu'en DX12 ; le sens à l'écran est donc le même : `VK_FRONT_FACE_CLOCKWISE`.

## Layout GPU
- `Vec3` = 3 floats (12 octets), aucun `alignas` : les structures de constant buffer suivent
  le packing HLSL (pas de champ qui chevauche une frontière de 16 octets, padding explicite).
- Chaque structure de `ShaderTypes.h` porte des `static_assert` sur `sizeof`/`offsetof`.
- SPIR-V compilé avec `-fvk-use-dx-layout` : même layout de cbuffer qu'en DXIL.

## Binding (shaders)
- Un espace de registres HLSL par fréquence de mise à jour, = un descriptor set Vulkan (`spaceN` = set N) :
  - `space0` frame : `b0` scène, `b1` lumières ;
  - `space1` matériau : `b0` constantes, `t0` texture, `s0` sampler ;
  - `space2` objet : `b0` constantes, buffer par frame + offset dynamique (DX12 : root CBV à
    l'adresse + offset ; Vulkan : `UNIFORM_BUFFER_DYNAMIC`).
- Bindings Vulkan d'un set : `bN` → `N`, `tN` → `N + 10`, `sN` → `N + 20` (options DXC
  `-fvk-t-shift 10 all -fvk-s-shift 20 all`, constantes `k*BindingShift` de `VkRhi.cpp`).
- Un seul fichier HLSL pour les deux backends, sans `#ifdef`. Pas de push/root constants ni de bindless.
- Attributs de vertex dans l'ordre de `VSInput` (location Vulkan = index de l'attribut).
