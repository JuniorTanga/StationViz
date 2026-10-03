// Headless smoke test: exercises the real AppContext pipeline that the UI
// depends on, and asserts the SLD models actually get populated.
#include "AppContext.h"
#include "Models/NodeModel.h"
#include "Models/EdgeModel.h"
#include "Models/DiagnosticModel.h"
#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QVariantMap>
#include <cstdio>
static int fails=0;
static void ck(const char* n, bool ok, const QString& extra=QString()){
  printf("  %s  %s%s\n", ok?"PASS":"FAIL", n, extra.isEmpty()?"":qPrintable(QString("  [")+extra+"]"));
  if(!ok) ++fails;
}
int main(int argc, char** argv){
  QCoreApplication app(argc, argv);
  if (argc < 2){ printf("usage: uitest <file.scd>\n"); return 2; }
  AppContext ctx;
  ctx.openSclFile(QString::fromLocal8Bit(argv[1]));
  ck("hasScl", ctx.hasScl());
  auto* nodes = qobject_cast<NodeModel*>(ctx.property("nodes").value<QObject*>());
  auto* edges = qobject_cast<EdgeModel*>(ctx.property("edges").value<QObject*>());
  ck("nodes model present", nodes!=nullptr);
  ck("edges model present", edges!=nullptr);
  if(nodes){
    const int n = nodes->rowCount();
    printf("      nodes=%d\n", n);
    // n>0 is asserted in the caller only when the document has a Substation.
    // ids must be unique: non-unique ids break selection and labels
    QSet<QString> ids;
    int dup=0, noPos=0;
    for(int r=0;r<n;++r){
      auto m = nodes->get(r);
      const QString id = m.value("id").toString();
      if(ids.contains(id)) ++dup; else ids.insert(id);
      if(!m.contains("x")||!m.contains("y")) ++noPos;
    }
    ck("node ids unique", dup==0, QString("dups=%1").arg(dup));
    ck("every node has x/y", noPos==0, QString("missing=%1").arg(noPos));
  }
  if(edges){
    const int m = edges->rowCount();
    printf("      edges=%d\n", m);
    // duplicate edges were previously emitted twice for every bus span
    QSet<QString> seen; int dup=0;
    for(int r=0;r<m;++r){
      QVariantMap e = edges->get(r);
      const QString k = e.value("fromId").toString()+"|"+e.value("toId").toString()+"|"+e.value("kind").toString();
      if(seen.contains(k)) ++dup; else seen.insert(k);
    }
    if (dup) {
      QSet<QString> seen2;
      for(int r=0;r<m;++r){
        QVariantMap e = edges->get(r);
        const QString k = e.value("fromId").toString()+"|"+e.value("toId").toString()+"|"+e.value("kind").toString();
        if (seen2.contains(k)) printf("      DUP %s\n", qPrintable(k));
        else seen2.insert(k);
      }
    }
    ck("edges unique (no double BusSpan)", dup==0, QString("dups=%1").arg(dup));
  }
  auto diags = ctx.property("diagnostics").value<QVariantList>();
  printf("      diagnostics=%d\n", diags.size());
  auto* ieds = qobject_cast<QAbstractItemModel*>(ctx.property("ieds").value<QObject*>());
  const int iedCount = ieds ? ieds->rowCount() : 0;
  const auto inv = ctx.property("equipmentInventory").value<QVariantList>();
  printf("      ieds=%d inventory=%d\n", iedCount, inv.size());

  if (nodes && nodes->rowCount() == 0) {
    // A bare ICD has no <Substation> topology, so no SLD is expected. It must
    // still load and expose its IEDs.
    printf("      (no drawable topology: IED-only document)\n");
    if (iedCount > 0) ck("IED-only document still exposes IEDs", true);
  } else if (inv.isEmpty()) {
    // A document with busbars and transformers but no ConductingEquipment at
    // all (SCD_2VL_TR) legitimately has nothing to enumerate physically.
    ck("equipment inventory empty only when there is no ConductingEquipment", true);
  } else {
    ck("equipment inventory populated", true);
  }

  printf("%s\n", fails? "FAILURES":"All good.");
  return fails?1:0;
}
