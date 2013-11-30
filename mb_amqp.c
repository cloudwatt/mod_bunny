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

static char *mb_amqp_bytes_to_cstring(amqp_bytes_t *ab) {
/* {{{ */
    char *buf = NULL;

    if (ab == NULL || ab->bytes == NULL)
        return (NULL);

    if (!(buf = calloc(1, ab->len + 1)))
        return (NULL);

    memcpy(buf, ab->bytes, ab->len);
    buf[ab->len] = 0;

    return (buf);
/* }}} */
}

static int mb_amqp_error(amqp_rpc_reply_t reply, const char *context) {
/* {{{ */
    switch (reply.reply_type) {
    case AMQP_RESPONSE_NORMAL:
        return (MB_OK);

    case AMQP_RESPONSE_NONE:
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: error: %s",
            context,
            "missing RPC reply type");
        return (MB_NOK);

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: error: %s",
            context,
            amqp_error_string(reply.library_error));
        return (MB_NOK);

    case AMQP_RESPONSE_SERVER_EXCEPTION: {
        amqp_connection_close_t *reply_decoded = (amqp_connection_close_t *)reply.reply.decoded;

        switch (reply.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD:
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: server connection error: %d: %.*s",
                context,
                reply_decoded->reply_code,
                (int)reply_decoded->reply_text.len,
                (char *)reply_decoded->reply_text.bytes);
            return (MB_NOK);

        case AMQP_CHANNEL_CLOSE_METHOD:
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: server channel error: %d: %.*s",
                context,
                reply_decoded->reply_code,
                (int)reply_decoded->reply_text.len,
                (char *)reply_decoded->reply_text.bytes);
            return (MB_NOK);

        default:
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: unknown server error, method ID 0x%08X",
                context,
                reply.reply.id);
            return (MB_NOK);
        }
    }
    }

    return (MB_OK);
/* }}} */
}

static int mb_amqp_connect(mb_amqp_connection_t *amqp_conn, const char *context) {
/* {{{ */
    *amqp_conn->conn = amqp_new_connection();

    if (amqp_conn->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: %s: connecting to host %s:%d using vhost %s",
            context,
            amqp_conn->host,
            amqp_conn->port,
            amqp_conn->vhost);

    if ((*amqp_conn->sockfd = amqp_open_socket(amqp_conn->host, amqp_conn->port)) < 0) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: error: %s",
            context,
            "amqp_open_socket() failed");
        amqp_destroy_connection(*amqp_conn->conn);
        return (MB_NOK);
    }

    if (amqp_conn->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: %s: successfully connected to broker", context);

    amqp_set_sockfd(*amqp_conn->conn, *amqp_conn->sockfd);

    if (mb_amqp_error(amqp_login(*amqp_conn->conn,  /* connection */
        amqp_conn->vhost,                           /* vhost */
        0,                                          /* channel_max */
        131072,                                     /* frame_max */
        0,                                          /* heartbeat */
        AMQP_SASL_METHOD_PLAIN,                     /* sasl_method */
        amqp_conn->user,                            /* login */
        amqp_conn->password                         /* password */
    ), context) == MB_NOK) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: error: %s",
            context,
            "amqp_login() failed");
        goto error;
    }

    if (amqp_conn->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: %s: logged in", context);

    amqp_channel_open(*amqp_conn->conn, AMQP_CHANNEL);
    if (mb_amqp_error(amqp_get_rpc_reply(*amqp_conn->conn), context) == MB_NOK) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: error: %s",
            context,
            "amqp_channel_open() failed");
        goto error;
    }

    if (amqp_conn->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: %s: opened channel", context);

    amqp_exchange_declare(*amqp_conn->conn,             /* connection*/
        AMQP_CHANNEL,                                   /* channel */
        amqp_cstring_bytes(amqp_conn->exchange),        /* exchange */
        amqp_cstring_bytes(amqp_conn->exchange_type),   /* type */
        false,                                          /* passive */
        true,                                           /* durable */
        AMQP_EMPTY_TABLE                                /* arguments */
    );
    if (mb_amqp_error(amqp_get_rpc_reply(*amqp_conn->conn), context) == MB_NOK) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: error: %s",
            context,
            "amqp_exchange_declare() failed");

        amqp_channel_close(*amqp_conn->conn, AMQP_CHANNEL, AMQP_REPLY_SUCCESS);
        goto error;
    }

    if (amqp_conn->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: %s: declared exchange \"%s\"",
            context,
            amqp_conn->exchange);

    return (MB_OK);

    error:
    close(*amqp_conn->sockfd);
    amqp_destroy_connection(*amqp_conn->conn);

    return (MB_NOK);
