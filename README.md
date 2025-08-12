# Модули Nginx для улучшения логгирования

Этот репозиторий содержит коллекцию пользовательских модулей для веб-сервера Nginx.

## Модули

### Request Log Module (`nginx-http-req-log-module`)

Этот модуль предоставляет расширенные возможности для логгирования запросов. В отличие от стандартного `ngx_http_log_module`, он работает на более ранней стадии обработки запроса (`NGX_HTTP_POST_READ_PHASE`), что позволяет логгировать запросы сразу после их получения. 

Вместо внесения изменений (патчинга) в стандартный код Nginx, был создан отдельный модуль. Это гарантирует, что решение не будет конфликтовать с будущими обновлениями Nginx и его проще поддерживать.

**Директивы:**

*   `log_req_format name string;`
    *   **Контекст:** `http`
    *   **Описание:** Задает формат лога.

*   `access_req_log path [format [buffer=size] [gzip[=level]] [flush=time] [if=condition]];`
    *   **Контекст:** `http`, `server`, `location`
    *   **Описание:** Включает логгирование запросов в указанный файл с использованием заданного формата.
    *   `off` - отключает логгирование.

*   `open_log_req_file_cache max=N [inactive=time] [min_uses=N] [valid=time];`
    *   **Контекст:** `http`, `server`, `location`
    *   **Описание:** Конфигурирует кеш для файловых дескрипторов логов.

### Test Header Module (`nginx-http-test-header-module`)

Этот модуль предназначен для тестирования и отладки. Он позволяет добавлять кастомный заголовок в ответ сервера и считывать значение заголовка из запроса.

**Директивы:**

*   `test_header_name <header_name>;`
    *   **Контекст:** `http`, `server`, `location`
    *   **Описание:** Устанавливает имя заголовка, который будет добавлен в ответ. Значением заголовка будет случайное число.

**Переменные:**

*   `$test_header_in`: Содержит значение заголовка из запроса, имя которого указано в директиве `test_header_name`.
*   `$test_header_out`: Содержит значение заголовка, добавленного этим модулем в ответ.

## Сборка

Для использования этих модулей необходимо скомпилировать Nginx из исходных кодов.

### Статическая сборка

Модули компилируются вместе с Nginx в один бинарный файл.

1.  **Скачайте исходный код Nginx.**

2.  **Распакуйте архив.**

3.  **Сконфигурируйте сборку**, указав путь к модулям с помощью опции `--add-module`:

    ```bash
    ./configure --add-module=/path/to/nginx-modules/nginx-http-req-log-module \
                --add-module=/path/to/nginx-modules/nginx-http-test-header-module
    ```

4.  **Скомпилируйте и установите Nginx:**

    ```bash
    make && make install
    ```

### Динамическая сборка

Модули компилируются в отдельные файлы (`.so`), которые можно подключать и отключать без пересборки Nginx.

1.  **Скачайте исходный код Nginx.**

2.  **Распакуйте архив.**

3.  **Сконфигурируйте сборку**, указав путь к модулям с помощью опции `--add-dynamic-module`:

    ```bash
    ./configure --add-dynamic-module=/path/to/nginx-modules/nginx-http-req-log-module \
                --add-dynamic-module=/path/to/nginx-modules/nginx-http-test-header-module
    ```

4.  **Скомпилируйте модули:**

    ```bash
    make && make install
    ```

5.  **Скопируйте скомпилированные модули** (файлы `.so`) в директорию с модулями Nginx (обычно `/usr/lib/nginx/modules` или `/etc/nginx/modules`).

6.  **Подключите модули** в файле `nginx.conf` с помощью директивы `load_module`:

    ```nginx
    load_module modules/ngx_http_req_log_module.so;
    load_module modules/ngx_http_test_header_module.so;
    ```

## Совместимость

Модули были протестированы и работают со следующими версиями Nginx:
*   1.28
*   1.29

## Пример конфигурации

Ниже приведен пример файла `nginx.conf`, демонстрирующий совместное использование модулей.

**ВАЖНО:** 
1.  Директива `access_req_log` из модуля `nginx-http-req-log-module` работает на ранней фазе обработки запроса (`POST_READ`), до того как будет выбран конкретный `location`. Поэтому, чтобы корректно логировать переменную `$test_header_in`, директива `test_header_name` **должна быть определена на уровне `http` или `server`**, а не `location`.
2.  По той же причине, в формате лога `log_req_format` **нельзя использовать переменные, относящиеся к ответу**, такие как `$status`, `$body_bytes_sent`, `$test_header_out` и т.д., так как они еще не определены на данной фазе.

```nginx
# Раскомментируйте, если модули собраны динамически
# load_module modules/ngx_http_req_log_module.so;
# load_module modules/ngx_http_test_header_module.so;

events {
    worker_connections  1024;
}

http {
    # Задаем имя тестового заголовка на уровне http, чтобы он был доступен в POST_READ фазе
    test_header_name X-Test-Header;

    # Формат для access_req_log (ранняя фаза, только переменные запроса)
    log_req_format test_format '$remote_addr - $remote_user [$time_local] '
                               '"$request" '
                               '"$http_referer" "$http_user_agent" '
                               'req_header: "$test_header_in"';

    # Формат для стандартного access_log (поздняя фаза, доступны все переменные)
    log_format main_format '$remote_addr - $remote_user [$time_local] '
                           '"$request" $status $body_bytes_sent '
                           '"$http_referer" "$http_user_agent" '
                           'req_header: "$test_header_in" '
                           'resp_header: "$test_header_out"';

    server {
        listen       8080;
        server_name  localhost;

        # Лог на ранней фазе (только запрос)
        access_req_log logs/req_log.log test_format;

        # Стандартный лог на поздней фазе (запрос и ответ)
        access_log logs/access.log main_format;

        location / {
            # При запросе к этому location, модуль test-header добавит
            # заголовок X-Test-Header в ответ
            root   html;
            index  index.html index.htm;
        }
    }
}
```

### Тестирование

1.  Запустите Nginx с этой конфигурацией.
2.  Выполните запрос к серверу: `curl -v -H "X-Test-Header: 123" http://localhost:8080/`
3.  Проверьте файл `logs/req_log.log`. В нем должна появиться запись, содержащая **только заголовок из запроса**:
    `... req_header: "123"`
4.  Проверьте файл `logs/access.log`. В нем будет полная запись, включающая **и запрос, и ответ**:
    `... req_header: "123" resp_header: "..."` (значение `resp_header` будет случайным числом).
