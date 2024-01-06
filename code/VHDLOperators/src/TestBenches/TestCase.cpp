#include "flopoco/TestBenches/TestCase.hpp"
#include "flopoco/Operator.hpp"

namespace flopoco{


	TestCaseList::TestCaseList() { }
	TestCaseList::~TestCaseList() { }


	void TestCaseList::add(TestCase* tc){
		v.push_back(tc);
		tc->setId(v.size()-1); // id is the index in this vector
	}

	void TestCaseList::add(TestCaseList* tcl){
		int nbTestcases = tcl->getNumberOfTestCases();

		for(int i=0; i<nbTestcases; i++){
			add(tcl->getTestCase(i));
		}
	}

	int TestCaseList::getNumberOfTestCases(){
		return v.size();
	}

	TestCase* TestCaseList::getTestCase(int i){
		return v[i];
	}



	/*
	  A test case is a mapping between I/O signal names and boolean values given as mpz.

	  The signal names must be those of Operator->iolist_. 

	  The emulate() function of Operator takes a partial test case (mapping
	  all the inputs) and completes it by mapping the outputs.

	*/


	TestCase::TestCase(Operator* op) : op_(op){
	}

	TestCase::~TestCase() {
	}


	void TestCase::addInput(string name, mpz_class v)
	{
		Signal* s = op_->getSignalByName(name);
		if (v >= (mpz_class(1) << s->width()))
			throw string("ERROR in TestCase::addInput, signal value out of range (" + v.get_str() + " is larger or equal than 2^" + to_string(s->width()) + ")");
		if (v<0) {
			if (v < - (mpz_class(1) << s->width()))
				throw string("ERROR in TestCase::addInput, negative signal value out of range");
			v += (mpz_class(1) << s->width());
		}
		inputs[name] = v;
	}


	void TestCase::addFPInput(string name, FPNumber::SpecialValue v) {
		// get signal size
		Signal* s = op_->getSignalByName(name);
		if(!s->isFP()) {
			throw string("TestCase::addFPInput: Cannot convert FPNumber::SpecialValue to non-FP signal");
		}
		int wE=s->wE();
		int wF=s->wF();
		FPNumber  fpx(wE, wF,v);
		mpz_class mpx = fpx.getSignalValue();
		inputs[name] = mpx;
	}


	void TestCase::addFPInput(string name, double x) {
		// get signal size
		Signal* s = op_->getSignalByName(name);
		if(!s->isFP()) {
			throw string("TestCase::addFPInput: Cannot convert a double into non-FP signal");
		}
		int wE=s->wE();
		int wF=s->wF();
		FPNumber  fpx(wE, wF);
		fpx=x;
		mpz_class mpx = fpx.getSignalValue();

		inputs[name] = mpx;
	}

	void TestCase::addFPInput(string name, FPNumber *x) {
		Signal* s = op_->getSignalByName(name);
		if(!s->isFP()) {
			throw string("TestCase::addFPInput: Cannot convert a FPNumber into non-FP signal");
		}
		int wE, wF;
		x->getPrecision(wE, wF);
		if((s->wE() != wE) || (s->wF() !=wF)) {
			throw string("TestCase::addFPInput(string, FPNumber): size of provided FPNumber does not match");
		}
		mpz_class mpx = x->getSignalValue();

		inputs[name] = mpx;
	}

	void TestCase::addIEEEInput(string name, IEEENumber::SpecialValue v) {
		// get signal size
		Signal* s = op_->getSignalByName(name);
		if(!s->isIEEE()) {
			throw string("TestCase::addIEEEInput: Cannot convert IEEENumber::SpecialValue to non-FP signal");
		}
		int wE=s->wE();
		int wF=s->wF();
		IEEENumber  fpx(wE, wF,v);
		mpz_class mpx = fpx.getSignalValue();
		inputs[name] = mpx;
	}


	void TestCase::addIEEEInput(string name, double x) {
		// get signal size
		Signal* s = op_->getSignalByName(name);
		if(!s->isIEEE()) {
			throw string("TestCase::addIEEEInput: Cannot convert a double into non-FP signal");
		}
		int wE=s->wE();
		int wF=s->wF();
		IEEENumber  fpx(wE, wF, x);
		mpz_class mpx = fpx.getSignalValue();

		inputs[name] = mpx;
	}


	void TestCase::addIEEEInput(string name, IEEENumber in) {
		// get signal size
		Signal* s = op_->getSignalByName(name);
		if(!s->isIEEE()) {
			throw string("TestCase::addIEEEInput: Cannot convert a double into non-FP signal");
		}
		mpz_class mpx = in.getSignalValue();
		inputs[name] = mpx;
	}


	mpz_class TestCase::getInputValue(string s){
		return inputs[s];
	}

	void TestCase::setInputValue(string s, mpz_class v){
		inputs[s]=v;
	}

