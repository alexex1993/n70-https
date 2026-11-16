# HTTPS для Nokia N70

Статическая C++ библиотека TLS 1.2 и HTTP/1.1 для S60 2nd Edition FP3 / Symbian OS 8.1a. Текущая версия — **0.9.1**. BearSSL 0.6 и распаковка gzip через zlib 1.4 входят в библиотеку.

## Где что находится

| Каталог | Содержимое |
| --- | --- |
| `library/` | Исходники, публичные заголовки, адаптация Symbian, корневые сертификаты и MMP библиотеки |
| `examples/N70Test/` | Исходники тестового приложения для телефона |
| `dist/` | Готовые `N70Test.sis`, `n70https.lib` и `N70HttpsSDK.zip` |
| `docs/` | API, подключение SDK, результаты, история проекта |
| `docs/articles/` | Статья для Хабра |
| `tests/` | Тесты на ПК; `device/n70-0.9.1/` — оригинальные результаты телефона; `results/host/` — журналы ПК |
| `tools/` | Подготовка, сборка SDK, тесты и диагностика |
| `vendor/` | Используемые версии BearSSL и zlib; исходные архивы в `archives/` |
| `skills/n70-https/` | Переносимый скилл агента для интеграции библиотеки |
| `build/` | Локальная копия SDK, промежуточные файлы, сгенерированные команды и журналы |
| `Trash/` | Устаревшие материалы и резервная копия прежней структуры; ничего не удалено |

## Использовать в своём приложении

Распакуйте [dist/N70HttpsSDK.zip](dist/N70HttpsSDK.zip) и следуйте [руководству подключения](docs/SDK_INTEGRATION.md). Используйте `RequestAutoL` и собственный `MN70HttpSink` из рабочего потока со стеком **64 КиБ**. Скилл для агента: **`$n70-https`**.

Поддерживаются GET/HEAD/POST/PUT/PATCH/DELETE, свои заголовки и Bearer, gzip, chunked, редиректы, ограничения размера, таймаут, отмена, безопасная публикация загрузок, повторное использование соединения и ограниченные повторы запросов. Подробности и ограничения — в [HTTP API](docs/HTTP_API.md).

Библиотека встраивается в ваше приложение при статической линковке. Сертификаты и TLS системного браузера она не меняет. Автоматическая первичная инициализация энтропии не требует действий пользователя; её криптографическая стойкость не оценена.

## Сборка

Windows, S60 2nd FP3 SDK и ActivePerl 5.6.1. По умолчанию SDK находится в `C:\Symbian\8.1a\S60_2nd_FP3`, Perl — в `C:\Perl\bin`. Каталог проекта должен быть без пробелов и на том же диске, что и выбранная копия SDK.

```bat
build.cmd
```

Для окна с журналом и паузой в конце — `build-and-log.cmd`. Журнал: `build/logs/build.log`. Генерируемые команды находятся в `build/generated/`; редактировать их не нужно. Для нестандартных путей:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/prepare.ps1 -Sdk C:\Symbian\8.1a\S60_2nd_FP3 -Perl C:\Perl\bin
cmd /d /c build\generated\build.cmd
```

Диагностика компилятора: `tools\diagnose-toolchain.cmd`.

## Скилл агента

Исходник — [skills/n70-https/SKILL.md](skills/n70-https/SKILL.md). Установка в локальный каталог скиллов Codex:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/install-skill.ps1
```

Установщик сохраняет подсказку с путём к этой библиотеке, чтобы агент мог найти её из другого проекта. Переносимый скилл также входит в SDK. На другом компьютере можно скопировать его в каталог скиллов и задать `N70_HTTPS_ROOT` или передать путь агенту.

Пример запроса: «Используй `$n70-https`, сделай для N70 приложение, которое отправляет JSON POST в мой API с Bearer-авторизацией».

## Проверка

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-host-bearssl.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test.ps1
# Дополнительно: реальные HTTPS-запросы с ПК
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test.ps1 -Live
```

Для тестов ПК нужны MSVC C++ Build Tools и AddressSanitizer. [Тесты](tests/README.md), [проверки на телефоне](docs/PHONE_TESTS.md), [результаты N70 0.9.1](docs/N70_RESULTS_0.9.1.md). На телефоне пройдено 43 проверки в шести наборах; отдельный обход сайтов — 9/10. Универсальная совместимость со всеми HTTPS-сервисами не заявляется.

## Перенос старой структуры

Старый `test_from_n70/` целиком находится в `tests/device/n70-0.9.1/`. Статья — [docs/articles/ARTICLE_HABR.md](docs/articles/ARTICLE_HABR.md). Подробный прежний README сохранён как [docs/PROJECT_NOTES.md](docs/PROJECT_NOTES.md).

Полный журнал переносов: [Trash/moves.csv](Trash/moves.csv); описание архива — [Trash/README.md](Trash/README.md). В `Trash/previous-layout/` сохранены прежние скрипты, документы и дистрибутив. `Trash` и данные состояния RNG телефона не включаются в SDK и исключены из Git.
