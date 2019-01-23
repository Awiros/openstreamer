
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

#ifndef __STREAMEYE_H
#define __STREAMEYE_H

#include "client.h"

#define STREAM_EYE_VERSION      "0.8"

#define BOUNDARY_SEPARATOR      "--FrameBoundary"

#define DEF_CLIENT_TIMEOUT      10
//#define DEF_TCP_PORT            8080

#define REQ_BUF_LEN             4096
#define INPUT_BUF_LEN           1024 * 1024 /* 1MB */
#define JPEG_BUF_LEN            1024 * 1024 * 10 /* 10MB */
#define JPEG_START              "\xFF\xD8"
#define JPEG_END                "\xFF\xD9"

#include <pthread.h>

struct seye_srv_t{

  int       ready_state;
  int       frame_no;
  int       id;
  char      *pimgbuf;
  int       bufsize;
  int       webport;
  int       running=1;
  char      jpeg_buf[JPEG_BUF_LEN];
  int       jpeg_size;

  client_t  **clients = NULL;
  int       num_clients = 0;

  char      *input_separator=NULL;
  int       tcp_port;


  pthread_cond_t frame_cond;
  pthread_mutex_t frame_mutex;

  pthread_cond_t jpeg_cond;
  pthread_mutex_t jpeg_mutex;
  pthread_mutex_t clients_mutex;
};

void        handle_client(client_t *client);
void        cleanup_client(client_t *client, seye_srv_t* psrv);
int         streameye_thread(void *arg);

#endif /* __STREAMEYE_H */
