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

#include "mod_bunny.h"
#include "mb_amqp.h"

NEB_API_VERSION(CURRENT_NEB_API_VERSION);

/* Nagios global variables */
extern check_result *check_result_list;
extern int          currently_running_host_checks;
extern int          currently_running_service_checks;
extern int          event_broker_options;

/* Our module handle */
static void *mod_bunny_handle;

/* Module arguments */
static char *mod_bunny_args;

/* mod_bunny instance configuration */
static mb_config_t mod_bunny_config;

/* mod_bunny internal threads */
static pthread_t mb_consumer_thread;
static pthread_t mb_publisher_thread;

void mb_stop_consumer_thread(void) {
/* {{{ */
    pthread_cancel(mb_consumer_thread);
    pthread_join(mb_consumer_thread, NULL);
/* }}} */
}

void mb_stop_publisher_thread(void) {
/* {{{ */
    pthread_cancel(mb_publisher_thread);
    pthread_join(mb_publisher_thread, NULL);
/* }}} */
}

int nebmodule_init(int flags __attribute__((__unused__)), char *args, nebmodule *handle) {
/* {{{ */
    mod_bunny_handle = handle;
    mod_bunny_args = args;

    neb_set_module_info(mod_bunny_handle, NEBMODULE_MODINFO_TITLE, "mod_bunny");
    neb_set_module_info(mod_bunny_handle, NEBMODULE_MODINFO_AUTHOR, "Marc Falzon");
    neb_set_module_info(mod_bunny_handle, NEBMODULE_MODINFO_VERSION, MB_VERSION);
    neb_set_module_info(mod_bunny_handle, NEBMODULE_MODINFO_LICENSE, "MIT");
    neb_set_module_info(mod_bunny_handle, NEBMODULE_MODINFO_DESC,
        "Send host/service checks to a RabbitMQ AMQP broker, fetch results back");

    logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: version %s", MB_VERSION);

    /* Check that the broker callbacks we need are available to us */
    if (!(event_broker_options & BROKER_PROGRAM_STATE)) {
        logit(NSLOG_RUNTIME_ERROR, TRUE,
            "mod_bunny: nebmodule_init: error: "
            "BROKER_PROGRAM_STATE (%i) callback unavailable, disabling module", BROKER_HOST_CHECKS);
        return (NEB_ERROR);
    }

    if (!(event_broker_options & BROKER_HOST_CHECKS)) {
        logit(NSLOG_RUNTIME_ERROR, TRUE,
            "mod_bunny: nebmodule_init: error: "
            "BROKER_HOST_CHECKS (%i) callback unavailable, disabling module", BROKER_HOST_CHECKS);
        return (NEB_ERROR);
    }

    if (!(event_broker_options & BROKER_SERVICE_CHECKS)) {
        logit(NSLOG_RUNTIME_ERROR, TRUE,
            "mod_bunny: nebmodule_init: error: "
            "BROKER_SERVICE_CHECKS (%i) callback unavailable, disabling module", BROKER_SERVICE_CHECKS);
        return (NEB_ERROR);
    }

    /*
        For now we only register to be notified when the main process is about to start,
        then we register all the callbacks we need to intercept host/service checks to
        publish them through the AMQP broker.
    */
    neb_register_callback(NEBCALLBACK_PROCESS_DATA, mod_bunny_handle, 0, mb_init);

    return (NEB_OK);
/* }}} */
}

