#pragma once
#include <string>
#include <algorithm>

namespace sld {

struct SldId {
  static std::string CN(const std::string& ss, const std::string& vl,
                        const std::string& bay, const std::string& cn) {
    return "CN:" + ss + "/" + vl + "/" + bay + "/" + cn;
  }
  static std::string CE(const std::string& ss, const std::string& vl,
                        const std::string& bay, const std::string& ce) {
    return "CE:" + ss + "/" + vl + "/" + bay + "/" + ce;
  }
  static std::string TR(const std::string& ss, const std::string& tr) {
    return "TR:" + ss + "/" + tr;
  }
  static std::string BUS(const std::string& ss, const std::string& vl, int k) {
    return "BUS:" + ss + "/" + vl + "/cluster#" + std::to_string(k);
  }
  static std::string keyVL(const std::string& ss, const std::string& vl) {
    return ss + ":" + vl;
  }
  // match suffixe CN: CN:SS/VL/BAY/CONNECTIVITY_NODE42  ~  "CONNECTIVITY_NODE42"
  static bool matchCNSuffix(const std::string& a, const std::string& b) {
    auto tail = [](const std::string& s){
      auto p = s.find_last_of('/');
      return (p==std::string::npos) ? s : s.substr(p+1);
    };
    return tail(a) == tail(b);
  }
};

} // namespace sld
