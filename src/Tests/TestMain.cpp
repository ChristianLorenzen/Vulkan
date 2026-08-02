// Shared doctest entry point for faye_tests. Per-module test files live in a
// Tests/ directory next to the implementation they cover
// (e.g. Core/Jobs/Tests/JobSystemTests.cpp).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// quill/Backend.h is where quill::detail::get_thread_id()/get_thread_name()
// (declared `extern` in quill/core/ThreadContextManager.h, used by Logger.hpp
// via LOG_* macros) are actually `inline`-defined. The app pulls this header
// in through main.cpp/Input.hpp, but no test TU otherwise does, so without
// this include the test binary fails to link. We deliberately do NOT call
// quill::Backend::start() here — tests don't need the backend worker thread,
// only something to give these inline symbols a home.
#include "quill/Backend.h"
