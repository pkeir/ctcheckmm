#include "ctcheckmm.hpp"

#define xstr(s) str(s)
#define str(s) #s

constexpr int run_checkmm()
{
  checkmm app;

#ifdef MMFILEPATH
  cest::string txt =
#include xstr(MMFILEPATH)
;
  return app.run("", txt);
#else
  return EXIT_SUCCESS;
#endif
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
    cest::cerr << "Syntax: checkmm <filename>" << cest::endl;                   
    return EXIT_FAILURE;                                                    
  }                                                                           
                                                                              
  static_assert(EXIT_SUCCESS == run_checkmm());
                                                                              
  checkmm app;                                                                
  int ret = app.run(argv[1]);                                                 
                                                                              
  return ret;                                                                 
} 
