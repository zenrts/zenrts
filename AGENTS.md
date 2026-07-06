# ZenRTS — Agent Guide

## Workflow

- All work is done on `feat/` or `fix/` branches. Never directly on `main`.
- When the user says "Listo", merge to main: `git checkout main && git pull && git merge <branch> && git push origin main`
- Always ask before merging if there are doubts.

## Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DZENRTS_BUILD_TESTS=ON -DZENRTS_BUILD_BENCHMARKS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
cmake --build build --target benchmark_zenrts -j$(nproc) && ./build/benchmarks/benchmark_zenrts
```

For coverage:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DZENRTS_BUILD_TESTS=ON -DZENRTS_BUILD_COVERAGE=ON -DZENRTS_STATIC_LINK=OFF
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
lcov --capture -d build --output-file coverage.info --branch-coverage --ignore-errors unused,version,empty
lcov --remove coverage.info -o coverage.info '/usr/*' '*/_deps/*' '*/tests/*' '*/benchmarks/*' '*/googletest/*' '*/googlebenchmark/*' --ignore-errors unused,empty
lcov --list coverage.info --ignore-errors empty
```

## Code style

- **C++20**, no comments in code (the code explains itself).
- `snake_case` for functions and methods, `PascalCase` for classes and types.
- `PascalCase` for test names (GTest convention).
- Use `auto` where the type is obvious.
- Prefer `std::string_view` over `const std::string&` for read-only parameters.
- Prefer `[[nodiscard]]` where it makes sense.
- No abbreviated names: `milliseconds`, `nanoseconds`, `microseconds`, `seconds`, not `millis`, `nanos`, etc.
- **Header-only** where possible. If a `.cpp` is needed, keep it minimal.
- Public headers in `include/zenrts/`. Include with `#include <zenrts/name.h>`.
- The main header `include/zenrts.h` includes all public modules.
- Tests in `tests/test_<module>.cpp`.
- Use the `zenrts` namespace.

## Git

- Commit messages in English, format `type: short message`.
  Eg: `feat: add time module with timer and scope_timer`
  Eg: `refactor(time): use full names for duration methods`
- Do not push to main. Only to the working branch.
- When starting a task: `git checkout -b feat/<name>`.
- When done: wait for user instructions before merging.

## New files

Every new file must include the AGPLv3 header at the top:

```cpp
// ZenRTS
// Copyright (C) 2026  Ian Torres
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License version 3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
```

## CI

- CI runs automatically on push/PR. Do not touch workflows without authorization.
- Release job only triggers on tags v*. Build and sanitizer jobs are skipped on tags.
