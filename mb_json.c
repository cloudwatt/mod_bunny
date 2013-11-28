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

#include <jansson.h>

#include "mod_bunny.h"

#define TIMEVAL_TO_FLOAT(t) ((double)t.tv_sec + ((double)t.tv_usec / 1000000.0))
#define FLOAT_TO_TIMEVAL(f, t) { t.tv_sec = f; t.tv_usec = (f - (double)t.tv_sec) * 1000000; }

static mb_hstgroups_t *mb_json_parse_local_hostgroups(json_t *json_local_hostgroups) {
/* {{{ */
    mb_hstgroups_t  *mb_local_hostgroups = NULL;
    mb_hstgroup_t   *hostgroup = NULL;
    const char      *hostgroup_pattern = NULL;
    int             hostgroups_added;

    if (!(mb_local_hostgroups = calloc(1, sizeof(mb_hstgroup_t)))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_hostgroups: error: "
            "unable to allocate memory");
        return (NULL);

    }

    TAILQ_INIT(mb_local_hostgroups);

    hostgroups_added = 0;
     for (int i = 0; i < json_array_size(json_local_hostgroups); i++) {
        if (!(hostgroup_pattern = json_string_value(json_array_get(json_local_hostgroups, i)))) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_hostgroups: error: "
                "unable to get hostgroup value, skipping");
            continue;
        }

        if (!(hostgroup = calloc(1, sizeof(mb_hstgroup_t)))) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_hostgroups: error: "
                "unable to allocate memory");
            goto error;
        }

        if (!(hostgroup->pattern = strdup(hostgroup_pattern))) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_hostgroups: error: "
                "unable to allocate memory");
            free(hostgroup);
            goto error;
        }

        TAILQ_INSERT_TAIL(mb_local_hostgroups, hostgroup, tq);
        hostgroups_added++;
    }

    // If no hostgroup pattern were successfully added to the list, destroy it
    if (hostgroups_added == 0)
        goto error;

    return (mb_local_hostgroups);

    error:
    mb_free_local_hostgroups_list(mb_local_hostgroups);
    free(mb_local_hostgroups);
    return (NULL);
/* }}} */
}

static mb_svcgroups_t *mb_json_parse_local_servicegroups(json_t *json_local_servicegroups) {
/* {{{ */
    mb_svcgroups_t  *mb_local_servicegroups = NULL;
    mb_svcgroup_t   *servicegroup = NULL;
    const char      *servicegroup_pattern = NULL;
    int             servicegroups_added;

    if (!(mb_local_servicegroups = calloc(1, sizeof(mb_svcgroup_t)))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_servicegroups: error: "
            "unable to allocate memory");
        return (NULL);

    }

    TAILQ_INIT(mb_local_servicegroups);

    servicegroups_added = 0;
     for (int i = 0; i < json_array_size(json_local_servicegroups); i++) {
        if (!(servicegroup_pattern = json_string_value(json_array_get(json_local_servicegroups, i)))) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_servicegroups: error: "
                "unable to get servicegroup value, skipping");
            continue;
        }

        if (!(servicegroup = calloc(1, sizeof(mb_svcgroup_t)))) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_servicegroups: error: "
                "unable to allocate memory");
            goto error;
        }

        if (!(servicegroup->pattern = strdup(servicegroup_pattern))) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_servicegroups: error: "
                "unable to allocate memory");
            free(servicegroup);
            goto error;
        }

        TAILQ_INSERT_TAIL(mb_local_servicegroups, servicegroup, tq);
        servicegroups_added++;
    }

    // If no servicegroup pattern were successfully added to the list, destroy it
    if (servicegroups_added == 0)
        goto error;

    return (mb_local_servicegroups);

    error:
    mb_free_local_servicegroups_list(mb_local_servicegroups);
    free(mb_local_servicegroups);
    return (NULL);
/* }}} */
}