	void TestCase::addExpectedOutput(string name, mpz_class v)
	{
		Signal* s = op_->getSignalByName(name);
		if (s->isSigned()) {
			// sanity checks
			mpz_class minval = - (mpz_class(1) << (s->width()-1)) ;
			mpz_class maxval =  (mpz_class(1) << (s->width()-1)) -1 ;
			if (v < minval || v>maxval){
				ostringstream e;
				e << "ERROR in TestCase::addExpectedOutput, negative value " << v << " of signal " << name << " out of range " << minval << " .. " << maxval;
				throw e.str();
			}
			// now we may perform the two's complement
			if (v<0) {
				v += (mpz_class(1) << s->width());
			}
		}
		else { // s is unsigned; This covers the FP and IEEE types as well.
			// sanity checks
			mpz_class maxval =  (mpz_class(1) << (s->width())) -1 ;
			if (v<0 || v >maxval){
				ostringstream e;
				e << "ERROR in TestCase::addExpectedOutput, value " << v << " of signal " << name << " out of range 0 .. " << maxval;
				throw e.str();
			}
		}
		outputType[name] = list_of_values;
		outputs[name].push_back(v);
	}


	// TODO 1/ OutputType should be removed, replaced by info read from s itself
	// 2/ implement FP and IEEE
	void TestCase::addExpectedOutputInterval(std::string name, mpz_class vinf, mpz_class vsup, OutputType type){
		
		Signal* s = op_->getSignalByName(name);
		if (type == unsigned_interval) {
			if (vinf<0 || vinf >= (mpz_class(1) << s->width())){
				ostringstream e;
				e << "ERROR in TestCase::addExpectedOutputInterval, value " << vinf << " of signal " << name << " out of range 0 .. " << (mpz_class(1) << s->width())-1;
				throw e.str();
			}
			if (vsup<0 || vsup >= (mpz_class(1) << s->width())){
				ostringstream e;
				e << "ERROR in TestCase::addExpectedOutputInterval, value " << vsup << " of signal " << name << " out of range 0 .. " << (mpz_class(1) << s->width())-1;
				throw e.str();
			}
		}
		else if (type == signed_interval) {
			mpz_class bound = mpz_class(1) << (s->width()-1);
			if ( (vinf < -bound)  || (vinf >= bound)  || (vsup < -bound)  || (vsup >= bound) ){
				ostringstream e;
				e << "ERROR in TestCase::addExpectedOutputInterval, interval [" << vinf << ", " << vsup <<"] of signal "
					<< name << " out of range [" << -bound << ", " << bound-1 << "]" << (mpz_class(1) << s->width())-1;
				throw e.str();
			}
		}	
		else if (type == IEEE_interval) {
			ostringstream e;
			e << "TestCase::addExpectedOutputInterval, IEEE_interval not yet supported" ;
			throw e.str();	
		}
		else if (type == floating_point_interval) {
			ostringstream e;
			e << "TestCase::addExpectedOutputInterval, floating_point_interval not yet supported" ;
			throw e.str();	
		}
		else {
			ostringstream e;
			e << "ERROR in TestCase::addExpectedOutputInterval: Type is not an interval type" << name ;
			throw e.str();	
		}
		outputs[name].push_back(vinf);
		outputs[name].push_back(vsup);
		outputType[name] = type;
	}

	vector<mpz_class> TestCase::getExpectedOutputValues(string s) {
		return outputs[s]; // return all possible output values as a vector of mpz_class
	}


	string TestCase::getInputVHDL(string prepend)
	{
		ostringstream o;

		/* Iterate through input signals */
		for (map<string, mpz_class>::iterator it = inputs.begin(); it != inputs.end(); it++)
			{
				string signame = it->first;
				Signal* s = op_->getSignalByName(signame);
				mpz_class v = it->second;
				o << prepend;
				o << signame << " <= " << s->valueToVHDL(v) << "; ";
				o << endl;
			}

		return o.str();
	}


	std::string TestCase::testFileString(list<string> inputSignalNames, list<string> outputSignalNames) {
		ostringstream o;
		// A line of comments that will be ignored
		//It would be nicer to have X=01001 etc but I am not clever enough to parse that in VHDL.
		o << "##### Test #" << getId();
		if(getComment()!="") {
			o << " (" << getComment() << ") ";
		}
		o << ": ";
		for (auto x: inputSignalNames) {
			o << " " << x;
		}
		o << " =" << endl << "  ";
		// now the input vector. 
		for (auto x: inputSignalNames) {
			Signal* s = op_->getSignalByName(x);
			mpz_class v = inputs[x];
			o << s->valueToVHDL(v,false) << " ";
		}
		o << endl;
		// Another line of comments that will be ignored
		o << "#  Expected outputs for ";
		for (auto x: outputSignalNames) {
			o << " " << x;			
		}
		o << endl;
		for (auto x: outputSignalNames) {
			o << " ";
			Signal* s = op_->getSignalByName(x);
			OutputType type = outputType[x];
			vector<mpz_class> vs = outputs[x];
			if(type==list_of_values) {
				o << vs.size() << " ";
				for (auto v: vs)
					{
						o << s->valueToVHDL(v,false) << " ";
					}
			}
			else if(type==unsigned_interval || type==signed_interval || type==IEEE_interval || type==floating_point_interval) {
				o << type << " ";
				for (auto v: vs)
					{
						o << s->valueToVHDL(v,false) << " ";
					}
			}
		}
		o << endl;
		return o.str();	
	}

	


	void TestCase::addComment(string c) {
		comment = c;
	}

	string TestCase::getComment() {
		return comment;
	}

	void TestCase::setId(int id) {
		intId = id;
	}
	
	int TestCase::getId() {
		return intId;
	}

	string TestCase::getDescription() {
		ostringstream msg;
		msg << "Test Case number : " << getId() << std::endl;
		msg << getComment();
		msg << std::endl;
		return msg.str();
	}
}

