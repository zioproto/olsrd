
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

#include "cmd_terminate.h"
#include "telnet_client.h"
#include "telnet_cmd.h"

DEFINE_TELNET_CMD(cmd_terminate_struct,
                  "terminate", handle_terminate,
                  "terminate olsr daemon",
                  " terminate");

int cmd_terminate_init(void)
{
  return telnet_cmd_add(&cmd_terminate_struct);
}

const char* cmd_terminate_get_command(void)
{
  return cmd_terminate_struct.command;
}

static telnet_cmd_function handle_inquiry(int c, int argc, char* argv[])
{
  if(argc != 1 || strcmp(argv[0], "YES")) {
    telnet_client_printf(c, "shutdown aborted\n\r");
    return NULL;
  }

  olsr_exit(argv[1], EXIT_SUCCESS);
  return NULL;
}
static struct telnet_cmd_functor handle_inquiry_functor = { &handle_inquiry };

static telnet_cmd_function handle_terminate(int c, int argc, char* argv[] __attribute__ ((unused)))
{
  if(argc != 1) {
    telnet_print_usage(c, cmd_terminate_struct);
    return NULL;
  }
  telnet_client_printf(c, "really want to quit olsr daemon?\n\r(type uppercase YES to confirm) .. ");
  return &handle_inquiry_functor;
}
