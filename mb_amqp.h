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

#ifndef _MB_AMQP_H_
#define _MB_AMQP_H_

#define AMQP_CHANNEL                    1
#define AMQP_DELIVERY_MODE_VOLATILE     1
#define AMQP_DELIVERY_MODE_PERSISTENT   2

typedef struct mb_amqp_connection {
/* {{{ */
    amqp_connection_state_t *conn;
#ifdef LIBRABBITMQ_LEGACY
    int                     *sockfd;
#else
    amqp_socket_t           *socket;
#endif
    char                    *host;
    int                     port;
    char                    *vhost;
    char                    *user;
    char                    *password;
    char                    *exchange;
    char                    *exchange_type;
    bool                    debug;
/* }}} */
} mb_amqp_connection_t;

#endif

// vim: ft=c ts=4 et foldmethod=marker
