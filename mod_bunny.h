/*
** Copyright (c) 2013 Marc Falzon / Cloudwatt
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
*/

#ifndef _MOD_BUNNY_H_
#define _MOD_BUNNY_H_

#ifndef NSCORE
#define NSCORE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <pthread.h>
#include <sys/queue.h>
#include <fnmatch.h>
#include <amqp.h>
#include <amqp_framing.h>

/* Nagios-related headers */
#include "nagios.h"
#include "macros.h"
#include "broker.h"
#include "nebmods.h"
#include "nebstructs.h"
#include "neberrors.h"

#define MB_VERSION                          "0.1.0"
#define MB_OK                               1
#define MB_NOK                              0
#define MB_BUF_LEN                          1024
#define MB_MAX_PATH_LEN                     PATH_MAX
#define MB_DEFAULT_DEBUG                    false
#define MB_DEFAULT_HOST                     "localhost"
#define MB_DEFAULT_PORT                     5672
#define MB_DEFAULT_VHOST                    "/"
#define MB_DEFAULT_USER                     "guest"
#define MB_DEFAULT_PASSWORD                 "guest"
#define MB_DEFAULT_PUBLISHER_EXCHANGE       "nagios"
#define MB_DEFAULT_PUBLISHER_EXCHANGE_TYPE  "direct"
#define MB_DEFAULT_PUBLISHER_ROUTING_KEY    "nagios_checks"
#define MB_DEFAULT_CONSUMER_EXCHANGE        "nagios"
#define MB_DEFAULT_CONSUMER_EXCHANGE_TYPE   "direct"
#define MB_DEFAULT_CONSUMER_QUEUE           "nagios_results"
#define MB_DEFAULT_CONSUMER_BINDING_KEY     "nagios_results"
#define MB_DEFAULT_RETRY_WAIT_TIME          3
#define MB_MAX_RETRY_WAIT_TIME              30

typedef TAILQ_HEAD(mb_hstgroups_s, mb_hstgroup_s) mb_hstgroups_t;
typedef struct mb_hstgroup_s {
/* {{{ */
    char *pattern;
    TAILQ_ENTRY(mb_hstgroup_s) tq;
/* }}} */
} mb_hstgroup_t;

typedef TAILQ_HEAD(mb_svcgroups_s, mb_svcgroup_s) mb_svcgroups_t;
typedef struct mb_svcgroup_s {
/* {{{ */
    char *pattern;
    TAILQ_ENTRY(mb_svcgroup_s) tq;
/* }}} */
} mb_svcgroup_t;

typedef TAILQ_HEAD(mb_hstgroup_routes_s, mb_hstgroup_route_s) mb_hstgroup_routes_t;
typedef struct mb_hstgroup_route_s {
    char            routing_key[MB_BUF_LEN];
    mb_hstgroups_t  *hstgroups;
    TAILQ_ENTRY(mb_hstgroup_route_s) tq;
} mb_hstgroup_route_t;

typedef struct mb_config_s {
/* {{{ */
    bool                    debug;
    int                     retry_wait_time;
    mb_hstgroup_routes_t    *hstgroups_routing_table;
    mb_hstgroups_t          *local_hstgroups;
    mb_svcgroups_t          *local_svcgroups;

    char                    host[MB_BUF_LEN];
    int                     port;
    char                    vhost[MB_BUF_LEN];
    char                    user[MB_BUF_LEN];
    char                    password[MB_BUF_LEN];

    amqp_connection_state_t publisher_amqp_conn;
    int                     publisher_amqp_sockfd;
    char                    publisher_exchange[MB_BUF_LEN];
    char                    publisher_routing_key[MB_BUF_LEN];
    char                    publisher_exchange_type[MB_BUF_LEN];
    bool                    publisher_connected;

    amqp_connection_state_t consumer_amqp_conn;
    int                     consumer_amqp_sockfd;
    char                    consumer_exchange[MB_BUF_LEN];
    char                    consumer_exchange_type[MB_BUF_LEN];
    char                    consumer_queue[MB_BUF_LEN];
    char                    consumer_binding_key[MB_BUF_LEN];
    bool                    consumer_connected;
/* }}} */
} mb_config_t;

/* mod_bunny.c */
void    mb_deregister_callbacks(void);
void    mb_free_hostgroups(mb_hstgroups_t *);
void    mb_free_hostgroups_routing_table(mb_hstgroup_routes_t *);
void    mb_free_local_servicegroups_list(mb_svcgroups_t *);
int     mb_handle_event(int, void *);
int     mb_handle_host_check(nebstruct_host_check_data *);
int     mb_handle_service_check(nebstruct_service_check_data *);
int     mb_in_local_hostgroups(host *);
int     mb_in_local_servicegroups(service *);
int     mb_init(int, void *);
int     mb_init_config();
char    *mb_lookup_hostgroups_routing_table(host *);
void    mb_register_callbacks(void);
void    mb_process_check_result(char *);
int     mb_publish_check(char *, char *);

/* mb_thread.c */
void    *mb_thread_consume(void *);
void    *mb_thread_publish(void *);

/* mb_amqp.c */
int     mb_amqp_connect_consumer(mb_config_t *);
int     mb_amqp_connect_publisher(mb_config_t *);
void    mb_amqp_consume(mb_config_t *, void (*)(char *));
int     mb_amqp_disconnect_consumer(mb_config_t *);
int     mb_amqp_disconnect_publisher(mb_config_t *);
int     mb_amqp_publish(mb_config_t *, char *, char *);

/* mb_json.c */
int             mb_json_parse_config(char *, mb_config_t *);
char            *mb_json_pack_host_check(nebstruct_host_check_data *, int, char *);
char            *mb_json_pack_service_check(nebstruct_service_check_data *, int, char *);
check_result    *mb_json_unpack_check_result(char *);

#endif

// vim: ft=c ts=4 et foldmethod=marker
