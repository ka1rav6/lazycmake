# LazyCMake AGENTS.md

Based on the project structure and requirements, here are the recommended practices for this CMake development project:

## Code Quality Guidelines

### 1. Commit Regularly and Properly
- Create feature branches for each major change or fix
- Write descriptive commit messages with file:line numbers where applicable
- Follow conventional commits where possible
- Run tests before each commit
- Use meaningful commit types (feat, fix, docs, style, refactor, test, chore)

### 2. Add Proper Comments (Beginner-Friendly)
- Each public class should have a brief comment explaining its purpose
- Methods should have docstrings describing parameters and return values
- Technical decisions should be explained inline
- Use simple language to explain complex concepts
- Include TODO/FIXME comments for incomplete work

Example comment style:
```cpp
// Project — represents a CMake project with its sources, targets, and configuration.
// This class handles loading/saving project manifests, managing targets, and
calculating build dependencies.
class Project {
private:
    std::string rootDir_;  // Project root directory path
    std::vector<Target> targets_;  // List of build targets
    std::vector<DependencySpec> dependencies_;  // External dependencies
};
```

### 3. Always Build and Try It Out
- Test changes by building the project before committing
- Run the example programs to verify functionality
- Use the TUI by running the executable (if built with TUI enabled)
- Validate with tests if available

### 4. Make Everything Work Perfectly
- Ensure all code compiles without warnings in production mode
- Handle edge cases in APIs (null checks, bounds checking)
- Test interactions between components
- Validate user input where applicable

### 5. Before Stopping, Recheck for Bugs
- Run the full test suite before ending work
- Check for memory leaks in long-running processes
- Verify state transitions and error handling
- Look for potential race conditions in async code

### 6. Set Up Valgrind and Check for Memory Leaks
- Install valgrind: `apt-get install valgrind` (Linux) or appropriate package
- After running the TUI for a while, run: `valgrind --leak-check=full ./lazycmake`
- Common leaks to watch for:
  - Not deleting dynamically allocated objects
  - Forgetting to clear container elements
  - Not unsubscribing from event buses

Example valgrind check:
```bash
# Build with debug symbols
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4

# Run for a short time
./lazycmake <project_dir> &
VALGRIND_PID=$!
sleep 5
kill $VALGRIND_PID

# Check for leaks
valgrind --leak-check=full --log-file=valgrind.log ./lazycmake <project_dir>
cat valgrind.log | grep "ERROR SUMMARY"
```

### 7. Create Tests for Every Feature
- Each header file should have corresponding test files
- Test public API surface thoroughly
- Test edge cases and error conditions
- Mock external dependencies for unit testing
- Ensure tests run consistently

Test structure example (using Catch2):
```cpp
// Test file: src/core/project_test.cpp
#include "core/project.hpp"  // Include the class being tested
#include "test_utils.hpp"   // Your test helper utilities

TEST_CASE("Project::setRootDir", "[project]" ) {
    Project project;
    
    GIVEN("An empty project") {
        REQUIRE(project.rootDir().empty());
        
        WHEN("Setting a valid root directory") {
            bool result = project.setRootDir("/tmp/test_project");
            THEN("The directory should be set correctly") {
                REQUIRE(result);
                REQUIRE(project.rootDir() == "/tmp/test_project");
            }
        }
    }
}
```

## Project-Specific Guidelines

### File Structure Conventions
- Headers (.hpp) should contain declarations only
- Implementations (.cpp) in the same directory as headers
- Forward headers for frequently included headers

### Code Style
- Use camelCase for variable and method names
- Use PascalCase for class names
- Use ALL_CAPS for constants and macros
- Maximum line length: 100 characters
- Indent with 4 spaces

### Safety Considerations
- Use const where parameters are not modified
- Prefer references over pointers for existing objects
- Use smart pointers (unique_ptr, shared_ptr) for ownership
- Validate input parameters
- Check for null pointers

## Running Tests
```bash
# Configure and build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=20 ..
make -j4

# Run tests (if enabled with LAZYCMAKE_BUILD_TESTS)
ctest --output-on-failure

# Or run specific test executables
./lazycmake_core_tests
```

## Building Examples
```bash
# Examples are built separately with LAZYCMAKE_BUILD_EXAMPLES
./examples/01_basic_project_generation
./examples/02_event_bus_usage
./examples/03_config_system_demo
./examples/04_build_pipeline_demo
```

## TUI Development
```bash
# Build with TUI enabled (default)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DLAZYCMAKE_BUILD_TUI=ON ..

# Run the TUI application
./lazycmake <project_directory>

# Keybindings (from main_workspace.cpp):
# 'q'    - Exit the application
# 'tab'  - Navigate between panels
# 'j/k'  - Navigate within panels
# 'b'    - Open build overlay
# 'r'    - Open run overlay
# 'd'    - Open dependency dialog
# 'h'    - Open help overlay
```