/* }}} */
}

static int mb_amqp_disconnect(mb_amqp_connection_t *amqp_conn, const char *context) {
/* {{{ */
    if (mb_amqp_error(amqp_channel_close(*amqp_conn->conn, AMQP_CHANNEL, AMQP_REPLY_SUCCESS),
        context) == MB_NOK) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: error: %s",
            context,
            "amqp_channel_close() failed");
        goto error;
    }

    if (amqp_conn->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: %s: closed channel", context);

    if (mb_amqp_error(amqp_connection_close(*amqp_conn->conn, AMQP_REPLY_SUCCESS), context) == MB_NOK) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: %s: error: %s",
            context,
            "amqp_connection_close() failed");
        goto error;
    }

    if (amqp_conn->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: %s: closed connection", context);

    amqp_destroy_connection(*amqp_conn->conn);

    return (MB_OK);

    error:
    close(*amqp_conn->sockfd);
    amqp_destroy_connection(*amqp_conn->conn);

    return (MB_NOK);
/* }}} */
}

static size_t mb_amqp_read_msg_header(amqp_connection_state_t *conn) {
/* {{{ */
    amqp_frame_t            amqp_frame;
    amqp_basic_properties_t *msg_props = NULL;
    char                    *msg_content_type = NULL;
    size_t                  msg_body_size;
    int                     rc;

    if ((rc = amqp_simple_wait_frame(*conn, &amqp_frame)) < 0) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_read_msg_header: error: "
            "amqp_simple_wait_frame() failed");
        return (0);
    }

    if (amqp_frame.frame_type != AMQP_FRAME_HEADER) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_read_msg_header: error: "
            "invalid frame type, expected header");
        return (0);
    }

    msg_props = amqp_frame.payload.properties.decoded;
    msg_content_type = mb_amqp_bytes_to_cstring(&msg_props->content_type);
    msg_body_size = (size_t)amqp_frame.payload.properties.body_size;

    // TODO: check for "application/json" content type
    if (!msg_content_type) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_read_msg_header: error: "
            "unable to determine content type");
        return (0);
    }

    free(msg_content_type);

    return (msg_body_size);
/* }}} */
}

static char *mb_amqp_read_msg_body(amqp_connection_state_t *conn, size_t msg_body_full_size) {
/* {{{ */
    amqp_frame_t    amqp_frame;
    char            *msg_body = NULL;
    size_t          msg_body_received_size = 0;
    size_t          msg_fragment_size;
    int             rc;

    if (!(msg_body = calloc(1, msg_body_full_size + 1))) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_read_msg_body: error: "
            "unable to allocate memory");
        return (NULL);
    }

    while (msg_body_received_size < msg_body_full_size) {
        if ((rc = amqp_simple_wait_frame(*conn, &amqp_frame)) < 0) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_read_msg_body: error: "
                "amqp_simple_wait_frame() failed");
            goto error;
        }

        if (amqp_frame.frame_type != AMQP_FRAME_BODY) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_read_msg_body: error: "
                "invalid frame type, expected body");
            goto error;
        }

        msg_fragment_size = amqp_frame.payload.body_fragment.len;

        if ((msg_body_full_size - msg_body_received_size) < msg_fragment_size) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_read_msg_body: error: "
                "received message body is larger than indicated by the message header");
            goto error;
        }

        memcpy(msg_body, amqp_frame.payload.body_fragment.bytes, msg_fragment_size);

        msg_body_received_size += msg_fragment_size;
    }

    return (msg_body);

    error:
    free(msg_body);
    return (NULL);
/* }}} */
}

