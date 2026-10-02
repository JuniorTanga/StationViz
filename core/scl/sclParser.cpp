#include "SclParser.h"
#include "pugixml/pugixml.hpp"
#include <sstream>
#include <vector>
#include <optional>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <string_view>

using namespace scl;

namespace {

// pugixml is not namespace aware: it matches element names verbatim, so a
// document declaring a prefix (<scl:SCL xmlns:scl="...">) failed every lookup
// with a misleading "Missing <SCL> root". Rather than making all 24 lookup
// sites prefix-aware, normalise the document once: rename every element to its
// local name. Attribute names are left untouched because SCL never qualifies
// them in practice.
void stripElementPrefixes(pugi::xml_node node) {
    if (const char* qualified = node.name()) {
        if (const char* colon = ::strrchr(qualified, ':'))
            node.set_name(colon + 1);
    }
    for (auto n : node.children()) stripElementPrefixes(n);
}

} // namespace

// ---- utils
// Parses "SS/VL/BAY/CN" into its segments. ss is always segment 0 and cn is
// always the last; ss used to be left empty, which silently defaulted every
// ResolvedEnd::ss to the containing substation and so broke any cross-substation
// reference.
static std::optional<CNAddress> parseConnectivityPath(const std::string& path) {
    std::vector<std::string> segs;
    std::string cur;
    for (char c : path) {
        if (c == '/') { if (!cur.empty()) { segs.push_back(cur); cur.clear(); } }
        else cur.push_back(c);
    }
    if (!cur.empty()) segs.push_back(cur);
    if (segs.size() < 3) return std::nullopt;

    CNAddress a;
    a.cn  = segs.back();
    a.bay = segs[segs.size()-2];
    a.vl  = segs[segs.size()-3];
    if (segs.size() >= 4) a.ss = segs[0];
    return a;
}

