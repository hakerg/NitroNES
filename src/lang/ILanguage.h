#pragma once

class ILanguage {
public:
    virtual ~ILanguage() = default;
    virtual const char *getName() const = 0;
    virtual const char *getCode() const = 0;
    virtual const char *tr(const char *id) const = 0;
};
