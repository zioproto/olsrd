/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 *                     includes code by Bruno Randolf
 *                     includes code by Andreas Lopatic
 *                     includes code by Sven-Ola Tuecke
 *                     includes code by Lorenz Schori
 *                     includes bugs by Markus Kittenberger
 *                     includes bugs by Hans-Christoph Steiner
 *                     includes bugs by Christian Pointner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */


#include <sys/types.h>
#include <sys/socket.h>
#ifndef _WIN32
#include <sys/select.h>
#endif /* _WIN32 */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <libgen.h>

#ifdef __linux__
#include <fcntl.h>
#endif /* __linux__ */

#include "olsr.h"
#include "olsr_types.h"
#include "scheduler.h"
#include "common/autobuf.h"

#include "olsrd_telnet.h"
#include "olsrd_plugin.h"

#include "cmd_handler.h"
#include "cmd_hna.h"
#include "cmd_interface.h"
#include "cmd_terminate.h"

#ifdef _WIN32
#define close(x) closesocket(x)
#endif /* _WIN32 */

static int telnet_socket;

/* TELNET initialization function */
static int plugin_telnet_init(void);

static void telnet_action(int, void *, unsigned int);

static int telnet_client_add(int);
static void telnet_client_remove(int);
static int telnet_client_find(int);
static void telnet_client_prompt(int);
static void telnet_client_handle_cmd(int, char*);
static void telnet_client_action(int, void *, unsigned int);
static void telnet_client_read(int);
static void telnet_client_write(int);

#define MAX_CLIENTS 3
#define BUF_SIZE 1024
#define MAX_ARGS 16

typedef struct {
  int fd;
  struct autobuf out;
  struct autobuf in;
} client_t;

static client_t clients[MAX_CLIENTS];


/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in olsrd_plugin.c
 */
int
olsrd_plugin_init(void)
{
  telnet_socket = -1;
  plugin_telnet_init();
  return 1;
}

/**
 * destructor - called at unload
 */
void
olsr_plugin_exit(void)
{
  if (telnet_socket != -1)
    close(telnet_socket);
}

static void enable_commands(void)
{
  if(cmd_terminate_init()) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) failed: enabling terminate command\n");
#endif /* NODEBUG */
  }

  if(cmd_hna_init()) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) failed: enabling hna command\n");
#endif /* NODEBUG */
  }

  if(cmd_interface_init()) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) failed: enabling interface command\n");
#endif /* NODEBUG */
  }
}

static int
plugin_telnet_init(void)
{
  union olsr_sockaddr sst;
  uint32_t yes = 1, i;
  socklen_t addrlen;

  /* Init telnet socket */
  if ((telnet_socket = socket(olsr_cnf->ip_version, SOCK_STREAM, 0)) == -1) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) socket()=%s\n", strerror(errno));
