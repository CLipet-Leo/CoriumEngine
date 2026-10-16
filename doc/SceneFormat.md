# Format de scène — CoriumEngine

Fichiers `.scene` dans `Assets/` (versionné, copié à côté de l'exe au build). Lus et écrits par
`SceneSerializer` (`CoriumEngine/Scene`). Exemple de référence : `Assets/Scenes/Demo.scene`.

## Syntaxe : sous-ensemble strict de YAML
Tout fichier valide est du YAML valide ; l'inverse est faux.

- UTF-8, fins de ligne LF (CRLF et BOM tolérés en lecture).
- Indentation par **espaces** (tabulation = erreur), 2 par niveau en écriture.
- Maps en bloc `clé: valeur` ; une clé sans valeur ouvre un bloc plus indenté.
- Listes en bloc `- clé: valeur` : chaque élément est une map dont la première entrée suit le tiret.
- Listes en ligne uniquement pour les nombres : `[x, y, z]` ; liste vide : `[]`.
- Chaînes entre guillemets doubles, échappements `\"` et `\\` seulement.
- Commentaires `#` en début de ligne ou après un espace, hors chaîne. Non conservés à la sauvegarde.
- Non supportés : ancres/alias, maps en ligne `{}`, blocs `|` `>`, tags, guillemets simples.

## Valeurs
- Flottants : `std::to_chars` (plus courte représentation exacte), lus par `std::from_chars` —
  aller-retour exact, indépendant de la locale.
- Booléens : `true` / `false`.
- UUID : `0x` + 16 chiffres hexadécimaux, non nul.
- Enums : noms en minuscules (`box`, `directional`, `point`, `spot`, `none`, `front`, `back`,
  `solid`, `wireframe`, `opaque`, `alphaBlend`).

## Structure
```yaml
version: 1              # obligatoire
meshes:                 # primitives procédurales nommées
  - name: "cube"
    primitive: box
    size: 1
materials:              # MaterialDesc nommés
  - name: "Wireframe"
    baseColor: [0.3, 1, 0.4, 1]
    cullMode: none
    fillMode: wireframe
    blendMode: opaque
entities:
  - uuid: 0x186a2c2cad749b6b
    name: "Wire Cube"
    active: true
    parent: 0x...       # uuid d'une entité écrite plus haut ; absent = racine
    transform:          # rotation = quaternion (x, y, z, w)
      position: [2, 0, 0]
      rotation: [0, 0, 0, 1]
      scale: [1, 1, 1]
    mesh:               # références par nom, jamais par index runtime
      mesh: "cube"
      material: "Wireframe"   # absent = matériau « Default » du renderer
      visible: true
      castShadows: true
    camera:
      fovY: 60
      nearPlane: 0.1
      farPlane: 1000
      isMain: true
    light:
      type: directional
      color: [1, 1, 1]
      intensity: 1.2
      range: 10
      spotAngle: 30
      enabled: true
```
Champ absent = valeur par défaut du composant. Les sections `meshes` et `materials` sont lues
avant `entities`, quel que soit leur ordre dans le fichier.

## Écriture
Déterministe : même scène = même fichier, octet pour octet.
- Entités dans l'ordre de la hiérarchie (racines, puis enfants en profondeur, ordre de `children`).
- Seuls les meshes et matériaux référencés sont écrits, dans l'ordre de première référence.
- Tous les champs sont écrits, sauf `mesh`/`material` non définis.
- Non sérialisés : `EntityID`, `localMatrix`, `worldMatrix`, `dirty` (recalculés).
- Sauvegarde via un fichier `.tmp` puis remplacement : une écriture interrompue ne détruit rien.

## Lecture
- Chargement dans une scène neuve ; la scène cible n'est remplacée qu'en cas de succès.
- Erreur (syntaxe, type, référence inconnue, UUID dupliqué, parent écrit après l'enfant) : échec
  avec numéro de ligne.
- Champ, composant ou section inconnu : ignoré, avertissement en sortie debug.
- Entité sans `uuid` : un nouvel UUID est généré (pratique pour un fichier écrit à la main).
- Quaternion renormalisé seulement s'il n'est pas unitaire (une renormalisation systématique
  pourrait changer le dernier bit et casser l'égalité des sauvegardes).

## Vérification
En Debug, `SceneSerializer::SelfCheck` (appelé par `Main.cpp`) charge, sauve, recharge et resauve
`Demo.scene` et un échantillon interne (hiérarchie, échappements, champs inconnus), et exige deux
sauvegardes identiques.
