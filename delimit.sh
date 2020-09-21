#/bin/bash

# bash delimit.sh peano.mm

# Using "#( and )#" as delimiters rather than "( and )" because )" is present
# in set.mm
sed -e '1 i\R"#(' -e '$a)#"' "$1" > "$1.raw" # Prepend R"#(  Append )#"
