#pragma once
#include "ILanguage.h"
#include "Polish.h"
#include "English.h"
#include <vector>
#include <memory>

class LanguageRegistry {
public:
	static LanguageRegistry& instance() {
		static LanguageRegistry reg;
		return reg;
	}

	const std::vector<std::unique_ptr<ILanguage>>& languages() const { return langs; }

	int currentIndex = 0;

	const ILanguage* current() const {
		if (currentIndex >= 0 && currentIndex < (int)langs.size())
			return langs[currentIndex].get();
		return langs[0].get();
	}

private:
	LanguageRegistry() {
		langs.push_back(std::make_unique<Polish>());
		langs.push_back(std::make_unique<English>());
	}

	std::vector<std::unique_ptr<ILanguage>> langs;
};

inline const char* tr(const char* id) {
	return LanguageRegistry::instance().current()->tr(id);
}
