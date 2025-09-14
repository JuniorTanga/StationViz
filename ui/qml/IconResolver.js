.pragma library
function forNode(kind, state) {
    // kind: int (NodeKind enum from C++), state: string ("opened","closed","intermediate"...)
    switch(kind) {
    case 0: return "qrc:/icons/busbar.svg"
    case 1: return "qrc:/icons/bay.svg"
    case 2: return "qrc:/icons/cbr_" + (state||"closed") + ".svg"
    case 3: return "qrc:/icons/ds_" + (state||"opened") + ".svg"
    case 4: return "qrc:/icons/transformer_2w.svg"
    case 5: return "qrc:/icons/label.svg"
    default: return "qrc:/icons/unknown.svg"
    }
}
