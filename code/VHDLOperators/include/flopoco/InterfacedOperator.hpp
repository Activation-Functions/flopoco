#ifndef FLOPOCO_INTERFACED_OPERATORS
#define FLOPOCO_INTERFACED_OPERATORS

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "flopoco/Operator.hpp"
#include "flopoco/Target.hpp"
#include "flopoco/UserInterface.hpp"

namespace flopoco {
	/**
	 * @brief Handles the global registration of factories
	 *
	 */
	struct FactoryRegistry {
		/**
		 * @brief register a factory that will be passed to
		 * UserInterface when initialising it
		 *
		 * @param factory the factory to register
		 * @param internalOnly Can the corresponding operator be created as the top level operator
		 */
		void registerFactory(OperatorFactory &factory, bool internalOnly);

		OperatorFactory const & getFactoryByIndex(std::size_t i) const;
		OperatorFactory const * getFactoryByName(std::string_view name) const;
		OperatorFactory const * getPublicFactoryByName(std::string_view name) const;
		std::unordered_map<std::string, OperatorFactory const *> const & getFactoryIndex() const;
		std::set<OperatorFactory const *> const & getPublicRegistry() const;
		std::size_t getFactoryCount() const;


		static FactoryRegistry& getFactoryRegistry();
		private:
		std::vector<OperatorFactory const *> registry;
		std::set<OperatorFactory const*> publicRegistry;
		std::unordered_map<std::string, OperatorFactory const *> factoryIndex;
		FactoryRegistry() = default;
	};

	/**
	 * @brief Helper class that is used to determine if a class has a static
	 * method with signature TestList unitTest(int)
	 *
	 */
	struct HasUnitTest {
	private:
		// These two methods used SFINAE to determine if
		// OpType::unitTest(int) exists and returns a TestList.

		// If OpType::unitTest(int) esists and returns TestList, this
		// method is generated
		template <typename OpType>
		static constexpr
		    typename std::is_same<decltype(OpType::unitTest(0)),
					  TestList>::type
		    check(OpType *);

		// It returns std::true_type iff the return type of the call is
		// TestList std::false_type otherwise

		// This method will be the "default case" that will be
		// instantiated if the first one is invalid
		//
		// Its argument lists is "less specialized" than the one above
		// (because of the ellipsis in the argument list, it is less
		// specialized than a method which input type is fully
		// specified).
		//
		// So if the two templates are valid (if the method exists), the
		// first one will be instantiated, else this one will be.
		//
		// It always returns false_type.
		template <typename OpType>
		static constexpr std::false_type check(...);

	public:
		// A template boolean value that will only be true if
		// check(OpType*) returns a std::true_type.
		// So will hold true iff OpType has the method unitTest(int)
		template <typename OpType>
		static constexpr bool value =
		    decltype(check<OpType>(nullptr))::value;

		// === Side note for interested people ===
		// we don't need to define the check functions here : indeed
		// what is of interest is the type of the result, which is
		// available from the signature and nobody will actually call
		// these functions.
	};

	// hasUnitTest_v<OpType> holds true iff OpType::unitTest(int) exists and
	// returns a TestList
	template <typename OpType>
	static constexpr bool hasUnitTest_v = HasUnitTest::value<OpType>;

    // This class stores information about the operator and registers the associated factory in the global Factory register
	template <typename T> struct OperatorDescription {
		const std::string Name;
		const std::string Descr;
		const std::string Category;
		const std::string SeeAlso;
		const std::string Parameters;
		const std::string ExtraHTMLDoc;

		/**
		 * @brief Construct a new Operator Description object, generates
		 * the coresponding factory and record it.
		 *
		 * @param name Name of the operator
		 * @param descr Description of the operator
		 * @param category Category under which to store it
		 * @param seeAlso name of the related operators
		 * @param parameters Structured list of parameters and their
		 * meaning. See TutorialOperator for more information
		 * @param extraHTMLDoc Extra doc to print in the web
		 * documentation of the operator.
		 */
		OperatorDescription(const std::string &name,
				    const std::string &descr,
				    const std::string &category,
				    const std::string &seeAlso,
				    const std::string &parameters,
				    const std::string &extraHTMLDoc, 
					bool isInternalOperator = false)
		    : Name{name}, Descr{descr}, Category{category},
		      SeeAlso{seeAlso}, Parameters{parameters},
		      ExtraHTMLDoc{extraHTMLDoc}, factory{name, descr, category, seeAlso, parameters, extraHTMLDoc, T::parseArguments, nullptr}
		{
		    if constexpr (hasUnitTest_v<T>) {
			    factory = OperatorFactory{name, descr, category, seeAlso, parameters, extraHTMLDoc, T::parseArguments, T::unitTest};
		    }
			FactoryRegistry::getFactoryRegistry().registerFactory(factory, isInternalOperator);
		}
		private:
			OperatorFactory factory;
	};

    /// Template variable that needs to be instantiated for each operator 
	template <typename T> const OperatorDescription<T> op_descriptor;
}

#endif