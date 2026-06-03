#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <iostream>

#include "MapperBase.h"

// ============================================================================
// MapperRegistry
// ----------------------------------------------------------------------------
// Singleton mapujacy iNES mapper ID -> factory tworzaca instancje konkretnej
// klasy Mapper. Kazdy plik MapperNNN.h rejestruje swoja klase makrem
// REGISTER_MAPPER(id, klasa), dzieki czemu Cartridge nie musi znac listy
// mapperow ani recznie kodowac switcha.
//
// UWAGA: rejestracja odbywa sie w czasie inicjalizacji statycznych obiektow
// (przed main). Wymaga to, by jednostka kompilacji zawierajaca makro byla
// faktycznie linkowana do binarki. Poniewaz wszystkie MapperNNN.h sa header-
// only i wlaczane przez AllMappers.h dolaczany do Cartridge.h (a Cartridge
// jest uzywany w binarce glownej), kompilator wygeneruje pojedynczy obiekt
// statyczny per mapper w jednostce, ktora go widzi.
// ============================================================================
class MapperRegistry {
public:
	using Factory = std::unique_ptr<Mapper>(*)(uint8_t prg, uint8_t chr);

	static MapperRegistry& instance() {
		static MapperRegistry r;
		return r;
	}

	void registerMapper(uint16_t id, Factory f) {
		// Pozwalamy nadpisac (np. mapper 213 == alias 058) - ostatnia rejestracja wygrywa.
		factories[id] = f;
	}

	std::unique_ptr<Mapper> create(uint16_t id, uint8_t prg, uint8_t chr) const {
		auto it = factories.find(id);
		if (it == factories.end()) return nullptr;
		return it->second(prg, chr);
	}

	bool isRegistered(uint16_t id) const {
		return factories.find(id) != factories.end();
	}

private:
	std::unordered_map<uint16_t, Factory> factories;
};

// Inline helper - opakowany w funkcje, by dac mu pewne miejsce na utworzenie
// inline static (gwarantowanie pojedynczego storage dla naglowka).
namespace mapper_registry_detail {
	template <typename T>
	inline std::unique_ptr<Mapper> makeMapper(uint8_t prg, uint8_t chr) {
		return std::unique_ptr<Mapper>(new T(prg, chr));
	}

	struct AutoRegister {
		AutoRegister(uint16_t id, MapperRegistry::Factory f) {
			MapperRegistry::instance().registerMapper(id, f);
		}
	};
}

// Makro do umieszczenia w MapperNNN.h NA KONCU pliku, w przestrzeni globalnej.
// Wykorzystuje inline zmienna w namespace (C++17) - kompilator gwarantuje
// dokladnie jeden storage per program, niezaleznie od liczby TU includujacych
// naglowek. Konstruktor AutoRegister rejestruje mapper w singletonie zanim
// ruszy main().
#define REGISTER_MAPPER(ID, CLASS)                                              \
	namespace mapper_registry_detail {                                          \
		inline AutoRegister CLASS##_auto_register{                              \
			(ID), &mapper_registry_detail::makeMapper<CLASS>                    \
		};                                                                      \
	}

// Wariant dla aliasow (np. mapper 213 == 058). SUFFIX musi byc unikalny w
// obrebie danej klasy, np. REGISTER_MAPPER_AS(213, Mapper058, alias213).
#define REGISTER_MAPPER_AS(ID, CLASS, SUFFIX)                                   \
	namespace mapper_registry_detail {                                          \
		inline AutoRegister CLASS##_##SUFFIX##_auto_register{                   \
			(ID), &mapper_registry_detail::makeMapper<CLASS>                    \
		};                                                                      \
	}
