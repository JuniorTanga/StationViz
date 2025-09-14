#include "SclParser.h"
#include <QDebug>
#include "sclNodes/voltageLevel.h"
#include "sclNodes/bay.h"

SclParser::SclParser(const QString& filePath)
    : m_filePath(filePath) {}

bool SclParser::loadFile() {
    pugi::xml_parse_result result = m_doc.load_file(m_filePath.toStdString().c_str());
    if (!result) {
        qWarning() << "Erreur de parsing XML:" << result.description()
        << "à offset" << result.offset;
        return false;
    }
    return true;
}

QList<Substation> SclParser::parseSubstations() {
    QList<Substation> substations;

    pugi::xml_node root = m_doc.child("SCL");
    if (!root) {
        qWarning() << "Erreur: noeud <SCL> introuvable.";
        return substations;
    }

    for (pugi::xml_node subNode : root.children("Substation")) {
        Substation sub;
        sub.name = QString::fromStdString(subNode.attribute("name").as_string());
        sub.desc = QString::fromStdString(subNode.attribute("desc").as_string());

        // VoltageLevels
        for (pugi::xml_node vlNode : subNode.children("VoltageLevel")) {
            VoltageLevel vl;
            vl.name = QString::fromStdString(vlNode.attribute("name").as_string());
            vl.desc = QString::fromStdString(vlNode.attribute("desc").as_string());

            // Sous-noeud <Voltage>
            pugi::xml_node vNode = vlNode.child("Voltage");
            if (vNode) {
                vl.multiplier     = QString::fromStdString(vNode.attribute("multiplier").as_string(""));
                vl.unit           = QString::fromStdString(vNode.attribute("unit").as_string(""));
                vl.nomFreq        = vNode.attribute("frequency").as_double(50.0);
                vl.numPhases      = vNode.attribute("numPhases").as_int(3);

                vl.nominalVoltage = vNode.text().as_double(0.0);

                // Conversion suivant le multiplicateur
                double factor = 1.0;
                if (vl.multiplier == "k") factor = 1e3;
                else if (vl.multiplier == "M") factor = 1e6;
                else if (vl.multiplier == "m") factor = 1e-3;

                vl.voltage = vl.nominalVoltage * factor;
            }

            // Bays
            for (pugi::xml_node bayNode : vlNode.children("Bay")) {
                Bay bay;
                bay.name = QString::fromStdString(bayNode.attribute("name").as_string());
                bay.desc = QString::fromStdString(bayNode.attribute("desc").as_string());

                // ConductingEquipment
                for (pugi::xml_node eqNode : bayNode.children("ConductingEquipment")) {
                    ConductingEquipment eq;
                    eq.name = QString::fromStdString(eqNode.attribute("name").as_string());
                    eq.type = QString::fromStdString(eqNode.attribute("type").as_string());


                    //Terminals
                    for(pugi::xml_node terminalNode : eqNode.children("Terminal"))
                    {
                        Terminal tl;
                        tl.name = QString::fromStdString(terminalNode.attribute("name").as_string());
                        tl.cNodeName = QString::fromStdString(terminalNode.attribute("cNodeName").as_string());
                        tl.connectivityNode = QString::fromStdString(terminalNode.attribute("connectivityNode").as_string());
                        eq.terminals.append(tl);
                    }

                    bay.equipments.append(eq);
                }

                //Connectivity Node

                for (pugi::xml_node connectNode : bayNode.children("ConnectivityNode")) {
                    ConnectivityNode cn;
                    cn.name = QString::fromStdString(connectNode.attribute("name").as_string());
                    cn.pathName = QString::fromStdString(connectNode.attribute("pathName").as_string());
                    bay.connectivityNodes.append(cn);
                }

                vl.bays.append(bay);
            }

            sub.voltageLevels.append(vl);
        }

        // PowerTransformers
        for (pugi::xml_node trNode : subNode.children("PowerTransformer")) {
            PowerTransformer tr;
            tr.name = QString::fromStdString(trNode.attribute("name").as_string());
            tr.desc = QString::fromStdString(trNode.attribute("desc").as_string());
            sub.transformers.append(tr);
        }

        substations.append(sub);
    }

    return substations;
}

QList<Ied> SclParser::parseIeds() {

    QList<Ied> ieds;

    pugi::xml_node root = m_doc.child("SCL");
    if (!root) {
        qWarning() << "Pas de noeud <SCL> trouvé.";
        return ieds;
    }

    for (pugi::xml_node iedNode : root.children("IED")) {
        Ied ied;
        ied.name          = QString::fromStdString(iedNode.attribute("name").as_string());
        ied.type          = QString::fromStdString(iedNode.attribute("type").as_string());
        ied.manufacturer  = QString::fromStdString(iedNode.attribute("manufacturer").as_string());
        ied.configVersion = QString::fromStdString(iedNode.attribute("configVersion").as_string());
        ied.owner         = QString::fromStdString(iedNode.attribute("owner").as_string());

        // --- AccessPoints ---
        for (pugi::xml_node apNode : iedNode.children("AccessPoint")) {
            AccessPoint ap;
            ap.name = QString::fromStdString(apNode.attribute("name").as_string());
            /*
            // --- LDevices ---
            for (pugi::xml_node ldNode : apNode.children("LDevice")) {
                LDevice ld;
                ld.inst = QString::fromStdString(ldNode.attribute("inst").as_string());

                // --- LN0 ---
                pugi::xml_node ln0Node = ldNode.child("LN0");
                if (ln0Node) {
                    LogicalNode ln0;
                    ln0.lnClass = QString::fromStdString(ln0Node.attribute("lnClass").as_string());
                    ln0.inst    = QString::fromStdString(ln0Node.attribute("inst").as_string());
                    ln0.lnType  = QString::fromStdString(ln0Node.attribute("lnType").as_string());
                    ln0.prefix  = QString::fromStdString(ln0Node.attribute("prefix").as_string());
                    ld.ln0 = ln0;
                }

                // --- LN ---
                for (pugi::xml_node lnNode : ldNode.children("LN")) {
                    LogicalNode ln;
                    ln.lnClass = QString::fromStdString(lnNode.attribute("lnClass").as_string());
                    ln.inst    = QString::fromStdString(lnNode.attribute("inst").as_string());
                    ln.lnType  = QString::fromStdString(lnNode.attribute("lnType").as_string());
                    ln.prefix  = QString::fromStdString(lnNode.attribute("prefix").as_string());
                    ld.lns.append(ln);
                }

                ap.ldevices.append(ld);
            }*/

            ied.accessPoints.append(ap);
        }

        ieds.append(ied);
    }

    return ieds;
}
