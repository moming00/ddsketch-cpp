#pragma once

#include <mutex>
#include <memory>
#include <string>
#include <cstddef>
#include <string.h>
#include <exception>
#include "spdlog/spdlog.h"

#ifdef WIN32
#include "../common/include/win32/StackWalker.h"
#elif !defined _AIX
#include <cxxabi.h>
#include <execinfo.h>
#include <dlfcn.h>
#endif

namespace Utils
{
    const static uint16_t MAX_FRAMES = 128;

    typedef enum ExceptType
    {
        NonExcept = 0, // RtlCaptureContext
        AfterExcept = 1,
        AfterCatch = 2, // get_current_exception_context
    } ExceptType;

#ifdef WIN32
    class MyStackWalker : public StackWalker
    {
    public:
        // Inherit the constructor from super class
        using StackWalker::StackWalker;

    protected:
        virtual void OnOutput(LPCSTR szText) { spdlog::trace(szText); }
    };

    static inline void LogBackTrace(ExceptType type = NonExcept)
    {
        // The dbghelp.dll is not thread-safe
        static std::mutex _mutex;
        std::lock_guard<std::mutex> locker(_mutex);

        MyStackWalker sw(StackWalker::ExceptType(type), StackWalker::StackWalkOptions::OptionsAll);
        spdlog::enable_backtrace(MAX_FRAMES);
        
        // Generate the trace to a ringbuffer with MAX_FRAMES lines of info
        sw.ShowCallstack();
        spdlog::dump_backtrace();
        spdlog::disable_backtrace();
    }

#elif defined _AIX
    // execinfo.h is not available on AIX
    static inline void LogBackTrace(ExceptType type = NonExcept)
    {
    }
#else
    /***
        GCC subscribes to a cross-vendor ABI for C++, sometimes called the IA64 ABI
        because it happens to be the native ABI for that platform. It is summarized at
        https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling
    ***/

    // The prefix used for mangled symbols
    const char kMangledSymbolPrefix[] = "_Z";
    // Characters that can be used for symbols：
    // https://itanium-cxx-abi.github.io/cxx-abi/abi-mangling.html
    const char kSymbolCharacters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
    // Demangles C++ symbols in the given text. Example:
    // "out/Debug/base_unittests(_ZN10StackTraceC1Ev+0x20) [0x817778c]"
    // =>
    // "out/Debug/base_unittests(StackTrace::StackTrace()+0x20) [0x817778c]"
    inline void DemangleSymbol(std::string &symbol)
    {
        std::string::size_type search_from = 0;
        while (search_from < symbol.size())
        {
            // Look for the start of a mangled symbol from search_from
            std::string::size_type mangled_start = symbol.find(kMangledSymbolPrefix, search_from);
            if (mangled_start == std::string::npos)
            {
                // Mangled symbol not found
                // Entities with C linkage and global namespace variables are not mangled.
                break;
            }

            // Look for the end of the mangled symbol
            std::string::size_type mangled_end = symbol.find_first_not_of(kSymbolCharacters, mangled_start);
            if (mangled_end == std::string::npos)
            {
                mangled_end = symbol.size();
            }
            std::string mangled_symbol = std::move(symbol.substr(mangled_start, mangled_end - mangled_start));

            // Try to demangle the mangled symbol candidate
            int status = -4; // some arbitrary value to eliminate the compiler warning
            std::unique_ptr<char, void (*)(void *)> demangled_symbol{abi::__cxa_demangle(mangled_symbol.c_str(), nullptr, 0, &status), std::free};
            // 0 Demangling is success
            if (0 == status)
            {
                // Remove the mangled symbol
                symbol.erase(mangled_start, mangled_end - mangled_start);
                // Insert the demangled symbol
                symbol.insert(mangled_start, demangled_symbol.get());
                // Next time, we will start right after the demangled symbol
                search_from = mangled_start + strlen(demangled_symbol.get());
            }
            else
            {
                // Failed to demangle. Retry after the "_Z" we just found
                search_from = mangled_start + 2;
            }
        }
    }

    static inline void LogBackTrace(ExceptType type = NonExcept)
    {
        void *addresses[MAX_FRAMES];

        // Get an array of stack frames and the exact symbols
        auto size = backtrace(addresses, MAX_FRAMES);
        std::unique_ptr<char *, void (*)(void *)> symbols{backtrace_symbols(addresses, size), std::free};

        static std::mutex _mutex;
        std::lock_guard<std::mutex> locker(_mutex);
        spdlog::enable_backtrace(size);

        // Generate the trace to a ringbuffer with size lines of info
        for (int i = 0; i < size; ++i)
        {
            // demangle a C++ symbol to plaintext
            std::string demangled(symbols.get()[i]);
            DemangleSymbol(demangled);
            spdlog::trace(demangled);
        }

        spdlog::dump_backtrace();
        spdlog::disable_backtrace();
    }

#endif
}
