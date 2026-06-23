#pragma once
#include "English.h"
#include "ILanguage.h"
#include "Polish.h"
#include <memory>
#include <vector>

class LanguageRegistry {
public:
    static LanguageRegistry &instance() {
        static LanguageRegistry reg;
        return reg;
    }

    const std::vector<std::unique_ptr<ILanguage>> &languages() const {
        return langs;
    }

    void bindIndex(int *external) {
        indexPtr = external ? external : &fallbackIndex;
    }

    int &currentIndex() { return *indexPtr; }

    const ILanguage *current() const {
        int i = *indexPtr;
        if (i >= 0 && i < (int)langs.size())
            return langs[i].get();
        return langs[0].get();
    }

private:
    LanguageRegistry() {
        langs.push_back(std::make_unique<Polish>());
        langs.push_back(std::make_unique<English>());
    }

    std::vector<std::unique_ptr<ILanguage>> langs;
    int fallbackIndex = 0;
    int *indexPtr = &fallbackIndex;
};

inline const char *tr(const char *id) {
    return LanguageRegistry::instance().current()->tr(id);
}
