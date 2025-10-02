

    // ---- Prints SCL
    std::cout << "==================== SCL ====================\n";
    sclMgr.printSubstations();
    sclMgr.printIEDs();
    sclMgr.printCommunication();
    sclMgr.printTopology();
    sclMgr.printEquipmentFromIEDs();
    printDiagnostics(sclMgr);

    // ---- Construire SLD
    SldManager sldMgr(&sclMgr, cfg);
    st = sldMgr.build();
    if (!st) {
        std::cerr << "[SLD] Echec build: " << st.error().message << "\n";
        return 3;
    }

    // ---- Prints SLD
    std::cout << "==================== SLD ====================\n";
    sldMgr.printStats();

    // ---- Dumps JSON (facultatifs)
    if (opt.count("--out-raw"))   writeFile(opt["--out-raw"],   sldMgr.rawJson());
    if (opt.count("--out-cond"))  writeFile(opt["--out-cond"],  sldMgr.condensedJson());
    if (opt.count("--out-plan"))  writeFile(opt["--out-plan"],  sldMgr.planJson());

    std::cout << "[OK] Terminé.\n";
    return 0;

