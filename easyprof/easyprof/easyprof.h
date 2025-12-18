

#pragma once


#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>


// IMPORTANT: This library is currently *unstable*, so expect breaking changes!

// TODO: Generally improve this library's frontend interface and portability.
// TODO: Better encapsulate result data and improve how it gets formatted.
// TODO: Maybe when reporting results, have it also detail easyprof overhead time.
// TODO: Decide upon how, and then impl, multi-threading support.
// TOOD: Maybe create a 'EASYPROF_SPECIAL' macro which lets end-user define a section
//       of code as a profiled pseudo-function, w/ the end-user providing an arbitrary
//       string name literal to use in place of a real funcsig.


// TODO: I'm not sure what platforms (ie. Windows, Mac, Linux) __PRETTY_FUNCTION__
//		 is used on, and which use __FUNCSIG__, so we'll have to make this code
//		 more portable later.

#define EASYPROF_FUNCSIG __FUNCSIG__


namespace easyprof {


    class ProfilerAgent;


    // Place this at the vary start of each and every function to be profiled.
    // Functions without this won't be acknowledged by the profiler.
    // Undefined behaviour if multiple EASPROF(s) are used, or if it's misplaced.
#define EASYPROF ::easyprof::ProfilerAgent _easyprof_agent(__LINE__, __FILE__, EASYPROF_FUNCSIG)


