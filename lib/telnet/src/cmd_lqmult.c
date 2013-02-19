
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
#include <unistd.h>

#include "olsr.h"
#include "olsr_types.h"
#include "ipcalc.h"
#include "interfaces.h"

#include "telnet_client.h"
#include "telnet_cmd.h"
#include "cmd_lqmult.h"

DEFINE_TELNET_CMD(cmd_lqmult_struct,
                  "lqmult", handle_lqmult,
                  "add/remove/change or show link quality multipliers",
                  " lqmult (add|update) (<interface>|all) <neigbor> <value>\n\r"
                  " lqmult del (<interface>|all) <neigbor>\n\r"
                  " lqmult get (<interface>|all) <neigbor>\n\r"
                  " lqmult flush [<interface>]\n\r"
                  " lqmult list [<interface>]");

int cmd_lqmult_init(void)
{
  return telnet_cmd_add(&cmd_lqmult_struct);
}

const char* cmd_lqmult_get_command(void)
{
  return cmd_lqmult_struct.command;
}


/* struct olsr_lq_mult { */
/*   union olsr_ip_addr addr; */
/*   uint32_t value; */
/*   struct olsr_lq_mult *next; */
/* }; */
  /* struct olsr_lq_mult *mult, *mult_temp; */
  /* ifs->cnf->lq_mult=NULL; */
  /* for (mult = olsr_cnf->interface_defaults->lq_mult; mult; mult=mult->next) { */
  /*   mult_temp=olsr_malloc(sizeof(struct olsr_lq_mult), "telnet inteface add mult_temp"); */
  /*   memcpy(mult_temp,mult,sizeof(struct olsr_lq_mult)); */
  /*   mult_temp->next=ifs->cnf->lq_mult; */
  /*   ifs->cnf->lq_mult=mult_temp; */
  /* } */

static telnet_cmd_function cmd_lqmult_add(struct olsr_if *ifs, int c)
{
  telnet_client_printf(c, "adding lqmult to interface '%s'\n\r", ifs->name);
  return NULL;
}

static telnet_cmd_function cmd_lqmult_update(struct olsr_if *ifs, int c)
{
  telnet_client_printf(c, "updating lqmult on interface '%s'\n\r", ifs->name);
  return NULL;
}

static telnet_cmd_function cmd_lqmult_del(struct olsr_if *ifs, int c)
{
  telnet_client_printf(c, "removing lqmult on interface '%s'\n\r", ifs->name);
  return NULL;
}

static telnet_cmd_function cmd_lqmult_get(const struct olsr_if *ifs, int c)
{
  telnet_client_printf(c, "retreiving lqmult on interface '%s'\n\r", ifs->name);
  return NULL;
}

static void cmd_lqmult_list(const struct olsr_if *ifs, int c)
{
  struct olsr_lq_mult *mult;

  if(!ifs)
    return;

  telnet_client_printf(c, "%s:\n\r", ifs->name);
  for(mult = ifs->cnf->lq_mult; mult; mult=mult->next) {
    struct ipaddr_str addrbuf;
    const char* n = (ipequal(&(mult->addr), &olsr_ip_zero)) ? "default" : olsr_ip_to_string(&addrbuf, &(mult->addr));
    telnet_client_printf(c, "  %s: %0.2f\n\r", n, (double)(mult->value) / (double)65536.0);
  }
}

static void cmd_lqmult_flush(const struct olsr_if *ifs, int c)
{
  if(!ifs)
    return;

  do {
    struct olsr_lq_mult *mult_temp = ifs->cnf->lq_mult->next;
    free(ifs->cnf->lq_mult);
    ifs->cnf->lq_mult = mult_temp;
  } while(ifs->cnf->lq_mult);
  telnet_client_printf(c, "removed all link quality multipliers for interface '%s'\n\r", ifs->name);
}

#define cmd_lqmult_foreach_interface(FUNC, ARGS...)                 \
  do {                                                              \
    const struct olsr_if *ifs;                                      \
    for (ifs = olsr_cnf->interfaces; ifs != NULL; ifs = ifs->next)  \
      FUNC (ifs, ARGS);                                             \
  } while(false)                                                    \


static struct olsr_if* cmd_lqmult_get_ifs(int c, const char* if_name)
{
  struct olsr_if *ifs = olsrif_ifwithname(if_name);
  if(!ifs) {
    telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", if_name);
    return NULL;
  }
  return ifs;
}

static telnet_cmd_function handle_lqmult(int c, int argc, char* argv[])
{
  if(argc == 2 || argc == 3) {

    if(!strcmp(argv[1], "list")) {
      if(argc == 2)
        cmd_lqmult_foreach_interface(cmd_lqmult_list, c);
      else
        cmd_lqmult_list(cmd_lqmult_get_ifs(c, argv[2]), c);
      return NULL;
    }

    if(!strcmp(argv[1], "flush")) {
      if(argc == 2)
        cmd_lqmult_foreach_interface(cmd_lqmult_flush, c);
      else
        cmd_lqmult_flush(cmd_lqmult_get_ifs(c, argv[2]), c);
      return NULL;
    }
  }

  if(argc > 3) {
    struct olsr_if *ifs = cmd_lqmult_get_ifs(c, argv[2]);
    if(!ifs)
      return NULL;

        // TODO: parse neigbor from argv[3]

    if(!strcmp(argv[1], "get")) {
      return cmd_lqmult_get(ifs, c);
    }
    else if(!strcmp(argv[1], "del")) {
      return cmd_lqmult_del(ifs, c);
    }

    if(argc == 5) {

          // TODO: parse value from argv[4]

      if(!strcmp(argv[1], "add")) {
        return cmd_lqmult_add(ifs, c);
      }
      else if(!strcmp(argv[1], "update")) {
        return cmd_lqmult_update(ifs, c);
      }
    }
  }

  telnet_print_usage(c, cmd_lqmult_struct);
  return NULL;
}
