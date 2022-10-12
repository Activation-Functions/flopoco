#include <algorithm>
#include <cctype>
#include <cwctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/UserInterface.hpp"


namespace {
    inline std::string lowercase(std::string const & input) {
        std::string ret{input};
        std::transform(input.begin(), input.end(), ret.begin(), [](auto c){
            return std::tolower(c);
        });
        return ret;
    }
}

namespace flopoco {
    void FactoryRegistry::registerFactory(OperatorFactory& factory, bool isInternal) {
        registry.emplace_back(&factory);
        factoryIndex.emplace(lowercase(factory.name()), &factory);
        if (!isInternal) {
            publicRegistry.emplace(&factory);
        }
    }

    OperatorFactory const & FactoryRegistry::getFactoryByIndex(std::size_t i) const {
        return *registry[i];
    }

    OperatorFactory const * FactoryRegistry::getFactoryByName(std::string_view name) const {
        std::string key = lowercase(std::string{name});
        if (factoryIndex.count(key) > 0) {
            return factoryIndex.at(key);
        }
        return nullptr;   
    }

    OperatorFactory const * FactoryRegistry::getPublicFactoryByName(std::string_view name) const {
        auto ret = getFactoryByName(name);
        if (publicRegistry.count(ret)) return ret;
        return nullptr;
    }

    std::set<OperatorFactory const *> const & FactoryRegistry::getPublicRegistry() const {
        return publicRegistry;
    }

    std::unordered_map<std::string, OperatorFactory const *> const & FactoryRegistry::getFactoryIndex() const {
        return factoryIndex;
    }

    std::size_t FactoryRegistry::getFactoryCount() const {
        return registry.size();
    }

    FactoryRegistry& FactoryRegistry::getFactoryRegistry() {
        static FactoryRegistry reg{};
        return reg;
    }
}