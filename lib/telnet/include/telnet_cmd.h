
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

#ifndef _OLSRD_TELNET_CMD
#define _OLSRD_TELNET_CMD

#include "telnet_cmd_common.h"

#ifdef TELNET_FOREIGN_CMDS
#define telnet_client_quit(C)                                           \
  do {                                                                  \
    if(olsr_cnf->telnet_foreign_cmds.client_quit)                       \
      olsr_cnf->telnet_foreign_cmds.client_quit(C);                     \
  } while(false)
#else
#define telnet_client_quit(C) do { } while(false)
#endif /* TELNET_FOREIGN_CMDS */


#ifdef TELNET_FOREIGN_CMDS
#define telnet_client_printf(C, FMT_ARGS...)                            \
  do {                                                                  \
    if(olsr_cnf->telnet_foreign_cmds.client_printf)                     \
      olsr_cnf->telnet_foreign_cmds.client_printf(C, FMT_ARGS);         \
  } while(false)
#else
#define telnet_client_printf(C, FMT, ARGS...) do { } while(false)
#endif /* TELNET_FOREIGN_CMDS */

#ifdef TELNET_FOREIGN_CMDS
#define telnet_cmd_add(CMD)                                             \
  do {                                                                  \
    cmd_t* cmd_ptr = CMD;                                               \
    if(!cmd_ptr || !cmd_ptr->command || !cmd_ptr->cmd_function ||       \
       !cmd_ptr->short_help || !cmd_ptr->usage_text)                    \
      break;                                                            \
    telnet_cmd_add_table(olsr_cnf->telnet_foreign_cmds.table, cmd_ptr); \
  } while(false)
#else
#define telnet_cmd_add(CMD) do { } while(false)
#endif /* TELNET_FOREIGN_CMDS */

#endif /* _OLSRD_TELNET_CMD */
