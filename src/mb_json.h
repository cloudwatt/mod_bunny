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

#ifndef _MB_JSON_H_
#define _MB_JSON_H_

#define TIMEVAL_TO_FLOAT(t) ((double)t.tv_sec + ((double)t.tv_usec / 1000000.0))
#define FLOAT_TO_TIMEVAL(f, t) { t.tv_sec = f; t.tv_usec = (f - (double)t.tv_sec) * 1000000; }

typedef struct mb_json_config_setting_s {
    const char  *name;
    void        *value;
    bool        (*type_check)(json_t *);
    int         (*parse)(json_t *, void *, int (*)(void *));
    int         (*check)(void *);
} mb_json_config_setting_t;

#endif

// vim: ft=c ts=4 et foldmethod=marker
