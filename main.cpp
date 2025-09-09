#include <QCoreApplication>
#include <QDebug>
#include "core/scl/sclManager.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    SclManager manager;
    if (manager.loadSclFile("ied.scd")) {
        manager.printIeds();
        manager.printSubstations();
    }
    return 0;
}