namespace {

static std::unordered_map<std::string, std::string>
readAddress(const pugi::xml_node &parent) {
    std::unordered_map<std::string, std::string> res;
    if (auto addr = parent.child("Address")) {
        for (auto p : addr.children("P")) {
            std::string key = p.attribute("type").as_string("");
            std::string val = p.text().as_string("");
            if (!key.empty())
                res[key] = val;
        }
    }
    return res;
}

static void readLNodes(const pugi::xml_node &parent,
                       std::vector<LNodeRef> &out) {
    for (auto ln : parent.children("LNode")) {
        LNodeRef r{};
        r.iedName = ln.attribute("iedName").as_string("");
        r.ldInst = ln.attribute("ldInst").as_string("");
        r.prefix = ln.attribute("prefix").as_string("");
        r.lnClass = ln.attribute("lnClass").as_string("");
        r.lnInst = ln.attribute("lnInst").as_string("");
        out.push_back(std::move(r));
    }
}

static void readTerminals(const pugi::xml_node &ceNode,
                          std::vector<Terminal> &out) {
    for (auto t : ceNode.children("Terminal")) {
        Terminal term{};
        term.name = t.attribute("name").as_string("");
        term.connectivityNodeRef = t.attribute("connectivityNode").as_string("");
        term.cNodeName = t.attribute("cNodeName").as_string("");
        out.push_back(std::move(term));
    }
}

static void readConnectivityNodes(const pugi::xml_node &parent,
                                  std::vector<ConnectivityNode> &out) {
    for (auto cn : parent.children("ConnectivityNode")) {
        ConnectivityNode c{};
        c.name = cn.attribute("name").as_string("");
        c.pathName = cn.attribute("pathName").as_string("");
        out.push_back(std::move(c));
    }
}

static void readConductingEquipments(const pugi::xml_node &parent,
                                     std::vector<ConductingEquipment> &out) {
    for (auto ce : parent.children("ConductingEquipment")) {
        ConductingEquipment e{};
        e.name = ce.attribute("name").as_string("");
        e.type = ce.attribute("type").as_string("");
        readTerminals(ce, e.terminals);
        readLNodes(ce, e.lnodes);
        out.push_back(std::move(e));
    }
}

// Locale-independent parse that never throws. std::stod used to throw
// out_of_range/invalid_argument on a garbage <Voltage> and silently accepted
// "nan"/"inf"; it is also locale-dependent (under fr_FR, "380.5" -> 380).
static bool parseDoubleStrict(const std::string& text, double& out) {
    if (text.empty()) return false;
    const char* first = text.c_str();
    char* last = nullptr;
    errno = 0;
    const double v = std::strtod(first, &last);
    if (last == first) return false;
    while (*last == ' ' || *last == '\t' || *last == '\r' || *last == '\n') ++last;
    if (*last != '\0') return false;
    if (errno == ERANGE) return false;
    if (!std::isfinite(v)) return false;
    out = v;
    return true;
}

static std::optional<ScalarWithUnit> readVoltageNode(const pugi::xml_node &vl) {
    if (auto volt = vl.child("Voltage")) {
        ScalarWithUnit sv{};
        const std::string text(volt.text().as_string(""));
        sv.valid = parseDoubleStrict(text, sv.value);
        sv.unit = volt.attribute("unit").as_string("");
        sv.multiplier = volt.attribute("multiplier").as_string("");
        return sv;
    }
    return std::nullopt;
}

static void readDataSetsUnderLN0(const pugi::xml_node& ln0, std::vector<DataSet>& out) {
    for (auto ds : ln0.children("DataSet")) {
        DataSet D{};
        D.name = ds.attribute("name").as_string("");
        for (auto f : ds.children("FCDA")) {
            FcdaRef r{};
            r.ldInst = f.attribute("ldInst").as_string("");      // optionnel
            r.lnClass = f.attribute("lnClass").as_string("");
            r.lnInst = f.attribute("lnInst").as_string("");
            r.doName = f.attribute("doName").as_string("");
            r.daName = f.attribute("daName").as_string("");
            r.fc = f.attribute("fc").as_string("");
            D.members.push_back(std::move(r));
        }
        // <Inputs><ExtRef>: members whose value arrives over GOOSE from another
        // IED, so there is no local FCDA for them.
        for (auto inputs : ds.children("Inputs")) {
            for (auto e : inputs.children("ExtRef")) {
                ExtRefRef r{};
                r.iedName  = e.attribute("iedName").as_string("");
                r.ldInst   = e.attribute("ldInst").as_string("");
                r.prefix   = e.attribute("prefix").as_string("");
                r.lnClass  = e.attribute("lnClass").as_string("");
                r.lnInst   = e.attribute("lnInst").as_string("");
                r.doName   = e.attribute("doName").as_string("");
                r.daName   = e.attribute("daName").as_string("");
                r.fc       = e.attribute("fc").as_string("");
                r.intgPd   = e.attribute("intgPd").as_string("");
                D.extRefs.push_back(std::move(r));
            }
        }
        out.push_back(std::move(D));
    }
}

// tns:TrgOps is a bit string whose SCL attributes are named (dchg, qchg,
// dupd, intg, gi), not positional. Read by name; leave a flag at its default
// when the attribute is absent.
static void applyTrgOps(const pugi::xml_node& parent, ReportControlMeta& rc) {
    auto node = parent.child("TrgOps");
    if (!node) return;
    struct Field { const char* attr; bool* flag; };
    const Field fields[] = {
        {"dchg", &rc.trgOps.dataChange},
        {"qchg", &rc.trgOps.qualityChange},
        {"dupd", &rc.trgOps.dataUpdate},
        {"intg", &rc.trgOps.integrity},
        {"gi",   &rc.trgOps.generalInterrogation},
    };
    for (const auto& f : fields) {
        const std::string v = node.attribute(f.attr).as_string("");
        if (!v.empty()) *f.flag = (v == "true" || v == "1");
    }
}

static void applyOptFields(const pugi::xml_node& parent, ReportControlMeta& rc) {
    auto node = parent.child("OptFields");
    if (!node) return;
    // These bits are named attributes, not positional.
    struct Field { const char* attr; bool* flag; };
    const Field fields[] = {
        {"sequenceNumber",     &rc.optFields.seqNum},
        {"timeStamp",          &rc.optFields.timeStamp},
        {"dataSet",            &rc.optFields.dataSet},
        {"reasonForInclusion", &rc.optFields.reasonForInclusion},
        {"configRef",          &rc.optFields.configRef},
        {"bufOvfl",            &rc.optFields.bufOvfl},
        {"entryID",            &rc.optFields.entryID},
        {"confRev",            &rc.optFields.confRev},
        {"subSeqNum",          &rc.optFields.subSeqNum},
    };
    for (const auto& f : fields) {
        const std::string v = node.attribute(f.attr).as_string("");
        if (!v.empty()) *f.flag = (v == "true" || v == "1");
    }
}

// tns:ReportControl (B-reports) and tns:ReportControlBlock (the historical
// name for a buffered block) carry the same attributes, so read both.
static void readRptCtrlsUnderLN0(const pugi::xml_node& ln0,
                                 std::vector<ReportControlMeta>& out) {
    for (const char* tag : {"ReportControl", "ReportControlBlock"}) {
        for (auto rcNode : ln0.children(tag)) {
            ReportControlMeta R{};
            R.name    = rcNode.attribute("name").as_string("");
            R.rptID   = rcNode.attribute("rptID").as_string("");
            R.datSet  = rcNode.attribute("datSet").as_string("");
            R.confRev = rcNode.attribute("confRev").as_string("");
            R.desc    = rcNode.attribute("desc").as_string("");
            R.intgPd  = rcNode.attribute("intgPd").as_string("");
            R.buffered = rcNode.attribute("buffered").as_bool(false);
            applyTrgOps(rcNode, R);
            applyOptFields(rcNode, R);
            out.push_back(std::move(R));
        }
    }
}

static void readGseCtrlsUnderLN0(const pugi::xml_node& ln0, std::vector<GseControlMeta>& out) {
    for (auto gse : ln0.children("GSEControl")) {
        GseControlMeta G{};
        G.name = gse.attribute("name").as_string("");
        G.datSet = gse.attribute("datSet").as_string("");
        G.appID = gse.attribute("appID").as_string("");
        out.push_back(std::move(G));
    }
}

static void readSmvCtrlsUnderLN0(const pugi::xml_node& ln0, std::vector<SmvControlMeta>& out) {
    for (auto sv : ln0.children("SampledValueControl")) {
        SmvControlMeta V{};
        V.name = sv.attribute("name").as_string("");
        V.datSet = sv.attribute("datSet").as_string("");
        V.appID = sv.attribute("smvID").as_string(""); // alias selon profil
        out.push_back(std::move(V));
    }
}

// ---- LDevice reader (unique, sans doublon)
static void readLDevicesUnder(const pugi::xml_node &parent,
                              std::vector<LogicalDevice> &out) {
    for (auto ld : parent.children("LDevice")) {
        LogicalDevice d{};
        d.inst = ld.attribute("inst").as_string("");
        d.desc = ld.attribute("desc").as_string(""); // added 20/09/2025

        if (auto ln0 = ld.child("LN0")) {
            readDataSetsUnderLN0(ln0, d.ln0.datasets);
            readGseCtrlsUnderLN0(ln0, d.ln0.gseCtrls);
            readSmvCtrlsUnderLN0(ln0, d.ln0.smvCtrls);
            readRptCtrlsUnderLN0(ln0, d.ln0.rptCtrls);

            LogicalNode ln{};
            ln.prefix = ln0.attribute("prefix").as_string("");
            ln.lnClass = ln0.attribute("lnClass").as_string("");
            ln.inst = ""; // LN0
            ln.desc = ln0.attribute("desc").as_string(""); // added 20/09/2025
            d.lns.push_back(std::move(ln));
        }
        for (auto lnNode : ld.children("LN")) {
            LogicalNode l{};
            l.prefix = lnNode.attribute("prefix").as_string("");
            l.lnClass = lnNode.attribute("lnClass").as_string("");
            l.inst = lnNode.attribute("inst").as_string("");
            l.desc = lnNode.attribute("desc").as_string(""); // added 20/09/2025
            d.lns.push_back(std::move(l));
        }
        out.push_back(std::move(d));
    }
}

static void readIEDs(const pugi::xml_node &root, std::vector<IED> &out) {
    for (auto ied : root.children("IED")) {
        IED I{};
        I.name = ied.attribute("name").as_string("");
        I.manufacturer = ied.attribute("manufacturer").as_string("");
        I.type = ied.attribute("type").as_string("");

        // (1) LDevice directement sous IED (toléré par certains outils)
        readLDevicesUnder(ied, I.ldevices);

        // (2) AccessPoint/Server/LDevice (forme canonique)
        // (2) AccessPoint/Server/LDevice (forme canonique).
        //     tns:AccessPoint permits an unbounded number of Server elements;
        //     reading only ap.child("Server") silently dropped every LD after
        //     the first one.
        for (auto ap : ied.children("AccessPoint")) {
            AccessPoint A{};
            A.name = ap.attribute("name").as_string("");
            A.address = readAddress(ap);
            for (auto server : ap.children("Server")) {
                A.serverAddresses.push_back(readAddress(server));
                readLDevicesUnder(server, A.ldevices);
            }
            I.accessPoints.push_back(std::move(A));
        }
        out.push_back(std::move(I));
    }
}

static Communication readCommunication(const pugi::xml_node &root) {
    Communication C{};
    if (auto comm = root.child("Communication")) {
        for (auto sn : comm.children("SubNetwork")) {
            SubNetwork S{};
            S.name = sn.attribute("name").as_string("");
            S.type = sn.attribute("type").as_string("");

            for (auto p : sn.children("P")) {
                std::string key = p.attribute("type").as_string("");
                if (!key.empty())
                    S.props[key] = p.text().as_string("");
            }

            for (auto cap : sn.children("ConnectedAP")) {
                ConnectedAP CAP{};
                CAP.iedName = cap.attribute("iedName").as_string("");
                CAP.apName = cap.attribute("apName").as_string("");
                CAP.address = readAddress(cap);

                for (auto g : cap.children("GSE")) {
                    GSE G{};
                    G.ldInst = g.attribute("ldInst").as_string("");
                    G.cbName = g.attribute("cbName").as_string("");
                    G.address = readAddress(g);
                    CAP.gses.push_back(std::move(G));
                }
                for (auto s : cap.children("SMV")) {
                    SMV V{};
                    V.ldInst = s.attribute("ldInst").as_string("");
                    V.cbName = s.attribute("cbName").as_string("");
                    V.address = readAddress(s);
                    CAP.smvs.push_back(std::move(V));
                }

                S.connectedAPs.push_back(std::move(CAP));
            }
            C.subNetworks.push_back(std::move(S));
        }
    }
    return C;
}

static Result<SclModel> parseDoc(pugi::xml_document &doc) {
    SclModel model{};

    // Normalise prefixed element names before any lookup, so all the
    // children("X") sites below keep working for both <SCL> and <scl:SCL>.
    for (auto top : doc.children()) stripElementPrefixes(top);

    auto root = doc.child("SCL");
    if (!root) {
        return Result<SclModel>({ErrorCode::XmlParseError, "Missing <SCL> root element"});
    }
    model.version = root.attribute("version").as_string("");
    model.revision = root.attribute("revision").as_string("");

    if (auto hdr = root.child("Header")) {
        model.header.id = hdr.attribute("id").as_string("");
        model.header.version = hdr.attribute("version").as_string("");
        model.header.revision = hdr.attribute("revision").as_string("");
        model.header.toolID = hdr.attribute("toolID").as_string("");
        model.header.nameStructure = hdr.attribute("nameStructure").as_string("");
        if (auto t = hdr.child("Text")) model.header.text = t.text().as_string("");
        for (auto h : hdr.children("History")) {
            for (auto it : h.children("Hitem")) {
                SclHeader::HistoryItem hi;
                hi.version = it.attribute("version").as_string("");
                hi.revision = it.attribute("revision").as_string("");
                hi.when = it.attribute("when").as_string("");
                hi.who = it.attribute("who").as_string("");
                hi.what = it.attribute("what").as_string("");
                hi.why = it.attribute("why").as_string("");
                model.header.history.push_back(std::move(hi));
            }
        }
    }

    // Substations
    for (auto ss : root.children("Substation")) {
        Substation S{};
        S.name = ss.attribute("name").as_string("");
        readLNodes(ss, S.lnodes);

        // PowerTransformers
        // PowerTransformers may sit directly under Substation or inside an
        // <Equipment>/<Container> wrapper (ED2 style). Reading only the direct
        // form made powerTransformers=0 on a valid ED2 file.
        auto readPTs = [&S](const pugi::xml_node& parent) {
            for (auto ptNode : parent.children("PowerTransformer")) {
                PowerTransformer pt;
                pt.name = ptNode.attribute("name").as_string();
                pt.desc = ptNode.attribute("desc").as_string();
                pt.type = ptNode.attribute("type").as_string();

                for (auto wNode : ptNode.children("TransformerWinding")) {
                    TransformerWinding w;
                    w.name = wNode.attribute("name").as_string();
                    w.type = wNode.attribute("type").as_string();

                    // A winding may carry several TapChanger plus one
                    // PhaseTapChanger, so a single optional could not hold them.
                    auto readTCs = [&w](const pugi::xml_node& wn, const char* tag) {
                        for (auto tc : wn.children(tag)) {
                            TapChangerInfo tci;
                            tci.name = tc.attribute("name").as_string();
                            tci.type = tc.attribute("type").as_string();
                            w.tapChangers.push_back(std::move(tci));
                        }
                    };
                    readTCs(wNode, "TapChanger");
                    readTCs(wNode, "PhaseTapChanger");

                    for (auto tNode : wNode.children("Terminal")) {
                        TerminalRef tr;
                        tr.name = tNode.attribute("name").as_string();
                        tr.cNodeName = tNode.attribute("cNodeName").as_string();
                        tr.connectivityPath = tNode.attribute("connectivityNode").as_string();
                        tr.substationName = tNode.attribute("substationName").as_string();
                        w.terminals.push_back(std::move(tr));
                    }
                    pt.windings.push_back(std::move(w));
                }
                S.powerTransformers.push_back(std::move(pt));
            }
        };
        readPTs(ss);
        for (auto eq : ss.children("Equipment")) {
            readPTs(eq);
            for (auto cont : eq.children("Container")) readPTs(cont);
        }

        for (auto vl : ss.children("VoltageLevel")) {
            VoltageLevel V{};
            V.name = vl.attribute("name").as_string("");
            V.nomFreq = vl.attribute("nomFreq").as_string("");
            V.voltage = readVoltageNode(vl);
            readLNodes(vl, V.lnodes);

            for (auto bay : vl.children("Bay")) {
                Bay B{};
                B.name = bay.attribute("name").as_string("");
                readConnectivityNodes(bay, B.connectivityNodes);
                readConductingEquipments(bay, B.equipments);
                readLNodes(bay, B.lnodes);
                V.bays.push_back(std::move(B));
            }
            S.vlevels.push_back(std::move(V));
        }
        model.substations.push_back(std::move(S));
    }

    // Résolution rapide des extrémités de transformateur
    for (auto &ss : model.substations) {
        for (auto &pt : ss.powerTransformers) {
            for (auto &w : pt.windings) {
                w.resolvedEnds.clear();
                for (const auto &tr : w.terminals) {
                    TransformerWinding::ResolvedEnd re;
                    re.ss = !tr.substationName.empty() ? tr.substationName : ss.name;

                    if (!tr.connectivityPath.empty()) {
                        if (auto addr = parseConnectivityPath(tr.connectivityPath)) {
                            re.vl = addr->vl; re.bay = addr->bay; re.cn = addr->cn;
                        } else {
                            re.cn = tr.cNodeName;
                        }
                    } else {
                        re.cn = tr.cNodeName;
                    }
                    w.resolvedEnds.push_back(re);
                }
            }
        }
    }

    // IEDs
    readIEDs(root, model.ieds);

    // Communication
    model.communication = readCommunication(root);

    return Result<SclModel>(std::move(model));
}

} // namespace

SclParser::SclParser() = default;

Result<SclModel> SclParser::parseFile(const std::string &path) {
    pugi::xml_document doc;
    pugi::xml_parse_result ok =
        doc.load_file(path.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!ok) {
        std::ostringstream oss;
        oss << "XML parse error: " << ok.description() << ", offset=" << ok.offset;
        return Result<SclModel>({ErrorCode::XmlParseError, oss.str()});
    }
    return parseDoc(doc);
}

Result<SclModel> SclParser::parseString(const std::string &xml) {
    pugi::xml_document doc;
    pugi::xml_parse_result ok =
        doc.load_string(xml.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!ok) {
        std::ostringstream oss;
        oss << "XML parse error: " << ok.description() << ", offset=" << ok.offset;
        return Result<SclModel>({ErrorCode::XmlParseError, oss.str()});
    }
    return parseDoc(doc);
}