int nebmodule_deinit(int flags __attribute__((__unused__)), int reason __attribute__((__unused__))) {
/* {{{ */
    /* Deregister for all events we previously registered for */
    mb_deregister_callbacks();

    if (mod_bunny_config.debug_level > 0)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: nebmodule_deinit: deregistered callbacks");

    mb_stop_publisher_thread();

    if (mod_bunny_config.debug_level > 0)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: nebmodule_deinit: stopped publisher thread");

    mb_stop_consumer_thread();

    if (mod_bunny_config.debug_level > 0)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: nebmodule_deinit: stopped consumer thread");

    /* Purge hostgroups routing table */
    if (mod_bunny_config.hstgroups_routing_table) {
        mb_free_hostgroups_routing_table(mod_bunny_config.hstgroups_routing_table);
        free(mod_bunny_config.hstgroups_routing_table);
        mod_bunny_config.hstgroups_routing_table = NULL;
    }

    /* Purge servicegroups routing table */
    if (mod_bunny_config.svcgroups_routing_table) {
        mb_free_servicegroups_routing_table(mod_bunny_config.svcgroups_routing_table);
        free(mod_bunny_config.svcgroups_routing_table);
        mod_bunny_config.svcgroups_routing_table = NULL;
    }

    /* Purge local hostgroups list */
    if (mod_bunny_config.local_hstgroups) {
        mb_free_hostgroups(mod_bunny_config.local_hstgroups);
        free(mod_bunny_config.local_hstgroups);
    }

    /* Purge local servicegroups list */
    if (mod_bunny_config.local_svcgroups) {
        mb_free_servicegroups(mod_bunny_config.local_svcgroups);
        free(mod_bunny_config.local_svcgroups);
    }

    return (NEB_OK);
/* }}} */
}

int mb_init(int event_type __attribute__((__unused__)), void *event_data) {
/* {{{ */
    struct nebstruct_process_struct *ps = NULL;

    ps = (struct nebstruct_process_struct *)event_data;

    if (ps->type == NEBTYPE_PROCESS_EVENTLOOPSTART) {
        /* We initialize the configuration now because we need the broker module arguments */
        if (!mb_init_config()) {
            logit(NSLOG_RUNTIME_ERROR, TRUE,
                "mod_bunny: mb_init: error while initializing configuration, disabling module");
            return (NEB_ERROR);
        } else {
            if (mod_bunny_config.debug_level > 0)
                logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_init: configuration initialized");
        }

        /* Start publisher thread */
        if (pthread_create(&mb_publisher_thread, NULL, mb_thread_publish, &mod_bunny_config) != 0) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_init: error: "
                "unable to start publisher thread");
            return (NEB_ERROR);
        } else {
            if (mod_bunny_config.debug_level > 0)
                logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_init: started publisher thread");
        }

        /* Start consumer thread */
        if (pthread_create(&mb_consumer_thread, NULL, mb_thread_consume, &mod_bunny_config) != 0) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_init: error: "
                "unable to start consumer thread");
            return (NEB_ERROR);
        } else {
            if (mod_bunny_config.debug_level > 0)
                logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_init: started consumer thread");
        }

        /* Now register necessary callbacks */
        mb_register_callbacks();

        if (mod_bunny_config.debug_level > 0)
            logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_init: registered callbacks");
    }

    return (NEB_OK);
/* }}} */
}

int mb_init_config() {
/* {{{ */
    /* Set default configuration settings */
    mod_bunny_config.debug_level = MB_DEFAULT_DEBUG_LEVEL;
    mod_bunny_config.retry_wait_time = MB_DEFAULT_RETRY_WAIT_TIME;

    strncpy(mod_bunny_config.host, MB_DEFAULT_HOST, MB_BUF_LEN - 1);
    mod_bunny_config.port = MB_DEFAULT_PORT;
    strncpy(mod_bunny_config.vhost, MB_DEFAULT_VHOST, MB_BUF_LEN - 1);
    strncpy(mod_bunny_config.user, MB_DEFAULT_USER, MB_BUF_LEN - 1);
    strncpy(mod_bunny_config.password, MB_DEFAULT_USER, MB_BUF_LEN - 1);

    strncpy(mod_bunny_config.publisher_exchange, MB_DEFAULT_PUBLISHER_EXCHANGE, MB_BUF_LEN - 1);
    strncpy(mod_bunny_config.publisher_exchange_type, MB_DEFAULT_PUBLISHER_EXCHANGE_TYPE, MB_BUF_LEN - 1);
    strncpy(mod_bunny_config.publisher_routing_key, MB_DEFAULT_PUBLISHER_ROUTING_KEY, MB_BUF_LEN - 1);

    strncpy(mod_bunny_config.consumer_exchange, MB_DEFAULT_CONSUMER_EXCHANGE, MB_BUF_LEN - 1);
    strncpy(mod_bunny_config.consumer_exchange_type, MB_DEFAULT_CONSUMER_EXCHANGE_TYPE, MB_BUF_LEN - 1);
    strncpy(mod_bunny_config.consumer_queue, MB_DEFAULT_CONSUMER_QUEUE, MB_BUF_LEN - 1);
    strncpy(mod_bunny_config.consumer_binding_key, MB_DEFAULT_CONSUMER_BINDING_KEY, MB_BUF_LEN - 1);

    mod_bunny_config.publisher_connected = false;
    mod_bunny_config.consumer_connected = false;

    if (mod_bunny_args != NULL && strlen(mod_bunny_args) > 0) {
        if (!mb_json_parse_config(mod_bunny_args, &mod_bunny_config))
            return (MB_NOK);
    }

    return (MB_OK);
/* }}} */
}

