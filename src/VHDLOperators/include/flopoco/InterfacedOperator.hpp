#ifndef FLOPOCO_INTERFACED_OPERATORS
#define FLOPOCO_INTERFACED_OPERATORS

#include <string>
#include <type_traits>
#include <vector>

#include "flopoco/Operator.hpp"
#include "flopoco/Target.hpp"
#include "flopoco/UserInterface.hpp"

namespace flopoco {

struct OperatorDescription {
    const std::string Name;
    const std::string Descr;
    const std::string Category;
    const std::string SeeAlso;
    const std::string Parameters;
    const std::string ExtraHTMLDoc;
};

template<typename T>
OperatorFactory op_factory();

struct HasUnitTest {
private:
    // If Operator::unitTest(int) -> TestList, returns std::true_type
    template<typename Operator>
    static constexpr typename std::is_same<decltype(Operator::unitTest(0)), TestList>::type check(Operator*);

    // Default fallback case
    template<typename Operator>
    static constexpr std::false_type check(...);
public:
    template<typename Operator>
    static constexpr bool value = decltype(check<Operator>(nullptr))::value;
};

// short form
template<typename Operator>
static constexpr bool hasUnitTest_v = HasUnitTest::value<Operator>; 

template<typename T, bool hasUnitTest = hasUnitTest_v<T>>
struct InterfacedOperator {
    public:
    static OperatorPtr __parser(OperatorPtr parent, Target* target, std::vector<string> & params, UserInterface& ui) {
        return T::parseArguments(parent, target, params, ui);
    }
    static TestList __unit_test(int val) {
        if constexpr (hasUnitTest) {
            return T::unitTest(val);
        } else {
            return {};
        }
    }
};

template<typename T>
OperatorFactory factoryBuilder(OperatorDescription descriptor){
    return {
      descriptor.Name,
      descriptor.Descr,
      descriptor.Category,
      descriptor.SeeAlso,
      descriptor.Parameters,
      descriptor.ExtraHTMLDoc,
      InterfacedOperator<T>::__parser,
      InterfacedOperator<T>::__unit_test
    };
}
}

#endif