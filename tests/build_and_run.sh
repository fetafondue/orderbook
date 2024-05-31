# build test folder
cmake -S . -B test
cmake --build test
# run tests
cd test && ctest