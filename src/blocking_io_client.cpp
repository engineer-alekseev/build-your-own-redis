#include <stdint.h>          // Стандартные целочисленные типы фиксированной ширины
#include <stdlib.h>          // Общие функции стандартной библиотеки (abort)
#include <string.h>          // Работа со строками/памятью (strlen)
#include <stdio.h>           // Ввод/вывод на C (printf, fprintf)
#include <errno.h>           // Коды ошибок системных вызовов (errno)
#include <unistd.h>          // POSIX-функции (read, write, close)
#include <arpa/inet.h>       // Сетевые функции и преобразования порядка байт
#include <sys/socket.h>      // Основной заголовок сокетов (socket, connect)
#include <netinet/ip.h>      // Структуры/константы для IP (sockaddr_in)
#include <assert.h>

const size_t K_MAX_MSG = 4096;


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

static void msg(const char *msg) {
    // печатает произвольное сообщение в поток ошибок (stderr)
    fprintf(stderr, "%s\n", msg);   // Печатаем строку и перенос строки
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

static int32_t query(int fd, const char *text) {
    uint32_t len = (uint32_t)strlen(text);
    if (len > K_MAX_MSG) {
        return -1;
    }

    char wbuf[4 + K_MAX_MSG];
    memcpy(wbuf, &len, 4);  // assume little endian
    memcpy(&wbuf[4], text, len);
    if (int32_t err = write_all(fd, wbuf, 4 + len)) {
        return err;
    }

    // 4 bytes header
    char rbuf[4 + K_MAX_MSG];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    memcpy(&len, rbuf, 4);  // assume little endian
    if (len > K_MAX_MSG) {
        msg("too long");
        return -1;
    }

    // reply body
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // do something
    printf("server says: %.*s\n", len, &rbuf[4]);
    return 0;
}


static void die(const char *msg) {
    // Аварийное завершение с выводом кода ошибки errno и сообщения
    int err = errno;                                 // Сохраняем errno (код ошибки)
    fprintf(stderr, "[%d] %s\n", err, msg);          // Печатаем код ошибки и пояснение
    abort();                                         // Завершаем процесс аварийно
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);        // Создаём IPv4 TCP-сокет
    if (fd < 0) {                                    // Проверяем успешность
        die("socket()");                             // Если ошибка — печатаем и выходим
    }

    struct sockaddr_in addr = {};                    // Целевой адрес сервера (IPv4)
    addr.sin_family = AF_INET;                       // Семейство адресов — IPv4
    addr.sin_port = htons(1234);                     // Порт сервера в сетевом порядке (обычно используют htons)
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // 127.0.0.1 (локальный хост); обычно htonl(INADDR_LOOPBACK)
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr)); // Устанавливаем TCP-подключение к серверу
    if (rv) {                                        // Если connect вернул ошибку
        die("connect");                             // Печатаем и выходим
    }

    int32_t err = query(fd, "hello1");
    if (err) {
        goto L_DONE;
    }
    err = query(fd, "hello2");
    if (err) {
        goto L_DONE;
    }
    err = query(fd, "hello3");
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;                                       // Успешное завершение
}
