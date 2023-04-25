g++ -o ./tests/test-progs/bench/bin/x86/linux/bench ./tests/test-progs/bench/src/main.cpp -pthread -O0 -std=c++11
./build/X86/gem5.opt configs/tutorial/multicore-x86.py
cp ./m5out/stats.txt ./m5out/simp.txt
g++ -o ./tests/test-progs/bench/bin/x86/linux/bench ./tests/test-progs/bench/src/main.cpp -pthread -O0 -std=c++11 -D FETCH
./build/X86/gem5.opt configs/tutorial/multicore-x86.py
cp ./m5out/stats.txt ./m5out/bench.txt