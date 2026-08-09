#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "MapperBase.h"

class MapperRegistry {
public:
    using Factory = std::unique_ptr<Mapper> (*)(uint16_t prg, uint8_t chr);

    static MapperRegistry &instance() {
        static MapperRegistry r;
        return r;
    }

    void registerMapper(uint16_t id, Factory f, size_t size) {
        factories[id] = f;
        sizes[id] = size;
    }

    std::unique_ptr<Mapper> create(uint16_t id, uint16_t prg,
                                   uint8_t chr) const {
        auto it = factories.find(id);
        if (it == factories.end())
            return nullptr;
        return it->second(prg, chr);
    }

    bool isRegistered(uint16_t id) const {
        return factories.find(id) != factories.end();
    }

    size_t mapperSize(uint16_t id) const {
        auto it = sizes.find(id);
        return it != sizes.end() ? it->second : 0;
    }

private:
    std::unordered_map<uint16_t, Factory> factories;
    std::unordered_map<uint16_t, size_t> sizes;
};

namespace mapper_registry_detail {
template <typename T>
std::unique_ptr<Mapper> makeMapper(uint16_t prg, uint8_t chr) {
    return std::unique_ptr<Mapper>(new T(prg, chr));
}

struct AutoRegister {
    AutoRegister(uint16_t id, MapperRegistry::Factory f, size_t size) {
        MapperRegistry::instance().registerMapper(id, f, size);
    }
};
} // namespace mapper_registry_detail

#define REGISTER_MAPPER(ID, CLASS)                                             \
    namespace mapper_registry_detail {                                         \
    inline AutoRegister CLASS##_auto_register{                                 \
        (ID), &mapper_registry_detail::makeMapper<CLASS>, sizeof(CLASS)};      \
    }

#define REGISTER_MAPPER_AS(ID, CLASS, SUFFIX)                                  \
    namespace mapper_registry_detail {                                         \
    inline AutoRegister CLASS##_##SUFFIX##_auto_register{                      \
        (ID), &mapper_registry_detail::makeMapper<CLASS>, sizeof(CLASS)};      \
    }
