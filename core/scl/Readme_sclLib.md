# StationViz — Documentation du module **SCL** (`sclLib`)

> **But** : Fournir un module C++17 robuste, extensible et **indépendant de Qt** pour parser, modéliser et interroger les fichiers **SCL** (IEC 61850‑6), avec un accent sur les besoins **FAT**, **SLD** (schéma unifilaire) et **Network** (MMS/GOOSE/SV).

---

## 1) Vue d’ensemble

Le module **sclLib** charge un fichier `.scl/.scd/.icd/...` puis construit un **modèle en mémoire** structuré autour de trois axes :

1. **Topologie primaire** *(Substation/VoltageLevel/Bay/ConductingEquipment)* destinée au **SLD** :
   - Voltage nominal via `<Voltage unit= multiplier=>value`.
   - **ConnectivityNode** et **Terminal** (liaisons CE ↔ CN) pour générer les arêtes du graphe unifilaire.
   - **LNode** placés au niveau Substation/VL/Bay/CE pour lier la primaire aux **IED/LD/LN**.

2. **Appareils numériques** *(IED → AccessPoint → Server → LDevice → LN0/LN)* pour le **FAT** :
   - Informations d’adresse (`<Address>/<P type=...>`) utiles à la configuration et aux tests.
   - Indexation rapide **IED → LD → LN** et résolution des références **LNodeRef**.

3. **Communication** *(SubNetwork / ConnectedAP / Address / GSE / SMV)* pour **MMS/GOOSE/SV** :
   - Récupération des paramètres réseau (MAC, VLAN, APPID, IP, etc.).
   - Association **ConnectedAP (iedName/apName)** avec les IED/AP du modèle logique.

Le module expose des **API de debug**, des **requêtes rapides** et des **exports JSON** minimalistes pour brancher rapidement une GUI (Qt/QML) sans dépendre de Qt dans le cœur.

---

## 2) Organisation des fichiers

Monodossier `scl/` :

```
scl/
  CMakeLists.txt
  Result.h             # Result<T> / Status / ErrorCode
  SclTypes.h           # Types du modèle en mémoire (Substation, IED, Network...)
  SclParser.h/.cpp     # Parsing SCL via pugixml
  SclManager.h/.cpp    # Interface publique, indexes, utilitaires
  demo_main.cpp        # Démo CLI (option SCL_BUILD_DEMO)
```

Bibliothèque générée : **`sclLib`**

---

## 3) Modèle de données (extrait)

```text
SclModel
 ├─ substations: [Substation]
 │   └─ Substation
 │      ├─ name
 │      ├─ lnodes: [LNodeRef]
 │      └─ vlevels: [VoltageLevel]
 │         └─ VoltageLevel
 │            ├─ name, nomFreq
 │            ├─ voltage: { value, unit, multiplier }
 │            ├─ lnodes: [LNodeRef]
 │            └─ bays: [Bay]
 │               └─ Bay
 │                  ├─ name
 │                  ├─ connectivityNodes: [ConnectivityNode{ name, pathName }]
 │                  ├─ equipments: [ConductingEquipment]
 │                  │   └─ ConductingEquipment{ name, type, terminals:[Terminal{name, connectivityNode|cNodeName}], lnodes:[LNodeRef] }
 │                  └─ lnodes: [LNodeRef]
 │
 ├─ ieds: [IED]
 │   └─ IED{ name, manufacturer, type,
 │           accessPoints:[AccessPoint{name, address:{P...}, ldevices:[LDevice]}],
 │           ldevices:[LDevice] /* fallback si présent directement */ }
 │       └─ LDevice{ inst, lns:[LogicalNode{prefix, lnClass, inst(LN0="")} ] }
 │
 └─ communication: Communication
     └─ subNetworks:[SubNetwork{ name, type, props:{...},
          connectedAPs:[ConnectedAP{ iedName, apName, address:{P...},
                                     gses:[GSE{ldInst, cbName, address:{P...}}],
                                     smvs:[SMV{ldInst, cbName, address:{P...}}] }]
        }]
```

Aides SLD :
- **EdgeCEtoCN**: arêtes CE→CN pour dessiner rapidement le graphe unifilaire.
- **ResolvedLNode**: résultat de la résolution d’un **LNodeRef** vers (IED, LD, LN).

---

## 4) API publique

### 4.1 `SclManager`

