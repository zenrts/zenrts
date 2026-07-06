# ZenRTS — Agent Guide

## Workflow

- Todo el trabajo se hace en ramas feat/ o fix/. Nunca directamente en main.
- Cuando el usuario dice "Listo", se mergea a main: `git checkout main && git pull && git merge <rama> && git push origin main`
- Siempre preguntar antes de mergear si hay dudas.

## Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DZENRTS_BUILD_TESTS=ON -DZENRTS_BUILD_BENCHMARKS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
cmake --build build --target benchmark_zenrts -j$(nproc) && ./build/benchmarks/benchmark_zenrts
```

Para coverage:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DZENRTS_BUILD_TESTS=ON -DZENRTS_BUILD_COVERAGE=ON -DZENRTS_STATIC_LINK=OFF
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
lcov --capture -d build --output-file coverage.info --branch-coverage --ignore-errors unused,version,empty
lcov --remove coverage.info -o coverage.info '/usr/*' '*/_deps/*' '*/tests/*' '*/benchmarks/*' '*/googletest/*' '*/googlebenchmark/*' --ignore-errors unused,empty
lcov --list coverage.info --ignore-errors empty
```

## Code style

- **C++20**, sin comentarios en el código (el código se explica solo).
- `snake_case` para funciones y métodos, `PascalCase` para clases y tipos.
- `PascalCase` para nombres de tests (GTest convention).
- Usar `auto` donde el tipo sea obvio.
- Preferir `std::string_view` sobre `const std::string&` en parámetros de solo lectura.
- Preferir `[[nodiscard]]` donde tenga sentido.
- No abreviar nombres: `milliseconds`, `nanoseconds`, `microseconds`, `seconds`, no `millis`, `nanos`, etc.
- **Header-only** donde sea posible. Si hay un `.cpp`, mantenerlo mínimo.
- Headers públicos en `include/zenrts/`. Incluir con `#include <zenrts/nombre.h>`.
- El header principal `include/zenrts.h` incluye todos los módulos públicos.
- Tests en `tests/test_<modulo>.cpp`.
- Usar el namespace `zenrts`.

## Git

- Commit messages en inglés, formato `tipo: mensaje corto`.
  Ej: `feat: add time module with timer and scope_timer`
  Ej: `refactor(time): use full names for duration methods`
- No pushear a main. Solo a la rama de trabajo.
- Al empezar una tarea: `git checkout -b feat/<nombre>`.
- Al terminar: esperar instrucciones del usuario para mergear.

## Archivos nuevos

Todo archivo nuevo debe incluir el header AGPLv3 al inicio:

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

- El CI corre automáticamente en push/PR. No tocar los workflows sin autorización.
- Release job solo se activa con tags v*. Ahí no corre build ni sanitizers.
