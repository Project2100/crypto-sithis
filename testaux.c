#include <stdlib.h>

#ifdef  _WIN32

#elif defined __unix__
#include <unistd.h>
#include <signal.h>
#endif

int main(int argc, char** argv) {
    if (argc == 1) return 0;


#ifdef _WIN32
    return *argv[1] - '0';
#elif defined __unix__

    switch (*argv[1] - '0') {
        case 0:
            exit(0);
        case 1:
            return 1;
        case 2:
            exit(2);
        case 3:
            raise(SIGINT);
        default:
            return 0;
    }
#endif
}
