# Bitvector cs-tulip - A Handy Guide

This Bitvector implementation cs-tulip builds on the works of Sebastiano Vigna, 
Simon Gog and Florian Kurpicz. Its name is a play on the way more professional implementation cs-poppy.

This Bitvector fully supports access, rank and select queries for Bitvectors smaller than 2^64 bit.
After this limit, features begin to fail at various sizes, until select caching falls apart after more than 2^88 bit.
It must be compiled using the C++20 or newer language standard.

---

## Compiling and Running cs-tulip

cs-tulip requires the C++20 Standard to work, as it's making use of the std::popcount function, which is only introduced in C++20.
To compile cs-tulip, run the following command to compile it using g++:
#### ```g++ -std=c++20 [OPTIONS] -O3 main.cpp bitvector.cpp -o <filename>```
This command must be executed in the same folder as main.cpp, bitvector.h and bitvector.cpp.
The -O3 flag is for optimization and should be used, the -D[argument] flag is optional and defines a flag, all flag arguments are explained below.

On macOS and possibly other unix-based operating systems, you might need to make the file executable using ```chmod +x <filename>```. After that,
the program can be run using ```./<filename> <path to input file> <output file>```.

### CMake

The Project also comes with a CMake Configuration that can be used to build the executable from the Command line or an IDE like CLion.
Depending on your configuration, you might need additional DLLs to run it from the command line when built this way.

### Optional Compiler Flags
- **EVAL**: Adds a second timer to measure query execution time only. The result is printed at the end in an extra line in nanoseconds. Example: ```EVAL query-only-time=1500```, where this means that the time for just performing the queries was 1500 nanoseconds.
- **CONSOLE**: Prints the answers to the console instead. In this case, no output file will be created and the file argument will be ignored.

Compiler flags must be set with -D[NAME], like -DCONSOLE. -D[NAME] defines the variable NAME, which is queried in the code with #ifdef NAME.
An example with all flags set: ```g++ -std=c++20 -DEVAL -DCONSOLE -O3 main.cpp bitvector.cpp -o cs-tulip-debug```

## Usage and File Input

The Bitvector takes exactly one additional command line argument, which is the filepath to an input file. The program
will attempt to read the file provided. If no argument is given or the file path is not valid, the program will terminate
immediately with an error message. The file contents are not checked for validity.

The first line must contain exactly one number *n*, the amount of queries, with ```0 < n < 2^64```. The second line
contains the bitvector, a long string of ones and zeros, the length of which should also not exceed 2^64. Then follow
at least *n* lines consisting of exactly one query each. All queries and content after will be ignored.

## Time and Space Measuring

Execution time of the bitvector is measured from before the construction of assisting data structures until after
all queries were processed. After the timer has stopped, all calculated query answers are printed in the correct
order. After that, an evaluation line is printed containing the time in milliseconds and space usage information
in bits of the bitvector.
This does include static overhead of the bitvector data structure, but not overhead like the command structs.

### Evaluation

If the Flag EVAL is set in the compiler arguments, a second timer will measure only the time of query execution, and
not the construction time of the bitvector assistant data structures. In addition to the RESULT print post-queries,
an additional line is printed containing the time of all queries combined in nanoseconds.

## Processing Queries

All queries are processed inside the measured execution time. The answer is stored and written to the output file at the end to avoid
time delays due to File IO. This means that if an error occurs and the program crashes, it will not make any
writes or even create the output file.

## Output

The answers are printed per-line to the output file. If the file already exists, it is overridden. The result and evaluation statements
are printed to the console only and not to the file.

## Return codes

The return codes display for what reason the program terminated. This section covers only the custom return codes,
not the standard C++ return codes for errors like BAD ACCESS. In own cases, an error message is usually provided
alongside the error code.

- EXIT CODE 0: Success, no errors.
- EXIT CODE 1: No input file specified in first command line argument.
- EXIT CODE 2: No output file specified in second command line argument and the flag console was not set.
- EXIT CODE 3: Could not open input file. Maybe invalid filepath?
- EXIT CODE 4: Could not create or access output file. Maybe invalid filepath or lacking permission?
- EXIT CODE 5: Could not find or create the directory of the output target file.

## Additional Notes and Input Validation

The file contents and queries are assumed to be correct. There is little to no validation performed on the given input values.
Incorrect input values may lead to bugs, crashes, or other undefined behavior.
Please do ensure the correctness of your data, the query count, the bitvector length, and the query arguments.

Contents and indices of the queries are also not validated. If the query ```select 0 1022``` is performed on the bitvector
```1000100010```, this will likely result in a crash, but this falls within undefined behavior.

For creation and testing of input files, you can use external tools such as [JustOneDeveloper's Bitvector Checker](https://github.com/Just1Developer/BitvectorChecker/).