void mb_register_callbacks(void) {
/* {{{ */
    /* Host checks */
    neb_register_callback(NEBCALLBACK_HOST_CHECK_DATA, mod_bunny_handle, 0, mb_handle_event);

    /* Service checks */
    neb_register_callback(NEBCALLBACK_SERVICE_CHECK_DATA, mod_bunny_handle, 0, mb_handle_event);
/* }}} */
}

void mb_deregister_callbacks(void) {
/* {{{ */
    neb_deregister_callback(NEBCALLBACK_HOST_CHECK_DATA, mod_bunny_handle);
    neb_deregister_callback(NEBCALLBACK_SERVICE_CHECK_DATA, mod_bunny_handle);
    neb_deregister_callback(NEBCALLBACK_PROCESS_DATA, mb_init);
/* }}} */
}

int mb_handle_event(int event_type, void *event_data) {
/* {{{ */
    host                            *hst = NULL;
    service                         *svc = NULL;
    nebstruct_host_check_data       *hstdata = NULL;
    nebstruct_service_check_data    *svcdata = NULL;

    /* Only handle events if we are able to publish them */
    if (!mod_bunny_config.publisher_connected) {
        return (NEB_OK);
    } else {
        switch (event_type) {
        case NEBCALLBACK_HOST_CHECK_DATA: {
            hstdata = (nebstruct_host_check_data *)event_data;
            hst = (host *)hstdata->object_ptr;

            /* Intercept host checks at the earliest stage */
            if (hstdata->type != NEBTYPE_HOSTCHECK_SYNC_PRECHECK
                && hstdata->type != NEBTYPE_HOSTCHECK_ASYNC_PRECHECK)
                return (NEB_OK);

            /* Let Nagios handle this check if the host is member of local hostgroups */
            if (mod_bunny_config.local_hstgroups && mb_in_local_hostgroups(hst)) {
                if (mod_bunny_config.debug_level > 0)
                    logit(NSLOG_INFO_MESSAGE, TRUE,
                        "mod_bunny: mb_handle_event: host [%s] is member of local hostgroups, "
                        "not handling its check",
                        hstdata->host_name);

                return (NEB_OK);
            }

            /*
                If this check is marked as "orphaned" by Nagios, fake a check result to
                notify users since something might be wrong on the worker side
            */
            if (((host *)hstdata->object_ptr)->check_options & CHECK_OPTION_ORPHAN_CHECK) {
                if (mod_bunny_config.debug_level > 0)
                    logit(NSLOG_INFO_MESSAGE, TRUE,
                        "mod_bunny: mb_handle_event: host check for [%s] has been flagged as orphaned",
                        hstdata->host_name);

                mb_mark_check_orphaned(hstdata->host_name, NULL);
                return (NEBERROR_CALLBACKOVERRIDE);
            }

            /* If we can't handle host check, tell Nagios to reschedule it later */
            if (!mb_handle_host_check(hstdata))
                return (NEBERROR_CALLBACKCANCEL);

            /* Otherwise tell Nagios that we handled this check by ourselves */
            return (NEBERROR_CALLBACKOVERRIDE);
        }

        case NEBCALLBACK_SERVICE_CHECK_DATA: {
            svcdata = (nebstruct_service_check_data *)event_data;
            svc = (service *)svcdata->object_ptr;

            /* Intercept service checks at the earliest stage */
            if (svcdata->type != NEBTYPE_SERVICECHECK_ASYNC_PRECHECK)
                return (NEB_OK);

            /* Let Nagios handle this check if the service is member of local servicegroups */
            if (mod_bunny_config.local_svcgroups && mb_in_local_servicegroups(svc)) {
                if (mod_bunny_config.debug_level > 0)
                    logit(NSLOG_INFO_MESSAGE, TRUE,
                        "mod_bunny: mb_handle_event: service [%s/%s] is member of local servicegroups, "
                        "not handling its check",
                        svcdata->host_name,
                        svcdata->service_description);

                return (NEB_OK);
            }

            /*
                If this check is marked as "orphaned" by Nagios, fake a check result to
                notify users since something might be wrong on the worker side
            */
            if (((service *)svcdata->object_ptr)->check_options & CHECK_OPTION_ORPHAN_CHECK) {
                if (mod_bunny_config.debug_level > 0)
                    logit(NSLOG_INFO_MESSAGE, TRUE,
                        "mod_bunny: mb_handle_event: service check for [%s/%s] has been flagged as orphaned",
                        svcdata->host_name,
                        svcdata->service_description);

                mb_mark_check_orphaned(svcdata->host_name, svcdata->service_description);
                return (NEBERROR_CALLBACKOVERRIDE);
            }

            /* If we can't handle service check, tell Nagios to reschedule it later */
            if (!mb_handle_service_check(svcdata))
                return (NEBERROR_CALLBACKCANCEL);

            /* Otherwise tell Nagios that we handled this check by ourselves */
            return (NEBERROR_CALLBACKOVERRIDE);
        }

        default:
            logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_handle_event: unhandled event");
                return (NEB_OK);
        }
    }
/* }}} */
}

