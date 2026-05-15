#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#include <catch2/catch_all.hpp>

// Enable CRT leak detection - reports at program exit
static struct LeakCheckEnabler {
    LeakCheckEnabler() {
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }
} leakCheckEnabler;
