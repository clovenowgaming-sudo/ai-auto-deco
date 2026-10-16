#pragma once
#include <Geode/Geode.hpp>
#include <set>
#include <sstream>

namespace cfg {
    inline std::string trim(std::string s) {
        auto ws = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!s.empty() && ws(s.front())) s.erase(s.begin());
        while (!s.empty() && ws(s.back())) s.pop_back();
        return s;
    }
    inline std::string get(char const* k, std::string const& d = "") {
        return geode::Mod::get()->getSavedValue<std::string>(k, d);
    }
    inline void set(char const* k, std::string const& v) {
        geode::Mod::get()->setSavedValue<std::string>(k, v);
    }
    inline bool ready() {
        return !trim(get("api-key")).empty() && !trim(get("model")).empty() && !trim(get("url")).empty();
    }
    // accepts "https://api.groq.com/openai/v1", ".../v3/" or full ".../chat/completions"
    inline std::string endpoint() {
        auto u = trim(get("url"));
        while (!u.empty() && u.back() == '/') u.pop_back();
        std::string suf = "/chat/completions";
        if (u.size() < suf.size() || u.compare(u.size() - suf.size(), suf.size(), suf) != 0) u += suf;
        return u;
    }
    inline std::set<int> parseIds(std::string const& s) {
        std::set<int> out;
        std::string cur;
        for (char c : s + ",") {
            if (std::isdigit(static_cast<unsigned char>(c))) cur += c;
            else { if (!cur.empty()) { try { out.insert(std::stoi(cur)); } catch (...) {} } cur.clear(); }
        }
        return out;
    }
}