int mb_amqp_connect_consumer(mb_config_t *config) {
/* {{{ */
    char                    *declared_queue = NULL;
    amqp_queue_declare_ok_t *qd_rc = NULL;

    mb_amqp_connection_t conn = {
        .conn           = &config->consumer_amqp_conn,
        .sockfd         = &config->consumer_amqp_sockfd,
        .host           = config->host,
        .port           = config->port,
        .vhost          = config->vhost,
        .user           = config->user,
        .password       = config->password,
        .exchange       = config->consumer_exchange,
        .exchange_type  = config->consumer_exchange_type,
        .debug          = config->debug
    };

    if (!mb_amqp_connect(&conn, "mb_amqp_connect_consumer"))
        return (MB_NOK);

    qd_rc = amqp_queue_declare(config->consumer_amqp_conn,  /* connection */
        1,                                                  /* channel */
        amqp_cstring_bytes(config->consumer_queue),         /* queue */
        false,                                              /* passive */
        true,                                               /* durable */
        false,                                              /* exclusive */
        false,                                              /* auto_delete */
        AMQP_EMPTY_TABLE                                    /* arguments */
    );
    if (mb_amqp_error(amqp_get_rpc_reply(config->consumer_amqp_conn), "mb_amqp_connect_consumer") == MB_NOK) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_connect_consumer: error: "
            "amqp_queue_declare() failed");

        amqp_channel_close(config->consumer_amqp_conn, AMQP_CHANNEL, AMQP_REPLY_SUCCESS);
        goto error;
    }

    declared_queue = mb_amqp_bytes_to_cstring(&qd_rc->queue);

    if (config->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE,
            "mod_bunny: mb_amqp_connect_consumer: declared queue \"%s\"", declared_queue);

    amqp_queue_bind(config->consumer_amqp_conn,             /* connection */
        1,                                                  /* channel */
        amqp_cstring_bytes(declared_queue),                 /* queue */
        amqp_cstring_bytes(config->consumer_exchange),      /* exchange */
        amqp_cstring_bytes(config->consumer_binding_key),   /* binding_key */
        AMQP_EMPTY_TABLE                                    /* arguments */
    );
    if (mb_amqp_error(amqp_get_rpc_reply(config->consumer_amqp_conn), "mb_amqp_connect_consumer") == MB_NOK) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_connect_consumer: error: "
            "amqp_queue_bind() failed");

        amqp_channel_close(config->consumer_amqp_conn, AMQP_CHANNEL, AMQP_REPLY_SUCCESS);
        goto error;
    }

    if (config->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE,
            "mod_bunny: mb_amqp_connect_consumer: bound queue \"%s\" to exchange \"%s\"",
            declared_queue,
            config->consumer_exchange);

    amqp_basic_consume(config->consumer_amqp_conn,  /* connection */
        1,                                          /* channel */
        qd_rc->queue,                               /* queue */
        amqp_cstring_bytes("nagios/mod_bunny"),     /* consumer_tag */
        false,                                      /* no_local */
        true,                                       /* no_ack */
        false,                                      /* exclusive */
        AMQP_EMPTY_TABLE                            /* arguments */
    );
    if (mb_amqp_error(amqp_get_rpc_reply(config->consumer_amqp_conn), "mb_amqp_connect_consumer") == MB_NOK) {
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_connect_consumer: error: "
            "amqp_basic_consume() failed");

        amqp_channel_close(config->consumer_amqp_conn, AMQP_CHANNEL, AMQP_REPLY_SUCCESS);
        goto error;
    }

    free(declared_queue);

    config->consumer_connected = true;

    return (MB_OK);

    error:
    free(declared_queue);
    close(config->consumer_amqp_sockfd);
    amqp_destroy_connection(config->consumer_amqp_conn);

    return (MB_NOK);
/* }}} */
}

int mb_amqp_connect_publisher(mb_config_t *config) {
/* {{{ */
    mb_amqp_connection_t conn = {
        .conn           = &config->publisher_amqp_conn,
        .sockfd         = &config->publisher_amqp_sockfd,
        .host           = config->host,
        .port           = config->port,
        .vhost          = config->vhost,
        .user           = config->user,
        .password       = config->password,
        .exchange       = config->publisher_exchange,
        .exchange_type  = config->publisher_exchange_type,
        .debug          = config->debug
    };

    if (mb_amqp_connect(&conn, "mb_amqp_connect_publisher")) {
        config->publisher_connected = true;
        return (MB_OK);
    } else
        return (MB_NOK);
/* }}} */
}

