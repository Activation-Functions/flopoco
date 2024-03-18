TODO file for FloPoCo

# Version 5 versus ASA book

## List of commands in the ASA book

flopoco target=virtex6 XilinxTernaryAddSub wIn=16
flopoco target=virtex6 XilinxTernaryAddSub wIn=16 AddSubBitMask=3
flopoco IntAdder wIn=1024
flopoco IntComparator w=1000
flopoco IntConstantComparator flags=1 w=64 c=17979737894628297144
flopoco compression=heuristicMaxEff IntMultiAdder n=10 signedIn=0 wIn=10
flopoco compression=heuristicMaxEff tiling=optimal IntMultiplier wX=10 wY=10 useDSP=0
flopoco IntMultiplier wX=24 wY=24
flopoco IntMultiplier wX=24 wY=24 tiling=optimal maxDSP=1
flopoco IntMultiplier wX=53 wY=53 tiling=optimal maxDSP=8
flopoco IntMultiplier wX=32 wY=32 wOut=32
flopoco FPDiv wE=8 wF=23 srt=42
flopoco FPDiv wE=8 wF=23 srt=43
flopoco Shifter wX=10 maxShift=15 dir=1
flopoco Shifter wX=12 maxShift=12 wR=12 dir=1 computeSticky=1
flopoco Normalizer wX=14 wR=14 maxShift=14
flopoco FPAdd we=8 wf=23
flopoco IEEEFPAdd wE=8 wF=23
flopoco FPAdd we=8 wf=23 dualpath=true
flopoco FPMult we=8 wf=23
flopoco FPDiv we=8 wf=23
flopoco FPSqrt we=8 wf=23
flopoco FPComparator we=8 wf=23
flopoco IntConstMult method=minAdd wIn=10 constant=173
flopoco IntConstMult method=minAddTernary wIn=10 constant=173
flopoco FixFunctionByTable f=\"2/pi\*x\" signedIn=false lsbIn=-8 lsbOut=-24
flopoco IntConstDiv wIn=32 d=3
flopoco IntConstDiv arch=1 wIn=32 d=3
flopoco IntConstDiv computeQuotient=false wIn=32 d=3
flopoco IntConstDiv arch=1 computeQuotient=false wIn=32 d=3:5
flopoco IntSquarer win=8
flopoco FPConstDiv we=8 wf=23 d=3
flopoco FPConstDiv we=8 wf=23 d=5 dExp=1
flopoco FPConstDiv we=8 wf=23 d=3:3
flopoco FixFunctionByTable f=\"sin(pi/4\*x)\" signedIn=false lsbIn=-8 lsbOut=-8
flopoco FixFunctionByTable f=\"sin(pi/2\*x)\" signedIn=false lsbIn=-8 lsbOut=-8
flopoco FixFunctionByTable f=\"255/256\*sin(pi/2\*x)\" signedIn=false lsbIn=-8 lsbOut=-8
flopoco FixFunctionByTable f=\"2/(x+1)-1\" signedIn=false lsbIn=-8 lsbOut=-16
flopoco FixFunctionByTable f=\"x\*1b6\*x\*1b6-23\*floor(x\*1b6\*x\*1b6/23)\" signedIn=false lsbIn=-6 lsbOut=0
flopoco FixFunctionByTable tableCompression=1 f=\"sin(pi/4\*x)\" signedIn=false lsbIn=-8 lsbOut=-8
flopoco FixFunctionByMultipartiteTable nbTO=1 f=\"63/64\*sin(pi/2\*x)\" signedIn=false lsbIn=-6 lsbOut=-6
flopoco FixFunctionByMultipartiteTable f=\"(1b16-1)/1b16\*sin(pi/2\*x)\" signedIn=false lsbIn=-16 lsbOut=-16
flopoco FixFunctionBySimplePoly plainVHDL=true f=\"exp(x)\" signedIn=true lsbIn=-24 lsbOut=-24
flopoco FixFunctionBySimplePoly plainVHDL=true f=\"exp(x/16+3/16)\" signedIn=true lsbIn=-24 lsbOut=-24
flopoco FixFunctionBySimplePoly plainVHDL=true f=\"(1-1b-24)\*sin(pi/2\*x)\" signedIn=true lsbIn=-24 lsbOut=-24
flopoco FixFunctionByPiecewisePoly plainVHDL=true f=\"log(1+x)\" signedIn=false lsbIn=-24 lsbOut=-24 d=3
flopoco FPSqrt method=0 wE=8 wF=23
flopoco FPSqrt wE=8 wF=23
flopoco FixSinCos
flopoco FixSinOrCos
flopoco FixIIR lsbin=-8 lsbout=-8 coeffb=\"1:0:-1\" coeffa=\"-1.99510896:0.999985754\"
flopoco IEEEFPAdd wE=8 wF=23
flopoco FPAdd wE=8 wF=23 FPMult we=8 wf=23 FPDiv we=8 wf=23 FPSqrt we=8 wf=23
flopoco frequency=200 FPAdd wE=8 wF=23 FPMult we=8 wf=23 FPDiv we=8 wf=23 FPSqrt we=8 wf=23
flopoco IntConstDiv wIn=16 d=3 TestBench
flopoco FPAdd we=8 wf=23 TestBench n=100000
flopoco frequency=400 FPAdd wE=8 wF=23 RegisterSandwich