int mb_handle_host_check(nebstruct_host_check_data *hstdata) {
/* {{{ */
    host    *hst = NULL;
    char    *json_check = NULL;
    char    *raw_command = NULL;
    char    *processed_command = NULL;
    float   prev_latency;
    char    *routing_key = NULL;

    hst = (host *)hstdata->object_ptr;

    if (mod_bunny_config.debug_level > 0)
        logit(NSLOG_INFO_MESSAGE, TRUE,
            "mod_bunny: mb_handle_host_check: handling host check for [%s]", hstdata->host_name);

    /*
        Since we intercepted the host check at early stage,
        we have to do some Nagios magic (the "I have no idea what I'm doing" part)
        before sending the check (taken from nagios-x.y.x/base/checks.c)
    */

    /* Clear check options - we don't want old check options retained */
    hst->check_options = CHECK_OPTION_NONE;

    /* Get the command start time */
    gettimeofday(&hstdata->start_time, NULL);

    /* Update latency for macros, event broker, save old value for later */
    prev_latency = hst->latency;
    hst->latency = hstdata->latency;

    /* Unset the freshening flag, otherwise only the first freshness check would be run */
    hst->is_being_freshened = FALSE;

    /* Adjust host check attempt */
    adjust_host_check_attempt_3x(hst, TRUE);

    /* Grab the host macro variables */
    clear_volatile_macros();
    grab_host_macros(hst);

    /* Get the raw command line */
    get_raw_command_line(hst->check_command_ptr, hst->host_check_command, &raw_command, 0);

    if (!raw_command) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_handle_host_check: error: "
            "host check command undefined");
        goto error;
    }

    /* Process any macros contained in the argument */
    process_macros(raw_command, &processed_command, 0);

    if (!processed_command) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_handle_host_check: error: "
            "unable to process check command line");
        goto error;
    }

    /* Serialize host check into JSON */
    if (!(json_check = mb_json_pack_host_check(hstdata, hst->check_options, processed_command))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE,
            "mod_bunny: mb_handle_host_check: error occurred while packing JSON check data");
        goto error;
    }

    /* Get AMQP routing key for this host check */
    if (mod_bunny_config.hstgroups_routing_table)
        routing_key = mb_lookup_hostgroups_routing_table(hst);

    /* If no specific routing key defined, use the global routing key */
    if (!routing_key)
        routing_key = mod_bunny_config.publisher_routing_key;

    if (mod_bunny_config.debug_level > 0)
        logit(NSLOG_INFO_MESSAGE, TRUE,
            "mod_bunny: mb_handle_host_check: publishing host check [%s] with routing key \"%s\"",
            hstdata->host_name,
            routing_key);

    /* Send the JSON-formatted host check message to the broker */
    if (!mb_publish_check(json_check, routing_key)) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_handle_host_check: error: "
            "could not publish host check message");
        goto error;
    }

    /* Set the execution flag */
    hst->is_executing = TRUE;

    /* Increment the number of host checks that are currently running */
    currently_running_host_checks++;

    free(json_check);
    free(raw_command);
    free(processed_command);

    return (MB_OK);

    error:
    hst->latency = prev_latency;

    if (json_check)
        free(json_check);

    if (raw_command)
        free(raw_command);

    if (processed_command)
        free(processed_command);

    return (MB_NOK);
