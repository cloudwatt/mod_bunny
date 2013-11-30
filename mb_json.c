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
#include "mb_json.h"

static inline int mb_json_config_check_broker_port(void *data) {
/* {{{ */
   int port = *(int *)data;

    if (port <= 0 || port > 65535) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: configuration error: "
            "invalid `port' setting value %d", port);
        return (MB_NOK);
    }

    return (MB_OK);
/* }}} */
}

static inline int mb_json_config_check_retry_wait_time(void *data) {
/* {{{ */
   int retry_wait_time = *(int *)data;

    if (retry_wait_time <= 0 || retry_wait_time > MB_MAX_RETRY_WAIT_TIME) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: "
            "invalid `retry_wait_time' setting value %d", retry_wait_time);
        return (MB_NOK);
    }

    return (MB_OK);
/* }}} */
}

static inline bool mb_json_is_string(json_t *obj) {
/* {{{ */
    return json_is_string(obj);
/* }}} */
}

static inline bool mb_json_is_integer(json_t *obj) {
/* {{{ */
    return json_is_integer(obj);
/* }}} */
}

static inline bool mb_json_is_boolean(json_t *obj) {
/* {{{ */
    return json_is_boolean(obj);
/* }}} */
}

static inline bool mb_json_is_array(json_t *obj) {
/* {{{ */
    return json_is_array(obj);
/* }}} */
}

static inline int mb_json_parse_string(json_t *json_obj, void *dst, int (*check)(void *)) {
/* {{{ */
    const char *tmp_str = NULL;

    tmp_str = json_string_value(json_obj);
    memset(dst, 0, MB_BUF_LEN);
    strncpy((char *)dst, tmp_str, MB_BUF_LEN - 1);

    /* Perform check on parsed value if specified */
    if (check) {
        if (!check(dst))
            return (MB_NOK);
    }

    return (MB_OK);
/* }}} */
}

static inline int mb_json_parse_int(json_t *json_obj, void *dst, int (*check)(void *)) {
/* {{{ */
    *(int *)dst = json_integer_value(json_obj);

    /* Perform check on parsed value if specified */
    if (check) {
        if (!check(dst))
            return (MB_NOK);
    }

    return (MB_OK);
/* }}} */
}

static inline int mb_json_parse_bool(json_t *json_obj, void *dst,
    int (*check)(void *) __attribute__((__unused__))) {
/* {{{ */
    *(bool *)dst = (json_is_true(json_obj) ? true : false);

    return (MB_OK);
/* }}} */
}

static inline int mb_json_parse_local_hostgroups(json_t *json_local_hostgroups, void *dst,
    int (*check)(void *) __attribute__((__unused__))) {
/* {{{ */
    mb_hstgroups_t  **mb_local_hostgroups = NULL;
    mb_hstgroup_t   *hostgroup = NULL;
    const char      *hostgroup_pattern = NULL;
    int             hostgroups_added;

    if (json_array_size(json_local_hostgroups) == 0)
        return (MB_OK);

    mb_local_hostgroups = (mb_hstgroups_t **)dst;

    if (!(*mb_local_hostgroups = calloc(1, sizeof(mb_hstgroup_t)))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_hostgroups: error: "
            "unable to allocate memory");
        return (MB_NOK);
    }

    TAILQ_INIT(*mb_local_hostgroups);

    hostgroups_added = 0;
     for (int i = 0; i < (int)json_array_size(json_local_hostgroups); i++) {
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

        TAILQ_INSERT_TAIL(*mb_local_hostgroups, hostgroup, tq);
        hostgroups_added++;
    }

    // If no hostgroup pattern were successfully added to the list, destroy it
    if (hostgroups_added == 0)
        goto error;

    return (MB_OK);

    error:
    mb_free_local_hostgroups_list(*mb_local_hostgroups);
    free(*mb_local_hostgroups);
    return (MB_NOK);
/* }}} */
}

static inline int mb_json_parse_local_servicegroups(json_t *json_local_servicegroups, void *dst,
    int (*check)(void *) __attribute__((__unused__))) {
/* {{{ */
    mb_svcgroups_t  **mb_local_servicegroups = NULL;
    mb_svcgroup_t   *servicegroup = NULL;
    const char      *servicegroup_pattern = NULL;
    int             servicegroups_added;

    if (json_array_size(json_local_servicegroups) == 0)
        return (MB_NOK);

    mb_local_servicegroups = (mb_svcgroups_t **)dst;

    if (!(*mb_local_servicegroups = calloc(1, sizeof(mb_svcgroup_t)))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_local_servicegroups: error: "
            "unable to allocate memory");
        return (MB_NOK);
    }

    TAILQ_INIT(*mb_local_servicegroups);

    servicegroups_added = 0;
     for (int i = 0; i < (int)json_array_size(json_local_servicegroups); i++) {
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

        TAILQ_INSERT_TAIL(*mb_local_servicegroups, servicegroup, tq);
        servicegroups_added++;
    }

    // If no servicegroup pattern were successfully added to the list, destroy it
    if (servicegroups_added == 0)
        goto error;

    return (MB_OK);

    error:
    mb_free_local_servicegroups_list(*mb_local_servicegroups);
    free(*mb_local_servicegroups);
    return (MB_NOK);
/* }}} */
}

int mb_json_parse_config(char *file, mb_config_t *mb_config) {
/* {{{ */
    json_error_t        json_error;
    json_t              *json_config = NULL;
    json_t              *setting_value = NULL;

    mb_json_config_setting_t config_settings[] = {
        { "host", mb_config->host, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "port", &mb_config->port, mb_json_is_integer, mb_json_parse_int,
            mb_json_config_check_broker_port },
        { "vhost", mb_config->vhost, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "user", mb_config->user, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "password", mb_config->password, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "publisher_exchange", mb_config->publisher_exchange, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "publisher_exchange_type", mb_config->publisher_exchange_type, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "publisher_routing_key", mb_config->publisher_routing_key, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "consumer_exchange", mb_config->consumer_exchange, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "consumer_exchange_type", mb_config->consumer_exchange_type, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "consumer_queue", mb_config->consumer_queue, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "consumer_binding_key", mb_config->consumer_binding_key, mb_json_is_string,
            mb_json_parse_string, NULL },
        { "local_hostgroups", &mb_config->local_hstgroups, mb_json_is_array,
            mb_json_parse_local_hostgroups, NULL },
        { "local_servicegroups", &mb_config->local_svcgroups, mb_json_is_array,
            mb_json_parse_local_servicegroups, NULL },
        { "retry_wait_time", &mb_config->retry_wait_time, mb_json_is_integer,
            mb_json_parse_int, mb_json_config_check_retry_wait_time },
        { "debug", &mb_config->debug, mb_json_is_boolean,
            mb_json_parse_bool, NULL },
        { NULL, NULL, NULL, NULL, NULL },
    };

    if (!(json_config = json_load_file(file, 0, &json_error))) {
        if (json_error.line == -1)
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: "
                "unable to read configuration file %s", file);
        else
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: "
                "syntax error in file %s at line %d: %s", file, json_error.line, json_error.text);

        return (MB_NOK);
    }

    for (mb_json_config_setting_t *setting = config_settings; setting->name ; setting++) {
        if ((setting_value = json_object_get(json_config, setting->name))) {
            if (!setting->type_check(setting_value)) {
                logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_json_parse_config: error: "
                    "incorrect value type for setting `%s'", setting->name);
                return (MB_NOK);
            }

            if (!setting->parse(setting_value, setting->value, setting->check))
                return (MB_NOK);
        }
    }

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
