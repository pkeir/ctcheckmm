// Compile-time Metamath database verifier
// Paul Keir, University of the West of Scotland
//
// This code is released to the public domain under the
// Creative Commons "CC0 1.0 Universal" Public Domain Dedication:
//
// http://creativecommons.org/publicdomain/zero/1.0/
//
// This is a C++20 standalone verifier for Metamath database files based on
// Eric Schmidt's original version (available at
// http://us.metamath.org/downloads/checkmm.cpp).

// To verify a database at runtime, the program may be run with a single file
// path as the parameter. In addition, to verify a database at compile-time,
// compile the program with MMFILEPATH defined as the path to a file containing
// a Metamath database encoded as a C++11 style raw string literal. The
// trivial delimit.sh bash script is provided to help convert database files to
// this format. The C'est library is at https://github.com/pkeir/cest

// wget https://raw.githubusercontent.com/metamath/set.mm/develop/peano.mm
// bash delimit.sh peano.mm

// clang++ -std=c++2a -I $CEST_INCLUDE ctcheckmm.cpp -fconstexpr-steps=2147483647 -DMMFILEPATH=peano.mm.raw

// or...
// sudo apt-get install g++-12
// g++-12 -std=c++20 -I $CEST_INCLUDE -fconstexpr-ops-limit=2147483647 -fconstexpr-loop-limit=2147483647 ctcheckmm.cpp -DMMFILEPATH=peano.mm.raw

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include <utility>

namespace ns = std;

