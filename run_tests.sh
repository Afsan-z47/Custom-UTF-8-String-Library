
#!/bin/bash

# Clean previous builds and coverage data
echo "Cleaning previous builds..."
rm -f utf8_tests *.gcda *.gcno coverage.info
rm -rf coverage_report

# Compile with coverage instrumentation and debug info
echo "Compiling with coverage flags..."
gcc -fsanitize=address -fprofile-arcs -ftest-coverage -Wall -Werror -g utf8_string.c test_utf8.c -o utf8_tests -lgcov

# Check if compilation succeeded
if [ ! -f "utf8_tests" ]; then
    echo "Compilation failed!"
    exit 1
fi

# Run tests
echo "Running tests..."
./utf8_tests

# Ensure coverage data is generated
echo "Checking for coverage data..."
if ls *.gcda 1>/dev/null 2>&1; then
    echo "Coverage data found."
else
    echo "Error: No .gcda files found!"
    exit 1
fi

# Generate coverage report
echo "Generating coverage report..."
lcov --capture --directory . --output-file coverage.info --ignore-errors mismatch,empty \
    --gcov-tool $(which gcov) --rc branch_coverage=1

genhtml coverage.info --output-directory coverage_report \
    --branch-coverage --title "UTF-8 Library Coverage"

# Open report if generated
if [ -f "coverage_report/index.html" ]; then
    echo "Opening coverage report..."
#    xdg-open coverage_report/index.html
else
    echo "Failed to generate coverage report!"
    exit 1
fi

echo "Script completed successfully!"

