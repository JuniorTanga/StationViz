#pragma once
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include "Internet.h"
#include "Result.h"
#include "SclTypes.h"

namespace scl {

class SclManager {
public:
    SclManager();


    // Charge et parse un fichier SCL + construit les indexes.
    // WARNING: on success this replaces the model, so every pointer previously
    // returned by model()/findIED()/findSubstation()/getLn0Dataset() dangles.
    // Consumers that cache raw pointers (core/sld does) must rebuild after a
    // reload.
    Status loadScl(const std::string& filepath);

    // Same, from an in-memory document. Useful for tests.
    Status loadSclString(const std::string& xml);

    // Accès lecture au modèle
    const SclModel* model() const { return model_ ? &(*model_) : nullptr; }

    // True if any diagnostic has Severity::Error. loadScl() can return Ok while
    // diagnostics() holds errors, so callers must check this explicitly.
    bool hasErrors() const;
    bool hasWarnings() const;

    // Debug helpers (garde-les pour l'instant)
    Status printSubstations() const;
    Status printIEDs() const;
    Status printCommunication() const;
    Status printTopology() const; // CE ↔ CN
    Status printEquipmentFromIEDs() const;


    // Requêtes simples
    Result<const Substation*> findSubstation(const std::string& name) const;
    Result<const IED*> findIED(const std::string& name) const;

    // Résolution d’un LNodeRef
    Result<ResolvedLNode> resolveLNodeRef(const LNodeRef& ref) const;
	
	// Retourne le DataSet LN0 s'il existe (nullptr sinon)
    const DataSet* getLn0Dataset(const std::string& ied,
                                 const std::string& ldInst,
                                 const std::string& dsName) const;

    // LDevice of an IED by @inst, searching both IED/LDevice and
    // IED/AccessPoint/Server/LDevice. nullptr when absent.
    const LogicalDevice* findLogicalDevice(const IED& ied,
                                            const std::string& ldInst) const;

    // Construit "LD/LN.DO(.DA)[FC]" pour un FCDA (helper pour libIEC61850)
    static std::string fcdaToMmsRef(const std::string& ldInst, const FcdaRef& f);

    // Résout un DataSet LN0 en liste d'objectRefs MMS
    std::vector<std::string> resolveDatasetMembers(const std::string& ied,
                                                   const std::string& ldInst,
                                                   const std::string& dsName) const;

    // Aides SLD/Network
    std::vector<EdgeCEtoCN> collectSldEdges() const; // liste des arêtes CE↔CN
    std::vector<ConnectivityNode> getConnectivityNodes(const std::string& ss,
                                                       const std::string& vl,
                                                       const std::string& bay) const;

    // CN
    bool matchCN(const std::string& a, const std::string& b) const;

    // Report control blocks. This is what a client needs to subscribe to
    // B-reports; it is independent of the Communication/GOOSE endpoints.
    // Key is ied|ldInst|rcbName, which is also the MMS rcbReference modulo the
    // LN name and '$' separator.
    const std::unordered_map<std::string, const ReportControlMeta*>& reportControls() const {
        return rptCtrls_;
    }

    // All RCBs for one IED, in document order.
    std::vector<const ReportControlMeta*> reportControlsOf(const std::string& iedName) const;

    // Full MMS object reference of an RCB, e.g. "LD1/LLN0$RP$urcbA".
    // Falls back to rptID when the SCL provides one, since that is what the
    // server reports and what libiec61850 matches on.
    static std::string rcbReference(const std::string& ldInst,
                                    const ReportControlMeta& rc);

    // Endpoints réseau
    const std::unordered_map<std::string, GseEndpoint>& gseEndpoints() const { return gseEndpoints_; }
    const std::unordered_map<std::string, SvEndpoint>&  svEndpoints()  const { return svEndpoints_; }
    const std::unordered_map<std::string, MmsEndpoint>& mmsEndpoints() const { return mmsEndpoints_; }