#endif /* NODEBUG */
    return 0;
  } else {
    if (setsockopt(telnet_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
#ifndef NODEBUG
      olsr_printf(1, "(TELNET) setsockopt()=%s\n", strerror(errno));
#endif /* NODEBUG */
      return 0;
    }
#if (defined __FreeBSD__ || defined __FreeBSD_kernel__) && defined SO_NOSIGPIPE
    if (setsockopt(telnet_socket, SOL_SOCKET, SO_NOSIGPIPE, (char *)&yes, sizeof(yes)) < 0) {
      perror("SO_REUSEADDR failed");
      return 0;
    }
#endif /* (defined __FreeBSD__ || defined __FreeBSD_kernel__) && defined SO_NOSIGPIPE */
    /* Bind the socket */

    /* complete the socket structure */
    memset(&sst, 0, sizeof(sst));
    if (olsr_cnf->ip_version == AF_INET) {
      sst.in4.sin_family = AF_INET;
      addrlen = sizeof(struct sockaddr_in);
#ifdef SIN6_LEN
      sst.in4.sin_len = addrlen;
#endif /* SIN6_LEN */
      sst.in4.sin_addr.s_addr = telnet_listen_ip.v4.s_addr;
      sst.in4.sin_port = htons(telnet_port);
    } else {
      sst.in6.sin6_family = AF_INET6;
      addrlen = sizeof(struct sockaddr_in6);
#ifdef SIN6_LEN
      sst.in6.sin6_len = addrlen;
#endif /* SIN6_LEN */
      sst.in6.sin6_addr = telnet_listen_ip.v6;
      sst.in6.sin6_port = htons(telnet_port);
    }

    /* bind the socket to the port number */
    if (bind(telnet_socket, &sst.in, addrlen) == -1) {
#ifndef NODEBUG
      olsr_printf(1, "(TELNET) bind()=%s\n", strerror(errno));
#endif /* NODEBUG */
      return 0;
    }

    /* show that we are willing to listen */
    if (listen(telnet_socket, 1) == -1) {
#ifndef NODEBUG
      olsr_printf(1, "(TELNET) listen()=%s\n", strerror(errno));
#endif /* NODEBUG */
      return 0;
    }

    /* Register with olsrd */
    add_olsr_socket(telnet_socket, &telnet_action, NULL, NULL, SP_PR_READ);

#ifndef NODEBUG
    olsr_printf(2, "(TELNET) listening on port %d\n", telnet_port);
#endif /* NODEBUG */

    for(i=0; i<MAX_CLIENTS; ++i) {
      int ret;
      clients[i].fd = -1;
      ret = abuf_init(&(clients[i].out), BUF_SIZE);
      if(ret) {
#ifndef NODEBUG
        olsr_printf(1, "(TELNET) abuf_init()=-1\n");
#endif /* NODEBUG */
        return 0;
      }
      ret = abuf_init(&(clients[i].in), BUF_SIZE);
      if(ret) {
        abuf_free(&(clients[i].out));
#ifndef NODEBUG
        olsr_printf(1, "(TELNET) abuf_init()=-1\n");
#endif /* NODEBUG */
        return 0;
      }
    }
  }

  enable_commands();
  return 1;
}

void telnet_client_quit(int c)
{
  telnet_client_remove(c);
}

void telnet_client_printf(int c, const char* fmt, ...)
{
  int ret, old_len = clients[c].out.len;
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  ret = abuf_vappendf(&(clients[c].out), fmt, arg_ptr);
  va_end(arg_ptr);

  if(ret < 0)
    return;

  if(!old_len)
    enable_olsr_socket(clients[c].fd, &telnet_client_action, NULL, SP_PR_WRITE);
}


static void
telnet_action(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  union olsr_sockaddr pin;

  char addr[INET6_ADDRSTRLEN];
  int client_fd, c;

  socklen_t addrlen = sizeof(pin);

  if ((client_fd = accept(fd, &pin.in, &addrlen)) == -1) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) accept()=%s\n", strerror(errno));
#endif /* NODEBUG */
    return;
  }

  if (olsr_cnf->ip_version == AF_INET) {
    if (inet_ntop(olsr_cnf->ip_version, &pin.in4.sin_addr, addr, INET6_ADDRSTRLEN) == NULL)
      addr[0] = '\0';
  } else {
    if (inet_ntop(olsr_cnf->ip_version, &pin.in6.sin6_addr, addr, INET6_ADDRSTRLEN) == NULL)
      addr[0] = '\0';
  }

  c = telnet_client_add(client_fd);
  if(c < MAX_CLIENTS) {
#ifndef NODEBUG
      olsr_printf(2, "(TELNET) Connect from %s (client: %d)\n", addr, c);
#endif /* NODEBUG */
  } else {
    close(client_fd);
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) Connect from %s (maximum number of clients reached!)\n", addr);
#endif /* NODEBUG */
  }
}

static void
telnet_client_prompt(int c)
{
  telnet_client_printf(c, "> ");
}

