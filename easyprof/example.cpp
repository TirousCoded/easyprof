

#include <cstdint>

#include "easyprof/easyprof.h"


template<typename... Args>
inline void println(std::format_string<Args...> fmt, Args&&... args) {
	EASYPROF;
	easyprof::println(fmt, std::forward<Args>(args)...);
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
	easyprof::Profiler prof{};
	easyprof::start(prof);
	bar();
	bar();
	bar();
	println("13! == {}", factorial(13));
	easyprof::stop();
	easyprof::println("{}", easyprof::fmt(prof.results()));
	return EXIT_SUCCESS;
}

