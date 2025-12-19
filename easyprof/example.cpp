

#include <cstdint>

#include "easyprof/easyprof.h"


// Put 'EASYPROF' at the top of ever fn you wish to have acknowledged by EasyProf.

template<typename... Args>
inline void println(std::format_string<Args...> fmt, Args&&... args) {
	EASYPROF;
	// NOTE: Disabling actual printing so we can call this a bunch of times w/out polluting the actual output.
	//easyprof::println(fmt, std::forward<Args>(args)...);
	(void)std::format(fmt, std::forward<Args>(args)...);
}

void foo() {
	EASYPROF;
	println("-- foo()");
}

void bar() {
	EASYPROF;
	for (size_t i = 0; i < 10; i++) {
		foo();
	}
}

size_t factorial(size_t n) {
	EASYPROF;
	return
		n >= 1
		? n * factorial(n - 1)
		: 1;
}


int32_t main(int32_t argc, char** argv) {
	easyprof::println("Simulating work. Just give it a bit...");

	// EasyProf is thread-safe in terms of being able to be used concurrently from
	// multiple threads, but it can't profile multiple threads at once.

	easyprof::Prof prof{};
	easyprof::start(prof);
	size_t workload = 31'142; // Arbitrary
	for (size_t i = 0; i < workload; i++) {
		bar();
		bar();
		bar();
		println("13! == {}", factorial(13));
	}
	easyprof::stop();
	easyprof::println("{}", prof.results());

	return EXIT_SUCCESS;
}

