A C++20 version of [Eric Schmidt's Metamath database
verifier](http://us.metamath.org/downloads/checkmm.cpp) which can run at
compile-time using the [C'est](https://github.com/pkeir/cest) library.

## Differences to the Original

1. Added the `constexpr` qualifier to all subroutines;
2. Changed all functions, to member functions (methods) of a trivial struct (called `checkmm`). Global variables (including constexpr ones) cannot be modified at compile-time;
3. Changed the static variable `names` within `readtokens` to a class member of `checkmm`. While `constexpr` static variables are now permitted in `constexpr` functions, they are constant, and `names` needs to be modified;
4. `constexpr` file IO is not possible, and `readtokens` now accepts a second string parameter, which is used if it isn't empty. An std::istringstream (from C'est 2) is then used to feed `nexttoken`, rather than an std::ifstream. P2448 from C++23 also allows non-constexpr file IO, when not on the control path taken by the constexpr evaluator;
5. File-includes within mm database files are not supported when processing at compile-time. An exception is thrown if this occurs;
**rem** 6. Headers included are from the cest library, so `#include "cest/vector.hpp"` rather than `#include <vector>` etc.
**rem** 7. Rather than replace names qualified `std::` with `cest::`, a namespace alias `ns` is used throughout; declared at global scope, and set to `cest` by default.
8. A macro system is leveraged, where a pair of C++11-style raw string literal delimiters are placed before and after the original contents of a file. The `delimit.sh` bash script is provided to help with this. To use this approach, set the preprocessor macro `MMFILEPATH` to the script's output, during compilation of `ctcheckmm.cpp` (e.g. `-DMMFILEPATH=miu.mm.raw`).

## In Practise

The `delimit.sh` script adds `R"#(` before, and `)#"` after, the
contents of its input file. The output file name appends `.raw` to the name of
the input. Within `ctcheckmm.cpp` a `#include` directive will bring that (raw
string) file in; as a `cest::string`. This happens if `MMFILEPATH` is set to a
valid file path. For example:

```
wget https://raw.githubusercontent.com/metamath/set.mm/develop/peano.mm
bash delimit.sh peano.mm
clang++ -std=c++20 -fconstexpr-steps=2147483647 -I $CEST_INCLUDE ctcheckmm.cpp -DMMFILEPATH=peano.mm.raw
```

With GCC, a recent version is required (e.g. GCC 12), and a different switch
enables a non-default `constexpr` operation count limit:

```
wget https://raw.githubusercontent.com/metamath/set.mm/develop/peano.mm
bash delimit.sh peano.mm
sudo apt-get install g++-12
g++-12 -std=c++20 -fconstexpr-ops-limit=2147483647 -fconstexpr-loop-limit=2147483647 -I $CEST_INCLUDE ctcheckmm.cpp -DMMFILEPATH=peano.mm.raw
```

Successful compilation (with either compiler) indicates that the Metamath
database was verified. The `wget` commands above relate to the
[](https://github.com/metamath/set.mm) repository.
