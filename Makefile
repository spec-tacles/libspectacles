.PHONY: lint
lint:
	./cpplint.py --linelength=200 --filter=-runtime/explicit,-build/c++11,-runtime/threadsafe_fn,-build/header_guard include/*.h include/etf/etf.h include/etf/data.h src/*.cc
	clang-tidy include/*.h include/etf/data.h src/*.cc -extra-arg-before=-xc++ -checks=-*,google-*,-google-explicit-constructor -warnings-as-errors=* -- -std=c++11