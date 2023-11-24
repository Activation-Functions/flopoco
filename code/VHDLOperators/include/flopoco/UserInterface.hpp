/**
A generic user interface class that manages the command line and the documentation for various operators
Includes an operator factory inspired by David Thomas

For typical use, see src/ExpLog/FPExp.*

Author : David Thomas, Florent de Dinechin

Initial software.
Copyright © INSA-Lyon, ENS-Lyon, INRIA, CNRS, UCBL,
2015-.
  All rights reserved.

*/
#ifndef UserInterface_hpp
#define UserInterface_hpp

#include <cstdint> //for int64_t
#include <ostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// Operator Factory, based on the one by David Thomas, with a bit of clean up.
// For typical use, see src/ShiftersEtc/Shifter  or   src/FPAddSub/FPAdd*

namespace flopoco
{
	class Operator;
	class UserInterface;
	class FactoryRegistry;
	class Target;
	typedef Operator* OperatorPtr;
	typedef std::vector<std::vector<std::pair<std::string,std::string>>> TestList;
	typedef TestList (*unitTest_func_t)(int);

	using parser_func_t = OperatorPtr (*)(OperatorPtr, Target *, std::vector<std::string> &, UserInterface& ui);	//this defines parser_func_t as a pointer to a function taking as parameters Target* etc., and returning an OperatorPtr
	class OperatorFactory;
	typedef std::shared_ptr<OperatorFactory> OperatorFactoryPtr;

	typedef std::pair<std::string, std::vector<std::string>> option_t;


	/** This is the class that manages a list of OperatorFactories, and the overall command line and documentation.
			Each OperatorFactory is responsible for the command line and parsing for one Operator sub-class. */
	class UserInterface
	{
	public:

		/** The function that does it all */
		static void main(int argc, char* argv[]);
		static UserInterface& getUserInterface();
		std::string const & getExecName() const;

	private:
		UserInterface();
		
		/** parse all the operators passed on the command-line */
		void buildAll(int argc, char* argv[]);

		/** starts the dot diagram plotter on the operators */
		void drawDotDiagram(std::vector<OperatorPtr> &oplist);

		/** generates the code to the default file */
		void outputVHDL();

		public:
		////////////////// Helper parsing functions to be used in each Operator parser ///////////////////////////////
		void parseBoolean(std::vector<std::string> &args, std::string key, bool* variable, bool genericOption=false);
		void parseInt(std::vector<std::string> &args, std::string key, int* variable, bool genericOption=false);
		void parsePositiveInt(std::vector<std::string> &args, std::string key, int* variable, bool genericOption=false);
		void parseStrictlyPositiveInt(std::vector<std::string> &args, std::string key, int* variable, bool genericOption=false);
		void parseFloat(std::vector<std::string> &args, std::string key, double* variable, bool genericOption=false);
		void parseString(std::vector<std::string> &args, std::string key, std::string* variable, bool genericOption=false);
		void parseColonSeparatedStringList(std::vector<std::string> &args, std::string key, std::vector<std::string>* variable, bool genericOption=false);
		void parseColonSeparatedIntList(std::vector<std::string> &args, std::string key, std::vector<int>* variable, bool genericOption=false);

        /** generates a report for operators in globalOpList, and all their subcomponents */
        void finalReport(std::ostream & s);

		/** Provide a std::string with the full documentation.*/
		std::string getFullDoc();

		/** add an operator to the global (first-level) list.
				This method should be called by
				1/ the main / top-level, or
				2/ for "shared" sub-components that are really basic operators,
				expected to be used several times, *in a way that is independent of the context/timing*.
				Typical example is a table designed to fit in a LUT, such as compressors etc.
				Such shared components are identified by their name. 
				If several components want to add the same shared component, only the first is added.
				Subsequent attempts to add a shared component with the same name instead return a pointer to the one in globalOpList.
		*/
		OperatorPtr addToGlobalOpList(OperatorPtr op);

		/** Saves the current globalOpList on globalOpListStack, then clears globalOpList. 
				Typical use is when you want to build various variants of a subcomponent before chosing the best one.
				TODO: a variant that leaves in globalOpList a deep copy of the saved one, so that such comparisons can be done in context 
		*/
		void pushAndClearGlobalOpList();

		/** Restores the globalOpList from top of globalOpListStack. 
				Typical use is when you want to build various variants of a subcomponent before chosing the best one.
		*/
		void popGlobalOpList();

		/** generates the code for operators in globalOpList, and all their subcomponents */
		static void outputVHDLToFile(std::ofstream& file);

		/** generates the code for operators in oplist, and all their subcomponents */
		static void outputVHDLToFile(std::vector<OperatorPtr> &oplist, std::ofstream& file, std::set<std::string> &alreadyOutput);

