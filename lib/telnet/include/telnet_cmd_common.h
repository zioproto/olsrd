
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 *                     includes code by Bruno Randolf
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

#ifndef _OLSRD_TELNET_CMD_COMMON
#define _OLSRD_TELNET_CMD_COMMON

#include "telnet_cmd_core.h"

#define STR_CONCAT(x, y) x ## y
#define DEFINE_TELNET_CMD(CMD_STRUCT, CMD_CODE, HANDLE_FUNCTION, SHORT_HELP, USAGE)     \
static telnet_cmd_function HANDLE_FUNCTION(int, int, char**);                           \
struct telnet_cmd_functor STR_CONCAT(HANDLE_FUNCTION, _functor) = { &HANDLE_FUNCTION }; \
cmd_t CMD_STRUCT = {                                                                    \
  CMD_CODE, &STR_CONCAT(HANDLE_FUNCTION, _functor),                                     \
  SHORT_HELP,                                                                           \
  USAGE,                                                                                \
  NULL                                                                                  \
}

#define telnet_cmd_find_table(TABLE, COMMAND)                      \
  do {                                                             \
    cmd_t* tmp_cmd;                                                \
    if(!COMMAND)                                                   \
      return NULL;                                                 \
    for(tmp_cmd = TABLE; tmp_cmd; tmp_cmd = tmp_cmd->next)         \
      if(!strcmp(tmp_cmd->command, COMMAND))                       \
        return tmp_cmd;                                            \
    return NULL;                                                   \
  } while(false)

#define telnet_cmd_add_table(TABLE, CMD)                           \
  do {                                                             \
    cmd_t* tmp_cmd;                                                \
    for(tmp_cmd = TABLE; tmp_cmd; tmp_cmd = tmp_cmd->next)         \
      if(!strcmp(tmp_cmd->command, CMD->command))                  \
        return 0;                                                  \
    CMD->next = TABLE;                                             \
    TABLE = CMD;                                                   \
  } while(false)

#define telnet_cmd_remove_table(TABLE, COMMAND)                    \
  do {                                                             \
    cmd_t* tmp_cmd;                                                \
    if(!strcmp(TABLE->command, COMMAND)) {                         \
      cmd_t* removee = TABLE;                                      \
      TABLE = TABLE->next;                                         \
      return removee;                                              \
    }                                                              \
    for(tmp_cmd = TABLE; tmp_cmd->next; tmp_cmd = tmp_cmd->next) { \
      if(!strcmp(tmp_cmd->next->command, COMMAND)) {               \
        cmd_t* removee = tmp_cmd->next;                            \
        tmp_cmd->next = tmp_cmd->next->next;                       \
        return removee;                                            \
      }                                                            \
    }                                                              \
  } while(false)

#define telnet_print_usage(c, cmd) telnet_client_printf(c, "usage:\n\r%s\n\r", cmd.usage_text)

#endif /* _OLSRD_TELNET_CMD_COMMON */
