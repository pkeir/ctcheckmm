A C++23 version of [Eric Schmidt's Metamath database
verifier](http://us.metamath.org/downloads/checkmm.cpp) which can run at
compile-time using the `constexpr-std-headers` branch of GCC
found [here](https://github.com/SCT4SP/gcc/tree/constexpr-std-headers);
and referred to as C'est 2.

## Differences to the Original

1. Added the `constexpr` qualifier to all subroutines;
2. Changed all functions, to member functions (methods) of a trivial struct (called `checkmm`). Global variables (including constexpr ones) cannot be modified at compile-time;
3. Changed the static variable `names` within `readtokens` to a class member of `checkmm`. While `constexpr` static variables are now permitted in `constexpr` functions, they are constant, and `names` needs to be modified;
4. `constexpr` file IO is not possible, and `readtokens` now accepts a second string parameter, which is used if it isn't empty. An std::istringstream (from C'est 2) is then used to feed `nexttoken`, rather than an std::ifstream. P2448 from C++23 also allows non-constexpr file IO, when not on the control path taken by the constexpr evaluator;
5. File-includes within mm database files are not supported when processing at compile-time. An exception is thrown if this occurs;
**rem** 6. Headers included are from the cest library, so `#include "cest/vector.hpp"` rather than `#include <vector>` etc.
**rem** 7. Rather than replace names qualified `std::` with `cest::`, a namespace alias `ns` is used throughout; declared at global scope, and set to `cest` by default.
8. A macro system is leveraged, where a pair of C++11-style raw string literal delimiters are placed before and after the original contents of a file. The `delimit.sh` bash script is provided to help with this. To use this approach, set the preprocessor macro `MMFILEPATH` to the script's output, during compilation of `ctcheckmm-std.cpp` (e.g. `-DMMFILEPATH=miu.mm.raw`).

## In Practise

The `delimit.sh` script adds `R"#(` before, and `)#"` after, the
contents of its input file. The output file name appends `.raw` to the name of
the input. Within `ctcheckmm-std.cpp` a `#include` directive will bring that
(raw string) file in; as a `std::string`. This happens if `MMFILEPATH` is set
to a valid file path. For example, with a recent version of clang++ (e.g. Clang
18):

```
wget https://raw.githubusercontent.com/metamath/set.mm/develop/peano.mm
bash delimit.sh peano.mm
clang++ -std=c++23 -Winvalid-constexpr -Wl,-rpath,"$CEST2_ROOT/lib64:$LD_LIBRARY_PATH" -I $CEST2_ROOT/constexpr-std-headers/include/c++/14.0.1 -I $CEST2_ROOT/constexpr-std-headers/include/c++/14.0.1/x86_64-pc-linux-gnu -L $CEST2_ROOT/lib64 -D_GLIBCXX_CEST_CONSTEXPR=constexpr -D_GLIBCXX_CEST_VERSION=1 -fsanitize=address -fconstexpr-steps=$((2**32-1)) -DMMFILEPATH=peano.mm.raw ctcheckmm-std.cpp
```

With G++, a recent version is required (e.g. GCC 14), and different switches
enables a non-default `constexpr` operation count limit:

```
wget https://raw.githubusercontent.com/metamath/set.mm/develop/peano.mm
bash delimit.sh peano.mm
g++ -std=c++23 -Winvalid-constexpr -Wl,-rpath,"$CEST2_ROOT/lib64:$LD_LIBRARY_PATH" -I $CEST2_ROOT/constexpr-std-headers/include/c++/14.0.1 -I $CEST2_ROOT/constexpr-std-headers/include/c++/14.0.1/x86_64-pc-linux-gnu -L $CEST2_ROOT/lib64 -D_GLIBCXX_CEST_CONSTEXPR=constexpr -D_GLIBCXX_CEST_VERSION=1 -fsanitize=address -static-libasan -fconstexpr-ops-limit=$((2**31-1)) -fconstexpr-loop-limit=$((2**31-1)) -DMMFILEPATH=peano.mm.raw ctcheckmm-std.cpp
```

Successful compilation (with either compiler) indicates that the Metamath
database was verified. The `wget` commands above relate to the
[](https://github.com/metamath/set.mm) repository.
