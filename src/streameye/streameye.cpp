
/*
 * Copyright (c) Calin Crisan
 * This file is part of streamEye.
 *
 * streamEye is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bits/stdc++.h>
//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "common.h"
#include "streameye.h"
#include "auth.h"
    /* locals */
#include <sys/syscall.h>


#define server_add "localhost"

static int client_timeout = DEF_CLIENT_TIMEOUT;
static int max_clients = 0;
static int g_tcp_port = 0;
static int listen_localhost = 0;
static char *g_input_separator = NULL;

extern const char *SERVER_ADD;

    /* globals */
int DEF_TCP_PORT = 8080;
int log_level = 1; /* 0 - quiet, 1 - info, 2 - debug */
int g_running = 1;


    /* local functions */

static int          init_server(seye_srv_t *psrv);
static client_t *   wait_for_client(int socket_fd, seye_srv_t *psrv);
static void         print_help();

int gettid_syscall_stream()
{
  pid_t tid;

  tid = syscall(SYS_gettid);
  return (int) tid;
}

    /* server socket */
int init_server(seye_srv_t *psrv) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        ERRNO("socket() failed");
        return -1;
    }

    int tr = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(tr)) < 0) {
        ERRNO("setsockopt() failed");
        return -1;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    /*
    if (listen_localhost) {
        server_addr.sin_addr.s_addr = inet_addr(server_add);
    }
    else {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    }
    */
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(psrv->tcp_port);

    if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        ERRNO("bind() failed");
        close(socket_fd);
        return -1;
    }

    if (listen(socket_fd, 5) < 0) {
        ERRNO("listen() failed");
        close(socket_fd);
        return -1;
    }

    if (fcntl(socket_fd, F_SETFL, O_NONBLOCK) < 0) {
        ERRNO("fcntl() failed");
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

client_t *wait_for_client(int socket_fd, seye_srv_t *psrv) {
  struct sockaddr_in client_addr;
  unsigned int client_len = sizeof(client_addr);

  /* wait for a connection */
  int stream_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client_len);
  if (stream_fd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      ERRNO("accept() failed");
    }

    return NULL;
  }

  /* set socket timeout */
  struct timeval tv;

  tv.tv_sec = client_timeout;
  tv.tv_usec = 0;

  setsockopt(stream_fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval));
  setsockopt(stream_fd, SOL_SOCKET, SO_SNDTIMEO, (char *) &tv, sizeof(struct timeval));

  /* create client structure */
  client_t *client = (client_t*)malloc(sizeof(client_t));
  if (!client) {
    ERROR("malloc() failed");
    return NULL;
  }

  memset(client, 0, sizeof(client_t));

  client->stream_fd = stream_fd;
  client->psrv = psrv;
  inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client->addr, INET_ADDRSTRLEN);
  client->port = ntohs(client_addr.sin_port);

  INFO("new client connection from %s:%d", client->addr, client->port);

  return client;
}

void cleanup_client(client_t *client, seye_srv_t *psrv) {
    DEBUG_CLIENT(client, "cleaning up");

    if (pthread_mutex_lock(&(psrv->clients_mutex))) {
        ERROR("pthread_mutex_lock() failed");
    }

    int i, j;
    for (i = 0; i < psrv->num_clients; i++) {
        if (psrv->clients[i] == client) {
            /* move all further entries back with one position */
            for (j = i; j < psrv->num_clients - 1; j++) {
                psrv->clients[j] = psrv->clients[j + 1];
            }

            break;
        }
    }

    close(client->stream_fd);
    if (client->auth_basic_hash) {
        free(client->auth_basic_hash);
    }
    if (client->jpeg_tmp_buf) {
        free(client->jpeg_tmp_buf);
    }
    free(client);

    psrv->clients = (client_t**)realloc(psrv->clients, sizeof(client_t *) * (--(psrv->num_clients)));
    DEBUG("current clients: %d", psrv->num_clients);

    if (pthread_mutex_unlock(&(psrv->clients_mutex))) {
        ERROR("pthread_mutex_unlock() failed");
    }
}


    /* main */

char *str_timestamp() {
    static __thread char s[20];

    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);

    strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", tmp);

    return s;
}

