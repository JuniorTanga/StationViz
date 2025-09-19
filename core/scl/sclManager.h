#pragma once
#include <memory>
#include <unordered_map>
#include <functional>
#include "Internet.h"
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

    // NEW: utilitaires & accès aux nouveaux index
    bool matchCN(const std::string& a, const std::string& b) const;

    // Network endpoints
    const std::unordered_map<std::string, GseEndpoint>& gseEndpoints() const { return gseEndpoints_; }
    const std::unordered_map<std::string, SvEndpoint>&  svEndpoints()  const { return svEndpoints_;  }
    const std::unordered_map<std::string, MmsEndpoint>& mmsEndpoints() const { return mmsEndpoints_; }

    // Lien primaire <-> LNodeRef
    const std::unordered_map<std::string, std::vector<LNodeRef>>& lnodesByPrimary() const { return lnodesByPrimary_; }
    const std::unordered_map<std::string, std::vector<std::string>>& primaryByLrefKey() const { return primaryByLref_; }

    // Diagnostics
    struct Diag { ErrorCode code; std::string location; std::string message; std::string hint; };
    const std::vector<Diag>& diagnostics() const { return diags_; }

    // JSON (nlohmann) – mêmes noms/retours, impl diff
    std::string toJsonSubstations() const;
    std::string toJsonNetwork() const;

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

    // --- NEW: interner + indexes CN canoniques
    StringInterner interner_;
    // logique "SS:VL:BAY:CN" -> fullPath
    std::unordered_map<std::string, std::string> mapCNByLogical_;
    // fullPath -> logique
    std::unordered_map<std::string, std::string> mapCNByFullToLogical_;
    // suffixe "CN" -> set de fullPath
    std::unordered_map<std::string, std::vector<std::string>> mapCNSuffix_;

    // Lien primaire <-> LNodeRef
    std::unordered_map<std::string, std::vector<LNodeRef>> lnodesByPrimary_;
    std::unordered_map<std::string, std::vector<std::string>> primaryByLref_;

    // Endpoints réseau (keys compactes)
    // keyGse = iedName + "|" + ldInst + "|" + cbName
    std::unordered_map<std::string, GseEndpoint> gseEndpoints_;
    std::unordered_map<std::string, SvEndpoint>  svEndpoints_;
    // keyMms  = iedName + "|" + apName
    std::unordered_map<std::string, MmsEndpoint> mmsEndpoints_;

    // Diagnostics
    std::vector<Diag> diags_;

};

} // namespace scl
