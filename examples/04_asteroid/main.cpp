#include "AsteroidApp.h"

#include <cstdlib>

int main(int argc, char **argv)
{
   try
   {
      AsteroidApp app;
      if (!app.run(argc, argv))
      {
         return EXIT_FAILURE;
      }
   }
   catch (const std::exception &ex)
   {
      std::fprintf(stderr, "Fatal error: %s\n", ex.what());
      return EXIT_FAILURE;
   }
   return EXIT_SUCCESS;
}


