# SAF repository for CMSB2023


## Very Quick Start

You can play with SAF using [the web application version of SAF](https://saf-app.herokuapp.com/).

## Quick Start requires only a Java runtime

The only requirement to run SAF is a Java Virtual Machine version 8
or greater.  You can check your Java Runtime version using the command `java -version`

In this case, the tool is using an embedded Java SAT solver, [Sat4j](http://www.sat4j.org/).

The following command will run SAF to find attractors of size less than or equal to 4. 

``` sh
java -jar saf.jar -k 4 example/toy-ex.an
```

It should return the following output:

``` sh
Using Sat4j SAT Solver

a,b,c,d
k: 1, #Var: 19, #Clause: 54
#1
0110
#2
0010
#3
0120
#4
0020
#5
1120
#6
1100
k: 2, #Var: 102, #Clause: 586
None
k: 3, #Var: 184, #Clause: 1395
None
k: 4, #Var: 292, #Clause: 2560
None
```

It means that there are 6 attractors of size 1 and no attractors of size 2, 3 and 4.

Thanks to [BioLQM](http://colomoto.org/biolqm/), SAF can accept various network formats (e.g., .an, .bnet, .booleannet, .sbml etc.)

``` sh
$ java -jar saf.jar -k 4 example/arellano_rootstem.bnet 
```

It should return the following output:

``` sh
Using Sat4j SAT Solver

AUXINS,SHR,ARF,IAA,JKD,MGP,SCR,WOX,PLT
k: 1, #Var: 30, #Clause: 102
#1
101000001
#2
111000001
#3
111011101
#4
111010111
k: 2, #Var: 180, #Clause: 1278
None
k: 3, #Var: 333, #Clause: 3172
None
k: 4, #Var: 537, #Clause: 5924
None
```

It means that there are 4 attractors of size 1 and no attractors of size 2, 3 and 4.

## Command line options

The usual `-h` or `--help` options provide a detailed usage of the tool.

```
$ java -jar saf.jar -h
Usage: java -jar saf.jar [options] [inputFile]
 -h         : show this help
 --help         : show this help
 -libname <Library Name>        : name of IPASIR Library
 -libpath <Library Path>        : directory where IPASIR Library is
 -k1solver <K1 Solver Path>        : path of the executable of SAT solver for k=1
 -k <INT>        : upper bound of k (default Int.MaxValue)
 -encode <cycle|symmetry|full>        : encoding option (default full)
```

Using the options `-libname`, `-libpath` or `-k1solver`, one can use state-of-the-art SAT/AllSAT solvers. Those options are detailed below.

## How to use state-of-the-art SAT solvers?

It is possible to use any SAT solver implementing the [IPASIR interface](https://github.com/biotomas/ipasir).

### Install CaDiCaL

[CaDiCaL](https://github.com/arminbiere/cadical) can be used when a very efficient SAT solver is required and a few attractors are expected. It is provided for convenience in this repository within the `cadical` directory.

To compile it on a Unix system, execute the following command:

``` sh
cd cadical
CXXFLAGS=-fPIC ./configure && make
```

The next execution command varies depending on OS (we consider Linux/macOS).

``` sh
(Linux)
g++ -g -O3 -I. -shared -o ./build/libcadical.so `ls build/*.o | grep -v mobical`

(macOS)
g++ -g -O3 -I. -dynamiclib -o ./build/libcadical.dylib `ls build/*.o | grep -v mobical`
```

Then,

- if it's Linux, you should be able to confirm the generation of `cadical/build/libcadical.so`, and 
- if it's Mac, you should be able to confirm the generation of `cadical/build/libcadical.dylib`.

which are used by SAF as an external solver on Linux or macOS respectively.

### Install BDD_MINISAT_ALL

[BDD_MINISAT_ALL](http://www.sd.is.uec.ac.jp/toda/code/cnf2obdd.html) can be used when the number of attractors is expected to be large. It is provided for convenience in this repository within the `bdd_minisat_all-1.0.2` directory.

To install it, execute the command below.

``` sh
cd bdd_minisat_all-1.0.2
make r
```

It will produce the file `bdd_minisat_all-1.0.2/bdd_minisat_all` which can be used by SAF as an external solver.

### Launching SAF by using both CaDiCaL and BDD_MINISAT_ALL

The following command will run SAF using both CaDiCaL and BDD_MINISAT_ALL.
BDD_MINISAT_ALL will be used to enumerate singleton attractors while CaDiCal will be used to compute the remaining attractors.

``` sh
$ java -jar saf.jar -k 4 -k1solver bdd_minisat_all-1.0.2/bdd_minisat_all_release -libname cadical -libpath cadical/build/ example/arellano_rootstem.bnet 
```

It will return the output 

``` sh
Using IPASIR SAT Solver and AllSAT Solver
Libpath: cadical/build/
Libname: cadical
K1solver: bdd_minisat_all-1.0.2/bdd_minisat_all_release

AUXINS,SHR,ARF,IAA,JKD,MGP,SCR,WOX,PLT
101000001
111000001
111010111
111011101
k: 2, #Var: 180, #Clause: 1278
None
k: 3, #Var: 333, #Clause: 3172
None
k: 4, #Var: 537, #Clause: 5924
None
```

## Building SAF from source

SAF is written in [Scala](https://www.scala-lang.org) and requires [sbt](https://www.scala-sbt.org) to manage its dependencies.

To build SAF from its source, just type

``` sh
$ sbt assembly
```

in the directory containing this repository.

The `saf.jar` file can be found in the directory `target/scala-2.12/`.

## Performance Evalution

- SAF's performance evaluation is given in [SAF-Evaluation](https://github.com/TakehideSoh/SAF-Evaluation).