/* }}} */
}

int mb_handle_service_check(nebstruct_service_check_data *svcdata) {
/* {{{ */
    host    *hst = NULL;
    service *svc = NULL;
    char    *json_check = NULL;
    char    *raw_command = NULL;
    char    *processed_command = NULL;
    float   prev_latency;
    char    *routing_key = NULL;

    if (mod_bunny_config.debug_level > 0)
        logit(NSLOG_INFO_MESSAGE, TRUE,
            "mod_bunny: mb_handle_service_check: handling service check for [%s/%s]",
            svcdata->host_name,
            svcdata->service_description);

    svc = (service *)svcdata->object_ptr;
    hst = svc->host_ptr;

    /*
        Since we intercepted the service check at early stage,
        we have to do some Nagios magic (the "I have no idea what I'm doing" part)
        before sending the check (taken from nagios-x.y.x/base/checks.c)
    */

    /* Clear check options - we don't want old check options retained */
    svc->check_options = CHECK_OPTION_NONE;

    /* Get the command start time */
    gettimeofday(&svcdata->start_time, NULL);

    /* Unset the freshening flag, otherwise only the first freshness check would be run */
    svc->is_being_freshened = FALSE;

    /* Update latency for macros, event broker, save old value for later */
    prev_latency = svc->latency;
    svc->latency = svcdata->latency;

    /* Grab the host and service macro variables */
    clear_volatile_macros();
    grab_host_macros(hst);
    grab_service_macros(svc);

    /* Get the raw command line */
    get_raw_command_line(svc->check_command_ptr, svc->service_check_command, &raw_command, 0);

    if (!raw_command) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_handle_service_check: error: "
            "service check command undefined");
        goto error;
    }

    /* Process any macros contained in the argument */
    process_macros(raw_command, &processed_command, 0);

    if (!processed_command) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_handle_service_check: error: "
            "unable to process check command line");
        goto error;
    }

    /* Serialize service check into JSON */
    if (!(json_check = mb_json_pack_service_check(svcdata, svc->check_options, processed_command))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE,
            "mod_bunny: mb_handle_service_check: error occurred while packing JSON check data");
        goto error;
    }

    /* Get AMQP routing key for this service check */
    if (mod_bunny_config.svcgroups_routing_table)
        routing_key = mb_lookup_servicegroups_routing_table(svc);

    /* If no specific routing key defined, use the global routing key */
    if (!routing_key)
        routing_key = mod_bunny_config.publisher_routing_key;

    if (mod_bunny_config.debug_level > 0)
        logit(NSLOG_INFO_MESSAGE, TRUE,
            "mod_bunny: mb_handle_service_check: publishing service check [%s/%s] with routing key \"%s\"",
            svcdata->host_name,
            svcdata->service_description,
            routing_key);

    /* Publish the service check through the AMQP broker */
    if (!mb_publish_check(json_check, routing_key)) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_handle_service_check: error: "
            "could not publish service check message");
        goto error;
    }

    /* Set the execution flag */
    svc->is_executing = TRUE;

    /* Increment the number of service checks that are currently running */
    currently_running_service_checks++;

    free(json_check);
    free(raw_command);
    free(processed_command);

    return (MB_OK);

    error:
    svc->latency = prev_latency;

    if (json_check)
        free(json_check);

    if (raw_command)
        free(raw_command);

    if (processed_command)
        free(processed_command);

    return (MB_NOK);