    // Lien primaire <-> LNodeRef
    const std::unordered_map<std::string, std::vector<LNodeRef>>& lnodesByPrimary() const { return lnodesByPrimary_; }
    const std::unordered_map<std::string, std::vector<std::string>>& primaryByLrefKey() const { return primaryByLref_; }

    // --- NEW: Équipements issus des IEDs (même sans <Substation>)
    std::vector<EquipmentFromIED> collectEquipmentFromIEDs(bool skipLogical = true) const;

    // --- NEW: Datasets & Mapping FCDA -> datasets
    struct DatasetKey {
        std::string ied, ld, name;
        bool operator==(DatasetKey const& other) const noexcept {
            return ied == other.ied && ld == other.ld && name == other.name;
        }
    };
    struct DatasetKeyHash {
        size_t operator()(DatasetKey const& k) const noexcept;
    };

    const std::unordered_map<DatasetKey, const DataSet*, DatasetKeyHash>& datasets() const { return datasets_; }
    const std::unordered_multimap<std::string, DatasetKey>& fcdaToDatasets() const { return fcdaToDatasets_; }

    // Diagnostics
    enum class Severity { Error, Warning, Info };
    struct Diag { ErrorCode code; std::string location; std::string message; std::string hint;Severity severity {Severity::Error}; };
    const std::vector<Diag>& diagnostics() const { return diags_; }

    // JSON (nlohmann)
    std::string toJsonSubstations() const;
    std::string toJsonNetwork() const;
    std::string toJsonIEDs() const; 
	std::string toJsonNetworkMap() const;

    // Observabilité (simple)
    using ReloadCallback = std::function<void(const SclManager&)>;
    void onReloaded(ReloadCallback cb) { reloadCbs_.push_back(std::move(cb)); }

private:
    void adopt_(SclModel&& m);
    void buildIndexes_();

    const LogicalDevice* findLD_(const IED& ied, const std::string& ldInst) const;
    // LDevice that owns a given ReportControlMeta (identified by address).
    std::string ldInstOf(const IED& ied, const ReportControlMeta* rc) const;
    const LogicalNode*   findLN_(const LogicalDevice& ld, const std::string& lnClass,
                                 const std::string& lnInst, const std::string& prefix) const;

    static std::string logicalCNKey(std::string_view ss, std::string_view vl,
                                    std::string_view bay, std::string_view cn);

    // Indexes
    std::unique_ptr<SclModel> model_;
    std::unordered_map<std::string, const IED*> iedByName_;
    std::unordered_map<std::string, const ConnectivityNode*> cnByPath_;

    // CN canoniques
    StringInterner interner_;
    std::unordered_map<std::string, std::string> mapCNByLogical_;       // logique "SS:VL:BAY:CN" -> fullPath
    std::unordered_map<std::string, std::string> mapCNByFullToLogical_; // fullPath -> logique
    std::unordered_map<std::string, std::vector<std::string>> mapCNSuffix_; // suffix -> fullPaths

    // Lien primaire <-> LNodeRef
    std::unordered_map<std::string, std::vector<LNodeRef>> lnodesByPrimary_;
    std::unordered_map<std::string, std::vector<std::string>> primaryByLref_;

    // Endpoints réseau
    std::unordered_map<std::string, GseEndpoint> gseEndpoints_; // key: ied|ld|cb
    std::unordered_map<std::string, SvEndpoint>  svEndpoints_;  // key: ied|ld|cb
    std::unordered_map<std::string, MmsEndpoint> mmsEndpoints_; // key: ied|ap

    // Report control blocks: key ied|ld|rcbName
    std::unordered_map<std::string, const ReportControlMeta*> rptCtrls_;

    // NEW: datasets & mapping FCDA
    std::unordered_map<DatasetKey, const DataSet*, DatasetKeyHash> datasets_;
    std::unordered_multimap<std::string, DatasetKey> fcdaToDatasets_;

    // Diagnostics
    std::vector<Diag> diags_;
    void validate_();

    // Observabilité
    std::vector<ReloadCallback> reloadCbs_;
};

} // namespace scl
