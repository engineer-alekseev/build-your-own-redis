#include <stdint.h>          // Стандартные целочисленные типы фиксированной ширины (uint32_t и т.д.)
#include <stdlib.h>          // Общие функции стандартной библиотеки (abort и т.д.)
#include <string.h>          // Работа со строками/памятью (strlen и т.д.)
#include <stdio.h>           // Ввод/вывод на C (printf, fprintf)
#include <errno.h>           // Глобальная переменная errno для кодов ошибок системных вызовов
#include <unistd.h>          // POSIX-функции (read, write, close)
#include <arpa/inet.h>       // Функции/структуры для работы с интернет-протоколами (htonl/ntohl, htons/ntohs)
#include <sys/socket.h>      // Основной заголовок сокетов (socket, bind, listen, accept)
#include <netinet/ip.h>      // Структуры/константы для IP (struct sockaddr_in и т.п.)
#include <assert.h>

// TODO вынести все общие функции в отдельный заголовок

static void msg(const char *msg) {
    // печатает произвольное сообщение в поток ошибок (stderr)
    fprintf(stderr, "%s\n", msg);   // Печатаем строку и перенос строки
}

static void die(const char *msg) {
    // функция аварийного завершения: выводит код errno и сообщение, затем падает
    int err = errno;                               // Сохраняем текущее значение errno (код ошибки)
    fprintf(stderr, "[%d] %s\n", err, msg);        // Печатаем код ошибки и контекстное сообщение
    abort();                                       // Немедленно завершаем процесс аварийно
}

// Обрабатывает одно подключение: читает сообщение от клиента и отвечает "world"
// static void do_something(int connfd) {
//     char rbuf[64] = {};                               // Приёмный буфер на 64 байта, нулевой инициализации достаточно для C-строки
//     ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1); // Читаем до 63 байт, оставляя место под завершающий нуль
//     if (n < 0) {                                      // Если чтение вернуло ошибку
//         msg("read() error");                          // Логируем проблему
//         return;                                       // И выходим из обработчика
//     }
//     fprintf(stderr, "client says: %s\n", rbuf);
//     struct sockaddr_in client_addr = {};
//     socklen_t addrlen = sizeof(client_addr);
//     getsockname(connfd, (struct sockaddr *)&client_addr, &addrlen);     // Печатаем, что прислал клиент
//     fprintf(stderr, "client addr: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)); // выводим адрес клиента
//     char wbuf[] = "world";                            // Подготавливаем ответную строку
//     write(connfd, wbuf, strlen(wbuf));             // Отправляем клиенту ответ
// }

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return - 1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return - 1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

const size_t K_MAX_MSG = 4096;

static int32_t one_request(int connfd) {
    char rbuf[4 + K_MAX_MSG];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }
    uint32_t len = 0;
    memcpy(&len, rbuf, 4);

    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    fprintf(stderr, "client says: %.*s\n", len, &rbuf[4]);

    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);

    return write_all(connfd, wbuf, 4 + len);
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);        // Создаём IPv4 TCP-сокет
    if (fd < 0) {                                    // Проверяем, что сокет создан
        die("socket()");                             // Если нет — печатаем ошибку и завершаемся
    }

    // Для серверов обычно включают переиспользование адреса, чтобы быстрее перезапускать процесс
    int val = 1;                                      // Булево «включено» для опции
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)); // Разрешаем повторное использование адреса

    // Привязка сокета к локальному адресу/порту (bind)
    struct sockaddr_in addr = {};                     // Адрес IPv4, нулевая инициализация
    addr.sin_family = AF_INET;                        // Семейство адресов — IPv4
    addr.sin_port = htons(1234);                      // Порт в сетевом порядке байт (чаще используют htons)
    addr.sin_addr.s_addr = htonl(0);                  // 0.0.0.0 (любой локальный интерфейс); обычно используют htonl(INADDR_ANY)
    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr)); // Привязываем адрес к сокету
    if (rv) {                                         // Если bind вернул не 0 — ошибка
        die("bind()");                               // Сообщаем и завершаемся
    }

    // Переводим сокет в пассивный режим прослушивания очереди подключений
    rv = listen(fd, SOMAXCONN);                       // SOMAXCONN — максимально допустимый размер очереди
    if (rv) {                                         // Проверяем успешность listen
        die("listen()");
    }

    while (true) {                                    // Главный цикл сервера: принимаем и обрабатываем подключения
        // Ожидаем новое входящее подключение
        struct sockaddr_in client_addr = {};          // Здесь будет адрес подключившегося клиента
        socklen_t addrlen = sizeof(client_addr);      // Длина структуры адреса
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen); // Блокируемся в ожидании клиента
        if (connfd < 0) {                             // Если принять подключение не удалось
            continue;                                 // Пропускаем итерацию и ждём следующее
        }

        while (true) {
            int32_t err = one_request(connfd);
            if (err) {
                break;
            } 
        }                      // Обрабатываем конкретное подключение
        close(connfd);                                // Закрываем дескриптор клиентского сокета
    }

    return 0;                                         // Теоретически недостижимо (бесконечный цикл), но корректно
}

// stopss at https://build-your-own.org/redis/03/03_server.cpp.htm