/* }}} */
}

int mb_publish_check(char *check, char *routing_key) {
/* {{{ */
    if (!mb_amqp_publish(&mod_bunny_config, check, routing_key)) {
        logit(NSLOG_RUNTIME_ERROR, TRUE,
            "mod_bunny: mb_amqp_publish_check: error occurred while publishing message");

        /* In case of AMQP publishing error, disconnect from the broker as a safety measure */
        mb_amqp_disconnect_publisher(&mod_bunny_config);

        return (MB_NOK);
    }

    return (MB_OK);
/* }}} */
}

void mb_process_check_result(char *msg) {
/* {{{ */
    check_result *cr = NULL;

    assert(msg);

    if (!(cr = mb_json_unpack_check_result(msg))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_process_check_result: error: "
            "unable to unpack received check result, discarding");
        return;
    }

#if NAGIOS_3_5_X
    add_check_result_to_list(&check_result_list, cr);
#else
    add_check_result_to_list(cr);
#endif

    if (cr->object_check_type == HOST_CHECK) {
        if (mod_bunny_config.debug_level > 0)
            logit(NSLOG_INFO_MESSAGE, TRUE,
                "mod_bunny: mb_process_check_result: processed host check result for [%s]", cr->host_name);
    } else {
        if (mod_bunny_config.debug_level > 0)
            logit(NSLOG_INFO_MESSAGE, TRUE,
                "mod_bunny: mb_process_check_result: processed service check result for [%s/%s]",
                cr->host_name,
                cr->service_description);
    }
/* }}} */
}

void mb_mark_check_orphaned(char *host, char *service) {
/* {{{ */
    check_result *cr = NULL;

    if (!(cr = (check_result *)calloc(1, sizeof(check_result)))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_mark_check_orphaned: error: "
        "unable to allocate memory");
        return;
    }

    init_check_result(cr);

    cr->scheduled_check = TRUE;
    cr->reschedule_check = TRUE;
    cr->exited_ok = TRUE;
    cr->early_timeout = FALSE;
    cr->output = strdup("[mod_bunny] error: check is orphaned (no workers running?)");
    cr->output_file = NULL;
    cr->output_file_fp = NULL;
    cr->check_options = CHECK_OPTION_NONE;
    cr->start_time.tv_sec = (unsigned long)time(NULL);
    cr->finish_time.tv_sec = (unsigned long)time(NULL);
    cr->latency = 0;

    if (!host) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_mark_check_orphaned: error: "
        "host name unspecified");
        goto error;
    } else
        cr->host_name = strdup(host);

    if (service) {
        cr->object_check_type = SERVICE_CHECK;
        cr->check_type = SERVICE_CHECK_ACTIVE;
        cr->return_code = STATE_CRITICAL;
        cr->service_description = strdup(service);
    } else {
        cr->object_check_type = HOST_CHECK;
        cr->check_type = HOST_CHECK_ACTIVE;
        cr->return_code = HOST_UNREACHABLE;
    }

#if NAGIOS_3_5_X
    add_check_result_to_list(&check_result_list, cr);
#else
    add_check_result_to_list(cr);
#endif

    return;

    error:
    if (cr->output)
        free(cr->output);
    free(cr);
    return;
/* }}} */
}

char *mb_lookup_hostgroups_routing_table(host *hst) {
/* {{{ */
    objectlist          *obj = NULL;
    mb_hstgroup_route_t *hstgroup_route = NULL;
    mb_hstgroup_t       *hstgroup = NULL;

    for (obj = hst->hostgroups_ptr; obj != NULL; obj = obj->next) {
        TAILQ_FOREACH(hstgroup_route, mod_bunny_config.hstgroups_routing_table, tq) {
            TAILQ_FOREACH(hstgroup, hstgroup_route->hstgroups, tq) {
                if ((fnmatch(hstgroup->pattern, ((hostgroup *)obj->object_ptr)->group_name, 0) == 0)) {
                    return (hstgroup_route->routing_key);
                }
            }
        }
    }

    return (NULL);
/* }}} */
}

