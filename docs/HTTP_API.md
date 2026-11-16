# HTTPS API, соединения, повторы и gzip — версия 0.9.1

Новый API добавлен к существующему TLS 1.2 клиенту. Старые GetL/GetAutoL
сохраняют формат исходного ответа и ограниченный буфер; для нового кода
используйте RequestAutoL / RequestL / DownloadAutoL.

## Возможности

- HTTP/1.1 GET, HEAD, POST, PUT, PATCH, DELETE; двоичное тело и Content-Type.
- Числовой HTTP-статус и исходные заголовки отдельно от тела.
- N70HeaderValue(headers,name): первое значение поля без учёта регистра.
- Content-Length, chunked с удалением framing, ответ до закрытия TLS.
- Промежуточные 100/102/103, отсутствие тела у HEAD/204/304.
- Автоматические HTTPS-перенаправления 301/302/303/307/308, максимум 5 по умолчанию.
- Порционная выдача тела, лимиты заголовков и размера тела.
- Сохранение файла без загрузки всего ответа в RAM.
- Общий монотонный deadline запроса, включая подготовку seed, TCP/TLS,
  ожидание выбора точки доступа, чтение и цепочку перенаправлений.
- Кооперативная отмена через отдельный объект; принудительный Kill потока
  больше не используется в тестовом приложении.

## Настройка

В MMP добавить hal.lib к прежним зависимостям. Перекомпилировать приложение
с новыми заголовками и .lib. Вызывать из рабочего потока со стеком **64 КиБ**.
Это блокирующий API; MN70HttpSink также вызывается в этом рабочем потоке.

```text
STATICLIBRARY n70https.lib
LIBRARY euser.lib esock.lib insock.lib efsrv.lib estlib.lib commdb.lib hal.lib
```

```cpp
TN70HttpOptions options;
options.iTotalTimeoutMs = 120000; // весь запрос, а не каждый redirect отдельно
options.iIoTimeoutMs = 30000;    // одна ожидаемая операция
options.iMaxBodyBytes = 8*1024*1024;
options.iMaxWireBytes = 8*1024*1024; // тело до распаковки gzip, без chunk framing
options.iMaxRedirects = 5;       // 0 — вернуть 3xx, не следовать
options.iMaxAddresses = 8;      // IPv4-ответы DNS, 1..32, дубликаты пропускаются
options.iKeepAlive = ETrue;
options.iKeepAliveIdleMs = 15000;
options.iMaxRetries = 2;        // общий бюджет дополнительных попыток
options.iRetryDelayMs = 500;
options.iMaxRetryDelayMs = 10000;
options.iRetryIdempotent = EFalse; // явно разрешить повторы PUT/DELETE
options.iPromptForAccessPoint = ETrue;
options.iCancel = cancelToken;  // либо NULL
client->SetOptionsL(options);  // до начала запроса
```

Параметры применяются и к старым методам GET: общий deadline теперь есть
и у них. Клиент и его сокеты принадлежат рабочему потоку. Между потоками
передаётся только cancel token, а не сами сокеты.

## Заголовки и Bearer

```cpp
client->SetBearerTokenL(_L8("your-access-token")); // без префикса Bearer
client->SetHeaderL(_L8("X-Api-Key"), _L8("your-api-key"));
client->SetHeaderL(_L8("Accept"), _L8("application/json"));
// Затем обычный RequestAutoL / RequestL или DownloadAutoL.
// После окончания работы с этими заголовками:
client->ClearHeadersL();
```

Значения копируются в клиент. SetHeaderL заменяет одноимённое поле без учёта
регистра; несколько вызовов не создают повторных Authorization. Эти заголовки
относятся к новым Request/Download методам, а не к старому raw GetL/GetAutoL.
Изменять их во время запроса нельзя: KErrInUse. Заголовки сохраняются между
вызовами; перед обращением к другому API вызывайте ClearHeadersL при необходимости.

Имена — HTTP token, максимум 128 байтов; значения — ASCII с допустимым HTAB,
максимум 2048 байтов. CR/LF, NUL и управляющие байты отклоняются до сети
(KErrArgument). Сумма строк ограничена 4096 байт (KErrOverflow). Ошибка замены
оставляет прежние поля неизменными. SetBearerTokenL принимает непустой токен
стандартного формата Bearer; для нестандартного значения используйте SetHeaderL.

Host, Content-Length, Content-Type, Transfer-Encoding, Connection, Accept-Encoding,
TE, Trailer, Upgrade, Proxy-Authorization, Proxy-Connection, Expect и Keep-Alive
зарезервированы. Content-Type задаётся аргументом Request; размеры и framing
вычисляет библиотека. Accept и User-Agent можно заменить через SetHeaderL.

