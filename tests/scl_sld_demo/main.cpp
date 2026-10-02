
// Requires user's modules: SclManager / SldManager (+ nlohmann::json, Boost Graph)
// C++17
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <filesystem>

#include "SclManager.h"
#include "SldManager.h"
#include "SldConfig.h"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace std::chrono;

// ---------------- CSV util ----------------
struct RowWriter {
    std::ofstream ofs;
    bool header_written{false};
    RowWriter(const fs::path& p){ ofs.open(p, std::ios::out | std::ios::trunc); }
    static std::string csv_escape(const std::string& s){
        bool need = s.find_first_of(",\"\n") != std::string::npos;
        if (!need) return s;
        std::string out = "\"";
        for (char c : s){
            if (c=='"') out += "\"\""; else out += c;
        }
        out += "\"";
        return out;
    }
    template<typename... Args>
    void write_header(Args&&... cols){
        if (header_written) return;
        std::vector<std::string> v{std::forward<Args>(cols)...};
        bool first=true;
        for (auto& c : v){ if(!first) ofs<<","; first=false; ofs<<csv_escape(c); }
        ofs << "\n";
        header_written = true;
    }
    template<typename... Args>
    void write_row(Args&&... cols){
        std::vector<std::string> v{std::forward<Args>(cols)...};
        bool first=true;
        for (auto& c : v){ if(!first) ofs<<","; first=false; ofs<<csv_escape(c); }
        ofs << "\n";
    }
};