void print_help() {
    fprintf(stderr, "\n");
    fprintf(stderr, "streamEye %s\n\n", STREAM_EYE_VERSION);
    fprintf(stderr, "Usage: <jpeg stream> | streameye [options]\n");
    fprintf(stderr, "Available options:\n");
    fprintf(stderr, "    -a off|basic       HTTP authentication mode (defaults to off)\n");
    fprintf(stderr, "    -c user:pass:realm credentials for HTTP authentication\n");
    fprintf(stderr, "    -d                 debug mode, increased log verbosity\n");
    fprintf(stderr, "    -h                 print this help text\n");
    fprintf(stderr, "    -l                 listen only on localhost interface\n");
    fprintf(stderr, "    -m max_clients     the maximal number of simultaneous clients (defaults to unlimited)\n");
    fprintf(stderr, "    -p port            tcp port to listen on (defaults to %d)\n", DEF_TCP_PORT);
    fprintf(stderr, "    -q                 quiet mode, log only errors\n");
    fprintf(stderr, "    -s separator       a separator between jpeg frames received at input\n");
    fprintf(stderr, "                       (will autodetect jpeg frame starts by default)\n");
    fprintf(stderr, "    -t timeout         client read/write timeout, in seconds (defaults to %d)\n", DEF_CLIENT_TIMEOUT);
    fprintf(stderr, "\n");
}

double get_now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void bye_handler(int signal) {
    if (!g_running) {
        INFO("interrupt already received, ignoring signal");
        return;
    }

    INFO("interrupt received, quitting");
    g_running = 0;
}

