# StationViz — SLD module (core)
Ce module construit un graphe *Single Line Diagram* (nœuds + arêtes) à partir du modèle SCL exposé par `SclManager`.

## Contenu
- `SldTypes.hpp` : types de base (Node, Edge, Scene, enums).
- `SldGraph.hpp` : conteneur graphe.
- `SldBuilder.hpp/.cpp` : construction SLD depuis `SclManager` (layout simple bus→bays).
- `SldLayout.hpp` : point d'extension pour les stratégies de layout/routage.
- `SldModel.hpp/.cpp` : modèles Qt (`QAbstractListModel`) pour exposer nœuds et arêtes à QML.
- `icons/` : répertoire pour les SVG de symboles (non utilisé par le core, mais recommandé).

## Dépendances
- Qt 6 (Core), pas de QML côté core.

## Intégration (CMake)
Ajoutez dans votre `core/CMakeLists.txt` (ou équivalent) :
```cmake
add_subdirectory(sld)
target_link_libraries(core PRIVATE sld)
```
et dans `core/sld/CMakeLists.txt`, voir le fichier fourni.
