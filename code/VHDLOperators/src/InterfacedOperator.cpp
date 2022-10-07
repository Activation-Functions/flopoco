#include <vector>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/UserInterface.hpp"

namespace {
    std::vector<OperatorFactory>& getFactoryVec() {
        static std::vector<OperatorFactory> factVec{};
        return factVec;
    }
}

namespace flopoco {
    void FactoryRegistrator::registerFactory(OperatorFactory&& factory) {
        getFactoryVec().emplace_back(std::forward<OperatorFactory>(factory));
    }

    void FactoryRegistrator::delegateRegisteredFactories(UserInterface &ui) {
        for (auto& fac : getFactoryVec()) {
            ui.registerFactory(fac);
        }
    }
}