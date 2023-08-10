/**
 *  Copyright (C) 2023 Masatoshi Fukunaga
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#ifndef lua_epoll_h
#define lua_epoll_h

#include "config.h"
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
// lualib
#include <lauxlib.h>

static inline int getref(lua_State *L)
{
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

static inline int getrefat(lua_State *L, int idx)
{
    lua_pushvalue(L, idx);
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

static inline int unref(lua_State *L, int ref)
{
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
    return LUA_NOREF;
}

static inline void pushref(lua_State *L, int ref)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
}

#if HAVE_EPOLL_CREATE1
# define poll_open() epoll_create1(EPOLL_CLOEXEC)
#else
# define poll_open() epoll_create(1)
#endif

#define EVFILT_READ   0x1
#define EVFILT_WRITE  0x2
#define EVFILT_SIGNAL 0x3
#define EVFILT_TIMER  0x4

#define EV_CLEAR   EPOLLET
#define EV_ONESHOT EPOLLONESHOT
#define EV_EOF     (EPOLLHUP | EPOLLRDHUP)
#define EV_ERROR   EPOLLERR

#ifndef EPOLLEXCLUSIVE
# define EPOLLEXCLUSIVE 0x0
#endif

typedef struct epoll_event event_t;

typedef struct {
    int fd;
    int ref_evset;
    int ref_evset_read;
    int ref_evset_write;
    int ref_evset_signal;
    int ref_evset_timer;
    int ref_evlist;
    int nreg;
    int nevt;
    int cur;
    int evsize;
    event_t *evlist;
} poll_t;

typedef struct {
    poll_t *p;
    int ref_poll;
    int ref_udata;
    int enabled;
    int ident;
    int filter;
    event_t reg_evt; // registered event
    event_t occ_evt; // occurred event
} poll_event_t;

#define POLL_MT        "epoll"
#define POLL_EVENT_MT  "epoll.event"
#define POLL_READ_MT   "epoll.read"
#define POLL_WRITE_MT  "epoll.write"
#define POLL_SIGNAL_MT "epoll.signal"
#define POLL_TIMER_MT  "epoll.timer"

void libopen_poll_event(lua_State *L);
void libopen_poll_read(lua_State *L);
void libopen_poll_write(lua_State *L);
void libopen_poll_signal(lua_State *L);
void libopen_poll_timer(lua_State *L);

int poll_raed_new(lua_State *L);
int poll_write_new(lua_State *L);
int poll_signal_new(lua_State *L);
int poll_timer_new(lua_State *L);

int poll_event_gc_lua(lua_State *L);
int poll_event_tostring_lua(lua_State *L, const char *tname);
int poll_event_renew_lua(lua_State *L, const char *tname);
int poll_event_revert_lua(lua_State *L, const char *tname);

int poll_evset_getflag(lua_State *L, int ref_filter_evset, int ident);
poll_event_t *poll_evset_get(lua_State *L, poll_t *p, event_t *evt);
void poll_evset_del(lua_State *L, poll_event_t *ev);

#define POLL_ERROR    -1
#define POLL_OK       0
#define POLL_EALREADY 1

int poll_watch_event(lua_State *L, poll_event_t *ev, int poll_event_idx);
int poll_unwatch_event(lua_State *L, poll_event_t *ev);

int poll_event_watch_lua(lua_State *L, const char *tname);
int poll_event_unwatch_lua(lua_State *L, const char *tname);

int poll_event_is_enabled_lua(lua_State *L, const char *tname);
int poll_event_is_oneshot_lua(lua_State *L, const char *tname);
int poll_event_as_oneshot_lua(lua_State *L, const char *tname);
int poll_event_ident_lua(lua_State *L, const char *tname);
int poll_event_udata_lua(lua_State *L, const char *tname);
int poll_event_getinfo_lua(lua_State *L, const char *tname);

#endif