char *mb_lookup_servicegroups_routing_table(service *svc) {
/* {{{ */
    objectlist          *obj = NULL;
    mb_svcgroup_route_t *svcgroup_route = NULL;
    mb_svcgroup_t       *svcgroup = NULL;

    for (obj = svc->servicegroups_ptr; obj != NULL; obj = obj->next) {
        TAILQ_FOREACH(svcgroup_route, mod_bunny_config.svcgroups_routing_table, tq) {
            TAILQ_FOREACH(svcgroup, svcgroup_route->svcgroups, tq) {
                if ((fnmatch(svcgroup->pattern, ((servicegroup *)obj->object_ptr)->group_name, 0) == 0)) {
                    return (svcgroup_route->routing_key);
                }
            }
        }
    }

    return (NULL);
/* }}} */
}

int mb_in_local_hostgroups(host *hst) {
/* {{{ */
    objectlist      *obj = NULL;
    mb_hstgroup_t   *hstgroup = NULL;

    for (obj = hst->hostgroups_ptr; obj != NULL; obj = obj->next) {
        TAILQ_FOREACH(hstgroup, mod_bunny_config.local_hstgroups, tq) {
            if ((fnmatch(hstgroup->pattern, ((hostgroup *)obj->object_ptr)->group_name, 0) == 0)) {
                return (MB_OK);
            }
        }
    }

    return (MB_NOK);
/* }}} */
}

int mb_in_local_servicegroups(service *svc) {
/* {{{ */
    objectlist      *obj = NULL;
    mb_svcgroup_t   *svcgroup = NULL;

    for (obj = svc->servicegroups_ptr; obj != NULL; obj = obj->next) {
        TAILQ_FOREACH(svcgroup, mod_bunny_config.local_svcgroups, tq) {
            if ((fnmatch(svcgroup->pattern, ((servicegroup *)obj->object_ptr)->group_name, 0) == 0)) {
                return (MB_OK);
            }
        }
    }

    return (MB_NOK);
/* }}} */
}

void mb_free_hostgroups(mb_hstgroups_t *hostgroups) {
/* {{{ */
    mb_hstgroup_t *hostgroup = NULL;

    while ((hostgroup = TAILQ_FIRST(hostgroups))) {
        TAILQ_REMOVE(hostgroups, hostgroup, tq);
        free(hostgroup->pattern);
        free(hostgroup);
    }
/* }}} */
}

void mb_free_hostgroups_routing_table(mb_hstgroup_routes_t *hg_routing_table) {
/* {{{ */
    mb_hstgroup_route_t *hostgroup_route = NULL;

    while ((hostgroup_route = TAILQ_FIRST(hg_routing_table))) {
        mb_free_hostgroups(hostgroup_route->hstgroups);
        TAILQ_REMOVE(hg_routing_table, hostgroup_route, tq);
        free(hostgroup_route);
    }
/* }}} */
}

void mb_free_servicegroups(mb_svcgroups_t *servicegroups) {
/* {{{ */
    mb_svcgroup_t *servicegroup = NULL;

    while ((servicegroup = TAILQ_FIRST(servicegroups))) {
        TAILQ_REMOVE(servicegroups, servicegroup, tq);
        free(servicegroup->pattern);
        free(servicegroup);
    }
/* }}} */
}

void mb_free_servicegroups_routing_table(mb_svcgroup_routes_t *sg_routing_table) {
/* {{{ */
    mb_svcgroup_route_t *servicegroup_route = NULL;

    while ((servicegroup_route = TAILQ_FIRST(sg_routing_table))) {
        mb_free_servicegroups(servicegroup_route->svcgroups);
        TAILQ_REMOVE(sg_routing_table, servicegroup_route, tq);
        free(servicegroup_route);
    }
/* }}} */
}

// vim: ft=c ts=4 et foldmethod=marker