int mb_json_parse_config(char *file, mb_config_t *mb_config) {
/* {{{ */
    json_error_t    json_error;
    json_t          *json_config = NULL;
    json_t          *json_host = NULL;
    json_t          *json_port = NULL;
    json_t          *json_vhost = NULL;
    json_t          *json_user = NULL;
    json_t          *json_password = NULL;
    json_t          *json_publisher_exchange = NULL;
    json_t          *json_publisher_exchange_type = NULL;
    json_t          *json_publisher_routing_key = NULL;
    json_t          *json_consumer_exchange = NULL;
    json_t          *json_consumer_exchange_type = NULL;
    json_t          *json_consumer_queue = NULL;
    json_t          *json_consumer_binding_key = NULL;
    json_t          *json_local_hostgroups = NULL;
    json_t          *json_local_servicegroups = NULL;
    json_t          *json_retry_wait_time = NULL;
    json_t          *json_debug = NULL;

    #define PARSE_JSON_STRING(element, json_obj, dst_str) {                             \
        const char *tmp_str = NULL;                                                     \
        if ((tmp_str = json_string_value(json_obj)) == NULL) {                          \
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: " \
                "`%s' setting value must be a JSON string", element);                   \
            return (MB_NOK);                                                            \
        } else {                                                                        \
            memset(dst_str, 0, MB_BUF_LEN);                                             \
            strncpy(dst_str, tmp_str, MB_BUF_LEN - 1);                                  \
        }                                                                               \
    }

    if (!(json_config = json_load_file(file, 0, &json_error))) {
        if (json_error.line == -1)
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: "
                "unable to read configuration file %s", file);
        else
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: "
                "syntax error in file %s at line %d: %s", file, json_error.line, json_error.text);

        return (MB_NOK);
    }

    if ((json_host = json_object_get(json_config, "host")))
        PARSE_JSON_STRING("host", json_host, mb_config->host);

    if ((json_port = json_object_get(json_config, "port"))) {
        mb_config->port = json_integer_value(json_port);

        if (mb_config->port <= 0 || mb_config->port > 65535) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: "
                "invalid `port' setting value %d", mb_config->port);
            return (MB_NOK);
        }
    }

    if ((json_vhost = json_object_get(json_config, "vhost")))
        PARSE_JSON_STRING("vhost", json_vhost, mb_config->vhost);

    if ((json_user = json_object_get(json_config, "user")))
        PARSE_JSON_STRING("user", json_user, mb_config->user);

    if ((json_password = json_object_get(json_config, "password")))
        PARSE_JSON_STRING("password", json_password, mb_config->password);

    if ((json_publisher_exchange = json_object_get(json_config, "publisher_exchange")))
        PARSE_JSON_STRING("publisher_exchange",
            json_publisher_exchange,
            mb_config->publisher_exchange);

    if ((json_publisher_exchange_type = json_object_get(json_config, "publisher_exchange_type")))
        PARSE_JSON_STRING("publisher_exchange_type",
            json_publisher_exchange_type,
            mb_config->publisher_exchange_type);

    if ((json_publisher_routing_key = json_object_get(json_config, "publisher_routing_key")))
        PARSE_JSON_STRING("publisher_routing_key",
            json_publisher_routing_key,
            mb_config->publisher_routing_key);

    if ((json_consumer_exchange = json_object_get(json_config, "consumer_exchange")))
        PARSE_JSON_STRING("consumer_exchange",
            json_consumer_exchange,
            mb_config->consumer_exchange);

    if ((json_consumer_exchange_type = json_object_get(json_config, "consumer_exchange_type")))
        PARSE_JSON_STRING("consumer_exchange_type",
            json_consumer_exchange_type,
            mb_config->consumer_exchange_type);

    if ((json_consumer_queue = json_object_get(json_config, "consumer_queue")))
        PARSE_JSON_STRING("consumer_queue",
            json_consumer_queue,
            mb_config->consumer_queue);

    if ((json_consumer_binding_key = json_object_get(json_config, "consumer_binding_key")))
        PARSE_JSON_STRING("consumer_binding_key",
            json_consumer_binding_key,
            mb_config->consumer_binding_key);

    if ((json_local_hostgroups = json_object_get(json_config, "local_hostgroups"))) {
        if (json_array_size(json_local_hostgroups) > 0)
            mb_config->local_hstgroups = mb_json_parse_local_hostgroups(json_local_hostgroups);
    }

    if ((json_local_servicegroups = json_object_get(json_config, "local_servicegroups"))) {
        if (json_array_size(json_local_servicegroups) > 0)
            mb_config->local_svcgroups = mb_json_parse_local_servicegroups(json_local_servicegroups);
    }

    if ((json_retry_wait_time = json_object_get(json_config, "retry_wait_time"))) {
        mb_config->retry_wait_time = json_integer_value(json_retry_wait_time);

        if (mb_config->retry_wait_time <= 0 || mb_config->retry_wait_time > MB_MAX_RETRY_WAIT_TIME) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: "
                "invalid `retry_wait_time' setting value %d", mb_config->retry_wait_time);
            return (MB_NOK);
        }
    }

    if ((json_debug = json_object_get(json_config, "debug")))
        mb_config->debug = (json_is_true(json_debug) ? true : false);

    json_decref(json_config);

    return (MB_OK);
/* }}} */
}

char *mb_json_pack_host_check(nebstruct_host_check_data *hst_check, int check_options, char *command_line) {
/* {{{ */
    json_t  *json_hst_check = NULL;
    char    *json_buf = NULL;

    if (!hst_check)
        return (NULL);

    json_hst_check = json_pack(
        "{s:s s:s s:s s:i s:f s:f s:i}",
        "type", "host",
        "host_name", (hst_check->host_name ? hst_check->host_name : ""),
        "command_line", command_line,
        "check_options", check_options,
        "start_time", TIMEVAL_TO_FLOAT(hst_check->start_time),
        "latency", hst_check->latency,
        "timeout", hst_check->timeout
    );

    if (!json_hst_check) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_pack_host_check: error: "
            "json_pack() failed");
        return (NULL);
    }

    json_buf = json_dumps(json_hst_check, JSON_COMPACT);

    if (!json_buf)
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_pack_host_check: error: "
            "json_dumps() failed");

    json_decref(json_hst_check);

    return (json_buf);