- `Status loadScl(const std::string& filepath)`
  - Parse le fichier, construit le modèle et les **indexes**.

- `const SclModel* model() const`
  - Accès read-only au modèle (pointeur nul si non chargé).

- **Debug helpers**
  - `printSubstations()` : Substations, VoltageLevels (avec Voltage), Bays, CN, CE, Terminals, LNode.
  - `printIEDs()` : IED/AP/Address + LDevice/LN0/LN.
  - `printCommunication()` : SubNetworks, ConnectedAP, GSE, SMV.
  - `printTopology()` : arêtes **CE → CN**.

- **Requêtes & résolutions**
  - `findSubstation(name)` → `Result<const Substation*>`
  - `findIED(name)` → `Result<const IED*>`
  - `resolveLNodeRef(const LNodeRef&)` → `Result<ResolvedLNode>`

- **Aides SLD / Network**
  - `collectSldEdges()` → `std::vector<EdgeCEtoCN>`
  - `getConnectivityNodes(ss, vl, bay)` → CN d’un bay.
  - `toJsonSubstations()` / `toJsonNetwork()` → export JSON léger.

### 4.2 `SclParser`

- `parseFile(path)` / `parseString(xml)` → `Result<SclModel>`
  - Utilisé en interne par `SclManager`, peut aussi être appelé directement pour des tests.

### 4.3 Gestion d’erreurs

- `Result<T>` et `Status` encapsulent **ErrorCode** + **message** détaillé.
- Codes : `FileNotFound`, `XmlParseError`, `MissingMandatoryField`, `InvalidPath`, etc.

---

## 5) Couverture de parsing (IEC 61850‑6)

- **Topologie primaire** : `Substation / VoltageLevel / Bay / ConductingEquipment / Terminal / ConnectivityNode / LNode`.
- **Tension nominale** : `<Voltage unit="V" multiplier="k">225</Voltage>` → `ScalarWithUnit` (`value=225, unit="V", multiplier="k"`).
- **IED** : `AccessPoint/Server/LDevice/LN0/LN` (et **fallback** pour LDevice sous IED si rencontré).
- **Communication** : `SubNetwork(type) / ConnectedAP(iedName, apName) / Address(P: IP, MAC‑Address, APPID, VLAN‑ID, VLAN‑PRIORITY, etc.) / GSE / SMV`.
- **LNodeRef** : lecture et **résolution** vers IED/LD/LN (via `iedName`, `ldInst`, `prefix`, `lnClass`, `lnInst`).

> **Note** : Les `DataTypeTemplates` ne sont pas encore parsés (volontaire pour limiter la portée). Le design laisse la place pour ajouter ce bloc ultérieurement si nécessaire pour naviguer jusqu’aux DO/DA/FCDA.

---

## 6) Flux SLD : de la Substation au graphe

1. **Terminals** des `ConductingEquipment` pointent vers des **ConnectivityNodes** (via `@connectivityNode` ou `@cNodeName`).
2. `collectSldEdges()` fabrique des **arêtes CE→CN** (avec contexte SS/VL/Bay) utilisables directement par un moteur de layout/dessin.
3. Les **LNode** présents sous CE/Bay/VL/SS permettent d’associer, pour chaque élément graphique, ses **fonctions logiques** (via `resolveLNodeRef`).

Exemple d’usage côté GUI :
- dessiner chaque `ConductingEquipment` (type `CB`, `DS`, `PT`, ...),
- relier ses `Terminal` aux `ConnectivityNode` correspondants (noeuds du graphe),
- colorer un CE selon l’état remonté par un LN (via ton backend comm MMS/GOOSE/SV).

---

## 7) Réseau (MMS/GOOSE/SV)

- **SubNetwork** : type (`"8-MMS"`, `"8-1"`, `"9-2-LE"`…), propriétés réseau (`props` si `<P>` sous `SubNetwork`).
- **ConnectedAP** : `iedName`, `apName`, `Address { P: IP, MAC, APPID, VLAN-ID, VLAN-PRIORITY, ... }`.
- **GSE/SMV** : `ldInst`, `cbName`, `Address{...}` → prêts pour résoudre les **ControlBlocks** côté IED.

