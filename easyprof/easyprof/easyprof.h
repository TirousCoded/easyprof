

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


// TODO: Optimize easyprof w/ regards to API processing time overhead.
// TODO: Decide upon how, and then impl, multi-threading support.


// NOTE: Be sure to keep this updated as we modify the frontend.

// EasyProf API Version (Major/Minor)
#define EASYPROF_VERSION "EasyProf version 1.0"


#if defined(__GNUC__) || defined(__clang__)
#define EASYPROF_FUNCSIG __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define EASYPROF_FUNCSIG __FUNCSIG__
#else
#error "Unknown compiler!"
#endif


// Used for unreachable code paths.
#define EASYPROF_DEADEND (assert(false))


namespace easyprof {}
namespace _easyprof { // Impl detail namespace.
    using namespace easyprof;
    class Agent;
}


// Place this at the vary start of each and every function to be profiled.
// Functions without this won't be acknowledged by the profiler.
// Undefined behaviour if multiple EASPROF(s) are used, or if it's misplaced.
#define EASYPROF ::_easyprof::Agent _easyprof_agent(__LINE__, __FILE__, EASYPROF_FUNCSIG)


namespace easyprof {
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


    inline std::string fmtBigInt(size_t n) {
        auto fmtHelper = [](size_t n, size_t scale, char suffix) -> std::string {
            auto integerPart = n / scale;
            auto decimalPart = (n % scale) / (scale / 10);
            return std::format("{}.{}{}", integerPart, decimalPart, suffix);
            };
        if (n >= 1'000'000'000)     return fmtHelper(n, 1'000'000'000, 'B');
        else if (n >= 1'000'000)    return fmtHelper(n, 1'000'000, 'M');
        else if (n >= 1'000)        return fmtHelper(n, 1'000, 'K');
        else                        return std::format("{}", n);
    }

    using Seconds = double;

    inline std::string fmtSeconds(Seconds s) {
        if (s < 0.000'001)  return std::format("{}ns", (float)(s * 1'000'000'000.0)); // nano
        else if (s < 0.001) return std::format("{}us", (float)(s * 1'000'000.0)); // micro
        else if (s < 1.0)   return std::format("{}ms", (float)(s * 1'000.0)); // milli
        else                return std::format("{}s", (float)s);
    }


    // NOTE: Stole this timer impl from TheCherno's Hazel Engine.

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

    class Stopwatch final {
    public:
        Stopwatch() = default;


        inline Seconds elapsed() const noexcept {
            return _elapsed;
        }
        inline void start() noexcept {
            if (_active) {
                stop();
            }
            _active = true;
            _timer.reset();
        }
        inline void stop() noexcept {
            if (_active) {
                _elapsed += _timer.elapsed();
            }
            _active = false;
        }
        inline void reset() noexcept {
            stop();
            _elapsed = 0.0;
        }


    private:
        bool _active = false;
        Timer _timer;
        Seconds _elapsed = 0.0;
    };


    enum class SortBy : uint8_t {
        Internal = 0,   // Sort by internal time.
        Cumulative,     // Sort by cumulative time.
        Calls,          // Sort by call count.
    };

    struct Result final {
        size_t              line        = 0;    // Line Number (of agent, not the function itself.)
        std::string_view    file;	            // File Path
        std::string_view    function;           // Function Signature 
        size_t              calls       = 0;    // The number of times the function was called.
        Seconds             internal    = 0.0;  // Total exec time of all calls, minus subprocedure time.
        Seconds             cumulative  = 0.0;	// Total exec time of all calls, including subprocedure time.


        inline Seconds internalPerCall() const noexcept { return internal / Seconds(calls); }
        inline Seconds cumulativePerCall() const noexcept { return cumulative / Seconds(calls); }

        inline std::string name() const {
            return std::format("{}:{}:{}",
                std::filesystem::proximate((std::string)file).string(),
                line,
                (std::string)function);
        }

        inline std::string fmt() const {
            return std::format("{: <12} {: <12} {: <12} {: <12} {: <12} {}",
                fmtBigInt(calls),
                fmtSeconds(internal),
                fmtSeconds(internalPerCall()),
                fmtSeconds(cumulative),
                fmtSeconds(cumulativePerCall()),
                name());
        }
    };
}

template<>
struct std::formatter<easyprof::Result> final : public std::formatter<std::string> {
    auto format(const easyprof::Result& x, format_context& ctx) const {
        return formatter<string>::format(x.fmt(), ctx);
    }
};
namespace std {
    inline std::ostream& operator<<(std::ostream& stream, const easyprof::Result& x) {
        return stream << x.fmt();
    }
}

namespace easyprof {
    class Results final {
    public:
        using Iterator = std::vector<Result>::const_iterator;


        inline Results(Seconds apiOverhead = 0.0) noexcept :
            _apiOverhead(apiOverhead) {}

        Results(const Results&) = default;
        Results(Results&&) noexcept = default;
        ~Results() noexcept = default;
        Results& operator=(const Results&) = default;
        Results& operator=(Results&&) noexcept = default;


        // Returns the total number of function calls recorded.
        inline size_t calls() const noexcept { return _totalCalls; }

        inline Iterator cbegin() const noexcept { return _results.begin(); }
        inline Iterator begin() const noexcept { return cbegin(); }
        inline Iterator cend() const noexcept { return _results.end(); }
        inline Iterator end() const noexcept { return cend(); }

        inline size_t size() const noexcept { return _results.size(); }

        inline const Result& at(size_t index) const { return _results.at(index); }
        inline const Result& operator[](size_t index) const noexcept { return _results[index]; }