struct checkmm
{

ns::queue<ns::string> tokens;

ns::set<ns::string> constants;

typedef ns::vector<ns::string> Expression;

// The first parameter is the statement of the hypothesis, the second is
// true iff the hypothesis is floating.
typedef ns::pair<Expression, bool> Hypothesis;

ns::map<ns::string, Hypothesis> hypotheses;

ns::set<ns::string> variables;

// An axiom or a theorem.
struct Assertion
{
    // Hypotheses of this axiom or theorem.
    ns::deque<ns::string> hypotheses;
    ns::set<ns::pair<ns::string, ns::string> > disjvars;
    // Statement of axiom or theorem.
    Expression expression;
};

ns::map<ns::string, Assertion> assertions;

struct Scope
{
    ns::set<ns::string> activevariables;
    // Labels of active hypotheses
    ns::vector<ns::string> activehyp;
    ns::vector<ns::set<ns::string> > disjvars;
    // Map from variable to label of active floating hypothesis
    ns::map<ns::string, ns::string> floatinghyp;
};

ns::vector<Scope> scopes;

// Determine if a string is used as a label
inline constexpr bool labelused(ns::string const label)
{
    return hypotheses.find(label) != hypotheses.end()
        || assertions.find(label) != assertions.end();
}

// Find active floating hypothesis corresponding to variable, or empty string
// if there isn't one.
constexpr ns::string getfloatinghyp(ns::string const var)
{
    for (ns::vector<Scope>::const_iterator iter(scopes.begin());
         iter != scopes.end(); ++iter)
    {
        ns::map<ns::string, ns::string>::const_iterator const loc
            (iter->floatinghyp.find(var));
        if (loc != iter->floatinghyp.end())
            return loc->second;
    }

    return ns::string();
}

// Determine if a string is an active variable.
constexpr bool isactivevariable(ns::string const str)
{
    for (ns::vector<Scope>::const_iterator iter(scopes.begin());
         iter != scopes.end(); ++iter)
    {
        if (iter->activevariables.find(str) != iter->activevariables.end())
            return true;
    }
    return false;
}

// Determine if a string is the label of an active hypothesis.
constexpr bool isactivehyp(ns::string const str)
{
    for (ns::vector<Scope>::const_iterator iter(scopes.begin());
         iter != scopes.end(); ++iter)
    {
        if (ns::find(iter->activehyp.begin(), iter->activehyp.end(), str)
            != iter->activehyp.end())
            return true;
    }
    return false;
}

// Determine if there is an active disjoint variable restriction on
// two different variables.
constexpr bool isdvr(ns::string var1, ns::string var2)
{
    if (var1 == var2)
        return false;
    for (ns::vector<Scope>::const_iterator iter(scopes.begin());
         iter != scopes.end(); ++iter)
    {
        for (ns::vector<ns::set<ns::string> >::const_iterator iter2
            (iter->disjvars.begin()); iter2 != iter->disjvars.end(); ++iter2)
        {
            if (   iter2->find(var1) != iter2->end()
                && iter2->find(var2) != iter2->end())
                return true;
        }
    }
    return false;
}

// Determine if a character is white space in Metamath.
inline constexpr bool ismmws(char const ch)
{
    // This doesn't include \v ("vertical tab"), as the spec omits it.
    return ch == ' ' || ch == '\n' || ch == '\t' || ch == '\f' || ch == '\r';
}

// Determine if a token is a label token.
constexpr bool islabeltoken(ns::string const token)
{
    for (ns::string::const_iterator iter(token.begin()); iter != token.end();
         ++iter)
    {
        unsigned char const ch(*iter);
        if (!(ns::isalnum(ch) || ch == '.' || ch == '-' || ch == '_'))
            return false;
    }
    return true;
}

// Determine if a token is a math symbol token.
inline constexpr bool ismathsymboltoken(ns::string const token)
{
    return token.find('$') == ns::string::npos;
}

// Determine if a token consists solely of upper-case letters or question marks
constexpr bool containsonlyupperorq(ns::string token)
{
    for (ns::string::const_iterator iter(token.begin()); iter != token.end();
         ++iter)
    {
        if (!ns::isupper(*iter) && *iter != '?')
            return false;
    }
    return true;
}

constexpr ns::string nexttoken(ns::istream & input)
{
    char ch;
    ns::string token;

    // Skip whitespace
    while (input.get(ch) && ismmws(ch)) { }
    if (input.good())
        input.unget();

    // Get token
    while (input.get(ch) && !ismmws(ch))
    {
        if (ch < '!' || ch > '~')
        {
            ns::cerr << "Invalid character read with code 0x";
            ns::cerr << ns::hex << (unsigned int)(unsigned char)ch
                      << ns::endl;
            return ns::string();
        }

        token += ch;
    }

    if (!input.eof() && input.fail())
        return ns::string();

    return token;
}

//   http://eel.is/c++draft/dcl.constexpr#3.5
// The definition of a constexpr function shall satisfy the following
// requirements: 
//  - its function-body shall not enclose ([stmt.pre]) 
//    ...
//    - a definition of a variable of non-literal type or of static or
//      thread storage duration.
ns::set<ns::string> names;

constexpr bool readtokens(ns::string filename, ns::string const &text = "")
{
    //static ns::set<ns::string> names;

    bool const alreadyencountered(!names.insert(filename).second);
    if (alreadyencountered)
        return true;

    ns::istringstream in;

    if (!text.empty())
    {
        in.str(text);
    }
    else
    {
        bool okay = [&]() // lambda: non-literal values in dead constexpr paths
        {
            ns::ifstream file(filename.c_str());
            if (!file)
                return false;
            ns::string str((ns::istreambuf_iterator(file)), {});
            in.str(str);
            return true;
        }();
        if (!okay)
        {
            ns::cerr << "Could not open " << filename << ns::endl;
            return false;
        }
    }

    bool incomment(false);
    bool infileinclusion(false);
    ns::string newfilename;

    ns::string token;
    while (!(token = nexttoken(in)).empty())
    {
        if (incomment)
        {
            if (token == "$)")
            {
                incomment = false;
                continue;
            }
            if (token.find("$(") != ns::string::npos)
            {
                ns::cerr << "Characters $( found in a comment" << ns::endl;
                return false;
            }
            if (token.find("$)") != ns::string::npos)
            {
                ns::cerr << "Characters $) found in a comment" << ns::endl;
                return false;
            }
            continue;
        }

        // Not in comment
        if (token == "$(")
        {
            incomment = true;
            continue;
        }

        if (infileinclusion)
        {
            if (newfilename.empty())
            {
                if (token.find('$') != ns::string::npos)
                {
                    ns::cerr << "Filename " << token << " contains a $"
                              << ns::endl;
                    return false;
                }
                newfilename = token;
                continue;
            }
            else
            {
                if (token != "$]")
                {
                    ns::cerr << "Didn't find closing file inclusion delimiter"
                              << ns::endl;
                    return false;
                }

                bool const okay(readtokens(newfilename));
                if (!okay)
                    return false;
                infileinclusion = false;
                newfilename.clear();
                continue;
            }
        }

        if (token == "$[")
        {
            if (std::is_constant_evaluated()) {
              throw std::runtime_error("File inclusion unsupported within constexpr evaluation.");
            }
            infileinclusion = true;
            continue;
        }

        tokens.push(token);
    }

    if (!in.eof())
    {
        if (in.fail())
            ns::cerr << "Error reading from " << filename << ns::endl;
        return false;
    }

    if (incomment)
    {
        ns::cerr << "Unclosed comment" << ns::endl;
        return false;
    }

    if (infileinclusion)
    {
        ns::cerr << "Unfinished file inclusion command" << ns::endl;
        return false;
    }

    return true;
}

// Construct an Assertion from an Expression. That is, determine the
// mandatory hypotheses and disjoint variable restrictions.
// The Assertion is inserted into the assertions collection,
// and is returned by reference.
constexpr Assertion & constructassertion
  (ns::string const label, Expression const & exp)
{
    Assertion & assertion
        (assertions.insert(ns::make_pair(label, Assertion())).first->second);

    assertion.expression = exp;

    ns::set<ns::string> varsused;

    // Determine variables used and find mandatory hypotheses

    for (Expression::const_iterator iter(exp.begin()); iter != exp.end();
         ++iter)
    {
        if (variables.find(*iter) != variables.end())
            varsused.insert(*iter);
    }

    for (ns::vector<Scope>::const_reverse_iterator iter(scopes.rbegin());
         iter != scopes.rend(); ++iter)
    {
        ns::vector<ns::string> const & hypvec(iter->activehyp);
        for (ns::vector<ns::string>::const_reverse_iterator iter2
            (hypvec.rbegin()); iter2 != hypvec.rend(); ++iter2)
        {
            Hypothesis const & hyp(hypotheses.find(*iter2)->second);
            if (hyp.second && varsused.find(hyp.first[1]) != varsused.end())
            {
                // Mandatory floating hypothesis
                assertion.hypotheses.push_front(*iter2);
            }
            else if (!hyp.second)
            {
                // Essential hypothesis
                assertion.hypotheses.push_front(*iter2);
                for (Expression::const_iterator iter3(hyp.first.begin());
                     iter3 != hyp.first.end(); ++iter3)
                {
                    if (variables.find(*iter3) != variables.end())
                        varsused.insert(*iter3);
                }
            }
        }
    }

    // Determine mandatory disjoint variable restrictions
    for (ns::vector<Scope>::const_iterator iter(scopes.begin());
         iter != scopes.end(); ++iter)
    {
        ns::vector<ns::set<ns::string> > const & disjvars(iter->disjvars);
        for (ns::vector<ns::set<ns::string> >::const_iterator iter2
            (disjvars.begin()); iter2 != disjvars.end(); ++iter2)
        {
            ns::set<ns::string> dset;
            ns::set_intersection
                 (iter2->begin(), iter2->end(),
                  varsused.begin(), varsused.end(),
                  ns::inserter(dset, dset.end()));

            for (ns::set<ns::string>::const_iterator diter(dset.begin());
                 diter != dset.end(); ++diter)
            {
                ns::set<ns::string>::const_iterator diter2(diter);
                ++diter2;
                for (; diter2 != dset.end(); ++diter2)
                    assertion.disjvars.insert(ns::make_pair(*diter, *diter2));
            }
        }
    }

    return assertion;
}

// Read an expression from the token stream. Returns true iff okay.
constexpr bool readexpression
    ( char stattype, ns::string label, ns::string terminator,
      Expression * exp)
{
    if (tokens.empty())
    {
        ns::cerr << "Unfinished $" << stattype << " statement " << label
                  << ns::endl;
        return false;
    }

    ns::string type(tokens.front());

    if (constants.find(type) == constants.end())
    {
        ns::cerr << "First symbol in $" << stattype << " statement " << label
                  << " is " << type << " which is not a constant" << ns::endl;
        return false;
    }

    tokens.pop();

    exp->push_back(type);

    ns::string token;

    while (!tokens.empty() && (token = tokens.front()) != terminator)
    {
        tokens.pop();

        if (constants.find(token) == constants.end()
         && getfloatinghyp(token).empty())
        {
            ns::cerr << "In $" << stattype << " statement " << label
                      << " token " << token
                      << " found which is not a constant or variable in an"
                         " active $f statement" << ns::endl;
            return false;
        }

        exp->push_back(token);
    }

    if (tokens.empty())
    {
        ns::cerr << "Unfinished $" << stattype << " statement " << label
                  << ns::endl;
        return false;
    }

    tokens.pop(); // Discard terminator token

    return true;
}

// Make a substitution of variables. The result is put in "destination",
// which should be empty.
constexpr void makesubstitution
    (Expression const & original, ns::map<ns::string, Expression> substmap,
     Expression * destination
    )
{
    for (Expression::const_iterator iter(original.begin());
         iter != original.end(); ++iter)
    {
        ns::map<ns::string, Expression>::const_iterator const iter2
            (substmap.find(*iter));
        if (iter2 == substmap.end())
        {
            // Constant
            destination->push_back(*iter);
        }
        else
        {
            // Variable
            ns::copy(iter2->second.begin(), iter2->second.end(),
                      ns::back_inserter(*destination));
        }
    }
}

// Get the raw numbers from compressed proof format.
// The letter Z is translated as 0.
constexpr bool getproofnumbers(ns::string label, ns::string proof,
                               ns::vector<ns::size_t> * proofnumbers)
{
    ns::size_t const size_max(ns::numeric_limits<ns::size_t>::max());

    ns::size_t num(0u);
    bool justgotnum(false);
    for (ns::string::const_iterator iter(proof.begin()); iter != proof.end();
         ++iter)
    {
        if (*iter <= 'T')
        {
            ns::size_t const addval(*iter - ('A' - 1));

            if (num > size_max / 20 || 20 * num > size_max - addval)
            {
                ns::cerr << "Overflow computing numbers in compressed proof "
                             "of " << label << ns::endl;
                return false;
            }

            proofnumbers->push_back(20 * num + addval);
            num = 0;
            justgotnum = true;
        }
        else if (*iter <= 'Y')
        {
            ns::size_t const addval(*iter - 'T');

            if (num > size_max / 5 || 5 * num > size_max - addval)
            {
                ns::cerr << "Overflow computing numbers in compressed proof "
                             "of " << label << ns::endl;
                return false;
            }

            num = 5 * num + addval;
            justgotnum = false;
        }
        else // It must be Z
        {
            if (!justgotnum)
            {
                ns::cerr << "Stray Z found in compressed proof of "
                          << label << ns::endl;
                return false;
            }

            proofnumbers->push_back(0);
            justgotnum = false;
        }
    }

    if (num != 0)
    {
        ns::cerr << "Compressed proof of theorem " << label
                  << " ends in unfinished number" << ns::endl;
        return false;
    }

    return true;
}

// Subroutine for proof verification. Verify a proof step referencing an
// assertion (i.e., not a hypothesis).
constexpr bool verifyassertionref
  (ns::string thlabel, ns::string reflabel, ns::vector<Expression> * stack)
{
    Assertion const & assertion(assertions.find(reflabel)->second);
    if (stack->size() < assertion.hypotheses.size())
    {
        ns::cerr << "In proof of theorem " << thlabel
                  << " not enough items found on stack" << ns::endl;
        return false;
    }

    ns::vector<Expression>::size_type const base
        (stack->size() - assertion.hypotheses.size());

    ns::map<ns::string, Expression> substitutions;

    // Determine substitutions and check that we can unify
    for (ns::deque<ns::string>::size_type i(0);
         i < assertion.hypotheses.size(); ++i)
    {
        Hypothesis const & hypothesis
            (hypotheses.find(assertion.hypotheses[i])->second);
        if (hypothesis.second)
        {
            // Floating hypothesis of the referenced assertion
            if (hypothesis.first[0] != (*stack)[base + i][0])
            {
                ns::cout << "In proof of theorem " << thlabel
                          << " unification failed" << ns::endl;
                return false;
            }
            Expression & subst(substitutions.insert
                (ns::make_pair(hypothesis.first[1],
                 Expression())).first->second);
            ns::copy((*stack)[base + i].begin() + 1, (*stack)[base + i].end(),
                      ns::back_inserter(subst));
        }
        else
        {
            // Essential hypothesis
            Expression dest;
            makesubstitution(hypothesis.first, substitutions, &dest);
            if (dest != (*stack)[base + i])
            {
                ns::cerr << "In proof of theorem "  << thlabel
                          << " unification failed" << ns::endl;
                return false;
            }
        }
    }

    // Remove hypotheses from stack
    stack->erase(stack->begin() + base, stack->end());

    // Verify disjoint variable conditions
    for (ns::set<ns::pair<ns::string, ns::string> >::const_iterator
         iter(assertion.disjvars.begin());
         iter != assertion.disjvars.end(); ++iter)
    {
        Expression const & exp1(substitutions.find(iter->first)->second);
        Expression const & exp2(substitutions.find(iter->second)->second);

        ns::set<ns::string> exp1vars;
        for (Expression::const_iterator exp1iter(exp1.begin());
             exp1iter != exp1.end(); ++exp1iter)
        {
            if (variables.find(*exp1iter) != variables.end())
                exp1vars.insert(*exp1iter);
        }

        ns::set<ns::string> exp2vars;
        for (Expression::const_iterator exp2iter(exp2.begin());
             exp2iter != exp2.end(); ++exp2iter)
        {
            if (variables.find(*exp2iter) != variables.end())
                exp2vars.insert(*exp2iter);
        }

        for (ns::set<ns::string>::const_iterator exp1iter
            (exp1vars.begin()); exp1iter != exp1vars.end(); ++exp1iter)
        {
            for (ns::set<ns::string>::const_iterator exp2iter
                (exp2vars.begin()); exp2iter != exp2vars.end(); ++exp2iter)
            {
                if (!isdvr(*exp1iter, *exp2iter))
                {
                    ns::cerr << "In proof of theorem " << thlabel
                              << " disjoint variable restriction violated"
                              << ns::endl;
                    return false;
                }
            }
        }
    }

    // Done verification of this step. Insert new statement onto stack.
    Expression dest;
    makesubstitution(assertion.expression, substitutions, &dest);
    stack->push_back(dest);

    return true;
}

// Verify a regular proof. The "proof" argument should be a non-empty sequence
// of valid labels. Return true iff the proof is correct.
constexpr bool verifyregularproof
     (ns::string label, Assertion const & theorem,
      ns::vector<ns::string> const & proof
     )
{
    ns::vector<Expression> stack;
    for (ns::vector<ns::string>::const_iterator proofstep(proof.begin());
         proofstep != proof.end(); ++proofstep)
    {
        // If step is a hypothesis, just push it onto the stack.
        ns::map<ns::string, Hypothesis>::const_iterator hyp
            (hypotheses.find(*proofstep));
        if (hyp != hypotheses.end())
        {
            stack.push_back(hyp->second.first);
            continue;
        }

        // It must be an axiom or theorem
        bool const okay(verifyassertionref(label, *proofstep, &stack));
        if (!okay)
            return false;
    }

    if (stack.size() != 1)
    {
        ns::cerr << "Proof of theorem " << label
                  << " does not end with only one item on the stack"
                  << ns::endl;
        return false;
    }

    if (stack[0] != theorem.expression)
    {
        ns::cerr << "Proof of theorem " << label << " proves wrong statement"
                  << ns::endl; 
    }

    return true;
}

// Verify a compressed proof
constexpr bool verifycompressedproof
    (ns::string label, Assertion const & theorem,
     ns::vector<ns::string> const & labels,
     ns::vector<ns::size_t> const & proofnumbers)
{
    ns::vector<Expression> stack;

    ns::size_t const mandhypt(theorem.hypotheses.size());
    ns::size_t const labelt(mandhypt + labels.size());

    ns::vector<Expression> savedsteps;
    for (ns::vector<ns::size_t>::const_iterator iter(proofnumbers.begin());
         iter != proofnumbers.end(); ++iter)
    {
        // Save the last proof step if 0
        if (*iter == 0)
        {
            savedsteps.push_back(stack.back());
            continue;
        }

        // If step is a mandatory hypothesis, just push it onto the stack.
        if (*iter <= mandhypt)
        {
            stack.push_back
                (hypotheses.find(theorem.hypotheses[*iter - 1])->second.first);
        }
        else if (*iter <= labelt)
        {
            ns::string const proofstep(labels[*iter - mandhypt - 1]);

            // If step is a (non-mandatory) hypothesis,
            // just push it onto the stack.
            ns::map<ns::string, Hypothesis>::const_iterator hyp
            (hypotheses.find(proofstep));
            if (hyp != hypotheses.end())
            {
                stack.push_back(hyp->second.first);
                continue;
            }

            // It must be an axiom or theorem
            bool const okay(verifyassertionref(label, proofstep, &stack));
            if (!okay)
                return false;
        }
        else // Must refer to saved step
        {
            if (*iter > labelt + savedsteps.size())
            {
                ns::cerr << "Number in compressed proof of " << label
                          << " is too high" << ns::endl;
                return false;
            }

            stack.push_back(savedsteps[*iter - labelt - 1]);
        }
    }

    if (stack.size() != 1)
    {
        ns::cerr << "Proof of theorem " << label
                  << " does not end with only one item on the stack"
                  << ns::endl;
        return false;
    }

    if (stack[0] != theorem.expression)
    {
        ns::cerr << "Proof of theorem " << label << " proves wrong statement"
                  << ns::endl; 
    }

    return true;
}

// Parse $p statement. Return true iff okay.
constexpr bool parsep(ns::string label)
{
    Expression newtheorem;
    bool const okay(readexpression('p', label, "$=", &newtheorem));
    if (!okay)
    {
        return false;
    }

    Assertion const & assertion(constructassertion(label, newtheorem));

    // Now for the proof

    if (tokens.empty())
    {
        ns::cerr << "Unfinished $p statement " << label << ns::endl;
        return false;
    }

    if (tokens.front() == "(")
    {
        // Compressed proof
        tokens.pop();

        // Get labels

        ns::vector<ns::string> labels;
        ns::string token;
        while (!tokens.empty() && (token = tokens.front()) != ")")
        {
            tokens.pop();
            labels.push_back(token);
            if (token == label)
            {
                ns::cerr << "Proof of theorem " << label
                          << " refers to itself" << ns::endl;
                return false;
            }
            else if (ns::find
                    (assertion.hypotheses.begin(), assertion.hypotheses.end(),
                     token) != assertion.hypotheses.end())
            {
                ns::cerr << "Compressed proof of theorem " << label
                          << " has mandatory hypothesis " << token
                          << " in label list" << ns::endl;
                return false;
            }
            else if (assertions.find(token) == assertions.end()
                  && !isactivehyp(token))
            {
                ns::cerr << "Proof of theorem " << label << " refers to "
                          << token << " which is not an active statement"
                          << ns::endl;
                return false;
            }
        }

        if (tokens.empty())
        {
            ns::cerr << "Unfinished $p statement " << label << ns::endl;
            return false;
        }

        tokens.pop(); // Discard ) token

        // Get proof steps

        ns::string proof;
        while (!tokens.empty() && (token = tokens.front()) != "$.")
        {
            tokens.pop();

            proof += token;
            if (!containsonlyupperorq(token))
            {
                ns::cerr << "Bogus character found in compressed proof of "
                          << label << ns::endl;
                return false;
            }
        }

        if (tokens.empty())
        {
            ns::cerr << "Unfinished $p statement " << label << ns::endl;
            return false;
        }

        if (proof.empty())
        {
            ns::cerr << "Theorem " << label << " has no proof" << ns::endl;
            return false;
        }

        tokens.pop(); // Discard $. token

        if (proof.find('?') != ns::string::npos)
        {
            ns::cerr << "Warning: Proof of theorem " << label
                      << " is incomplete" << ns::endl;
            return true; // Continue processing file
        }

        ns::vector<ns::size_t> proofnumbers;
        proofnumbers.reserve(proof.size()); // Preallocate for efficiency
        bool okay(getproofnumbers(label, proof, &proofnumbers));
        if (!okay)
            return false;

        okay = verifycompressedproof(label, assertion, labels, proofnumbers);
        if (!okay)
            return false;
    }
    else
    {
        // Regular (uncompressed proof)
        ns::vector<ns::string> proof;
        bool incomplete(false);
        ns::string token;
        while (!tokens.empty() && (token = tokens.front()) != "$.")
        {
            tokens.pop();
            proof.push_back(token);
            if (token == "?")
                incomplete = true;
            else if (token == label)
            {
                ns::cerr << "Proof of theorem " << label
                          << " refers to itself" << ns::endl;
                return false;
            }
            else if (assertions.find(token) == assertions.end()
                  && !isactivehyp(token))
            {
                ns::cerr << "Proof of theorem " << label << " refers to "
                          << token << " which is not an active statement"
                          << ns::endl;
                return false;
            }
        }

        if (tokens.empty())
        {
            ns::cerr << "Unfinished $p statement " << label << ns::endl;
            return false;
        }

        if (proof.empty())
        {
            ns::cerr << "Theorem " << label << " has no proof" << ns::endl;
            return false;
        }

        tokens.pop(); // Discard $. token

        if (incomplete)
        {
            ns::cerr << "Warning: Proof of theorem " << label
                      << " is incomplete" << ns::endl;
            return true; // Continue processing file
        }

        bool okay(verifyregularproof(label, assertion, proof));
        if (!okay)
            return false;
    }

    return true;
}

// Parse $e statement. Return true iff okay.
constexpr bool parsee(ns::string label)
{
    Expression newhyp;
    bool const okay(readexpression('e', label, "$.", &newhyp));
    if (!okay)
    {
        return false;
    }

    // Create new essential hypothesis
    hypotheses.insert(ns::make_pair(label, ns::make_pair(newhyp, false)));
    scopes.back().activehyp.push_back(label);

    return true;
}

// Parse $a statement. Return true iff okay.
constexpr bool parsea(ns::string label)
{
    Expression newaxiom;
    bool const okay(readexpression('a', label, "$.", &newaxiom));
    if (!okay)
    {
        return false;
    }

    constructassertion(label, newaxiom);

    return true;
}

// Parse $f statement. Return true iff okay.
constexpr bool parsef(ns::string label)
{
    if (tokens.empty())
    {
        ns::cerr << "Unfinished $f statement" << label << ns::endl;
        return false;
    }

    ns::string type(tokens.front());

    if (constants.find(type) == constants.end())
    {
        ns::cerr << "First symbol in $f statement " << label << " is "
                  << type << " which is not a constant" << ns::endl;
        return false;
    }

    tokens.pop();

    if (tokens.empty())
    {
        ns::cerr << "Unfinished $f statement " << label << ns::endl;
        return false;
    }

    ns::string variable(tokens.front());
    if (!isactivevariable(variable))
    {
        ns::cerr << "Second symbol in $f statement " << label << " is "
                  << variable << " which is not an active variable"
                  << ns::endl;
        return false;
    }
    if (!getfloatinghyp(variable).empty())
    {
        ns::cerr << "The variable " << variable
                  << " appears in a second $f statement "
                  << label << ns::endl;
        return false;
    }

    tokens.pop();

    if (tokens.empty())
    {
        ns::cerr << "Unfinished $f statement" << label << ns::endl;
        return false;
    }

    if (tokens.front() != "$.")
    {
        ns::cerr << "Expected end of $f statement " << label
                  << " but found " << tokens.front() << ns::endl;
        return false;
    }

    tokens.pop(); // Discard $. token

    // Create new floating hypothesis
    Expression newhyp;
    newhyp.push_back(type);
    newhyp.push_back(variable);
    hypotheses.insert(ns::make_pair(label, ns::make_pair(newhyp, true)));
    scopes.back().activehyp.push_back(label);
    scopes.back().floatinghyp.insert(ns::make_pair(variable, label));

    return true;
}

// Parse labeled statement. Return true iff okay.
constexpr bool parselabel(ns::string label)
{
    if (constants.find(label) != constants.end())
    {
        ns::cerr << "Attempt to reuse constant " << label << " as a label"
                  << ns::endl;
        return false;
    }

    if (variables.find(label) != variables.end())
    {
        ns::cerr << "Attempt to reuse variable " << label << " as a label"
                  << ns::endl;
        return false;
    }

    if (labelused(label))
    {
        ns::cerr << "Attempt to reuse label " << label << ns::endl;
        return false;
    }

    if (tokens.empty())
    {
        ns::cerr << "Unfinished labeled statement" << ns::endl;
        return false;
    }

    ns::string const type(tokens.front());
    tokens.pop();

    bool okay(true);
    if (type == "$p")
    {
        okay = parsep(label);
    }
    else if (type == "$e")
    {
        okay = parsee(label);
    }
    else if (type == "$a")
    {
        okay = parsea(label);
    }
    else if (type == "$f")
    {
        okay = parsef(label);
    }
    else
    {
        ns::cerr << "Unexpected token " << type << " encountered"
                  << ns::endl;
        return false;
    }

    return okay;
}

// Parse $d statement. Return true iff okay.
constexpr bool parsed()
{
    ns::set<ns::string> dvars;

    ns::string token;

    while (!tokens.empty() && (token = tokens.front()) != "$.")
    {
        tokens.pop();

        if (!isactivevariable(token))
        {
            ns::cerr << "Token " << token << " is not an active variable, "
                      << "but was found in a $d statement" << ns::endl;
            return false;
        }

        bool const duplicate(!dvars.insert(token).second);
        if (duplicate)
        {
            ns::cerr << "$d statement mentions " << token << " twice"
                      << ns::endl;
            return false;
        }
    }

    if (tokens.empty())
    {
        ns::cerr << "Unterminated $d statement" << ns::endl;
        return false;
    }

    if (dvars.size() < 2)
    {
        ns::cerr << "Not enough items in $d statement" << ns::endl;
        return false;
    }

    // Record it
    scopes.back().disjvars.push_back(dvars);

    tokens.pop(); // Discard $. token

    return true;
}

// Parse $c statement. Return true iff okay.
constexpr bool parsec()
{
    if (scopes.size() > 1)
    {
        ns::cerr << "$c statement occurs in inner block"
                  << ns::endl;
        return false;
    }

    ns::string token;
    bool listempty(true);
    while (!tokens.empty() && (token = tokens.front()) != "$.")
    { 
        tokens.pop();
        listempty = false;

        if (!ismathsymboltoken(token))
        {
            ns::cerr << "Attempt to declare " << token
                      << " as a constant" << ns::endl;
            return false;
        }
        if (variables.find(token) != variables.end())
        {
            ns::cerr << "Attempt to redeclare variable " << token
                      << " as a constant" << ns::endl;
            return false;
        }
        if (labelused(token))
        {
            ns::cerr << "Attempt to reuse label " << token
                      << " as a constant" << ns::endl;
            return false;
        }
        bool const alreadydeclared(!constants.insert(token).second);
        if (alreadydeclared)
        {
            ns::cerr << "Attempt to redeclare constant " << token
                      << ns::endl;
            return false;
        }
    }

    if (tokens.empty())
    {
        ns::cerr << "Unterminated $c statement" << ns::endl;
        return false;
    }

    if (listempty)
    {
        ns::cerr << "Empty $c statement" << ns::endl;
        return false;
    }

    tokens.pop(); // Discard $. token

    return true;
}

// Parse $v statement. Return true iff okay.
constexpr bool parsev()
{
    ns::string token;
    bool listempty(true);
    while (!tokens.empty() && (token = tokens.front()) != "$.")
    { 
        tokens.pop();
        listempty = false;

        if (!ismathsymboltoken(token))
        {
            ns::cerr << "Attempt to declare " << token
                      << " as a variable" << ns::endl;
            return false;
        }
        if (constants.find(token) != constants.end())
        {
            ns::cerr << "Attempt to redeclare constant " << token
                      << " as a variable" << ns::endl;
            return false;
        }
        if (labelused(token))
        {
            ns::cerr << "Attempt to reuse label " << token
                      << " as a variable" << ns::endl;
            return false;
        }
        bool const alreadyactive(isactivevariable(token));
        if (alreadyactive)
        {
            ns::cerr << "Attempt to redeclare active variable " << token
                      << ns::endl;
            return false;
        }
        variables.insert(token);
        scopes.back().activevariables.insert(token);
    }

    if (tokens.empty())
    {
        ns::cerr << "Unterminated $v statement" << ns::endl;
        return false;
    }

    if (listempty)
    {
        ns::cerr << "Empty $v statement" << ns::endl;
        return false;
    }

    tokens.pop(); // Discard $. token

    return true;
}

constexpr int run(ns::string const filename, ns::string const &text = "")
{
    bool const okay(readtokens(filename, text));
    if (!okay)
        return EXIT_FAILURE;

    scopes.push_back(Scope());

    while (!tokens.empty())
    {
        ns::string const token(tokens.front());
        tokens.pop();

        bool okay(true);

        if (islabeltoken(token))
        {
            okay = parselabel(token);
        }
        else if (token == "$d")
        {
            okay = parsed();
        }
        else if (token == "${")
        {
            scopes.push_back(Scope());
        }
        else if (token == "$}")
        {
            scopes.pop_back();
            if (scopes.empty())
            {
                ns::cerr << "$} without corresponding ${" << ns::endl;
                return EXIT_FAILURE;
            }
        }
        else if (token == "$c")
        {
            okay = parsec();
        }
        else if (token == "$v")
        {
            okay = parsev();
        }
        else
        {
            ns::cerr << "Unexpected token " << token << " encountered"
                      << ns::endl;
            return EXIT_FAILURE;
        }
        if (!okay)
            return EXIT_FAILURE;
    }

    if (scopes.size() > 1)
    {
        ns::cerr << "${ without corresponding $}" << ns::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

}; // struct checkmm

#define xstr(s) str(s)
#define str(s) #s

constexpr int app_run()
{
    checkmm app;
//    ns::string txt = R"($( Declare the constant symbols we will use $)
//                        $c 0 + = -> ( ) term wff |- $.)";
//    ns::string txt = "$c 0 + = -> ( ) term wff |- $.";
//    ns::string txt = "$( The comment is not closed!";

#ifdef MMFILEPATH
    ns::string txt =
#include xstr(MMFILEPATH)
;
    int ret = app.run("", txt);
#else
    int ret = EXIT_SUCCESS;
#endif

    return ret;
}

int main(int argc, char ** argv)
{
    if (argc != 2)
    {
        ns::cerr << "Syntax: checkmm <filename>" << ns::endl;
        return EXIT_FAILURE;
    }

    static_assert(EXIT_SUCCESS == app_run());

    checkmm app;
    int ret = app.run(argv[1]);

    return ret;
}

