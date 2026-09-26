#pragma once
// Release-Konfigurationen definieren NDEBUG automatisch. Tests muessen assert() dennoch
// ausfuehren. Dieser Header wird unter MSVC per /FI vor jeder Test-Quelldatei eingebunden,
// damit NDEBUG ohne widerspruechliche Compiler-Schalter entfernt wird.
#ifdef NDEBUG
#undef NDEBUG
#endif