При редиректе на другой origin (имя сервера **или порт**) все пользовательские
заголовки удаляются из последующих запросов этой цепочки, в том числе Bearer,
API-ключи и Cookie. Возврат редиректом к исходному origin их не восстанавливает.
В самом объекте поля сохраняются для следующего самостоятельного вызова.
Куки из Set-Cookie автоматически не сохраняются. Значения ключей библиотека
не пишет в журнал, но ваш sink/сервер может вернуть их в ответе: не логируйте
такие ответы с настоящими токенами. Тесты используют только публичный фиктивный токен.

## Gzip

Новые методы отправляют `Accept-Encoding: gzip, identity` и распаковывают gzip
потоком после удаления chunked framing. Используется [zlib 1.4](https://zlib.net/),
только inflate и проверки контрольных сумм, с Z_SOLO и отдельными аллокаторами
Symbian. Внешнюю DLL устанавливать не требуется. Окно — до 32 КиБ, буфер выдачи
2048 байтов; хранить весь сжатый или распакованный ответ в RAM не нужно.

Проверяются заголовок gzip, FHCRC при наличии, CRC32 и ISIZE. Поддержаны до
128 последовательных gzip members. Мусор после последнего member, обрыв и
повреждение возвращают KErrCorrupt; превышение лимитов — KErrOverflow.
Отдельные лимиты iMaxWireBytes и iMaxBodyBytes ограничивают сжатый поток и
результат распаковки, включая сильно сжимаемые ответы. Brotli, HTTP Content-Encoding
deflate и цепочки кодирований вроде `gzip, br` пока не поддержаны.

HeadersL получает **исходные** HTTP-заголовки: Content-Encoding и Content-Length
сохраняют смысл полученного ответа. DataL получает уже распакованные байты;
поэтому их число может отличаться от исходного Content-Length.
iBodyBytes — число выданных распакованных байтов, iWireBodyBytes — число байтов
HTTP-тела до распаковки, без chunk framing. HEAD/204/304 не требуют gzip-тела.
DownloadAutoL сохраняет распакованный файл и публикует его только после проверки
gzip и полного HTTP-ответа. До успешного возврата Request все полученные порции
считайте частичными: ошибка CRC может обнаружиться после части вызовов DataL.

## Обработка ответа порциями

```cpp
class TFileBody : public MN70HttpSink
    {
public:
    TFileBody(RFile& file) : iFile(file), iStatus(0) {}
    void HeadersL(TInt status, const TDesC8& headers)
        {
        iStatus = status;
        TPtrC8 contentType = N70HeaderValue(headers, _L8("Content-Type"));
        // status, headers и contentType доступны здесь.
        // При необходимости скопировать нужные поля до выхода из callback.
        }
    void DataL(const TDesC8& part)
        {
        User::LeaveIfError(iFile.Write(part));
        }
    RFile& iFile;
    TInt iStatus;
    };
```

```cpp
client->RequestAutoL(
    _L8("POST"), _L8("https://httpbingo.org/post"),
    _L8("application/json"), _L8("{\"n70\":\"hello\"}"),
    anchors, anchorCount,
    _L("C:\\system\\apps\\MyApp\\rng-device-v1.bin"),
    sink, result);
```

GET/HEAD: соответствующий метод и KNullDesC8 для Content-Type и тела.
POST-тело передаётся дескриптором, оно должно оставаться доступным до возврата
метода. Потоковая отправка тела запроса отдельным producer API пока не реализована.

HeadersL вызывается один раз для конечного ответа; ответы промежуточных
перенаправлений не отправляются в sink. DataL содержит только декодированное
тело, без размеров chunk и CRLF. Сетевой буфер чтения — 2048 байт. Ничего
не читается из сети до возврата текущего DataL, что обеспечивает backpressure.

Заимствованные дескрипторы действительны только во время callback. Leave
из callback прерывает запрос и возвращается вызывающему коду после очистки
сокетов. Если запрос завершился с ошибкой, уже полученные порции являются
частичным ответом. Успешный возврат метода означает завершение HTTP framing.

TN70HttpResult содержит iStatus, iRedirects, iBodyBytes, iWireBodyBytes и iFinalUrl.
HTTP 404/500 сами по себе не приводят к Leave: это корректные HTTP-ответы,
которые приложение может обработать. Сетевые, TLS, framing и callback-ошибки
возвращаются через Leave. Некоторые поля result при ошибке могут быть частичными.

RequestAutoL использует прежний экспериментальный автоматический источник
случайности. Его криптографическая стойкость не подтверждена. RequestL принимает
явный свежий seed от приложения с собственным источником. Для каждого redirect
из seed выводится отдельное значение через HMAC-DRBG; один TLS seed между
соединениями не повторяется в пределах запроса.

## Загрузка файла

```cpp
client->DownloadAutoL(
    _L8("https://httpbingo.org/bytes/131072"),
    _L("C:\\system\\apps\\MyApp\\download.bin"),
    anchors, anchorCount,
    _L("C:\\system\\apps\\MyApp\\rng-device-v1.bin"), result);
```

Helper требует конечный статус 2xx. Он пишет в уникальный временный файл
рядом с назначением, вызывает Flush и заменяет назначение только после
полностью полученного ответа. При ошибке/отмене временный файл удаляется,
старое назначение остаётся. Это логика публикации файла, а не гарантия
устойчивости файловой системы к потере питания. Из callback-файла собственного
приложения частичные данные автоматически не удаляются.

## Перенаправления

Поддерживаются абсолютные HTTPS URL, //host/path, абсолютные пути, относительные
пути, ../ и ссылки с query/fragment. Фрагмент не отправляется серверу. Доступны
DNS/IPv4 и явный HTTPS-порт; IPv6, userinfo, Unicode/пробелы/обратный слеш
отклоняются. Unicode и пробелы в URL нужно кодировать заранее.

303 меняет запрос на GET, сохраняя HEAD; 301/302 меняют POST на GET и убирают
тело. 307/308 сохраняют метод и тело. Повтор POST на другой origin запрещён
(KErrAccessDenied); origin включает имя и порт. Переход HTTPS → HTTP запрещён.
Нет автоматических повторов POST после сетевой ошибки. Отмена не означает,
что сервер не успел обработать уже отправленный POST.

## Таймауты и отмена

Исправление 0.9.1 подтверждено на N70: client 14/14, TCP timeout 2/2 за
2140 и 2031 мс; полный итог шести наборов — 43/43.
[Отчёт и отдельно оставшийся EOF letsencrypt.org](N70_RESULTS_0.9.1.md).

В 0.9.1 исправлено повторное ожидание уже обработанных сигналов сокета и
таймера. Это вызывало зависания 0.9.0 при backoff и TCP timeout. Интерфейс API
не изменился; приложения со статической .lib нужно пересобрать.
[Разбор и подтверждённые результаты N70](N70_RESULTS_0.9.0.md).

В 0.9 Request/Download поддерживают сохраняемое соединение с проверкой
origin, содержимого roots и idle timeout. Оно сохраняется только после полного
HTTP/gzip и при отсутствии Connection: close. Raw GET по-прежнему закрывает TLS.
DisconnectL между запросами освобождает собственные handles, следующий запрос
подключается заново. DNS/TCP используют перебор IPv4 и один перезапуск
RConnection при ошибке bearer; Stop чужого подключения не вызывается.

Автоповторы по умолчанию разрешены GET/HEAD при временных сетевых ошибках и
408/429/502/503/504 до выдачи HeadersL. PUT/DELETE требуют iRetryIdempotent;
POST/PATCH автоматически не повторяются. Повторные промежуточные ответы
не выдаются sink. Backoff растёт экспоненциально с jitter; Retry-After
поддерживает секунды и три HTTP-date формы. Значение дольше cap возвращается
приложению без раннего повтора. Deadline включает попытки, DNS и задержки;
отмена работает во время backoff. После выдачи HeadersL или DataL повтора нет.
Результат содержит iAttempts, iRetries, iNewConnections, iReusedConnections,
iAddressAttempts, iRetryAfterMs (-1, если отсутствует/некорректно).

CN70CancelToken создаётся до запуска worker и живёт до его завершения:

```cpp
CN70CancelToken* token = CN70CancelToken::NewL();
// Передать в options.iCancel в worker.
// По нажатию Cancel в UI:
token->Cancel();
// Reset() допустим только перед следующим запросом, когда старый завершён.
```

Token защищён RMutex. Worker проверяет его и deadline при ожиданиях сети
приблизительно каждые 100 мс. DNS, сокет и асинхронный RConnection::Start
отменяются из их собственного потока. В 0.8 общий deadline и I/O timeout
опираются на отдельные RTimer, не на пересчёт TickCount через HAL и не на
календарные часы. После CancelConnect сокет закрывается до ожидания завершения
исходного TCP-запроса; ожидание больше не полагается только на CancelAll.
Та же очистка применяется при отмене. Для длинных интервалов таймер переармляется
частями до 30 минут, сохраняя разрешённый диапазон опций до 24 часов.

KErrCancel = -3, KErrTimedOut = -33. Один clock охватывает всю redirect-цепочку.
Отмена кооперативная: продолжительный пользовательский callback, криптографическое
вычисление или синхронная операция файла не прерываются посреди исполнения;
состояние проверяется в контрольных точках после их возврата. Поэтому это не
жёсткая real-time гарантия возврата ровно в заданную миллисекунду. Не уничтожать
client/token и не завершать worker принудительно до завершения запроса.

## Ограничения и строгий разбор

- Content-Encoding: identity и gzip; br/deflate и цепочки кодирований дают KErrNotSupported.
- Transfer-Encoding: только chunked; другие цепочки кодирования отклоняются.
- Совместные TE+CL, повторные Content-Length (даже одинаковые), повторные
  Location, obsolete folded headers и неверные размеры chunk отклоняются.
- Заголовки одного блока ограничены 16 КиБ, сумма блоков — 64 КиБ, до 8
  информационных ответов. Trailer-часть chunked ограничена 8 КиБ; framing-поля
  в ней запрещены, остальные trailer-поля проверяются и не выдаются в HeadersL.
- CL/chunked завершаются по точным границам, без ожидания TCP close. Для
  ответа без framing требуется закрытие TLS с close_notify. Обрыв framed тела
  возвращает ошибку; нельзя считать частичный результат полным.
- Превышение размера тела/заголовков/redirect limit — KErrOverflow. Некорректный
  framing — KErrCorrupt. Автоматические куки, HTTP-прокси, HTTP/2, Brotli,
  Content-Encoding deflate и проверка отзыва сертификатов пока не добавлены.

## Проверка

Версия 0.7.1 подтверждена журналами с N70 в `tests/device/n70-0.9.1`: 13/13 HTTP-проверок,
загрузка 131072 байта, сохранность старого файла и отдельный timeout; 8/10 сайтов.
В серии Python обнаружено ожидание TCP 153 с; специализированная отмена
остановилась ещё на прогреве. Это исходные журналы, версия 0.8 ими не проверена.

В 0.8 добавлены ASan-проверки заголовков и gzip, а также компиляция реального
`library/src/n70wait.cpp` с моделями таймеров/сокетов на ПК: зависший TCP на 153 с,
отмена, общий и I/O deadline, DNS, bearer, длинные интервалы. Это модель ОС,
не запуск Symbian. Запуск: `tools/test-http08.ps1`, с сетью — добавить `-Live`.
В SIS новые пункты **Test API + gzip** (12 проверок, api-tests.txt) и
**Test TCP timeout** (2 проверки, tcp-timeout.txt). Требуется повтор на N70.

На ПК: 2151 parser/URL/fragmentation/truncation/limit/mutation проверка под
AddressSanitizer в версии 0.7; тест deadline (границы, wrap счётчика, отмена, 64-битные
интервалы). Настоящий BearSSL HTTPS: GET, POST JSON, chunked /stream/20,
131072 байта, ответ 302, HEAD — 6/6, TLS 1.2. Журнал host-http-results.txt.
Хостовый клиент не исполняет Symbian transport, redirect-driver, RMutex,
RConnection cancellation или атомарную публикацию файла; эти части требуют N70.

В новом SIS:

1. Test new features → http-features.txt: 13 проверок, включая POST JSON/формы,
   редиректы GET/POST 303/307, HEAD, chunked, HTTP 404, лимит тела, download.bin,
   сохранность файла по SHA-256 при ошибке и таймаут. [Инструкция](PHONE_TESTS.md).
2. Test timeout → timeout.txt: /delay/10 с deadline 2 с после прогрева bearer.
   В 2 с входит и новое TCP/TLS-рукопожатие, поэтому на медленном телефоне
   таймаут может наступить до ожидания ответа сервера — это допустимо.
3. Test cancellation → cancel.txt: во время запроса Options → Cancel request.
   Ожидается -3; это PASS для специального теста отмены.
4. Проверить повторный запуск после отмены, отключение сети и нехватку диска.

Тестовый сервис httpbingo.org требует User-Agent; без него при проверке
возвращался HTTP 402. Новый клиент отправляет User-Agent: N70Https/0.7.
Ответы внешнего сервиса и цепочки сертификатов могут меняться. Новые функции
0.8 собраны для N70, но пока не подтверждены испытаниями на самом телефоне.
