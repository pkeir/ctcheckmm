#include "ctcheckmm.hpp"

#define xstr(s) str(s)
#define str(s) #s

constexpr int run_checkmm()
{
  checkmm app;
//    ns::string txt = R"($( Declare the constant symbols we will use $)
//                        $c 0 + = -> ( ) term wff |- $.)";
//    ns::string txt = "$c 0 + = -> ( ) term wff |- $.";
//    ns::string txt = "$( The comment is not closed!";

#ifdef MMFILEPATH
  cest::string txt =
#include xstr(MMFILEPATH)
;
  int ret = app.run("", txt);
#else
  int ret = EXIT_SUCCESS;
#endif

  return ret;
}

constexpr int basic_checkmm_test()
{
  checkmm app;
  cest::string txt = R"($c 0 + = -> ( ) term wff |- $.)";
  return app.run("", txt);
}

static_assert(EXIT_SUCCESS == basic_checkmm_test());

int main(int argc, char ** argv)                                                
{                                                                               
  if (argc != 2)                                                              
  {                                                                           
      ns::cerr << "Syntax: checkmm <filename>" << ns::endl;                   
      return EXIT_FAILURE;                                                    
  }                                                                           
                                                                              
  static_assert(EXIT_SUCCESS == run_checkmm());
                                                                              
  checkmm app;                                                                
  int ret = app.run(argv[1]);                                                 
                                                                              
  return ret;                                                                 
} 