		/** error reporting */
		void throwMissingArgError(std::string opname, std::string key);
		/** parse all the generic options such as name, target, verbose, etc. */
		void parseGenericOptions(std::vector<std::string>& args);

		/** Build operators.html directly into the doc directory. */
		void buildHTMLDoc();

		/** Build JSON files of FloPoCo operators and FloPoCo categories for the web Interface. Writes operators.json into the current directory. */
		void buildOperatorsJSON();
		void buildOperatorListJSON();

		/** Build flopoco bash autocompletion file **/
		void buildAutocomplete();

	public:
		std::vector<OperatorPtr>  globalOpList;  /**< Level-0 operators. Each of these can have sub-operators */
		std::vector<std::vector<OperatorPtr>>  globalOpListStack;  /**< a stack on which to save globalOpList when you don't want to mess with it */
		int pipelineActive_;
		bool allRegistersWithAsyncReset; // too lazy to write setters/getters
		void setOutputFileName(std::string name);

	private:
		std::string outputFileName;
		std::string entityName;
		std::string targetFPGA;
		std::string programName;
		FactoryRegistry const & factRegistry;
		double targetFrequencyMHz;
//		bool   pipeline; //not used at all, uncomment for now, remove this later!
		bool   clockEnable;
		bool   nameSignalByCycle;
		bool   useHardMult;
		bool   plainVHDL;
		bool   registerLargeTables;
		bool   tableCompression;
		bool   generateFigures;
		double unusedHardMultThreshold;
		bool   useTargetOptimizations;
		std::string compression;
		std::string tiling;
		std::string ilpSolver;
		int    ilpTimeout;
#if 0 // Shall we resurrect all this some day?
		static int    resourceEstimation;
		static bool   floorplanning;
		static bool   reDebug;
		static bool   flpDebug;
#endif
		static const std::vector<std::pair<std::string,std::string>> categories;

		static const std::vector<std::string> known_fpgas;
		static const std::vector<std::string> special_targets;
		static const std::vector<option_t> options;

		static std::string depGraphDrawing;
	};

	/** This is the abstract class that each operator factory will inherit.
			Each OperatorFactory is responsible for the command line and parsing for one Operator sub-class.  */
	class OperatorFactory
	{
		friend UserInterface;
	private:

		std::string m_name; /**< see constructor doc */
		std::string m_description;  /**< see constructor doc */
		std::string m_category;  /**< see constructor doc */
		std::string m_seeAlso; /**< see constructor doc */
		std::vector<std::string> m_paramNames;  /**< list of paramater names */
		std::map<std::string,std::string> m_paramType;  /**< type of parameters listed in m_paramNames */
		std::map<std::string,std::string> m_paramDoc;  /**< description of parameters listed in m_paramNames */
		std::map<std::string,std::string> m_paramDefault; /* If equal to "", this parameter is mandatory (no default). Otherwise, default value (as a std::string, to be parsed) */
		std::string m_extraHTMLDoc;
		parser_func_t m_parser;
		unitTest_func_t m_unitTest;

	public:

		/** Implements a no-frills factory
				\param name         Name for the operator.
				\param description  The short documentation
				\param category     A category used to organize the doc.
				\param parameters   A semicolon-separated list of parameter description, each being name(type)[=default]:short_description
				\param parser       A function that can parse a std::vector of std::string arguments into an Operator instance
				\param extraHTMLDoc Extra information to go to the HTML doc, for instance links to articles or details on the algorithms
		**/
		OperatorFactory(
						 std::string name,
						 std::string description,
						 std::string category,
						 std::string seeAlso,
						 std::string parameters,
						 std::string extraHTMLDoc,
						 parser_func_t parser,
						 unitTest_func_t unitTest	);

		virtual const std::string &name() const // You can see in this prototype that it was not written by Florent
		{ return m_name; }

		/** Provide a std::string with the full documentation. */
		std::string getFullDoc() const;
		/** Provide a std::string with the full documentation in HTML. */
		std::string getHTMLDoc() const;
		/** Provide a std::string with the full JSON description. */
		std::string getJSONDescription() const;

		const std::vector<std::string> &param_names(void) const;

		std::string getOperatorFunctions(void) const;

		/** get the default value associated to a parameter (empty std::string if there is no default)*/
		std::string getDefaultParamVal(const std::string& key) const;

		/*! Consumes zero or more std::string arguments, and creates an operator
			The parentOp may be nullptr for top-level entities
			\param args The offered arguments start at index 0 of the std::vector, and it is up to the
			factory to check the types and whether there are enough.
			\param consumed On exit, the factory indicates how many of the arguments are used up.
		*/
		virtual OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, std::vector<std::string> &args, UserInterface& ui) const;
		
		/*! Generate a list of arg-value pairs out of the index value
		*/
		virtual TestList unitTestGenerator(int index) const;

	};
}; // namespace flopoco

#endif

