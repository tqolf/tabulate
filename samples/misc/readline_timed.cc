#include <string>
#include <time.h>
#include <stdio.h>
#include <termios.h>
#include <sys/select.h>

#define READLINE_ECHO_RAW  0
#define READLINE_ECHO_STAR 1
#define READLINE_ECHO_HIDE 2

#define READLINE_ETIMEOUT -1024

int ReadLineTimed(const char *prompt, std::string &line, int mode, struct timeval *timeout, FILE *in, FILE *out)
{
    int err = 0, rfd = fileno(in);
    struct termios oriattr, curattr;
    if (tcgetattr(rfd, &oriattr) != 0) {
        return -errno;
    }
    curattr = oriattr;
    curattr.c_lflag &= ~(ECHO | ECHONL);
    curattr.c_lflag &= ~(ECHO | ECHONL | ICANON);
    if (tcsetattr(rfd, TCSANOW, &curattr)) {
        return -errno;
    }

    tcflush(rfd, TCIFLUSH); // discard data received but not read
    if (prompt != nullptr) {
        fputs(prompt, out), fflush(out);
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    while (true) {
        FD_SET(rfd, &rfds);
        if (select(rfd + 1, &rfds, NULL, NULL, timeout) >= 1) {
            if (FD_ISSET(rfd, &rfds)) {
                FD_CLR(rfd, &rfds);
                auto ch = getchar();
                if (ch == '\n' || ch == '\r' || ch == EOF) {
                    break;
                } else if (ch == 0x04 /* Control-D */) {
                    if (!line.empty()) {
                        break;
                    }
                } else if (ch == '\b' || ch == 127 /* Backspace */) {
                    if (!line.empty()) {
                        fputs("\b \b", out), fflush(out);
                        line.pop_back();
                    } else {
                        fputc('\a', out);
                    }
                } else if (!isprint(ch)) {
                    fputc('\a', out);
                } else {
                    switch (mode) {
                        case READLINE_ECHO_RAW:
                            fputc(ch, out), fflush(out);
                            break;
                        case READLINE_ECHO_STAR:
                            fputc('*', out), fflush(out);
                            break;
                        case READLINE_ECHO_HIDE:
                            break;
                    }
                    line += ch;
                }
            }
        } else {
            fputc('\a', out);
            err = READLINE_ETIMEOUT;
            break;
        }
    }
    fputc('\n', out);
    if (tcsetattr(rfd, TCSANOW, &oriattr)) {
        return -errno;
    }

    return err;
}

int ReadLineTimed(const char *prompt, std::string &line, int mode, time_t tosec)
{
    struct timeval timeout = {.tv_sec = tosec, .tv_usec = 0};
    return ReadLineTimed(prompt, line, mode, tosec == 0 ? nullptr : &timeout, stdin, stdout);
}

#include <time.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <sstream>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "cxxopt.h"

int getch_noblocking(void)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    return select(1, &rfds, NULL, NULL, &tv) >= 1 ? getchar() : -1;
}

int main()
{
    int timeout = getarg(10, "--timeout");
    if (!hasarg("--cov")) {
        std::string username, password;
        int err = ReadLineTimed("Username: ", username, READLINE_ECHO_RAW, timeout);
        if (err == READLINE_ETIMEOUT) {
            std::cerr << "ERROR: Timeout to get username" << std::endl;
        } else if (err == 0) {
            err = ReadLineTimed("Password: ", password, READLINE_ECHO_STAR, timeout);
            if (err == READLINE_ETIMEOUT) {
                std::cerr << "ERROR: Timeout to get password" << std::endl;
            } else if (err == 0) {
                std::cout << "Username: " << username << ", Password: " << password << std::endl;
            }
        }
    }

    // coverage
    if (hasarg("--cov")) {
        struct termios oriattr, curattr;
        if (tcgetattr(STDIN_FILENO, &oriattr) != 0) {
            fprintf(stderr, "readline: tcgetattr failed\n");
            return -errno;
        }
        curattr = oriattr;
        curattr.c_cflag |= (CLOCAL | CREAD | NOFLSH);
        curattr.c_lflag &= ~(ECHO | ECHONL);
        curattr.c_lflag &= ~(ECHO | ECHONL | ICANON);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &curattr)) {
            fprintf(stderr, "readline: tcsetattr failed\n");
            return -errno;
        }

        struct InputSimulator {
            FILE *fp;
            const char *text;
            unsigned int before_input;
            unsigned int between_char;
            unsigned int before_newline;

            InputSimulator(const char *text, unsigned int before_input, FILE *in = stdin)
                : fp(in), text(text), before_input(before_input), between_char(10), before_newline(10)
            {
            }

            void Run() const
            {
                auto Reverse = [](std::string input) {
                    std::reverse(input.begin(), input.end());
                    return input;
                };

                usleep(before_input);
                if (text != nullptr) {
                    for (auto ch : Reverse(text)) {
                        ungetc(ch, fp);
                        usleep(between_char);
                    }
                    usleep(before_newline);
                    ioctl(fileno(fp), TIOCSTI, "\n");
                }
            }
        };

        // clang-format off
        struct {
            struct timeval timeout;
            InputSimulator UsernameSimulator;
            InputSimulator PasswordSimulator;
        } operations[] = {
            {
                {.tv_sec = 1, .tv_usec = 0},
                InputSimulator("usernamm\be\x04", 200 * 1000), InputSimulator("\b\tpassworr\bd\x04", 200 * 1000),
            },
            {
                {.tv_sec = 1, .tv_usec = 0},
                InputSimulator("usernamm\be\x04", 1500 * 1000), InputSimulator("\b\tpassworr\bd\x04", 700 * 1000),
            },
            {
                {.tv_sec = 1, .tv_usec = 0},
                InputSimulator("username\n\n", 200 * 1000), InputSimulator("password", 100 * 1000),
            },
        };
        // clang-format on

        for (auto &operation : operations) {
            auto ithread = std::thread([&]() {
                operation.UsernameSimulator.Run();
                operation.PasswordSimulator.Run();
            });

            auto tthread = std::thread([&]() {
                int err;
                struct timeval timeout;
                std::string username, password;

                timeout = operation.timeout;
                err = ReadLineTimed("UserName: ", username, READLINE_ECHO_RAW, &timeout, stdin, stdout);
                if (err == READLINE_ETIMEOUT) {
                    std::cerr << "ERROR: Timeout to get username" << std::endl;
                }

                timeout = operation.timeout;
                err = ReadLineTimed("Password: ", password, READLINE_ECHO_STAR, &timeout, stdin, stdout);
                if (err == READLINE_ETIMEOUT) {
                    std::cerr << "ERROR: Timeout to get password" << std::endl;
                }

                std::cout << "Username: " << username << ", Password: " << password << std::endl;
            });

            ithread.join();
            tthread.join();
        }

        if (tcsetattr(STDIN_FILENO, TCSANOW, &oriattr)) {
            fprintf(stderr, "readline: tcsetattr failed\n");
            return -errno;
        }
    }

    return 0;
}