int mb_amqp_disconnect_consumer(mb_config_t *config) {
/* {{{ */
    mb_amqp_connection_t conn = {
        .conn           = &config->consumer_amqp_conn,
        .sockfd         = &config->consumer_amqp_sockfd,
        .host           = config->host,
        .port           = config->port,
        .vhost          = config->vhost,
        .user           = config->user,
        .password       = config->password,
        .exchange       = config->consumer_exchange,
        .exchange_type  = config->consumer_exchange_type,
        .debug          = config->debug
    };

    /* Check if the consumer isn't already disconnected */
    if (!config->consumer_connected)
        return (MB_OK);

    config->consumer_connected = false;

    return mb_amqp_disconnect(&conn, "mb_amqp_disconnect_consumer");
/* }}} */
}

int mb_amqp_disconnect_publisher(mb_config_t *config) {
/* {{{ */
    mb_amqp_connection_t conn = {
        .conn           = &config->publisher_amqp_conn,
        .sockfd         = &config->publisher_amqp_sockfd,
        .host           = config->host,
        .port           = config->port,
        .vhost          = config->vhost,
        .user           = config->user,
        .password       = config->password,
        .exchange       = config->publisher_exchange,
        .exchange_type  = config->publisher_exchange_type,
        .debug          = config->debug
    };

    /* Check if the publisher isn't already disconnected */
    if (!config->publisher_connected)
        return (MB_OK);

    config->publisher_connected = false;

    return mb_amqp_disconnect(&conn, "mb_amqp_disconnect_publisher");
/* }}} */
}

int mb_amqp_publish(mb_config_t *config, char *message) {
/* {{{ */
    amqp_bytes_t            message_bytes;
    amqp_basic_properties_t message_props;
    int                     rc;

    message_bytes.bytes = message;
    message_bytes.len = strlen(message);

    message_props.app_id = amqp_cstring_bytes("Nagios/mod_bunny");
    message_props.content_type = amqp_cstring_bytes("application/json");
    message_props.delivery_mode = AMQP_DELIVERY_MODE_VOLATILE;
    message_props._flags =
        AMQP_BASIC_APP_ID_FLAG
        | AMQP_BASIC_CONTENT_TYPE_FLAG
        | AMQP_BASIC_DELIVERY_MODE_FLAG;

    rc = amqp_basic_publish(config->publisher_amqp_conn,    /* connection */
        AMQP_CHANNEL,                                       /* channel */
        amqp_cstring_bytes(config->publisher_exchange),     /* exchange */
        amqp_cstring_bytes(config->publisher_routing_key),  /* routing_key */
        false,                                              /* mandatory */
        false,                                              /* immediate */
        &message_props,                                     /* properties */
        message_bytes                                       /* body */
    );

    if (rc != 0)
        return (MB_NOK);

    return (MB_OK);
/* }}} */
}

void mb_amqp_consume(mb_config_t *config, void(* handler)(char *)) {
/* {{{ */
    amqp_frame_t    amqp_frame;
    size_t          msg_body_size;
    char            *msg = NULL;
    int             rc;

    amqp_connection_state_t *conn = (amqp_connection_state_t *)&config->consumer_amqp_conn;

    while (config->consumer_connected) {
        amqp_maybe_release_buffers(*conn);

        if ((rc = amqp_simple_wait_frame(*conn, &amqp_frame))) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_consume: error: "
                "amqp_simple_wait_frame() failed, skipping frame");

            /* As a safety measure in case of error here, we stop consuming and return */
            break;
        }

        if (amqp_frame.frame_type != AMQP_FRAME_METHOD) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_consume: error: "
                "unexpected frame type, skipping frame");
            continue;
        }

        if (amqp_frame.payload.method.id != AMQP_BASIC_DELIVER_METHOD) {
            logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_amqp_consume: error: "
                "unexpected method ID, skipping frame");
            continue;
        }

        if ((msg_body_size = mb_amqp_read_msg_header(conn)) == 0) {
            logit(NSLOG_RUNTIME_ERROR, TRUE,
                "mod_bunny: mb_amqp_consume: error while reading message header, skipping");
            continue;
        }

        if (!(msg = mb_amqp_read_msg_body(conn, msg_body_size))) {
            logit(NSLOG_RUNTIME_ERROR, TRUE,
                "mod_bunny: mb_amqp_consume: error while reading message body, skipping");
            continue;
        }

        /* Pass the received message to the handler */
        handler(msg);

        free(msg);
    }

    if (config->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_amqp_consume: stopped consuming");
/* }}} */
}

// vim: ft=c ts=4 et foldmethod=marker
