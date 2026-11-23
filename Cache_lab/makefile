all: cache_size cache_line_size cache_associativity matrix

cache_size: src/cache_size.cpp
	g++ -std=c++17 -O2 -o build/cache_size src/cache_size.cpp

cache_line_size: src/cache_line_size.cpp
	g++ -std=c++17 -O2 -o build/cache_line_size src/cache_line_size.cpp

cache_associativity: src/cache_associativity.cpp
	g++ -std=c++17 -O2 -o build/cache_associativity src/cache_associativity.cpp

matrix: src/matrix.cpp
	g++ -std=c++11 -O0 src/matrix.cpp -o build/matrix_mul

optional: cache_policy_batch cache_write_phases

cache_policy_batch: src/cache_policy_batch.cpp src/cache_policy_common.h
	g++ -std=c++17 -O2 src/cache_policy_batch.cpp -o build/cache_policy_batch

cache_write_phases: src/cache_write_phases.cpp src/cache_policy_common.h
	g++ -std=c++17 -O2 src/cache_write_phases.cpp -o build/cache_write_phases