// ---------------- Helpers -----------------
static std::string now_iso(){
    auto t = system_clock::now();
    auto tt = system_clock::to_time_t(t);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Error codes map to their own names via scl::to_string, which lives in the
// library. The previous hand-rolled switch here fell through to "Other" for
// every code added since, silently weakening the acceptance policy.
static std::string code_to_string(scl::ErrorCode c){ return scl::to_string(c); }

static std::string severity_to_string(scl::SclManager::Severity s){
    using S = scl::SclManager::Severity;
    switch(s){
        case S::Error:   return "Error";
        case S::Warning: return "Warning";
        case S::Info:    return "Info";
        default:         return "Error";
    }
}

// ---------------- Main --------------------
int main(int argc, char** argv){
    if (argc < 2){
        std::cerr << "Usage: " << argv[0] << " test_manifest.json\n";
        return 2;
    }
    fs::path manifestPath = argv[1];
    std::ifstream mf(manifestPath);
    if (!mf){
        std::cerr << "Cannot open manifest: " << manifestPath << "\n";
        return 2;
    }
    json M; mf >> M;

    // Resolve relative paths against the manifest's own directory, so the
    // manifest is portable and does not hard-code an absolute install path.
    const fs::path base = manifestPath.has_parent_path() ? manifestPath.parent_path() : fs::path(".");
    const auto resolve = [&base](const fs::path& p){
        return p.is_absolute() ? p : base / p;
    };

    fs::path outdir = resolve(M.value("output_dir", "test_out"));
    fs::create_directories(outdir);

    const bool strict_policy = M.value("strict_policy", true);

    // CSV writers (same as before) + new results.csv
    RowWriter cov(outdir / "coverage.csv");
    cov.write_header("id","type","file","size_bytes","ss","vl","bay","cn","ce","ied","ld","ln","has_network");

    RowWriter sclw(outdir / "scl.csv");
    sclw.write_header("id","status","diag_count","diag_sample1","diag_sample2","diag_sample3");

    RowWriter sldw(outdir / "sld.csv");
    sldw.write_header("id","rawV","rawE","condV","condE","buses","feeders","couplers","transformers");

    RowWriter netw(outdir / "network.csv");
    netw.write_header("id","gse","gse_dataset_missing","sv","sv_dataset_missing","mms","sv_missing_smpRate");

    RowWriter perf(outdir / "perf.csv");
    perf.write_header("id","t_load_ms","t_sld_ms","timestamp");

    RowWriter resw(outdir / "results.csv");
    resw.write_header("id","type","accepted","reason");

    // Options for JSON dumps
    bool dump_sub = M.value("dump_json_substations", false);
    bool dump_net = M.value("dump_json_network", false);
    bool dump_ied = M.value("dump_json_ieds", false);
    bool dump_sld = M.value("dump_json_sld", false);

    // Optional SLD config
    sld::HeuristicsConfig cfg;

    auto scan_model = [](const scl::SclModel* m){
        struct C { size_t ss=0, vl=0, bay=0, cn=0, ce=0, ied=0, ld=0, ln=0; };
        C c;
        if (!m) return c;
        c.ied = m->ieds.size();
        for (const auto& ied : m->ieds){
            c.ld += ied.ldevices.size();
            for (const auto& ap : ied.accessPoints) c.ld += ap.ldevices.size();
            for (const auto& ld : ied.ldevices) c.ln += ld.lns.size();
            for (const auto& ap : ied.accessPoints)
                for (const auto& ld : ap.ldevices) c.ln += ld.lns.size();
        }
        c.ss = m->substations.size();
        for (const auto& ss : m->substations){
            for (const auto& vl : ss.vlevels){
                ++c.vl;
                for (const auto& bay : vl.bays){
                    ++c.bay;
                    c.cn += bay.connectivityNodes.size();
                    c.ce += bay.equipments.size();
                }
            }
        }
        return c;
    };

    bool any_fail = false;
    for (const auto& T : M["tests"]){
        const std::string id   = T.value("id","");
        const std::string type = T.value("type","");
        const fs::path file    = resolve(T.at("file").get<std::string>());
        const std::vector<std::string> expected_errors = T.value("expected_errors", std::vector<std::string>{});

        fs::path fileOutDir = outdir / id;
        fs::create_directories(fileOutDir);

        // file size
        size_t file_size = 0;
        std::error_code ec;
        file_size = fs::file_size(file, ec);
        if (ec) file_size = 0;

        // The large fixtures are gitignored (see .gitignore): they exist only to
        // prove parse performance on a big SCD and add ~19 MB to the repo. Skip
        // them cleanly when absent so a fresh clone still passes, and make the
        // omission visible rather than silently green.
        if (file_size == 0) {
            printf("  SKIP %s: %s not present (large fixture, gitignored)\n",
                   id.c_str(), file.string().c_str());
            resw.write_row(id, type, "1", "SKIP: fixture absent (gitignored large file)");
            perf.write_row(id, "", "", now_iso());
            continue;
        }

        scl::SclManager sm;

        // timing: load (parse + index + validate)
        auto t0 = std::chrono::steady_clock::now();
        auto st = sm.loadScl(file.string());
        auto t1 = std::chrono::steady_clock::now();
        auto dt_load = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        // coverage
        auto counters = scan_model(sm.model());
        bool has_net = (!sm.mmsEndpoints().empty() || !sm.gseEndpoints().empty() || !sm.svEndpoints().empty());
        cov.write_row(id, type, file.string(),
                      std::to_string(file_size),
                      std::to_string(counters.ss),
                      std::to_string(counters.vl),
                      std::to_string(counters.bay),
                      std::to_string(counters.cn),
                      std::to_string(counters.ce),
                      std::to_string(counters.ied),
                      std::to_string(counters.ld),
                      std::to_string(counters.ln),
                      has_net ? "1" : "0");

        // diagnostics (collect)
        const auto& diags = sm.diagnostics();
        std::string s1, s2, s3;
        for (size_t i=0;i<diags.size() && i<3;i++){
            const auto& d = diags[i];
            std::ostringstream oss;
            oss << "[" << severity_to_string(d.severity) << ":" << code_to_string(d.code) << "] "
                << d.message;
            if (!d.location.empty()) oss << " @ " << d.location;
            if (!d.hint.empty())     oss << " (hint: " << d.hint << ")";
            if (i==0) s1 = oss.str(); else if (i==1) s2 = oss.str(); else s3 = oss.str();
        }
        sclw.write_row(id, st ? "OK" : "ERR", std::to_string(diags.size()), s1, s2, s3);

        // dumps
        if (dump_sub){ std::ofstream js(fileOutDir / "substations.json"); js << sm.toJsonSubstations(); }
        if (dump_net){ std::ofstream jn(fileOutDir / "network.json");    jn << sm.toJsonNetwork(); }
        if (dump_ied){ std::ofstream ji(fileOutDir / "ieds.json");       ji << sm.toJsonIEDs(); }

        // Build SLD + timing
        auto t2 = std::chrono::steady_clock::now();
        sld::SldManager sldm(&sm, cfg);
        auto st2 = sldm.build();
        auto t3 = std::chrono::steady_clock::now();
        auto dt_sld = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();

        // SLD counts
        const auto& plan = sldm.plan();
        size_t rawV = num_vertices(sldm.raw());
        size_t rawE = num_edges(sldm.raw());
        size_t condV = num_vertices(sldm.condensed());
        size_t condE = num_edges(sldm.condensed());
        size_t buses = plan.buses.size();
        size_t feeders = plan.feeders.size();
        size_t couplers = plan.couplers.size();
        size_t transformers = plan.transformers.size();

        sldw.write_row(id,
                       std::to_string(rawV),
                       std::to_string(rawE),
                       std::to_string(condV),
                       std::to_string(condE),
                       std::to_string(buses),
                       std::to_string(feeders),
                       std::to_string(couplers),
                       std::to_string(transformers));

        if (dump_sld){
            std::ofstream jr(fileOutDir / "sld_raw.json");       jr << sldm.rawJson();
            std::ofstream jc(fileOutDir / "sld_condensed.json"); jc << sldm.condensedJson();
            std::ofstream jp(fileOutDir / "sld_plan.json");      jp << sldm.planJson();
        }

        // Network checks (runner-side metrics)
        size_t gse = sm.gseEndpoints().size();
        size_t sv  = sm.svEndpoints().size();
        size_t mms = sm.mmsEndpoints().size();
        size_t gse_ds_missing = 0, sv_ds_missing = 0, sv_missing_smp = 0;
        for (const auto& kv : sm.gseEndpoints())
            if (kv.second.datasetRef.empty()) ++gse_ds_missing;
        for (const auto& kv : sm.svEndpoints()){
            if (kv.second.datasetRef.empty()) ++sv_ds_missing;
            if (kv.second.smpRate.empty()) ++sv_missing_smp;
        }
        netw.write_row(id,
                       std::to_string(gse), std::to_string(gse_ds_missing),
                       std::to_string(sv),  std::to_string(sv_ds_missing),
                       std::to_string(mms), std::to_string(sv_missing_smp));

        // perf
        perf.write_row(id,
                       std::to_string(dt_load),
                       std::to_string(dt_sld),
                       now_iso());

        // ----------------- STRICT ACCEPTANCE -----------------
        auto is_error = [](const scl::SclManager::Diag& d){
            return d.severity == scl::SclManager::Severity::Error;
        };
        std::unordered_set<std::string> errcodes;
        for (const auto& d : diags){
            if (is_error(d)) errcodes.insert(code_to_string(d.code));
        }

        bool accepted = true;
        std::ostringstream why;

        if (type=="ref" || type=="variant" || type=="network" || type=="heavy"){
            if (!errcodes.empty()){
                accepted = false;
                why << "Erreur(s) non permise(s): ";
                bool first=true;
                for (const auto& c : errcodes){ if(!first) why<<","; first=false; why<<c; }
            } else {
                why << "OK: aucun diagnostic de niveau Error.";
            }
        } else if (type=="invalid"){
            std::unordered_set<std::string> expected(expected_errors.begin(), expected_errors.end());
            for (const auto& need : expected){
                if (!errcodes.count(need)){
                    accepted = false;
                    if (why.tellp()>0) why<<" | ";
                    why << "Erreur attendue manquante: " << need;
                }
            }
            for (const auto& got : errcodes){
                if (!expected.count(got)){
                    accepted = false;
                    if (why.tellp()>0) why<<" | ";
                    why << "Erreur non attendue: " << got;
                }
            }
            if (accepted && expected.empty()){
                accepted = false;
                if (why.tellp()>0) why<<" | ";
                why << "Manifest invalide: expected_errors vide pour un test invalide";
            }
            if (accepted && errcodes.empty()){
                accepted = false;
                if (why.tellp()>0) why<<" | ";
                why << "Aucune erreur émise alors qu'au moins une était attendue";
            }
            if (accepted && why.tellp()==0) {
                why << "OK: erreurs attendues présentes et aucune erreur parasite.";
            }
        } else {
            accepted = false;
            why << "Type de test inconnu: " << type;
        }

        resw.write_row(id, type, accepted ? "1" : "0", why.str());
        if (!accepted) any_fail = true;
    }

    if (strict_policy && any_fail){
        std::cerr << "[STRICT] Some tests failed acceptance. See results.csv\n";
        return 1;
    }

    std::cout << "Done. Outputs written to: " << outdir << std::endl;
    return 0;
}