    template<typename... Args>
    inline void print(std::format_string<Args...> fmt, Args&&... args) {
        std::cout << std::format(fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    inline void println(std::format_string<Args...> fmt, Args&&... args) {
        std::cout << std::format(fmt, std::forward<Args>(args)...) << '\n';
    }
    inline void println() {
        std::cout << '\n';
    }


    // NOTE: Stole this timer impl from TheCherno's Hazel Engine.

    using Seconds = double;

    class Timer final {
    public:
        inline Timer() {
            reset();
        }


        inline Seconds elapsed() const noexcept {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - _start).count() * 0.001 * 0.001 * 0.001;
        }
        inline void reset() noexcept {
            _start = std::chrono::high_resolution_clock::now();
        }


    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> _start = {};
    };


    // TODO: Replace below w/ better way to store/format results.

    struct Result final {
        size_t              line        = 0;    // Line Number (of agent, not the function itself.)
        std::string_view    file;	            // File Path
        std::string_view    function;           // Function Signature 
        size_t              calls       = 0;    // The number of times the function was called.
        Seconds             internal    = 0.0;  // Total exec time of all calls, minus subprocedure time.
        Seconds             cumulative  = 0.0;	// Total exec time of all calls, including subprocedure time.
    };

    using Results = std::vector<Result>;

    inline std::string fmt(const Results& results) {
        auto fmtCalls = [](size_t n) -> std::string {
            if (n >= 10'000'000'000)    return std::format("{}B", (n - n % 1000'000'000) / 1000'000'000);
            else if (n >= 10'000'000)   return std::format("{}M", (n - n % 1000'000) / 1000'000);
            else if (n >= 10'000)       return std::format("{}K", (n - n % 1000) / 1000);
            else                        return std::format("{}", n);
            };
        auto fmtSecs = [](Seconds s) -> std::string {
            if (s < 0.000'001)  return std::format("{}ns", (float)(s * 1'000'000'000.0)); // nano
            else if (s < 0.001) return std::format("{}us", (float)(s * 1'000'000.0)); // micro
            else if (s < 1.0)   return std::format("{}ms", (float)(s * 1'000.0)); // milli
            else                return std::format("{}s", (float)s);
            };
        std::string output{};
        output += std::format("Profiler Results ({} fns):", results.size());
        output += std::format("\ncalls        internal     cumulative   funcsig:line:file");
        for (const Result& result : results) {
            output += std::format("\n{: <12} {: <12} {: <12} {}:{}:{}",
                fmtCalls(result.calls),
                fmtSecs(result.internal),
                fmtSecs(result.cumulative),
                (std::string)result.function,
                result.line,
                std::filesystem::proximate((std::string)result.file).string());
        }
        return output;
    }

    enum class SortBy : uint8_t {
        Default = 0,    // Sort by internal time.
        Cumulative,     // Sort by cumulative time.
        Calls,          // Sort by call count.
    };

    class Profiler final {
    public:
        Profiler() = default;


        // Returns the profiling results.
        // Undefined behaviour if this is called while the profiler is still running.
        inline Results results(SortBy sortBy = SortBy::Default) {
            Results output{};
            for (const auto& [funcsig, result] : _results) {
                output.push_back(result);
            }
            switch (sortBy) {
            case SortBy::Default:
            {
                std::sort(output.begin(), output.end(),
                    [](const Result& a, const Result& b) -> bool {
                        return a.internal > b.internal;
                    });
            }
            break;
            case SortBy::Cumulative:
            {
                std::sort(output.begin(), output.end(),
                    [](const Result& a, const Result& b) -> bool {
                        return a.cumulative > b.cumulative;
                    });
            }
            break;
            case SortBy::Calls:
            {
                std::sort(output.begin(), output.end(),
                    [](const Result& a, const Result& b) -> bool {
                        return a.calls > b.calls;
                    });
            }
            break;
            default: assert(false); break;
            }
            return output;
        }


        // Returns the currently running profiler for this thread, if any.
        inline static Profiler* current() noexcept {
            return _current;
        }


    private:
        friend class ProfilerAgent;
        friend void start(Profiler& profiler) noexcept;
        friend void stop() noexcept;


        // Maps function signature to corresponding results.
        // This is why it's important for ProfilerAgent ctor arg strings to be to memory which will be
        // good for the lifetime of the program.
        std::unordered_map<std::string_view, Result> _results;


        static thread_local Profiler* _current;
    };

    inline thread_local Profiler* Profiler::_current = nullptr;


    // Start (or resume) a profiler for this thread.
    // Any currently running profiler for this thread will be stopped.
    inline void start(Profiler& profiler) noexcept {
        Profiler::_current = &profiler;
    }

    // Stops the profiler running for this thread, if any.
    inline void stop() noexcept {
        Profiler::_current = nullptr;
    }


    class ProfilerAgent final {
    public:
        // file and function are expected to be C-strings who's lifetime extends until the
        // end of program execution (ie. they should be in static readonly memory.)
        inline ProfilerAgent(size_t line, std::string_view file, std::string_view function) :
            _line(line),
            _file(file),
            _function(function) {
            // Make this agent the new top agent.
            _pushAgent(*this);
        }

        inline ~ProfilerAgent() noexcept { // RAII
            Seconds finalCumulative = _cumulative.elapsed();
            // Remove this agent from agent stack.
            _popAgent();
            // If there was an agent below us on the agent stack, then propagate our
            // cumulative time to their subprocedure time.
            if (_topAgent) {
                _topAgent->_subprocedure += finalCumulative;
            }
            // If there's a running profiler on this thread, write our results to it.
            if (auto prof = Profiler::current()) {
                if (auto it = prof->_results.find(_function); it != prof->_results.end()) {
                    it->second.calls++;
                    it->second.internal += finalCumulative - _subprocedure;
                    it->second.cumulative += finalCumulative;
                }
                else {
                    prof->_results.try_emplace(_function, Result{
                        .line = _line,
                        .file = _file,
                        .function = _function,
                        .calls = 1,
                        .internal = finalCumulative - _subprocedure,
                        .cumulative = finalCumulative,
                        });
                }
            }
        }


    private:
        size_t _line;
        std::string_view _file;
        std::string_view _function;

        Timer _cumulative; // Time of procedure and all subprocedures.
        Seconds _subprocedure = 0.0; // Time of subprocedures.

        ProfilerAgent* _below = nullptr; // Agent below this one on the agent stack.


        // easyprof operates by defining a per-thread linked-list-esque stack of agents such
        // that this stack aligns w/ the thread's call stack.

        static thread_local ProfilerAgent* _topAgent; // Top of the agent stack.

        inline static void _pushAgent(ProfilerAgent& agent) noexcept {
            assert(!agent._below);
            agent._below = _topAgent;
            _topAgent = &agent;
        }
        inline static void _popAgent() noexcept {
            if (auto agent = _topAgent) {
                _topAgent = agent->_below;
                agent->_below = nullptr;
            }
        }
    };

    inline thread_local ProfilerAgent* ProfilerAgent::_topAgent = nullptr;
}

