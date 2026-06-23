#pragma once
#include "English.h"
#include "ILanguage.h"
#include "Polish.h"
#include <map>
#include <memory>
#include <string>

class LanguageRegistry {
public:
    using Map = std::map<std::string, std::unique_ptr<ILanguage>>;

    static LanguageRegistry &instance() {
        static LanguageRegistry reg;
        return reg;
    }

    const Map &languages() const { return langs; }

    const ILanguage *current() const {
        auto it = langs.find(*currentCode);
        return it != langs.end() ? it->second.get()
                                 : langs.begin()->second.get();
    }

    const std::string &getCurrentCode() const { return *currentCode; }

    void setCode(const std::string &code) {
        if (langs.count(code))
            *currentCode = code;
    }

    void bindStorage(std::string *external) {
        currentCode = external ? external : &fallback;
    }

private:
    LanguageRegistry() {
        langs.emplace("en", std::make_unique<English>());
        langs.emplace("pl", std::make_unique<Polish>());
    }

    Map langs;
    std::string fallback = "en";
    std::string *currentCode = &fallback;
};

inline const char *tr(const char *id) {
    return LanguageRegistry::instance().current()->tr(id);
}
