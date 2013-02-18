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


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "olsr.h"
#include "olsr_types.h"
#include "scheduler.h"
#include "common/autobuf.h"

#include "telnet_client.h"

#include "cmd_handler.h"

#ifdef _WIN32
#define close(x) closesocket(x)
#endif /* _WIN32 */



#define MAX_CLIENTS 3
#define BUF_SIZE 1024
#define MAX_ARGS 16

typedef struct {
  int fd;
  struct autobuf out;
  struct autobuf in;
  telnet_cmd_function continue_function;
} client_t;

static client_t clients[MAX_CLIENTS];

static void telnet_client_prompt(int);
static void telnet_client_remove(int);
static void telnet_client_action(int, void *, unsigned int);
static int telnet_client_find(int);
static void telnet_client_handle_cmd(int, char*);
static void telnet_client_read(int);
static void telnet_client_write(int);


/* needed by olsrd_telnet.c */
int telnet_client_init(void)
{
  int i, ret;
  for(i=0; i<MAX_CLIENTS; ++i) {
    clients[i].fd = -1;
    clients[i].continue_function = NULL;
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
  return 1;
}

void telnet_client_cleanup(void)
{
  int i;
  for(i=0; i<MAX_CLIENTS; ++i) {
    if(clients[i].fd != -1) {
      remove_olsr_socket(clients[i].fd, &telnet_client_action, NULL);
      close(clients[i].fd);
    }
    abuf_free(&(clients[i].out));
    abuf_free(&(clients[i].in));
  }
}


int
telnet_client_add(int fd)
{
  int c;
  for(c=0; c < MAX_CLIENTS; c++) {
    if(clients[c].fd == -1) {
      clients[c].fd = fd;
      clients[c].continue_function = NULL;
      abuf_pull(&(clients[c].out), clients[c].out.len);
      abuf_pull(&(clients[c].in), clients[c].in.len);
      add_olsr_socket(fd, &telnet_client_action, NULL, NULL, SP_PR_READ);
      telnet_client_prompt(c);
      break;
    }
  }
  return c < MAX_CLIENTS ? c : -1;
}


/* needed by command handler */
void telnet_client_quit(int c)
{
  if(c < 0 || c >= MAX_CLIENTS)
    return;

  telnet_client_remove(c);
}

void telnet_client_printf(int c, const char* fmt, ...)
{
  int ret, old_len;
  va_list arg_ptr;

  if(c < 0 || c >= MAX_CLIENTS)
    return;

  old_len = clients[c].out.len;
  va_start(arg_ptr, fmt);
  ret = abuf_vappendf(&(clients[c].out), fmt, arg_ptr);
  va_end(arg_ptr);

  if(ret < 0)
    return;

  if(!old_len)
    enable_olsr_socket(clients[c].fd, &telnet_client_action, NULL, SP_PR_WRITE);
}

void telnet_client_set_continue_function(int c, telnet_cmd_function f)
{
  if(c < 0 || c >= MAX_CLIENTS)
    return;

  clients[c].continue_function = f;
}

telnet_cmd_function telnet_client_get_continue_function(int c)
{
  if(c < 0 || c >= MAX_CLIENTS)
    return NULL;

  return clients[c].continue_function;
}


/* internal functions */
static void
telnet_client_prompt(int c)
{
  telnet_client_printf(c, "> ");
}

static void
telnet_client_remove(int c)
{
  remove_olsr_socket(clients[c].fd, &telnet_client_action, NULL);
  close(clients[c].fd);
  clients[c].fd = -1;
  clients[c].continue_function = NULL;
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
  if(!clients[c].continue_function)
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
