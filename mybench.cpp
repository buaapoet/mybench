#include <config.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <curl/curl.h>

#include "mybench.h"

volatile int timerexpired = 0;
int success = 0;
int failed = 0;
int bytes = 0;
int keepalive = 0;
int round = 0;
int clients = 1;
int benchtime = 30;
int mypipe[2];
char* myurl;

/* Http Methods: GET, HEAD */
enum Method {
    METHOD_GET = 0,
    METHOD_HEAD
};

const char* http_method_names[] = {
    "GET",
    "HEAD"
};
int method = METHOD_GET;

static const char* short_options = "r:t:c:v?hk";

static const struct option long_options[]=
{
    {"proxy",required_argument,NULL,'p'},

    {"round",       required_argument,  NULL,    'r'},
    {"time",        required_argument,  NULL,    't'},
    {"clients",     required_argument,  NULL,    'c'},
    {"get",         no_argument,        &method, METHOD_GET},
    {"version",     no_argument,        NULL,    'v'},
    {"head",        no_argument,        &method, METHOD_HEAD},
    {"help",        no_argument,        NULL,    'h'},
    {"keepalive",   no_argument,        NULL,    'k'},
    {NULL,          0,                  NULL,    0}
};

static void alarm_handler(int signal)
{
    (void)signal;
    timerexpired = 1;
}

static void usage(void)
{
    fprintf(stderr,
            "webbench [option]... URL\n"
            "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
            "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
            "  -k|--keepalive           Keep alive.\n"
            "  -r|--round               max round per child process.\n"
            "  --get                    Use GET request method.\n"
            "  --head                   Use HEAD request method.\n"
            "  -?|-h|--help             Display help information.\n"
            "  -v|--version             Display program version.\n"
           );
};

int main(int argc, char *argv[])
{
    int opt = 0;

    while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != EOF) {
        switch (opt) {
        case  0 : break;
        case 't':
                  benchtime = atoi(optarg);
                  if (benchtime <= 0) {
                      fprintf(stderr, "webbench: benchtime should be greater than zero [-t].\n");
                      return 1;
                  }
                  break;
        case 'r':
                  round = atoi(optarg);
                  if (round <= 0) {
                      fprintf(stderr, "webbench: round should be greater than zero [-r].\n");
                      return 2;
                  }
                  break;
        case 'k':
                  keepalive = 1;
                  break;
        case 'c':
                  clients = atoi(optarg);
                  if (clients <= 0) {
                      fprintf(stderr, "webbench: clients should be greater than zero [-c].\n");
                      return 6;
                  }
                  break;
        case 'h':
        case '?':
                  usage();
                  return 5;
                  break;
        case 'v':
                  printf("webbench: v"PACKAGE_VERSION"\n");
                  exit(0);
        }
    }

    // url check
    if (optind == argc) {
        fprintf(stderr, "webbench: Missing URL!\n");
        usage();
        return 2;
    }

    /* title */
    fprintf(stderr, "mybench - Another Http Benchmark Tool v"PACKAGE_VERSION"\n");
    myurl = argv[optind];
    fprintf(stderr, "Benchmarking: %s %s\n", http_method_names[method], myurl);

    fprintf(stderr, "\n%d clients running %d sec",clients, benchtime);
    if (round != 0) {
        fprintf(stderr, " with %d max round", round);
    }
    fprintf(stderr, ".\n");

    return bench();
}

/* vraci system rc error kod */
static int bench(void)
{
    int i,j,k;
    pid_t pid = 0;
    FILE *f;

    if (pipe(mypipe))
    {
        perror("pipe failed.");
        return 3;
    }

    /* fork childs */
    for (i = 0; i < clients; ++i) {
        pid = fork();

        if (pid == 0) { /* child */
            sleep(1);
            break;
        } else if (pid == -1) { /* error */
            fprintf(stderr, "child #%d fork failed(err: %d)\n", i, errno);
        }
    }

    if (pid == 0) { /* child */
        curl_global_init(CURL_GLOBAL_DEFAULT);
        benchcore();
        curl_global_cleanup();

        /* write results to pipe */
        f = fdopen(mypipe[1], "w");
        if (f == NULL) {
            perror("open pipe for writing failed.");
            return 3;
        }

        fprintf(stderr, "Child #%d - %d %d\n", i, success, failed);
        fprintf(f, "%d %d %d\n", success, failed, bytes);
        fclose(f);

        return 0;
    } else {
        f = fdopen(mypipe[0], "r");
        if (f == NULL) {
            perror("open pipe for reading failed.");
            return 3;
        }

        setvbuf(f, NULL, _IONBF, 0);
        success = 0;
        failed = 0;
        bytes = 0;

        while (true) {
            pid = fscanf(f, "%d %d %d", &i, &j, &k);
            if (pid < 3) {
                fprintf(stderr, "Some of our childrens died.\n");
                break;
            }

            success += i;
            failed += j;
            bytes += k;
            if (--clients == 0) {
                break;
            }
        }

        fclose(f);
        fprintf(stderr,
                "Speed: %d qps, %d bytes/sec.\n"
                "Requests: %d susceed, %d failed.\n",
                (int)((success + failed) / (float)benchtime),
                (int)(bytes / (float)benchtime),
                success,
                failed);
    }

    return 0;
}

static size_t write_memory_callback(void * contents , size_t size, size_t nmemb, void * userp)
{
    (void)contents;
    (void)userp;
    size_t realsize = size * nmemb;
    bytes += realsize;
    return realsize;
}

static CURLcode my_curl_easy_set_method(CURL* curl)
{
    CURLcode code;

    if (method == METHOD_GET) {
        code = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == METHOD_HEAD) {
        char head[] = "HEAD";
        code = curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, head);
        if (code == CURLE_OK) {
            code = curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        }
    }

    return code;
}

int benchcore()
{
    CURL *curl = NULL;
    CURLcode res;
    char error[CURL_ERROR_SIZE];

    /* setup alarm signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL)) {
        fprintf(stderr, "sigaction failed(err: %d)\n", errno);
        return -1;
    }
    alarm(benchtime);

    bool is_setted_round = (round != 0);

    while (true && (!is_setted_round || --round >= 0)) {
        if (timerexpired) {
            break;
        }

        if (curl == NULL) {
            curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, myurl);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
                my_curl_easy_set_method(curl);
            } else {
                ++failed;
                fprintf(stderr, "curl init error\n");
                continue;
            }
        }

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            ++failed;
            fprintf(stderr, "curl perform error: %s\n", error);
            curl_easy_cleanup(curl);
            curl = NULL;
        } else {
            ++success;
            if (!keepalive) {
                curl_easy_cleanup(curl);
                curl = NULL;
            }
        }
    }

    if (curl != NULL) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }

    return 0;
}
