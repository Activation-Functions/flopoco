#ifndef FLOPOCO_INTERFACED_OPERATORS
#define FLOPOCO_INTERFACED_OPERATORS

#include <string>
#include <type_traits>
#include <vector>

#include "flopoco/Operator.hpp"
#include "flopoco/Target.hpp"
#include "flopoco/UserInterface.hpp"

namespace flopoco {

struct FactoryRegistrator {
    static void registerFactory(OperatorFactory&& factory);
    static void delegateRegisteredFactories(UserInterface& ui);
    struct RecordInserter {
        RecordInserter(OperatorFactory&& factory);
    };
};

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

template<typename T> struct Registrator {
    Registrator() {
        (void) recorder;
    }
    private:
    static const FactoryRegistrator::RecordInserter recorder;
};

template<typename T>
struct OperatorDescription {
    const std::string Name;
    const std::string Descr;
    const std::string Category;
    const std::string SeeAlso;
    const std::string Parameters;
    const std::string ExtraHTMLDoc;
    OperatorDescription(const std::string &name, const std::string &descr,
			const std::string &category, const std::string &seeAlso,
			const std::string &parameters,
			const std::string &extraHTMLDoc)
	: Name{name}, Descr{descr}, Category{category}, SeeAlso{seeAlso},
	  Parameters{parameters}, ExtraHTMLDoc{extraHTMLDoc}, registrator{}
    {
        (void) registrator;
    }
    private:
    Registrator<T> registrator;
};

template<typename T>
OperatorDescription<T> op_descriptor;

template<typename T>
OperatorFactory factoryBuilder(){
    auto& descriptor = op_descriptor<T>;
    unitTest_func_t testfunc = nullptr;
    if constexpr (hasUnitTest_v<T>) {
        testfunc = T::unitTest;
    }
    return {
      descriptor.Name,
      descriptor.Descr,
      descriptor.Category,
      descriptor.SeeAlso,
      descriptor.Parameters,
      descriptor.ExtraHTMLDoc,
      T::parseArguments,
      testfunc
    };
}

template <typename T>
const FactoryRegistrator::RecordInserter Registrator<T>::recorder{factoryBuilder<T>()};
}

#endif