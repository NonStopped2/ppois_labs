# PPOIS labs — C++20

Один репозиторий для лабораторных работ по ППОИС.

| Каталог                | Содержание                           |
| ---------------------- | ------------------------------------ |
| [lab1](lab1/README.md) | Кубик Рубика и машина Поста          |
| [lab2](lab2/README.md) | Следующая работа, ещё не реализована |
| [lab3](lab3/README.md) | Следующая работа, ещё не реализована |
| [lab4](lab4/README.md) | Следующая работа, ещё не реализована |

## Быстрый старт

Нужны CMake >= 3.20, компилятор C++20 и Python >= 3.9 для CLI-тестов.
Catch2 v3.8.1 загружается CMake автоматически с проверкой SHA-256.

```bash
cmake -S . -B build
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/lab1/rubiks_cube
./build/lab1/post_machine
./build/lab1/post_machine lab1/examples/post_increment.txt -log
```

Команды выполняются из корня репозитория. Для сборки без загрузки тестовой
библиотеки используйте `-DBUILD_TESTING=OFF` в отдельном build-каталоге.

## Документация, покрытие, качество

Полные команды, форматы файлов, ограничения и правила работы машины:
**[lab1/README.md](lab1/README.md)**.

- `cmake --build build --target docs` — Doxygen HTML (нужен установленный Doxygen).
- `python3 tools/quality.py` — clang-format 18.1.8 и cppcheck.
- `python3 tools/coverage.py --build-dir build/coverage` — тесты и реальный порог
  покрытия после отдельной конфигурации с `ENABLE_COVERAGE=ON`.
- `python3 tools/package.py --build-dir build --output-dir build/packages` —
  установка в отдельный staging-каталог, проверка запуска и архив приложений.

На macOS инструменты можно установить через Homebrew:

```bash
brew install cmake doxygen cppcheck
python3 -m venv build/tools
build/tools/bin/pip install clang-format==18.1.8
```

LLVM coverage-инструменты на macOS предоставляет установленный Xcode Command Line Tools.
На Ubuntu 24.04 используются `clang-18`, `llvm-18`, `cppcheck`, `doxygen` и
clang-format 18.1.8 из pip. Подробные команды есть в workflow.

## Процесс разработки

Работа ведётся в `feature/lab1`. Слияние в `main` — через Pull Request после
одобрения преподавателем. Коммиты имеют содержательные английские сообщения.

Workflow `.github/workflows/lab1.yml` проверяет сборку на Linux/macOS, тесты,
покрытие, качество и документацию. После слияния в `main` и успешных проверок
автоматически публикуется полноценный Release с новым тегом
`lab1-main-<SHA проверенного коммита>`. Выпуск по ручным тегам `lab1-vX.Y.Z` сохранён.
Прямой push в `main` тоже вызывает выпуск; правило одобренного PR обеспечивается
процессом и защитой ветки. Feature-ветки и PR выполняют только проверки.
Старые теги и релизы не меняются. Фактическую публикацию нужно проверить на GitHub:
локальные проверки не означают, что удалённый pipeline уже выполнялся.
