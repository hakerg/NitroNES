#pragma once
#include <string>

class ILanguage {
public:
	virtual ~ILanguage() = default;
	virtual const char* getName() const = 0;
	virtual const char* tr(const char* id) const = 0;
};