int streameye_thread(void *arg) {
   
    seye_srv_t *psrv = (seye_srv_t*) arg;
    
    int auth_mode = AUTH_OFF;
    char *auth_username = NULL;
    char *auth_password = NULL;
    char *auth_realm = NULL;

    opterr = 0;
       if (auth_mode) {
        if (!auth_username || !auth_password || !auth_realm) {
            ERROR("credentials are required when using authentication");
            return -1;
        }

        set_auth(auth_mode, auth_username, auth_password, auth_realm);
    }
    psrv->tcp_port = 0;
    if (!psrv->webport) {
        psrv->tcp_port = DEF_TCP_PORT;
    }

    psrv->tcp_port = psrv->webport; 
    INFO("tp = %d", psrv->tcp_port);
    INFO("streamEye %s", STREAM_EYE_VERSION);
    //server_add_without_port();
    if (psrv->input_separator && strlen(psrv->input_separator) < 4) {
        INFO("the input separator supplied is very likely to appear in the actual frame data (consider a longer one)");
    }

    /* signals */
    DEBUG("installing signal handlers");
    struct sigaction act;
    g_running = psrv->running; //TODO: hack to pass running to bye_handler, not thread safe
    act.sa_handler = bye_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGINT, &act, NULL) < 0) {
        ERRNO("sigaction() failed");
        return -1;
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        ERRNO("sigaction() failed");
        return -1;
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        ERRNO("signal() failed");
        return -1;
    }

    /* threading */
    DEBUG("initializing thread synchronization");
    if (pthread_cond_init(&(psrv->frame_cond), NULL)) {
      ERROR("pthread_cond_init() failed");
      return -1;
    }
    if (pthread_mutex_init(&(psrv->frame_mutex), NULL)) {
      ERROR("pthread_mutex_init() failed");
      return -1;
    }
    if (pthread_cond_init(&(psrv->jpeg_cond), NULL)) {
        ERROR("pthread_cond_init() failed");
        return -1;
    }
    if (pthread_mutex_init(&(psrv->jpeg_mutex), NULL)) {
        ERROR("pthread_mutex_init() failed");
        return -1;
    }
    if (pthread_mutex_init(&(psrv->clients_mutex), NULL)) {
        ERROR("pthread_mutex_init() failed");
        return -1;
    }

    /* tcp server */
   // static int start_server = 1;
    //int socket_fd;
   // if(start_server == 1)
   // {
    DEBUG("starting server");
    int socket_fd = init_server(psrv);
    if (socket_fd < 0) {
	ERROR("failed to start server");
	return -1;
    }
     //       start_server = 0;
   // }
    //INFO("listening on %s:%d", listen_localhost ? "127.0.0.1" : "0.0.0.0", tcp_port);
    INFO("listening on %s:%d", listen_localhost ? server_add : "localhost", psrv->tcp_port);

    /* main loop */
    //char input_buf[INPUT_BUF_LEN];
    char *input_buf;
    char *sep = NULL;
    int size, rem_len = 0, i;

    double now, min_client_frame_int;
    double frame_int_adj;
    double frame_int = 0;
    double last_frame_time = get_now();

    int auto_separator = 0;
    int input_separator_len;
    if (!psrv->input_separator) {
        auto_separator = 1;
        input_separator_len = 4; /* strlen(JPEG_START) + strlen(JPEG_END) */;
        psrv->input_separator = (char*)malloc(input_separator_len + 1);
        snprintf(psrv->input_separator, input_separator_len + 1, "%s%s", JPEG_END, JPEG_START);
    }
    else {
        input_separator_len = strlen(psrv->input_separator);
    }

    FILE *img;
    sleep(2);
    INFO("Streameye Thread Id = %d",gettid_syscall_stream());
    //while (running) {
    while (1) {

      if (pthread_mutex_lock(&(psrv->frame_mutex))){
        std::cerr << "pthread_mutex_lock() failed frame_muted" << std::endl;
        return -1;
      }
      while(!psrv->ready_state){
        if (pthread_cond_wait(&(psrv->frame_cond), &(psrv->frame_mutex))){
          std::cerr << "pthread_cond_wait() failed frame_muted" << std::endl;
          return -1;
        }
      }
      psrv->ready_state = false;
      if (pthread_mutex_unlock(&(psrv->frame_mutex))){
        std::cerr << "pthread_mutex_lock() failed frame_muted" << std::endl;
        return -1;
      }
      /*
      if (psrv->ready_state == false){
        usleep(10*1000);
        continue;
      }
     */
      
      psrv->ready_state = false;

      input_buf = psrv->pimgbuf;
      size = psrv->bufsize;

        if (size < 0) {
            if (errno == EINTR) {
                break;
            }

            ERRNO("input: read() failed");
            return -1;
        }
        else if (size == 0) {
            DEBUG("input: end of stream");
            psrv->running = 0;
            break;
        }

        if (size > JPEG_BUF_LEN - 1 - psrv->jpeg_size) {
            ERROR("input: jpeg size too large, discarding buffer %d", size);
            psrv->jpeg_size = 0;
            continue;
        }

        if (pthread_mutex_lock(&(psrv->jpeg_mutex))) {
            ERROR("pthread_mutex_lock() failed");
            return -1;
        }

        /* clear the ready flag for all clients,
         * as we start building the next frame */
        //TODO may not be suitable for multi threaded sever

        for (i = 0; i < psrv->num_clients; i++) {
          DEBUG("[%d] clients %d %d %d\n", psrv->tcp_port, psrv->num_clients, psrv->jpeg_size, psrv->clients[i]->stream_fd);
            psrv->clients[i]->jpeg_ready = 0;
        }


        if (rem_len) {
            /* copy the remainder of data from the previous iteration back to the jpeg buffer */
            memmove(psrv->jpeg_buf, sep + (auto_separator ? 2 /* strlen(JPEG_END) */ : input_separator_len), rem_len);
            psrv->jpeg_size = rem_len;
        }

        memcpy(psrv->jpeg_buf + psrv->jpeg_size, input_buf, size);
        psrv->jpeg_size += size;

        /* look behind at most 2 * INPUT_BUF_LEN for a separator */
        sep = (char *) memmem(psrv->jpeg_buf + psrv->jpeg_size - MIN(2 * INPUT_BUF_LEN, psrv->jpeg_size), MIN(2 * INPUT_BUF_LEN, psrv->jpeg_size),
                psrv->input_separator, input_separator_len);

        if (sep) { /* found a separator, jpeg frame is ready */
            if (auto_separator) {
                rem_len = psrv->jpeg_size - (sep - psrv->jpeg_buf) - 2 /* strlen(JPEG_START) */;
                psrv->jpeg_size = sep - psrv->jpeg_buf + 2 /* strlen(JPEG_END) */;
            }
            else {
                rem_len = psrv->jpeg_size - (sep - psrv->jpeg_buf) - input_separator_len;
                psrv->jpeg_size = sep - psrv->jpeg_buf;
            }

            DEBUG("input: jpeg buffer ready with %d bytes", psrv->jpeg_size);

            /* set the ready flag and notify all client threads about it */
            for (i = 0; i < psrv->num_clients; i++) {
                psrv->clients[i]->jpeg_ready = 1;
            }
            if (pthread_cond_broadcast(&(psrv->jpeg_cond))) {
                ERROR("pthread_cond_broadcast() failed");
                return -1;
            }

            now = get_now();
            frame_int = frame_int * 0.7 + (now - last_frame_time) * 0.3;
            last_frame_time = now;
        }
        else {
            rem_len = 0;
        }

        if (pthread_mutex_unlock(&(psrv->jpeg_mutex))) {
            ERROR("pthread_mutex_unlock() failed");
            return -1;
        }

        if (sep) {
            DEBUG("current fps: %.01lf", 1 / frame_int);

            if (psrv->num_clients) {
                min_client_frame_int = psrv->clients[0]->frame_int;
                for (i = 0; i < psrv->num_clients; i++) {
                    if (psrv->clients[i]->frame_int < min_client_frame_int) {
                        min_client_frame_int = psrv->clients[i]->frame_int;
                    }
                }

                frame_int_adj = (min_client_frame_int - frame_int) * 1000000;
                if (frame_int_adj > 0) {
                    DEBUG("input frame int.: %.0lf us, client frame int.: %.0lf us, frame int. adjustment: %.0lf us",
                            frame_int * 1000000, min_client_frame_int * 1000000, frame_int_adj);

                    /* sleep between 1000 and 50000 us, depending on the frame interval adjustment */
                    usleep(MAX(1000, MIN(4 * frame_int_adj, 50000)));
                }
            }

            /* check for incoming clients;
             * placing this code inside the if (sep) will simply
             * reduce the number of times we check for incoming clients,
             * with no particular relation to the frame separator we've just found */
            client_t *client = NULL;

            if (!max_clients || psrv->num_clients < max_clients) {
                //INFO("Socket fd = %d", socket_fd);
                client = wait_for_client(socket_fd, psrv);
            }

            if (client) {
                if (pthread_create(&client->thread, NULL, (void *(*) (void *)) handle_client, client)) {
                    ERROR("pthread_create() failed");
                    return -1;
                }

                if (pthread_mutex_lock(&(psrv->clients_mutex))) {
                    ERROR("pthread_mutex_lock() failed");
                    return -1;
                }

                psrv->clients = (client_t**)realloc(psrv->clients, sizeof(client_t *) * (psrv->num_clients + 1));
                psrv->clients[psrv->num_clients++] = client;

                DEBUG("current clients: %d", psrv->num_clients);

                if (pthread_mutex_unlock(&(psrv->clients_mutex))) {
                    ERROR("pthread_mutex_unlock() failed");
                    return -1;
                }
            }
        }
    }
    
    psrv->running = 0;

    DEBUG("closing server");
    close(socket_fd);

    DEBUG("waiting for clients to finish");
    for (i = 0; i < psrv->num_clients; i++) {
        psrv->clients[i]->jpeg_ready = 1;
    }
    if (pthread_cond_broadcast(&(psrv->jpeg_cond))) {
        ERROR("pthread_cond_broadcast() failed");
        return -1;
    }

    for (i = 0; i < psrv->num_clients; i++) {
        pthread_join(psrv->clients[i]->thread, NULL);
    }

    if (pthread_mutex_destroy(&(psrv->clients_mutex))) {
        ERROR("pthread_mutex_destroy() failed");
        return -1;
    }
    if (pthread_mutex_destroy(&(psrv->jpeg_mutex))) {
        ERROR("pthread_mutex_destroy() failed");
        return -1;
    }
    if (pthread_cond_destroy(&(psrv->jpeg_cond))) {
        ERROR("pthread_cond_destroy() failed");
        return -1;
    }
    if (pthread_mutex_destroy(&(psrv->frame_mutex))) {
        ERROR("pthread_mutex_destroy() failed");
        return -1;
    }
    if (pthread_cond_destroy(&(psrv->frame_cond))) {
        ERROR("pthread_cond_destroy() failed");
        return -1;
    }


    INFO("bye!");

    return 0;
}
