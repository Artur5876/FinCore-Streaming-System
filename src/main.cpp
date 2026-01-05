#include <iostream>
#include <string>

// PostgreSQL headers (optional)
#ifdef __has_include
# if __has_include(<libpq-fe.h>)
#  include <libpq-fe.h>
#  define HAVE_POSTGRESQL 1
# endif
#endif

// Redis headers (optional)
#ifdef __has_include
# if __has_include(<hiredis/hiredis.h>)
#  include <hiredis/hiredis.h>
#  define HAVE_REDIS 1
# endif
#endif

int main() {
    std::cout << "=== Simple C++ Project ===" << std::endl;
    std::cout << "Built with: g++" << std::endl;
    
    #ifdef HAVE_POSTGRESQL
    std::cout << "✓ PostgreSQL support available" << std::endl;
    #else
    std::cout << "✗ PostgreSQL headers not found" << std::endl;
    #endif
    
    #ifdef HAVE_REDIS
    std::cout << "✓ Redis support available" << std::endl;
    #else
    std::cout << "✗ Redis headers not found" << std::endl;
    #endif
    
    std::cout << "\nReady to code!" << std::endl;
    
    return 0;
}