        inline void add(Result result) {
            _results.push_back(result);
            _totalCalls += result.calls;
        }

        // Predicate is the same as the predicate of std::sort.
        template<typename Predicate>
        inline void sort(const Predicate& predicate) {
            std::sort(_results.begin(), _results.end(), predicate);
        }
        inline void sort(SortBy sortBy) {
            auto internalPred = [](const Result& a, const Result& b) -> bool { return a.internal > b.internal; };
            auto cumulativePred = [](const Result& a, const Result& b) -> bool { return a.cumulative > b.cumulative; };
            auto callsPred = [](const Result& a, const Result& b) -> bool { return a.calls > b.calls; };
            switch (sortBy) {
            case SortBy::Internal:      sort(internalPred);     break;
            case SortBy::Cumulative:    sort(cumulativePred);   break;
            case SortBy::Calls:         sort(callsPred);        break;
            default:                    EASYPROF_DEADEND;       break;
            }
        }

        inline std::string fmt() const {
            std::string output{};
            output += std::format("EasyProf Results (fns: {}, calls: {}, API overhead: {})",
                size(), fmtBigInt(calls()), fmtSeconds(_apiOverhead));
            // Each of the words below (calls, internal, per-call, etc.), plus the whitespace ahead of it,
            // should take up 13 characters.
            output += std::format("\ncalls        internal     per-call     cumulative   per-call     file:line:funcsig");
            for (const auto& result : *this) {
                output += std::format("\n{}", result);
            }
            return output;
        }


    private:
        std::vector<Result> _results;
        size_t _totalCalls = 0;
        Seconds _apiOverhead = 0.0;
    };
}

template<>
struct std::formatter<easyprof::Results> final : public std::formatter<std::string> {
    auto format(const easyprof::Results& x, format_context& ctx) const {
        return formatter<string>::format(x.fmt(), ctx);
    }
};
namespace std {
    inline std::ostream& operator<<(std::ostream& stream, const easyprof::Results& x) {
        return stream << x.fmt();
    }
}

namespace easyprof {
    class Prof final {
    public:
        Prof() = default;


        // Returns the profiling results.
        // Undefined behaviour if this is called while the profiler is still running.
        inline Results results() {
            assert(current() != this); // Doesn't cover if *this is current in another thread.
            Results output(_apiOverhead.elapsed());
            for (const auto& [funcsig, result] : _results) {
                output.add(result);
            }
            return output;
        }

        // Resets profiler state.
        // Undefined behaviour if this is called while the profiler is still running.
        inline void reset() noexcept {
            assert(current() != this); // Doesn't cover if *this is current in another thread.
            _results.clear();
            _apiOverhead.reset();
        }


        // Returns the currently running profiler for this thread, if any.
        inline static Prof* current() noexcept {
            return _current;
        }


    private:
        friend class _easyprof::Agent;
        friend void start(Prof& profiler) noexcept;
        friend void stop() noexcept;


        // Maps function signature to corresponding results.
        // This is why it's important for Agent ctor arg strings to be to memory which will be
        // good for the lifetime of the program.
        std::unordered_map<std::string_view, Result> _results;

        // This is used to measure API processing time overhead.
        Stopwatch _apiOverhead;


        static thread_local Prof* _current;
    };

    inline thread_local Prof* Prof::_current = nullptr;


    // Start (or resume) a profiler for this thread.
    // Any currently running profiler for this thread will be stopped.
    // Profiling acknowledges functions when they exit, thus functions started prior to start being called may be recorded.
    inline void start(Prof& profiler) noexcept {
        Prof::_current = &profiler;
    }

    // Stops the profiler running for this thread, if any.
    inline void stop() noexcept {
        Prof::_current = nullptr;
    }
}

namespace _easyprof {
    class Agent final {
    public:
        // file and function are expected to be C-strings who's lifetime extends until the
        // end of program execution (ie. they should be in static readonly memory.)
        inline Agent(size_t line, std::string_view file, std::string_view function) :
            _line(line),
            _file(file),
            _function(function) {
            _startTrackAPIOverhead();
            // Make this agent the new top agent.
            _pushAgent(*this);
            _stopTrackAPIOverhead();
        }

        inline ~Agent() noexcept { // RAII
            _startTrackAPIOverhead();
            Seconds finalCumulative = _cumulative.elapsed();
            // Remove this agent from agent stack.
            _popAgent();
            // If there was an agent below us on the agent stack, then propagate our
            // cumulative time to their subprocedure time.
            if (_topAgent) {
                _topAgent->_subprocedure += finalCumulative;
            }
            // If there's a running profiler on this thread, write our results to it.
            if (auto prof = Prof::current()) {
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
            _stopTrackAPIOverhead();
        }


    private:
        size_t _line;
        std::string_view _file;
        std::string_view _function;

        Timer _cumulative; // Time of procedure and all subprocedures.
        Seconds _subprocedure = 0.0; // Time of subprocedures.

        Agent* _below = nullptr; // Agent below this one on the agent stack.


        // easyprof operates by defining a per-thread linked-list-esque stack of agents such
        // that this stack aligns w/ the thread's call stack.

        static thread_local Agent* _topAgent; // Top of the agent stack.

        inline static void _pushAgent(Agent& agent) noexcept {
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


        inline static void _startTrackAPIOverhead() noexcept {
            if (Prof::_current) {
                Prof::_current->_apiOverhead.start();
            }
        }
        inline static void _stopTrackAPIOverhead() noexcept {
            if (Prof::_current) {
                Prof::_current->_apiOverhead.stop();
            }
        }
    };

    inline thread_local Agent* Agent::_topAgent = nullptr;
}

