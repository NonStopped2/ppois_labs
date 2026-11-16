# Сборка, автоматические проверки и пакеты lab3

Все команды из корня репозитория. Изолированная сборка:

```bash
cmake -S . -B build/lab3 -DBUILD_LAB1=OFF -DBUILD_LAB2=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build/lab3 --parallel 4
ctest --test-dir build/lab3 -L '^lab3-' --output-on-failure
python3 lab3/tools/quality.py
```

Catch2 3.8.1 и nlohmann/json 3.12.0 скачиваются по закреплённым версиям с SHA-256.
`BUILD_TESTING=OFF` исключает Catch2 и поиск Python из изолированной сборки приложения.
Для качества нужны clang-format **18.1.8**, cppcheck и C++20-компилятор.

## Раздельное покрытие C++ unit-тестами

В сборке с Clang/AppleClang включить `LAB3_ENABLE_COVERAGE=ON`:

```bash
python3 lab3/tools/coverage.py --build-dir build/lab3 --scope core
python3 lab3/tools/coverage.py --build-dir build/lab3 --scope persistence
python3 lab3/tools/coverage.py --build-dir build/lab3 --with-ui
```

Каждый режим требует **строго >90% исполняемых строк**. Core измеряется без UI,
JSON и Python CLI-сценариев; persistence — только собственный файловый/JSON-код,
без nlohmann. Третий режим — core с приложением по C++ unit/UI-тестам.
Это не покрытие всех ветвей или входных данных. Сторонние библиотеки/тестовые
исходники/другие работы исключены. Профили каждого запуска изолированы.
Форматирование и статический анализ не являются автоматическим подтверждением
каждого стилистического пункта чеклиста: некоторые координирующие функции,
командные обработчики и тесты длиннее ориентира в 20 строк.

`LAB3_ENABLE_SANITIZERS=ON` включает ASan/UBSan. Внешняя проверка именно бинарника:

```bash
python3 lab3/tools/cli_tests.py build/lab3/lab3/shop_cli
```

## Doxygen и переносимый нативный пакет

```bash
cmake --build build/lab3 --target lab3_docs
python3 lab3/tools/package.py --build-dir build/lab3 --output-dir build/lab3-packages --docs-only
cmake -S . -B build/lab3-release -DBUILD_LAB1=OFF -DBUILD_LAB2=OFF -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build build/lab3-release --target shop_cli --parallel 4
python3 lab3/tools/package.py --build-dir build/lab3-release --output-dir build/lab3-packages
```

На macOS для минимальной цели 14.0 добавить `-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0`.
Пакетный установщик использует только CMake-компонент `lab3` и проверяет точный
состав: бинарник, руководства, примеры, RUNNING и MIT-лицензия JSON-зависимости.
Catch2, исходники других работ, AGENTS.md и личные файлы в пакет не входят.
Инструментация/санитайзеры/shared libraries для исполняемого релизного пакета запрещены.

Приложение переносится в другой каталог и тестируется; затем архив распаковывается
и тестируется повторно. Для запуска пользователю не нужны CMake, Python или IDE.
Имена: `ppois-lab3-linux-x86_64.tar.gz`, `ppois-lab3-macos-arm64.tar.gz`,
`ppois-lab3-docs.tar.gz`; рядом сохраняется SHA-256.

Проверка уже скачанного архива без пересборки:

```bash
python3 lab3/tools/package.py --verify-archive path/to/ppois-lab3-macos-arm64.tar.gz
```

## CI и публикация

`.github/workflows/lab3.yml` — отдельная проверка lab3; зелёный lab1/lab2 не
подтверждает её готовность. Проверки охватывают Linux/macOS, санитайзеры,
раздельное покрытие, качество, CLI, документацию и пакеты.

Согласованы две схемы:

- Ручной тег `lab3-v0.1.1` из `feature/lab3` выпускает исправляющий **Pre-release**,
  не выполняя merge. Workflow принимает теги формата `lab3-vMAJOR.MINOR.PATCH`.
- Push в `main` после одобренного merge создаёт стабильный
  `lab3-main-<полный SHA>` только при изменениях lab3 или общей зависимой
  инфраструктуры. Изменения только lab1/lab2 не выпускают lab3 заново.

Release-job зависит от `changes`, обеих платформ `build` и `quality`:
публикация невозможна при неуспехе зависимых jobs. Перед публикацией проверяются
три архива и их SHA-256. События pull_request, feature-push и workflow_dispatch
не публикуют релиз. Проверки PR и push — отдельные запуски, их статус различается.
Нехватка hosted macOS runner не означает ошибку приложения; такой запуск
не считается успешным и при необходимости повторяется без ослабления проверок.

Слияние PR выполняет пользователь. Пользователь подтвердил разрешение преподавателя
мерджить самостоятельно; агент merge не выполняет.
Опубликованные теги не переставляются; main-теги однозначно привязаны к проверенному
коммиту. Подготовка пакета/артефакт Actions не равны публикации в Releases.
После публикации нужно скачать именно релизный пакет, проверить SHA-256 и
запустить CLI-проверки. Linux-архив запускается только на Linux; осмотр ELF
на macOS не считается запуском.
Зависимая задача `Verify published Linux package` автоматически скачивает
Linux-архив и документацию уже из Releases, проверяет суммы и запускает CLI
распакованного Linux-бинарника. macOS-пакет дополнительно проверяется на macOS.

Первый предварительный выпуск:
[lab3-v0.1.0](https://github.com/NonStopped2/ppois_labs/releases/tag/lab3-v0.1.0).
Исправляющий выпуск:
[lab3-v0.1.1](https://github.com/NonStopped2/ppois_labs/releases/tag/lab3-v0.1.1).
Даты коммитов реальные; Release не означает защиты или одобрения merge.