## problematic flopoco commands in the book 
### (->Martin) test takes ages to solve with LPSolve (but eventually succeeds)
time ./flopoco IntMultiplier wX=53 wY=53 tiling=optimal maxDSP=8 
with SCIP:      1m4,584s
with LPSolve:   5m44,218s

### (->Martin) test fails because I am lazy to compile Xilinx libs
./flopoco target=virtex6 XilinxTernaryAddSub wIn=16																			  TestBench 
./flopoco target=virtex6 XilinxTernaryAddSub wIn=16		 AddSubBitMask=3													  TestBench 
### (-> Florent) test fails but the bug is likely in emulate, TODO add unitTest to the Shifters
./flopoco Shifter wX=12 maxShift=12 wR=12	   dir=1 computeSticky=1														  TestBench 

### (->Martin) test passes but for the wrong reason
./flopoco IntConstMult method=minAdd wIn=10 constant=173																	  TestBench 
./flopoco IntConstMult method=minAddTernary wIn=10		constant=173														  TestBench 
Martin, there is no such methods in the CLI help:
method (string): desired method. Can be 'KCM', 'ShiftAdd', 'ShiftAddRPAG' or 'auto' (let FloPoCo decide which operator performs best)
## ToDo right now on FloPoCo (by priority order)
* Martin: IntConstMultShiftAddOpt, IntConstMultOptTernary -\> IntConstMult 
method=\"KCM,ShiftAdd\" addmethod=\"RPAG,minAdd,minAddILP,minAddSCM,minAddTernary,ternary,auto\" (optional, default: auto), addergraph=\"...\" constant=42, constant=\[42 13\], constant=\[42 13;21 19\], constant=\[42,12 13,19;21,34 19,34\]
* Florent: FixRealKCM -\> FixRealConstMult 
method=\"KCM\" (\"ShiftAdd\" in future) constant=\"pi\" msbIn, lsbIn, lsbOut
* Florent: add FloPoCo calls of the book to standard test cases of FloPoCo
* Florent: I must write a FixMultiplier wrapper. 
Tiling IntMult contradicts my section 1, update \"Truncated integer multipliers\" Hands on
* IntMultiplier: use global options useHardMult and hardMultThreshold instead of the local ones.
* IntMultiplier: pipeline
* isShared, isLibraryComponent
* revive FixMultAdd so that plainVHDL is no longer a mandatory option
* Other command line stuff
	* Add wiki style links in FloPoCo help (translated to \<a href=\"\"\> for website, printed URL in cmd line)
	* Move all 3rd strings to 2nd string (otherwise hidden in cmd line interface)
	* provide a flag to show the 3rd strings in cmd line interface (otherwise they only appear in the web)
* TODO from org to md
Here is the command I used for the ASA TODOs
pandoc --atx-headers -s TODOs.org -o TODOs.md* 

F2D: harmonize signedInput among the various FixFunctionBy
* resurrect DualPortROM
* pipeline in IntAdder broken: fix discrepancy between prediction and adderDelay()
* F2D: fix-point-in, FP out version of FPExp
* Update paper list in flopoco.bib

## pipelining with primitives
./flopoco dependencygraph=full  target=virtex6 XilinxTernaryAddSub wIn=16

# List of operators to port to the new pipeline framework

Remove an operator from the list below when it is done.
THIS INCLUDES WRITING ITS AUTOTEST

IntConstMult: still need to
1/ add the delays
2/ resurrect IntConstMultRational (parseArgument etc)
3 fix the mult by 7 in 2 additions

IntConstDiv: still need to
1/ add the delays
2/ fix the bug that duplicates the tables

//FOR TEST PURPOSES ONLY
Table

IntSquarer

FPRealKCM
FPAddSub
FPAdd3Input
FPMult
//FPMultKaratsuba
FPSquare

FPLargeAcc
LargeAccToFP
FPDotProduct

FixRootRaisedCosine
add FixSinc?

Complex/\* (lots of cleanup)

UserDefinedOperator

## ported but without autotest

Fix2FP
FP2Fix
FPConstMult
FPConstDiv

## [TODO]{.todo .TODO} some day {#some-day}

### Resurrect and fuse CordicAtan2 and FixAtan2

### Resurrect Cordic2DNorm (or whatever it is called)

### resurrect DualTable

### resurrect the low-latency adders

Problems:
1/ we need a performance test framework
2/ it is probably quite xilinx-only at the moment

### Check IntDualSub situation

### resurrect Guillaume\'s work (IntPower etc)

### FixSinCosPoly: use a signed reduced argument

### generic Hsiao compression could improve most table-based algorithms

### UniformPiecewisePolyApprox: we never try to give a0 more bits than the others.

### Class hierarchy for FixFunctionApprox

### FixSinCosPoly: use all the bit heaps that we advertize

### FixSinCosPoly: see comments in FixSinCos.cpp One optim for 24 bits would be to compute z² for free by a table using the second unused port of the blockram

# Small things in the pipeline framework

## constant signal scheduling TODO: check

The constant signals are currently all scheduled to cycle 0.
This is stupid: once the schedule is done (all the outputs of the top-level are scheduled).
they should be rescheduled ALAP (time zero, cyle= min of the successors)

## Signal::hasBeenScheduled\_

Apart from the previous issue with constant signals, it seems we should remove most accesses to Signal::hasBeenScheduled\_ that allow to re-schedule a signal:
it should be initialized to false, then set once to true forever.
A quick grep seems to show it is the case.

## Re-check table attributes for 7 series; update Table, probably refactor this into Target

## In the dot output, package shared components into dotted boxes (subgraph cluster\_)

It doesn\'t seem that simple

# Bugs to fix

## assertion failed in ./flopoco FixFunctionByTable f=\"1/(x+1)\" signedin=0 lsbin=-8 lsbout=-8 tablecompression=1

## ./flopoco FixFunctionByTable f=\"1/(x+1b-4+1)-.5-1b-6\" signedin=false lsbin=-4 lsbout=-5

has a constant 0 MSB

## constant 1000 bits in TestBench doesn\'t allow for parallel FFTs

## ./flopoco verbose=2 FixFunctionBySimplePoly plainvhdl=true f=\"sin(x)\" lsbIn=-16 msbOut=4 lsbout=-16 TestBench n=-2

The parity problem leads to wrong alignment.
Nobody should do this if they have read the Muller book, so... people will try this and it is a bug

## ./flopoco verbose=2 FixFunctionBySimplePoly plainvhdl=true f=\"exp(x)\" lsbIn=-16 msbOut=4 lsbout=-16 TestBench n=-2

## ./flopoco plainVHDL=1 FixFunctionByPiecewisePoly f=\"(2^x^-1)\" d=2 lsbIn=-1 lsbOut=-8 msbout=0 testbench

## (check, it is an old bug) compression bug: ./flopoco IntMultiplier 2 16 16 1 0 0 does not produce a simple adder

# Wanted operators
## FP sin cos tangent abs neg max min

## NormalCDF

... exists in the branch statistical~ops~, old framework.

## FloatApprox

... exists in the random branch

## all in the random branch

## HOTBM

## Sum of n squares

## LUT-based integer comparators

## BoxMuller


# Current regressions:

## FPPipeline

## lut~rng~


# Cleaning up

## Here and there, fix VHDL style issues needed for whimsical simulators or synthesizers. See doc/VHDLStyle.txt

## For Kentaro: avoid generating multiple times the same operators.

## Doxygenize while it\'s not too late

## clean up Target

# Targets

## [DONE]{.done .DONE} Xilinx series 7 {#xilinx-series-7}

## Altera 10

# Towards continuous integration

## [DONE]{.done .DONE} move to gitlab {#move-to-gitlab}

## autotest at commit

## set up a performance regression test as well

# Possible improvements, operator by operator

## Square root

-   table look ups for all the small sizes
-   initialization of the iteration by a table lookup to save the first 5-6 iterations
-   radix 4 version as in ASA book
-   low-latency version using HCRS radix 4
-   resurrect the multiplier-based version of Mioara and Bogdan

## Division

-   low-latency version using HCRS radix 4
-   integration of the multiplier-based version

## Collision

### manage infinities etc

### decompose into FPSumOf3Squares and Collision

## HOTBM
	
### true FloPoCoization, pipeline

### better (DSP-aware) architectural exploration

ConstMult:

## ConstMult

### group KCM and shift-and-add in a single OO hierearchy (selecting the one with less hardware)

### For FPConstMult, don\'t output the LSBs of the IntConstMult

but only their sticky

### more clever, Lefevre-inspired algorithm

### Use DSP: find the most interesting constant fitting on 18 bits

### compare with Spiral.net and Gustafsson papers

### Implement the continued fraction stuff for FPCRConstMult

## Shifters

### provide finer spec, see the TODOs inside Shifter.cpp

# Janitoring

### replace inPortMap and outPortMap by the modern interface newInstance()

See FPAddSinglePath for examples

### build a SNAP package <https://docs.snapcraft.io/build-snaps/>

### Add modern targets

### replace the big ifs in Target.cpp with method overloading in subtargets ?

### gradually convert everything to standard lib arithmetic, getting rid of the synopsis ones.

### TargetFactory

### rename pow2, intpow2 etc as exp2

### doxygen: exclude unplugged operators

### See table attributes above

### remove Operator::signalList, replace it with signalMap altogether

(this must be considered carefully, we have several lists)

### Replace pointers with smart pointers ?

# Bit heap and multipliers (old list, may be obsolete)

## [DONE]{.done .DONE} rewrite BitHeap with fixed-point support and better compression (see Kumm papers and uni~kassel~ branch) {#rewrite-bitheap-with-fixed-point-support-and-better-compression-see-kumm-papers-and-unikassel-branch}

## pipeline virtual IntMult

## See UGLYHACK in IntMultiplier

## IntSquarer should be made non-xilinx-specific, and bitheapized

## Same for IntKaratsuba and FPKaratsuba, which have been disabled completely

## Get rid of SignedIO in BitHeap: this is a multiplier concern, not a bit heap concern

## get rid of Operator::useNumericStd~Signed~ etc

## get rid of bitHeap::setSignedIO(signedIO);

## Check all these registered etc nonsense in Signal. Is it really used?

## Bug (ds FixRealKCM?) ./flopoco -verbose=3 FPExp 7 12

## With Matei: see the nextCycles in FPExp and see if we can push them in IntMultiplier somehow

# BitHeapization (old list, may be obsolete)

(and provide a bitheap-only constructor for all the following):

## systematic constructor interface with Signal variable

## Rework Guillaume Sergent\'s operators around the bit heap

## define a policy for enableSuperTile: default to false or true?

## Push this option to FPMult and other users of IntMult.

## Replace tiling exploration with cached/classical tilings

## More debogdanization: Get rid of

IntAddition/IntCompressorTree
IntAddition/NewIntCompressorTree
IntAddition/PopCount
after checking the new bit heap compression is at least as good...

## Check all the tests for \"Virtex4\" src/IntAddSubCmp and replace them with tests for the corresponding features

Testbench

# Framework (old list, may be obsolete)

## Bug on outputs that are bits with isBus false and multiple-valued

(see the P output of Collision in release 2.1.0)

## Multiple valued outputs should always be intervals, shouldn\'t they?

## global switch to ieee standard signed and unsigned libraries

## fix the default getCycleFromSignal .

# Options for signed/unsigned DONE, text should stay here while the janitoring isn\'t done

Option 0: Do nothing radical. It seems when the options
--ieee=standard --ieee=synopsys
are passed to ghdl in this order, we may mix standard and synopsys entities
See directory TestsSigned
Incrementally move towards option 1 (for new operators, and when needed on legacy ones)

Option 1:

-   Keep only std~logicvector~ as IO,
-   Add an option to declare() for signed / unsigned / std~logicvector~ DONE
    The default should still be std~logicvector~ because we don\'t want to edit all the existing operators
-   add conversions to the VHDL. DONE
-   No need to edit the TestBench architecture (DONE, actually some editing was needed)

Option 2 (out: see discussion below)
Same as Option 1, but allow signed/unsigned IOs

-   Need to edit the TestBench architecture
-   Cleaner but adds more coding. For instance, in Table, need to manage the types of IOs.
-   Too many operators have sign-agnostic information, e.g. Table and all its descendants

------------------------------------------------------------------------

Should we allow signed/unsigned IO?

-   Good reason for yes: it seems to be better (cleaner etc)
-   Good reason for no: many operators don\'t care (IntAdder, all the Tables)
    and we don\'t want to add noise to their interface if it brings no new functionality.
-   Bad reason for no: it is several man-days of redesign of the framework, especially TestBench
    Plus several man-weeks to manually upgrade all the existing operators

Winner: NO, we keep IOs as std~logicvector~.

Should the default lib be standard (currently synopsys)?
Good reason for yes: it is the way forward
Bad reasons for no: it requires minor editing of all existing operators
Winner: YES, but after the transition to sollya4 is complete and we have a satisfiying regression test framework.

# DONE

## [DONE]{.done .DONE} cleanup of the pipeline framework {#cleanup-of-the-pipeline-framework}

2 use cases from Kassel:

-   we want to call optimal bit-heap scheduling algorithms, which will not be ASAP.

```{=html}
<!-- -->
```
-   we want to generate optimal adder DAGs, also not ASAP.

In both case, we want to provide to these algorithms the schedule of all the inputs.
Typical case of the bit heap of a large multiplier: it adds

-   bits from its DSP blocks (arrive after 2 or 3 cycles)
-   bits from the logic-based multipliers (arrive at cycle 0 after a small delay)

Real-world bit heaps (e.g. sin/cos or exp or log) have even more complex, difficult to predict BH structures.
1 use case from Lyon: pipelined adders (should know the schedule of the inputs to

We want a robust solution that works for these use cases.
Current version 5 (hereafter refered to as Matei\'s code) is not efficient (it reschedules all the time) and overengineered WRT to these use cases.
Only drawback of the solution proposed below WRT Matei\'s code: it requires explicit calls to schedule() in some situations.
I consider this a good thing, it gives control.

Hypotheses:
H0: schedule is always called on the top-level operator.
Even an explicit call to schedule() in a sub-component will schedule its top-level
Beware: Wrapper and TestBench should not be parent operators of the operators they wrap, so as not to modify the schedule.
H1: default schedule will always be ASAP.
A call to schedule() does what it can, then stops.
H2: schedule() does not reschedule anything: if a signal is already scheduled, it is skipped.
H3: shared operators are exclusively sub-cycle LUT-like operators (use cases so far: compressors, LUT-based mults in IntMultiplier)
They define (possibly explicitely) the delay(s) between their input and output, but need not be scheduled.

Schedule is called implicitely after the constructor of the top-level operator.
It may be called explicity by some code, in particular bit heap compression.
This somehow constraints the order of writing operator constructor code, but it is OK.

The algo should be:

If a bit heap bh is involved, the constructor
1/ perform all the bh-\>addBit(),
2/ explicitely calls schedule(), which is supposed to schedule all the inputs
(this constrains constructor code order)
3/ calls generateCompressorVHDL(), which we delegate to Kassel.
Kassel compute their optimal architecture + schedule, and add it to the VHDL stream already scheduled
so that (thanks to H2) it will remain (and not be rescheduled ASAP)

For Martin:

-   Before generateCompressorVHDL is called, we will have the lexicographic timing
    (i.e. cycle + delay within a cycle) for all the bits that are input to the bit heap.
    We really want Martin\'s algos to manage that.

-   Martin\'s algorithms compute cycles + delays. Two options to exploit this information:
    a/ ignore the cycles, just have each signal declared with a delay in the compressor trees,
    and hope the ASAP scheduler re-computes cycles that will match those computed by Martin
    b/ let Martin directly hack the cycles and delays into the DAG -- probably much more code.
    I would vote for a/, but as Martin also minimizes registers, we should go for b/

To discuss.

-   The BitHeap should be simplified, all the timing information should be removed:
    it is now in the Signals (once they have been scheduled).
    So the actual interface to provide to Martin is not yet fixed.

## [DONE]{.done .DONE} Plan for bringup of the new pipeline framework {#plan-for-bringup-of-the-new-pipeline-framework}

## [DONE]{.done .DONE} Shifter for basic pipeline: DONE {#shifter-for-basic-pipeline-done}

## [DONE]{.done .DONE} IntAdder for explicit call to schedule(): DONE {#intadder-for-explicit-call-to-schedule-done}

## [DONE]{.done .DONE} FPAdd for simple subcomponents : DONE {#fpadd-for-simple-subcomponents-done}

## [DONE]{.done .DONE} FPDiv for low complexity shared subcomponents DONE {#fpdiv-for-low-complexity-shared-subcomponents-done}

## [DONE]{.done .DONE} FixRealKCM for simple bit heap DONE, {#fixrealkcm-for-simple-bit-heap-done}

## [DONE]{.done .DONE} FixSOPC DONE {#fixsopc-done}

## [DONE]{.done .DONE} FixFIR DONE {#fixfir-done}

## [DONE]{.done .DONE} FixIIR for large bit heaps + functional delays: DONE {#fixiir-for-large-bit-heaps-functional-delays-done}

## [DONE]{.done .DONE} IntMult DONE {#intmult-done}

## [DONE]{.done .DONE} FixFunctionByTable (check that Table does the delay properly in the blockram case) {#fixfunctionbytable-check-that-table-does-the-delay-properly-in-the-blockram-case}

## [DONE]{.done .DONE} ALAP rescheduling for constant signals {#alap-rescheduling-for-constant-signals}

## [DONE]{.done .DONE} FixSinCos for method=0 {#fixsincos-for-method0}

## [DONE]{.done .DONE} replace target-\>isPipelined() (and getTarget-\>isPipelined()) with isSequential() {#replace-target-ispipelined-and-gettarget-ispipelined-with-issequential}

Rationale: the two are redundant. isSequential is less prone to change during the life of an Operator...
isSequential is properly initialized out of isPipelined in the default Operator constructor.
DONE more or less in Operator

## [DONE]{.done .DONE} Check that ?? and \$\$ and \"port map\" in comments don\'t ruin the pipeline framework {#check-that-and-and-port-map-in-comments-dont-ruin-the-pipeline-framework}

## [DONE]{.done .DONE} get rid of rst signal {#get-rid-of-rst-signal}

Observation: no operator uses rst, except FixFIR and LargeAcc.
There is a good reason for that: it would prevent the inference of srl logic.

Now FixFir doesn\'t manage rst in emulate(), which is a framework limitation.
LargeAcc ignores rst. Instead it has an additional newDataSet input, which technically induces a synchronous reset
We should generalize this way of expressing reset information.
Benefit: it will remove rst from all the classical pipelined operators, and explicit it only when it is useful.

## [DONE]{.done .DONE} get rid of use() in Operator {#get-rid-of-use-in-operator}

## [DONE]{.done .DONE} Get rid of the useBitHeap arg in KCMs {#get-rid-of-the-usebitheap-arg-in-kcms}

## [DONE]{.done .DONE} bug ./flopoco FixSinCos -16 TestBenchFile 1000 {#bug-.flopoco-fixsincos--16-testbenchfile-1000}

(close corresponding bug when fixed)

## [DONE]{.done .DONE} change interface to FixSinCos and CordicSinCos to use lsb and not w {#change-interface-to-fixsincos-and-cordicsincos-to-use-lsb-and-not-w}

## [DONE]{.done .DONE} IntConstMult: signed or unsigned int? (fix main.cpp) {#intconstmult-signed-or-unsigned-int-fix-main.cpp}

## [DONE]{.done .DONE} rounding bug: ./flopoco FixRealKCM 1 3 -10 -10 \"Pi\" 1 TestBenchFile 1000 {#rounding-bug-.flopoco-fixrealkcm-1-3--10--10-pi-1-testbenchfile-1000}

(close corresponding bug when fixed)

## [DONE]{.done .DONE} interface: simple and expert versions of IntMultiplier {#interface-simple-and-expert-versions-of-intmultiplier}

## If we could start pipeline from scratch MOSTLY DONE

If we were to redo the pipeline framework from scratch, here is the proper way to do it.

The current situation has a history: we first added cycle management, then, as a refinement, critical-path based subcycle timing.
So we have to manage explicitely the two components of a lexicographic time (cycle and delay within a cycle)
But there is only one wallclock time, and the decomposition of this wallclock time into cycles and sub-cycles could be automatic. And should.

The following version of declare() could remove the need for manageCriticalPath as well as all the explicit synchronization methods.
declare(name, size, delay)
declares a signal, and associates its computation delay to it. This delay is what we currently pass to manageCriticalPath. Each signal now will have a delay associated to it (with a default of 0 for signals that do not add to the critical path).
The semantics is: this signal will not be assigned its value before the instant delta + max(instants of the RHS signals)
This is all what the first pass, the one that populates the vhdl stream, needs to do. No explicit synchronization management needed. No need to setCycle to \"come back in time\", etc.

Then we have a retiming procedure that must associate a cycle to each signal.
It will do both synchronization and cycle computation. According to Alain Darte there is an old retiming paper that shows that the retiming problem can be solved optimally in linear time for DAGs, which is not surprising.
Example of simple procedure:
first build the DAG of signals (all it takes is the same RHS parsing, looking for signal names, as we do)
Then sit on the existing scheduling literature...
For instance
1/ build the operator\'s critical path
2/ build the ASAP and ALAP instants for each signal
3/ progress from output to input, allocating a cycle to each signal, with ALAP scheduling (should minimize register count for compressing operators)
4/ possibly do a bit of Leiserson and Saxe retiming

We keep all the current advantages:

-   still VHDL printing based
-   When developing an operator, we initially leave all the deltas to zero to debug the combinatorial version. Then we incrementally add deltas, just like we currently add manageCriticalPath().
-   etc

The difference is that the semantic is now much clearer. No more notion of a block following a manageCriticalPath(), etc

The question is: don\'t we loose some control on the circuit with this approach, compared to what we currently do?

Note that all this is so much closer to textbook literature, with simple DAGs labelled by delay...

Questions and remarks:

-   what to do with setPipeline depth? Currently, it is set by hand, but the new framework allows for it to be computed automatically from the cycles of the circuit\'s outputs. What to do when the outputs are not synchronized?
-   should it be allowed to have delayed signals in a port map?
-   should the constant signals be actual signals?
-   how to handle instances:
    -   we should create a new class Instance, which contains a reference to the instanced Operator and a portMap for its inputs and outputs
    -   Operator should have a flag isGlobal
    -   Instance should have a flag isImplemented, signaling if the operator is on the global operator list and whether it has already been implemented, or not
    -   Operator has a list of the instances it creates
    -   Operator has a list of sub-operators
    -   Target has the global operator list
    -   when creating a new instance of a global operator
        -   if it is the first, then just add it to the global operator list, with the isImplemented flag to true
        -   if it is not the first, then clone the existing operator, connecting the clone\'s inputs/outputs to the right signals, and set the isImplemented flag to true
    -   the global operators exist in Target as well, and will be implemented there
    -   there should be no cycles in the graph
    -   all architectures are unrolled in the signal graph

    !- resource estimation during timing: we already have some information about the circuit\'s interal, so why not use this information for resource estimation, as well?