/* }}} */
}

char *mb_json_pack_service_check(nebstruct_service_check_data *svc_check, int check_options, char *command_line) {
/* {{{ */
    json_t  *json_svc_check = NULL;
    char    *json_buf = NULL;

    if (!svc_check)
        return (NULL);

    json_svc_check = json_pack(
        "{s:s s:s s:s s:s s:i s:f s:f s:i}",
        "type", "service",
        "host_name", (svc_check->host_name ? svc_check->host_name : ""),
        "service_description", (svc_check->service_description ? svc_check->service_description : ""),
        "command_line", command_line,
        "check_options", check_options,
        "start_time", TIMEVAL_TO_FLOAT(svc_check->start_time),
        "latency", svc_check->latency,
        "timeout", svc_check->timeout
    );

    if (!json_svc_check) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_pack_service_check: error: "
            "json_pack() failed");
        return (NULL);
    }

    json_buf = json_dumps(json_svc_check, JSON_COMPACT);

    if (!json_buf)
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_pack_service_check: error: "
            "json_dumps() failed");

    json_decref(json_svc_check);

    return (json_buf);
/* }}} */
}

check_result *mb_json_unpack_check_result(char *msg) {
/* {{{ */
    check_result    *cr = NULL;
    json_t          *json_cr = NULL;
    json_t          *json_host_name = NULL;
    json_t          *json_service_description = NULL;
    json_t          *json_check_options = NULL;
    json_t          *json_scheduled_check = NULL;
    json_t          *json_reschedule_check = NULL;
    json_t          *json_latency = NULL;
    json_t          *json_start_time = NULL;
    json_t          *json_finish_time = NULL;
    json_t          *json_early_timeout = NULL;
    json_t          *json_exited_ok = NULL;
    json_t          *json_return_code = NULL;
    json_t          *json_output = NULL;
    const char      *host_name = NULL;
    const char      *service_description = NULL;
    const char      *output = NULL;

    if (!(json_cr = json_loads(msg, 0, NULL))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_unpack_check_result: error: "
        "unable to parse JSON data");
        return (NULL);
    }

    if (!(cr = (check_result *)calloc(1, sizeof(check_result)))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: process_check_result: error: "
        "unable to allocate memory");
        return (NULL);
    }

    init_check_result(cr);
    cr->output_file = NULL;

    if (!(json_host_name = json_object_get(json_cr, "host_name"))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_unpack_check_result: error: "
        "missing `host_name` entry in received JSON data");
        goto error;
    } else {
        if ((host_name = json_string_value(json_host_name))) {
            cr->host_name = strdup(host_name);
        }
    }

    if (!(json_return_code = json_object_get(json_cr, "return_code"))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_unpack_check_result: error: "
        "missing `return_code` entry in received JSON data");
        goto error;
    } else {
        cr->return_code = json_integer_value(json_return_code);
    }

    if (!(json_start_time = json_object_get(json_cr, "start_time"))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_unpack_check_result: error: "
        "missing `start_time` entry in received JSON data");
        goto error;
    } else {
        FLOAT_TO_TIMEVAL(json_real_value(json_start_time), cr->start_time);
    }

    if (!(json_finish_time = json_object_get(json_cr, "finish_time"))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_unpack_check_result: error: "
        "missing `finish_time` entry in received JSON data");
        goto error;
    } else {
        FLOAT_TO_TIMEVAL(json_real_value(json_finish_time), cr->finish_time);
    }

    /* Set this check result as service check only if `service_description`
    element exists and is not empty */
    if ((json_service_description = json_object_get(json_cr, "service_description"))) {
        if ((service_description = json_string_value(json_service_description))
            && strlen(service_description) > 0) {
            cr->service_description = strdup(service_description);
            cr->object_check_type = SERVICE_CHECK;
        }
    }

    if ((json_output = json_object_get(json_cr, "output"))) {
        if ((output = json_string_value(json_output))) {
            cr->output = strdup(output);
        }
    }

    if ((json_check_options = json_object_get(json_cr, "check_options"))) {
        cr->check_options = json_integer_value(json_check_options);
    }

    if ((json_scheduled_check = json_object_get(json_cr, "scheduled_check"))) {
        cr->scheduled_check = json_integer_value(json_scheduled_check);
    }

    if ((json_reschedule_check = json_object_get(json_cr, "reschedule_check"))) {
        cr->reschedule_check = json_integer_value(json_reschedule_check);
    }

    if ((json_exited_ok = json_object_get(json_cr, "exited_ok"))) {
        cr->exited_ok = json_integer_value(json_exited_ok);
    }

    if ((json_early_timeout = json_object_get(json_cr, "early_timeout"))) {
        cr->early_timeout = json_integer_value(json_early_timeout);
    }

    if ((json_latency = json_object_get(json_cr, "latency"))) {
        cr->latency = json_real_value(json_latency);
    }

    json_decref(json_cr);

    return (cr);

    error:
    free(cr);
    return (NULL);
/* }}} */
}

// vim: ft=c ts=4 et foldmethod=marker