Tu pourras ainsi :
- faire correspondre **un IED/AP** à son **ConnectedAP** (via `iedName`/`apName`),
- extraire IP/MAC/VLAN pour configurer des capture‑filters (*Wireshark*) ou alimenter ton moteur de souscription GOOSE/SV.

---

## 8) Construction & intégration

### 8.1 CMake (lib + démo)

```cmake
add_subdirectory(scl)
# Optionnel : binaire de démonstration
# cmake -DSCL_BUILD_DEMO=ON .. && make scl_demo
```

Dans `scl/CMakeLists.txt` : la lib **`sclLib`** est auto‑contenue et fetch **pugixml** si non présent.

### 8.2 Exemple CLI

```bash
./scl_demo poste.scd
# Affiche Substations, IEDs, Communication, Topology, JSON
```

### 8.3 Intégration Qt/QML (suggestion)

- Créer un `QObject SclFacade` qui wrappe `SclManager` et expose :
  - `Q_INVOKABLE QString load(const QString& path)` (retourne erreur vide si OK),
  - `Q_INVOKABLE QString topologyJson()` / `networkJson()` pour QML,
  - ou des `QAbstractItemModel` dédiés (plus riche mais plus long à coder).

---

## 9) Performance & robustesse

- **pugixml** est rapide et peu allocant ; parsing en **C++17** sans exceptions coûteuses côté API (erreurs via `Result/Status`).
- Indexation **O(1)** sur IED par nom et CN par chemin pour des requêtes instantanées.
- Le module est **thread‑compatible en lecture** après chargement (accès `const`).

---

## 10) Tests & validation

Idées de tests unitaires (Catch2 / GTest) :
- SCL minimal vs SCL riche (plusieurs SS/VL/Bay).
- Voltage avec/ sans `unit/multiplier`.
- IED avec AP/Server/LDevice et fallback LDevice direct.
- Résolution `LNodeRef` (cas valides et erreurs : IED inconnu, LD inconnu, LN inconnu).
- Communication : extraction d’IP/MAC/VLAN/APPID ; présence GSE/SMV.
- Topologie : cohérence `collectSldEdges()` ↔ `ConnectivityNode`.

---

## 11) Limites actuelles & feuille de route

**Limites** :
- Pas de parsing de `DataTypeTemplates` (DO/DA/DOType/DAType/DAI/SDI/FCDA) — volontaire pour garder le cœur léger.
- JSON utilitaire minimal (pas d’échappement complet Unicode, ni schéma strict).

**Roadmap suggérée** :
1. **DataTypeTemplates** (pour naviguer jusqu’aux FCDA, lier GSE/SMV ↔ Dataset ↔ DO/DA).
2. **XPath helpers** (activer `PUGIXML_HAS_XPATH`) : requêtes express (ex. *trouve le GSE d’un IED par cbName*).
3. **Exports JSON** complets (schémas, versions, hash de modèle).
4. **Validation SCL** (contrôles croisés : CN référencés, LNodeRef résolus, AP↔ConnectedAP présents, etc.).
5. **Mapping SLD enrichi** (positionnement/attributs graphiques si présents dans des profils outillés).

---

## 12) Exemples d’utilisation (C++)

```cpp
#include "SclManager.h"

scl::SclManager mgr;
if (!mgr.loadScl("poste.scd")) { /* gérer l’erreur */ }

// Debug
mgr.printSubstations();
mgr.printIEDs();

// Topologie
auto edges = mgr.collectSldEdges();
for (auto& e : edges) {
    // dessiner une arête e.ceName -> e.cnPath
}

// Résolution LNodeRef (ex: le 1er LNode du 1er CE)
const auto* model = mgr.model();
const auto& ce = model->substations[0].vlevels[0].bays[0].equipments[0];
if (!ce.lnodes.empty()) {
    auto r = mgr.resolveLNodeRef(ce.lnodes[0]);
    if (r) {
        // r->ied, r->ld, r->ln utilisables pour relier primaire ↔ logique
    }
}

// JSON pour QML/REST
std::string sldJson = mgr.toJsonSubstations();
std::string netJson = mgr.toJsonNetwork();
```

---

## 13) Licence & dépendances

- **sclLib** : composant du projet StationViz (licence du repo parent).
- **pugixml** : licence MIT.

---

## 14) Contact / Support

- Ouvrir une *issue* dans le repo GitHub du projet.
- Préciser un extrait SCL anonymisé si le problème concerne un parsing spécifique.

