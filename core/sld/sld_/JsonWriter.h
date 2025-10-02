// =============================================================
// File: JsonWriter.h
// Minimalist JSON Writer (header-only)
// StationViz project
// =============================================================
#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <locale>   // <-- ajoute cet include
#include <cstdio>

class JsonWriter {
public:
    JsonWriter() {
        ss_.imbue(std::locale::classic());   // <-- forcer locale "C" (point décimal)
        ss_ << std::boolalpha;
    }

    // Démarrage / fin de document
    JsonWriter& beginObject() { startValue(); ss_ << "{"; stack_.push_back(Context::Object); first_.push_back(true); return *this; }
    JsonWriter& endObject()  { if (stack_.empty() || stack_.back()!=Context::Object) throw std::logic_error("Mismatched endObject"); ss_ << "}"; stack_.pop_back(); first_.pop_back(); return *this; }

    JsonWriter& beginArray() { startValue(); ss_ << "["; stack_.push_back(Context::Array); first_.push_back(true); return *this; }
    JsonWriter& endArray()   { if (stack_.empty() || stack_.back()!=Context::Array) throw std::logic_error("Mismatched endArray"); ss_ << "]"; stack_.pop_back(); first_.pop_back(); return *this; }

    // Clé pour un objet
    JsonWriter& key(const std::string& k) {
        if (stack_.empty() || stack_.back()!=Context::Object) throw std::logic_error("Key outside object");
        if (!first_.back()) ss_ << ","; else first_.back() = false;
        ss_ << "\"" << escape(k) << "\":";
        return *this;
    }

    // Valeurs
    JsonWriter& value(const std::string& v) { startValue(); ss_ << "\"" << escape(v) << "\""; return *this; }
    JsonWriter& value(const char* v)        { return value(std::string(v)); }
    JsonWriter& value(double d)             { startValue(); ss_ << d; return *this; }
    JsonWriter& value(int i)                { startValue(); ss_ << i; return *this; }
    JsonWriter& value(bool b)               { startValue(); ss_ << (b?"true":"false"); return *this; }
    JsonWriter& nullValue()                 { startValue(); ss_ << "null"; return *this; }

    // Récupérer le JSON complet
    std::string str() const { return ss_.str(); }

private:
    enum class Context { Object, Array };

    std::ostringstream ss_;
    std::vector<Context> stack_;
    std::vector<bool> first_;

    void startValue() {
        if (!stack_.empty() && stack_.back()==Context::Array) {
            if (!first_.back()) ss_ << ","; else first_.back() = false;
        }
    }

    static std::string escape(const std::string& s) {
        std::string out; out.reserve(s.size()+8);
        for (unsigned char uc : s) {          // <-- itérer en unsigned char
            switch(uc) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (uc < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(uc));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(uc));
                }
            }
        }
        return out;
    }
};
