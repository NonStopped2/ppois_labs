# PPOIS labs — C++20

Один репозиторий для лабораторных работ по ППОИС.

| Каталог | Содержание |
| --- | --- |
| [lab1](lab1/README.md) | Кубик Рубика и машина Поста |
| [lab2](lab2/README.md) | Почта: оформление, оплата, хранение, сортировка, перевозка, выдача и курьерская доставка отправлений |
| [lab3](lab3/README.md) | Интернет-магазин: каталог, цены, купоны, склад, покупки, доставка, возвраты, отзывы, отчёты и JSON-снимки |
| [lab4](lab4/README.md) | Следующая работа, ещё не реализована |

В этой версии объединены реализации lab1, lab2 и lab3. Наличие реализации, успешного
CI или релиза не означает прохождения защиты и принятия работы преподавателем.

## Быстрый старт

Нужны CMake ≥3.20, компилятор C++20 и Python ≥3.9 для вспомогательных проверок.
Catch2 v3.8.1 и nlohmann/json v3.12.0 для lab3 загружаются CMake с проверкой SHA-256;
при первой сборке тестов нужен доступ к сети.

Команды выполняются **из корня репозитория**:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/lab1/rubiks_cube
./build/lab1/post_machine lab1/examples/post_increment.txt -log
./build/lab2/post_cli --demo
./build/lab3/shop_cli --demo
```

У `post_cli` флаг `--demo` добавляет примеры клиентов и отправлений. Без него приложение
начинает работу с пустыми пользовательскими данными. Справка:

```bash
./build/lab2/post_cli --help
```

Для сборки только приложения, без Python и загрузки Catch2:

```bash
cmake -S . -B build/lab2-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DBUILD_LAB3=OFF
cmake --build build/lab2-release --target post_cli --parallel 4
./build/lab2-release/lab2/post_cli --demo
```

## Документация, покрытие, качество

- **[lab1/README.md](lab1/README.md)** — команды, форматы и ограничения кубика и машины Поста.
- **[lab2/README.md](lab2/README.md)** — предметная модель, классы, поля,
  поведения, связи, исключения, ограничения и результаты проверок.
- **[lab2/CLI.md](lab2/CLI.md)** — меню, команды и демонстрационные сценарии.
- **[lab2/BEHAVIORS.md](lab2/BEHAVIORS.md)** — обоснование подсчёта содержательных поведений.
- **[lab2/CI.md](lab2/CI.md)** — сборка, автоматические проверки, пакеты и выпуск.
- **[lab3/README.md](lab3/README.md)** — интернет-магазин и инструкции сборки.
- **[lab3/docs/CLI.md](lab3/docs/CLI.md)** — команды, роли и воспроизводимые сценарии.
- **[lab3/docs/BEHAVIORS.md](lab3/docs/BEHAVIORS.md)** — контракты и типизированные связи.
- **[lab3/docs/SNAPSHOTS.md](lab3/docs/SNAPSHOTS.md)** — полное сохранение/восстановление.
- **[lab3/docs/CI.md](lab3/docs/CI.md)** — отдельные проверки и пакеты lab3.

Основные команды:

```bash
python3 tools/quality.py
cmake --build build --target docs
python3 lab2/tools/quality.py
cmake --build build --target lab2_docs
python3 lab2/tools/package.py --build-dir build/lab2-release --output-dir build/lab2-packages
```

Для проверки качества нужны clang-format 18.1.8 и cppcheck,
для генерации HTML-документации — Doxygen.

Покрытие измеряется в отдельной сборке с Clang/AppleClang и совместимыми
`llvm-cov` / `llvm-profdata`:

```bash
cmake -S . -B build/lab2-coverage -DCMAKE_BUILD_TYPE=Debug -DLAB2_ENABLE_COVERAGE=ON -DBUILD_LAB3=OFF
cmake --build build/lab2-coverage --target post_core_tests post_ui_tests post_cli --parallel 4
python3 lab2/tools/coverage.py --build-dir build/lab2-coverage
python3 lab2/tools/coverage.py --build-dir build/lab2-coverage --with-ui
```

Первый режим проверяет предметную модель, второй — модель вместе с приложением.
В обоих случаях используются C++ unit-тесты и проверяется строгий порог
**более 90% строк**; внешние CLI-сценарии не увеличивают эти показатели.
Эти проверки относятся к lab2, а не ко всем лабораторным репозитория.

Для покрытия lab1 создайте отдельную сборку с `-DENABLE_COVERAGE=ON`,
соберите её и запустите `python3 tools/coverage.py --build-dir <каталог-сборки>`.
Упаковка lab1: `python3 tools/package.py --build-dir <каталог-сборки> --output-dir build/lab1-packages`.
Каждый упаковщик устанавливает только компонент своей лабораторной.

На macOS необходимые инструменты можно установить так:

```bash
brew install cmake doxygen cppcheck
python3 -m venv build/tools
build/tools/bin/pip install clang-format==18.1.8
```

LLVM coverage-инструменты предоставляет Xcode Command Line Tools.

## Процесс разработки

Работа ведётся в отдельных feature-ветках, слияние в `main` — через Pull Request.
Для lab1 и lab2 преподаватель разрешил слияние до review; это отдельное
разрешение на Git-процесс, а не подтверждение сдачи этих работ.

Workflow [`.github/workflows/lab1.yml`](.github/workflows/lab1.yml) проверяет
lab1 на Linux/macOS, покрытие, качество и Doxygen. После затрагивающего lab1
слияния в `main` и успешных проверок публикуется Release с тегом
`lab1-main-<SHA проверенного коммита>`. Ручные теги `lab1-vX.Y.Z` также поддерживаются.

Workflow [`.github/workflows/lab2.yml`](.github/workflows/lab2.yml)
проверяет сборку и тесты на Linux/macOS, покрытие, качество и документацию.
После слияния в `main` и успешных проверок публикуется полноценный Release
с тегом `lab2-main-<SHA проверенного коммита>`, только если изменились lab2
или общая сборочная/релизная инфраструктура. Изменения только lab1 не перевыпускают lab2.
Анализируется весь диапазон push; проверки от этой фильтрации не отключаются.
Ручные теги `lab2-vX.Y.Z` сохраняют прежний режим предварительного выпуска
и не зависят от фильтра путей.
Пакеты доступны в [Releases](https://github.com/NonStopped2/ppois_labs/releases).

Релиз относится к коммиту своего тега: последующие изменения ветки
не обновляют опубликованные архивы автоматически. Актуальные сведения
о версиях и проверенных пакетах приведены в [документации lab2](lab2/README.md).

Изменения только одной лабораторной не должны перевыпускать другую. Для общих
сборочных и релизных файлов проверяется полный диапазон push. Feature-ветки и PR
выполняют только проверки, старые теги не переставляются.
