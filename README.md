# PPOIS labs — C++20

Один репозиторий для лабораторных работ по ППОИС.
В этой ветке разрабатывается **лабораторная №2 — «Почта»**.

| Каталог | Содержание |
| --- | --- |
| [lab1](lab1/) | Кубик Рубика и машина Поста; версия первой работы — в ветке [`feature/lab1`](https://github.com/NonStopped2/ppois_labs/tree/feature/lab1) |
| [lab2](lab2/README.md) | Почта: оформление, оплата, хранение, сортировка, перевозка, выдача и курьерская доставка отправлений |
| [lab3](lab3/README.md) | Следующая работа, ещё не реализована |
| [lab4](lab4/README.md) | Следующая работа, ещё не реализована |

Ветки содержат версии всего репозитория, а не только отдельной лабораторной.
Наличие каталога не означает, что соответствующая работа уже реализована
или одобрена преподавателем. Полная версия первой работы не переносится
в эту ветку автоматически.

## Быстрый старт

Нужны CMake ≥3.20, компилятор C++20 и Python ≥3.9 для вспомогательных проверок.
Catch2 v3.8.1 загружается CMake автоматически с проверкой SHA-256;
при первой сборке тестов нужен доступ к сети.

Команды выполняются **из корня репозитория**:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target post_cli post_core_tests post_ui_tests --parallel 4
ctest --test-dir build -L '^lab2($|-)' --output-on-failure
./build/lab2/post_cli --demo
```

`--demo` добавляет примеры клиентов и отправлений. Без этого флага приложение
начинает работу с пустыми пользовательскими данными. Справка:

```bash
./build/lab2/post_cli --help
```

Для сборки только приложения, без Python и загрузки Catch2:

```bash
cmake -S . -B build/lab2-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/lab2-release --target post_cli --parallel 4
./build/lab2-release/lab2/post_cli --demo
```

## Документация, покрытие, качество

- **[lab2/README.md](lab2/README.md)** — предметная модель, классы, поля,
  поведения, связи, исключения, ограничения и результаты проверок.
- **[lab2/CLI.md](lab2/CLI.md)** — меню, команды и демонстрационные сценарии.
- **[lab2/BEHAVIORS.md](lab2/BEHAVIORS.md)** — обоснование подсчёта содержательных поведений.
- **[lab2/CI.md](lab2/CI.md)** — сборка, автоматические проверки, пакеты и выпуск.

Основные команды:

```bash
python3 lab2/tools/quality.py
cmake --build build --target lab2_docs
python3 lab2/tools/package.py --build-dir build/lab2-release --output-dir build/lab2-packages
```

Для проверки качества нужны clang-format 18.1.8 и cppcheck,
для генерации HTML-документации — Doxygen.

Покрытие измеряется в отдельной сборке с Clang/AppleClang и совместимыми
`llvm-cov` / `llvm-profdata`:

```bash
cmake -S . -B build/lab2-coverage -DCMAKE_BUILD_TYPE=Debug -DLAB2_ENABLE_COVERAGE=ON
cmake --build build/lab2-coverage --target post_core_tests post_ui_tests post_cli --parallel 4
python3 lab2/tools/coverage.py --build-dir build/lab2-coverage
python3 lab2/tools/coverage.py --build-dir build/lab2-coverage --with-ui
```

Первый режим проверяет предметную модель, второй — модель вместе с приложением.
В обоих случаях используются C++ unit-тесты и проверяется строгий порог
**более 90% строк**; внешние CLI-сценарии не увеличивают эти показатели.
Эти проверки относятся к lab2, а не ко всем лабораторным репозитория.

## Процесс разработки

Работа ведётся в `feature/lab2`. Слияние в `main` — через Pull Request
после одобрения преподавателем.

Workflow [`.github/workflows/lab2.yml`](.github/workflows/lab2.yml)
проверяет сборку и тесты на Linux/macOS, покрытие, качество и документацию.
Релиз публикуется при отправке тега `lab2-v*` после успешных проверок.
Пакеты доступны в [Releases](https://github.com/NonStopped2/ppois_labs/releases).

Релиз относится к коммиту своего тега: последующие изменения ветки
не обновляют опубликованные архивы автоматически. Актуальные сведения
о версиях и проверенных пакетах приведены в [документации lab2](lab2/README.md).
