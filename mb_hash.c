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
#include "mb_hash.h"

/* From http://www.cse.yorku.ca/~oz/hash.html */
static unsigned long djb2_hash(unsigned char *str) {
/* {{{ */
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return (hash);
/* }}} */
}

void mb_gen_cid(char *cid_buf, size_t cid_buf_len, char *host, char *service) {
/* {{{ */
    char            buf[256] = {0};
    struct timeval  now;

    gettimeofday(&now, NULL);

    /* Correlation ID = host + service + msec time */
    snprintf(buf,
        sizeof (buf) - 1,
        "%s%s%ld.%ld",
        host,
        (service ? service : ""),
        now.tv_sec, now.tv_usec);

   snprintf(cid_buf, cid_buf_len, "%lX", djb2_hash((unsigned char *)buf));
/* }}} */
}

// vim: ft=c ts=4 et foldmethod=marker
