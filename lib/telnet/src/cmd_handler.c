
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

#include <string.h>

#include "olsrd_telnet.h"
#include "telnet_client.h"

#include "telnet_cmd.h"
#include "cmd_handler.h"

#ifdef TELNET_FOREIGN_CMDS
#include "olsr_cfg.h"
#endif /* TELNET_FOREIGN_CMDS */

static telnet_cmd_function handle_help(int, int, char**);
struct telnet_cmd_functor cmd_help_functor = { &handle_help };
cmd_t cmd_help_struct = {
  "help", &cmd_help_functor,
  "prints usage strings",
  " help [<command>]",
  NULL
};

static telnet_cmd_function handle_quit(int, int, char**);
struct telnet_cmd_functor cmd_quit_functor = { &handle_quit };
cmd_t cmd_quit_struct = {
  "quit", &cmd_quit_functor,
  "terminates telnet connection",
  " quit",
  &cmd_help_struct
};

static cmd_t* local_dispatch_table = &cmd_quit_struct;

static inline cmd_t* telnet_cmd_find(const char* command)
{
  telnet_cmd_find_table(local_dispatch_table, command);
}

#ifdef TELNET_FOREIGN_CMDS
static inline cmd_t* telnet_cmd_find_foreign(const char* command)
{
  telnet_cmd_find_table(olsr_cnf->telnet_foreign_cmds.table, command);
}
#endif /* TELNET_FOREIGN_CMDS */

int telnet_cmd_add(cmd_t* cmd)
{
  if(!cmd || !cmd->command || !cmd->cmd_function || !cmd->short_help || !cmd->usage_text)
    return 0;

  telnet_cmd_add_table(local_dispatch_table, cmd);
  return 1;
}

cmd_t* telnet_cmd_remove(const char* command)
{
  if(!command || !local_dispatch_table ||
     !strcmp(command, cmd_help_struct.command) ||
     !strcmp(command, cmd_quit_struct.command))
    return NULL;

  telnet_cmd_remove_table(local_dispatch_table, command);
  return NULL;
}


int telnet_cmd_dispatch(int c, int argc, char* argv[])
{
  telnet_cmd_function cmd_f;

  if(argc < 1)
    return -1;

  cmd_f = telnet_client_get_continue_function(c);
  if(!cmd_f) {
    cmd_t* tmp_cmd = telnet_cmd_find(argv[0]);
    if(tmp_cmd)
      cmd_f = tmp_cmd->cmd_function;
#ifdef TELNET_FOREIGN_CMDS
    if(telnet_allow_foreign) {
      tmp_cmd = telnet_cmd_find_foreign(argv[0]);
      if(tmp_cmd)
        cmd_f = tmp_cmd->cmd_function;
    }
#endif /* TELNET_FOREIGN_CMDS */
  }
  if(cmd_f) {
    telnet_client_set_continue_function(c, cmd_f->f(c, argc, argv));
    return 0;
  }

  telnet_client_printf(c, "FAILED: command '%s' unknown - enter help for a list of commands\n\r", argv[0]);
  return -1;
}



static telnet_cmd_function handle_quit(int c, int argc, char* argv[] __attribute__ ((unused)))
{
  if(argc != 1) {
    telnet_print_usage(c, cmd_quit_struct);
    return NULL;
  }

  telnet_client_quit(c);
  return NULL;
}

static telnet_cmd_function handle_help(int c, int argc, char* argv[])
{
  cmd_t* tmp_cmd;

  switch(argc) {
  case 1:
    for(tmp_cmd = local_dispatch_table; tmp_cmd; tmp_cmd = tmp_cmd->next)
      telnet_client_printf(c, " %-16s %s\n\r", tmp_cmd->command, tmp_cmd->short_help);
#ifdef TELNET_FOREIGN_CMDS
    if(telnet_allow_foreign) {
      for(tmp_cmd = olsr_cnf->telnet_foreign_cmds.table; tmp_cmd; tmp_cmd = tmp_cmd->next)
        telnet_client_printf(c, " %-16s %s\n\r", tmp_cmd->command, tmp_cmd->short_help);
    }
#endif /* TELNET_FOREIGN_CMDS */
    return NULL;
  case 2:
    tmp_cmd = telnet_cmd_find(argv[1]);
#ifdef TELNET_FOREIGN_CMDS
    if(!tmp_cmd && telnet_allow_foreign)
      tmp_cmd = telnet_cmd_find_foreign(argv[1]);
#endif /* TELNET_FOREIGN_CMDS */
    if(tmp_cmd) {
      telnet_client_printf(c, "%s: %s\n\r\n\r", tmp_cmd->command, tmp_cmd->short_help);
      telnet_print_usage(c, (*tmp_cmd));
    } else
      telnet_client_printf(c, "FAILED: command '%s' unknown\n\r", argv[1]);
    return NULL;
  default:
    telnet_print_usage(c, cmd_help_struct);
    return NULL;
  }
}
