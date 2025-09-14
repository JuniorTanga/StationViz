#pragma once
#include <memory>
#include <unordered_map>
#include <functional>
#include "Result.h"
#include "SclTypes.h"

namespace scl {

class SclManager {
public:
    SclManager();

    // Charge et parse un fichier SCL + construit les indexes
    Status loadScl(const std::string& filepath);

    // Accès lecture au modèle
    const SclModel* model() const { return model_ ? &(*model_) : nullptr; }

    // Debug helpers
    Status printSubstations() const;
    Status printIEDs() const;
    Status printCommunication() const;
    Status printTopology() const; // CE ↔ CN

    // Requêtes simples
    Result<const Substation*> findSubstation(const std::string& name) const;
    Result<const IED*> findIED(const std::string& name) const;

    // Résolution d’un LNodeRef
    Result<ResolvedLNode> resolveLNodeRef(const LNodeRef& ref) const;

    // Aides SLD/Network
    std::vector<EdgeCEtoCN> collectSldEdges() const; // liste des arêtes CE↔CN
    std::vector<ConnectivityNode> getConnectivityNodes(const std::string& ss,
                                                       const std::string& vl,
                                                       const std::string& bay) const;

    // JSON utilitaires (sorties rapides pour debug/GUI)
    std::string toJsonSubstations() const;   // Substations + topology légère
    std::string toJsonNetwork() const;       // Communication

private:
    void buildIndexes_();

    const LogicalDevice* findLD_(const IED& ied, const std::string& ldInst) const;
    const LogicalNode*   findLN_(const LogicalDevice& ld, const std::string& lnClass,
                               const std::string& lnInst, const std::string& prefix) const;

    // Indexes
    std::unique_ptr<SclModel> model_;
    std::unordered_map<std::string, const IED*> iedByName_;
    // CN index par chemin (pathName ou fallback composé "SS/VL/BAY/CN")
    std::unordered_map<std::string, const ConnectivityNode*> cnByPath_;
};

} // namespace scl
