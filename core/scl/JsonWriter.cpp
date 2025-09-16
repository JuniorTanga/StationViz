#include "JsonWriter.h"
#include <iomanip>
#include <sstream>

using namespace sld;

void JsonWriter::beginObject() {
    sep(); os_ << '{'; stack_.push_back(Ctx::Object); first_.push_back(true);
}
void JsonWriter::endObject() {
    os_ << '}';
    stack_.pop_back(); first_.pop_back();
}

void JsonWriter::beginArray() {
    sep(); os_ << '['; stack_.push_back(Ctx::Array); first_.push_back(true);
}
void JsonWriter::endArray() {
    os_ << ']';
    stack_.pop_back(); first_.pop_back();
}

void JsonWriter::key(const std::string& k) {
    // dans un objet : "key":
    if (!inObject()) return;              // option: assert
    sep();
    os_ << '\"'; writeEscaped(k); os_ << "\":";
    // après une clé, on NE bascule pas first_.back(), c’est la valeur qui décidera si la prochaine entrée a besoin d’une virgule
    // on triche en mettant first_.back() = true pour forcer le prochain sep() à ne pas mettre de virgule
    // puis on le passera à false après la value()
    // => plus simple: on ne touche pas ici; sep() s'occupe de la virgule uniquement quand on ouvre un nouvel élément
    // et c’est appeler `sep()` AVANT chaque élément qui garantit la virgule.
}

void JsonWriter::value(const std::string& s) {
    // valeur chaîne
    os_ << '\"'; writeEscaped(s); os_ << '\"';
    if (!first_.empty()) first_.back() = false;
}

void JsonWriter::value(const char* s) {
    value(std::string(s ? s : ""));
}

void JsonWriter::value(double d) {
    os_ << d;
    if (!first_.empty()) first_.back() = false;
}

void JsonWriter::value(int i) {
    os_ << i;
    if (!first_.empty()) first_.back() = false;
}

void JsonWriter::value(bool b) {
    os_ << (b ? "true" : "false");
    if (!first_.empty()) first_.back() = false;
}

void JsonWriter::null() {
    os_ << "null";
    if (!first_.empty()) first_.back() = false;
}

void JsonWriter::sep() {
    if (first_.empty()) return;           // top-level
    if (first_.back()) {                  // premier élément du niveau -> pas de virgule
        first_.back() = false;
    } else {
        os_ << ',';                       // éléments suivants -> virgule
    }
}

std::string JsonWriter::escape(const std::string& s) {
    std::ostringstream os;
    for (unsigned char c : s) {
        switch (c) {
        case '\"': os << "\\\""; break;
        case '\\': os << "\\\\"; break;
        case '\b': os << "\\b";  break;
        case '\f': os << "\\f";  break;
        case '\n': os << "\\n";  break;
        case '\r': os << "\\r";  break;
        case '\t': os << "\\t";  break;
        default:
            if (c < 0x20) { // contrôle ASCII
                os << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << int(c);
            } else {
                os << c;
            }
        }
    }
    return os.str();
}

void JsonWriter::writeEscaped(const std::string& s) {
    os_ << escape(s);
}
