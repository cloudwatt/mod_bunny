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

static void mb_thread_publish_shutdown(void *args) {
/* {{{ */
    mb_config_t *mb_config = (mb_config_t *)args;

    if (mb_config->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_thread_publish: received shutdown signal");

    if (mb_config->publisher_connected) {
        if (mb_amqp_disconnect_publisher(mb_config)) {
            if (mb_config->debug)
                logit(NSLOG_INFO_MESSAGE, TRUE,
                    "mod_bunny: mb_thread_publish: successfully closed connection to AMQP broker");
        }
    }

    if (mb_config->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_thread_publish: terminating");
} /* }}} */

static void mb_thread_consume_shutdown(void *args) {
/* {{{ */
    mb_config_t *mb_config = (mb_config_t *)args;

    if (mb_config->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_thread_consume: received shutdown signal");

    if (mb_config->consumer_connected) {
        if (mb_amqp_disconnect_consumer(mb_config)) {
            if (mb_config->debug)
                logit(NSLOG_INFO_MESSAGE, TRUE,
                    "mod_bunny: mb_thread_consume: successfully closed connection to AMQP broker");
        }
    }

    if (mb_config->debug)
        logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_thread_consume: terminating");
} /* }}} */

void *mb_thread_publish(void *args)
{ /* {{{ */
    mb_config_t *mb_config = (mb_config_t *)args;

    /* Set this thread cancelable at any time */
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    /* Cleanup handler */
    pthread_cleanup_push(mb_thread_publish_shutdown, args);

    while (true) {
        /* Loop until we successfully connect to AMQP broker */
        while (!mb_config->publisher_connected) {
            if (!mb_amqp_connect_publisher(mb_config)) {
                if (mb_config->debug)
                    logit(NSLOG_INFO_MESSAGE, TRUE,
                        "mod_bunny: mb_thread_publish: waiting for %d seconds before retry connecting",
                        mb_config->retry_wait_time);

                sleep(mb_config->retry_wait_time);
            }
        }

        /* We are connected, idle for a while */
        sleep(1);
    }

    pthread_cleanup_pop(0);
} /* }}} */

void *mb_thread_consume(void *args)
{ /* {{{ */
    mb_config_t *mb_config = (mb_config_t *)args;

    /* Set this thread cancelable at any time */
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    /* Cleanup handler */
    pthread_cleanup_push(mb_thread_consume_shutdown, args);

    while (true) {
        /* Loop until we successfully connect to AMQP broker */
        while (!mb_config->consumer_connected) {
            if (!mb_amqp_connect_consumer(mb_config)) {
                if (mb_config->debug)
                    logit(NSLOG_INFO_MESSAGE, TRUE,
                        "mod_bunny: mb_thread_consume: waiting for %d seconds before retry connecting",
                        mb_config->retry_wait_time);

                sleep(mb_config->retry_wait_time);
            }
        }

        if (mb_config->debug)
            logit(NSLOG_INFO_MESSAGE, TRUE, "mod_bunny: mb_thread_consume: start consuming");

        /* Process received check results */
        mb_amqp_consume(mb_config, mb_process_check_result);

        /* We get here if an error occurs while consuming, so explicitely close the connection */
        logit(NSLOG_RUNTIME_ERROR, TRUE, "mod_bunny: mb_thread_consume: error: "
            "consuming loop stopped, disconnecting from broker");
        mb_amqp_disconnect_consumer(mb_config);
    }

    pthread_cleanup_pop(0);
} /* }}} */

// vim: ft=c ts=4 et foldmethod=marker
