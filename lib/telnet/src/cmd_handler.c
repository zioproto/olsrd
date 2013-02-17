
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

#include "olsr.h"
#include "olsr_types.h"
#include "ipcalc.h"

#include "olsrd_telnet.h"
#include "cmd_handler.h"

#include "cmd_hna.h"
#include "cmd_interface.h"

static void cmd_quit(int, int, char**);
cmd_t quit_cmd = {
   "quit", cmd_quit,
   "terminates telnet connection",
   " quit"
};

static void cmd_terminate(int, int, char**);
cmd_t terminate_cmd = {
   "terminate", cmd_terminate,
   "terminate olsr daemon",
   " terminate <reason>"
};

static void cmd_help(int, int, char**);
cmd_t help_cmd = {
   "help", cmd_help,
   "prints usage strings",
   " help [<command>]"
};

cmd_t* dispatch_table[] = {
  &hna_cmd,
  &interface_cmd,
  &quit_cmd,
  &terminate_cmd,
  &help_cmd
};

inline void telnet_print_usage(int c, cmd_t* cmd)
{
  telnet_client_printf(c, "usage:\n\r%s\n\r", cmd->usage_text);
}

void telnet_cmd_dispatcher(int c, int argc, char* argv[])
{
  size_t i;

  if(argc < 1)
    return;

  for(i = 0; i < sizeof(dispatch_table)/sizeof(cmd_t*); ++i) {
    if(!strcmp(dispatch_table[i]->command, argv[0]))
      return dispatch_table[i]->cmd_function(c, argc, argv);
  }

  telnet_client_printf(c, "command '%s' unknown - enter help for a list of commands\n\r", argv[0]);
}

static void cmd_quit(int c, int argc __attribute__ ((unused)), char* argv[] __attribute__ ((unused)))
{
  if(argc != 1) {
    telnet_print_usage(c, &quit_cmd);
    return;
  }

  telnet_client_quit(c);
}

static void cmd_terminate(int c, int argc, char* argv[])
{
  if(argc != 2) {
    telnet_print_usage(c, &terminate_cmd);
    return;
  }

  olsr_exit(argv[1], EXIT_SUCCESS);
}

static void cmd_help(int c, int argc __attribute__ ((unused)), char* argv[] __attribute__ ((unused)))
{
  size_t i;

  switch(argc) {
  case 1:
    for(i = 0; i < sizeof(dispatch_table)/sizeof(cmd_t*); ++i) {
      telnet_client_printf(c, " %-16s %s\n\r", dispatch_table[i]->command, dispatch_table[i]->short_help);
    }
    return;
  case 2:
    for(i = 0; i < sizeof(dispatch_table)/sizeof(cmd_t*); ++i) {
      if(!strcmp(dispatch_table[i]->command, argv[1])) {
        telnet_client_printf(c, "%s: %s\n\r\n\r", dispatch_table[i]->command, dispatch_table[i]->short_help);
        telnet_print_usage(c, dispatch_table[i]);
        return;
      }
    }
    return telnet_client_printf(c, "command '%s' unknown\n\r", argv[1]);
  default:
    telnet_print_usage(c, &help_cmd); return;
  }

}
