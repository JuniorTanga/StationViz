#include <QCoreApplication>
#include <QDebug>
#include "core/scl/sclManager.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    SclManager manager;
    if (manager.loadSclFile("teste.scd")) {
        manager.printIeds();
        manager.printSubstations();
        manager.buildBayGraphs();
        manager.printBays();
    }
    return 0;
}