static int
telnet_client_add(int fd)
{
  int c;
  for(c=0; c < MAX_CLIENTS; c++) {
    if(clients[c].fd == -1) {
      clients[c].fd = fd;
      abuf_pull(&(clients[c].out), clients[c].out.len);
      abuf_pull(&(clients[c].in), clients[c].in.len);
      add_olsr_socket(fd, &telnet_client_action, NULL, NULL, SP_PR_READ);
      telnet_client_prompt(c);
      break;
    }
  }
  return c;
}

static void
telnet_client_remove(int c)
{
  remove_olsr_socket(clients[c].fd, &telnet_client_action, NULL);
  close(clients[c].fd);
  clients[c].fd = -1;
  abuf_pull(&(clients[c].out), clients[c].out.len);
  abuf_pull(&(clients[c].in), clients[c].in.len);
}

static int
telnet_client_find(int fd)
{
  int c;
  for(c=0; c<MAX_CLIENTS; c++) {
    if(clients[c].fd == fd)
      break;
  }
  return c;
}

static void
telnet_client_handle_cmd(int c, char* cmd)
{
  int i;
  char* argv[MAX_ARGS];

  if(!strlen(cmd))
    return;

  argv[0] = strtok(cmd, " \t");
  for(i=1; i<MAX_ARGS;++i) {
    argv[i] = strtok(NULL, " \t");
    if(argv[i] == NULL)
      break;
  }

  telnet_cmd_dispatch(c, i, argv);
  telnet_client_prompt(c);
}

static void
telnet_client_action(int fd, void *data __attribute__ ((unused)), unsigned int flags)
{
  int c = telnet_client_find(fd);
  if(c == MAX_CLIENTS) {        // unknown client...???
    remove_olsr_socket(fd, &telnet_client_action, NULL);
    close(fd);
    return;
  }

  if(flags & SP_PR_WRITE)
    telnet_client_write(c);

  if(flags & SP_PR_READ)
    telnet_client_read(c);
}

static void
telnet_client_read(int c)
{
  char buf[BUF_SIZE];
  ssize_t result = recv(clients[c].fd, (void *)buf, sizeof(buf)-1, 0);
  if (result > 0) {
    size_t offset = clients[c].in.len;
    buf[result] = 0;
    abuf_puts(&(clients[c].in), buf);

    for(;;) {
      char* line_end = strpbrk(&(clients[c].in.buf[offset]), "\n\r");
      if(line_end == NULL)
        break;

      *line_end = 0;
      telnet_client_handle_cmd(c, clients[c].in.buf);
      if(clients[c].fd < 0)
        break; // client connection was terminated

      if(line_end >= &(clients[c].in.buf[clients[c].in.len - 1])) {
        abuf_pull(&(clients[c].in), clients[c].in.len);
        break;
      }

      abuf_pull(&(clients[c].in), line_end + 1 - clients[c].in.buf);
      offset = 0;
    }
  }
  else {
    if(result == 0) {
#ifndef NODEBUG
      olsr_printf(2, "(TELNET) client %i: disconnected\n", c);
#endif /* NODEBUG */
      telnet_client_remove(c);
    } else {
      if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return;

#ifndef NODEBUG
      olsr_printf(1, "(TELNET) client %i recv(): %s\n", c, strerror(errno));
#endif /* NODEBUG */
      telnet_client_remove(c);
    }
  }
}

static void
telnet_client_write(int c)
{
  ssize_t result = send(clients[c].fd, (void *)clients[c].out.buf, clients[c].out.len, 0);
  if (result > 0) {
    abuf_pull(&(clients[c].out), result);
    if(clients[c].out.len == 0)
      disable_olsr_socket(clients[c].fd, &telnet_client_action, NULL, SP_PR_WRITE);
  }
  else if(result < 0) {
    if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return;

#ifndef NODEBUG
    olsr_printf(1, "(TELNET) client %i write(): %s\n", c, strerror(errno));
#endif /* NODEBUG */
    telnet_client_remove(c);
  }
}
