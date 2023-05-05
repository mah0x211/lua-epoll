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

#include "lua_epoll.h"

static inline void event_closefd(poll_event_t *ev)
{
    switch (ev->filter) {
    case EVFILT_READ:
    case EVFILT_WRITE:
        // close duplicated fd
        if (ev->reg_evt.data.fd != ev->ident) {
            close(ev->reg_evt.data.fd);
            ev->reg_evt.data.fd = -1;
        }
        break;

    case EVFILT_SIGNAL:
    case EVFILT_TIMER:
        // close signalfd or timerfd
        close(ev->reg_evt.data.fd);
        ev->reg_evt.data.fd = -1;
        break;
    }
}

int poll_event_gc_lua(lua_State *L)
{
    poll_event_t *ev = lua_touserdata(L, 1);
    unref(L, ev->ref_poll);
    unref(L, ev->ref_udata);
    event_closefd(ev);
    return 0;
}

int poll_event_tostring_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);
    lua_pushfstring(L, "%s: %p", tname, ev);
    return 1;
}

int poll_event_renew_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);
    poll_t *p        = ev->p;

    if (lua_gettop(L) > 1) {
        p = luaL_checkudata(L, 2, POLL_MT);
    }

    int rc = poll_unwatch_event(L, ev);
    if (rc == POLL_ERROR) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }

    // replace poll instance
    if (ev->p != p) {
        ev->p        = p;
        ev->ref_poll = unref(L, ev->ref_poll);
        lua_settop(L, 2);
        ev->ref_poll = getref(L);
    }

    // watch event again in new poll instance
    if (rc == POLL_OK) {
        return poll_event_watch_lua(L, tname);
    }

    lua_pushboolean(L, 1);
    return 1;
}

int poll_event_revert_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);

    if (poll_unwatch_event(L, ev) == POLL_ERROR) {
        // got error
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }

    event_closefd(ev);
    ev->filter    = 0;
    ev->reg_evt   = (event_t){0};
    ev->occ_evt   = (event_t){0};
    ev->ref_udata = unref(L, ev->ref_udata);
    lua_settop(L, 1);
    luaL_getmetatable(L, POLL_EVENT_MT);
    lua_setmetatable(L, -2);
    lua_settop(L, 1);
    return 1;
}

int poll_evset_getflag(lua_State *L, int ref_filter_evset, int ident)
{
    pushref(L, ref_filter_evset);
    lua_rawgeti(L, -1, ident);
    if (!lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return 1;
    }
    lua_pop(L, 1);
    return 0;
}

static void evset_setflag(lua_State *L, poll_event_t *ev)
{
    int ref_evset = LUA_NOREF;

    switch (ev->filter) {
    case EVFILT_READ:
        ref_evset = ev->p->ref_evset_read;
        break;
    case EVFILT_WRITE:
        ref_evset = ev->p->ref_evset_write;
        break;
    case EVFILT_SIGNAL:
        ref_evset = ev->p->ref_evset_signal;
        break;
    case EVFILT_TIMER:
        ref_evset = ev->p->ref_evset_timer;
        break;
    }

    pushref(L, ref_evset);
    lua_pushboolean(L, 1);
    lua_rawseti(L, -2, ev->ident);
    lua_pop(L, 1);
}

static void evset_unsetflag(lua_State *L, poll_event_t *ev)
{
    int ref_evset = LUA_NOREF;

    switch (ev->filter) {
    case EVFILT_READ:
        ref_evset = ev->p->ref_evset_read;
        break;
    case EVFILT_WRITE:
        ref_evset = ev->p->ref_evset_write;
        break;
    case EVFILT_SIGNAL:
        ref_evset = ev->p->ref_evset_signal;
        break;
    case EVFILT_TIMER:
        ref_evset = ev->p->ref_evset_timer;
        break;
    }

    pushref(L, ref_evset);
    lua_pushnil(L);
    lua_rawseti(L, -2, ev->ident);
    lua_pop(L, 1);
}

poll_event_t *poll_evset_get(lua_State *L, poll_t *p, event_t *evt)
{
    // get poll_event_t at the ident index
    pushref(L, p->ref_evset);
    lua_rawgeti(L, -1, evt->data.fd);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return NULL;
    }
    // remove event set table
    lua_replace(L, -2);
    return lua_touserdata(L, -1);
}

static int evset_add(lua_State *L, poll_event_t *ev, int poll_event_idx)
{
    // check if event fd is already registered
    pushref(L, ev->p->ref_evset);
    lua_rawgeti(L, -1, ev->reg_evt.data.fd);
    if (!lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return POLL_EALREADY;
    }
    lua_pop(L, 1);

    // set poll_event_t at the fd index
    lua_pushvalue(L, poll_event_idx);
    lua_rawseti(L, -2, ev->reg_evt.data.fd);
    // increment registered event counter
    ev->p->nreg++;
    lua_pop(L, 1);

    // set flag to prevent double registration
    evset_setflag(L, ev);

    return POLL_OK;
}

int poll_watch_event(lua_State *L, poll_event_t *ev, int poll_event_idx)
{
    if (ev->enabled) {
        // return error if already registered
        errno = EEXIST;
        return POLL_EALREADY;
    } else if (evset_add(L, ev, poll_event_idx) != POLL_OK) {
        return luaL_error(L, "[BUG] %s:%d: invalid implementation", __FILE__,
                          __LINE__);
    }

    // register event
    if (epoll_ctl(ev->p->fd, EPOLL_CTL_ADD, ev->reg_evt.data.fd,
                  &ev->reg_evt) == -1) {
        poll_evset_del(L, ev);
        return POLL_ERROR;
    }
    ev->enabled = 1;

    return POLL_OK;
}

void poll_evset_del(lua_State *L, poll_event_t *ev)
{
    // delete poll_event_t at the ident index
    pushref(L, ev->p->ref_evset);
    lua_rawgeti(L, -1, ev->reg_evt.data.fd);
    if (!lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_rawseti(L, -2, ev->reg_evt.data.fd);
        ev->p->nreg--;
        // unset flag
        evset_unsetflag(L, ev);
    }
    lua_pop(L, 1);
}

int poll_unwatch_event(lua_State *L, poll_event_t *ev)
{
    if (!ev->enabled) {
        // not watched
        return POLL_EALREADY;
    }

    // unregister event
    if (epoll_ctl(ev->p->fd, EPOLL_CTL_DEL, ev->reg_evt.data.fd, NULL) == -1) {
        switch (errno) {
        case EBADF:
        case ENOENT:
            break;
        default:
            return POLL_ERROR;
        }
    }
    ev->enabled = 0;
    poll_evset_del(L, ev);

    return POLL_OK;
}

int poll_event_watch_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);

    switch (poll_watch_event(L, ev, 1)) {
    case POLL_OK:
        // success
        lua_pushboolean(L, 1);
        return 1;

    case POLL_EALREADY:
        // already watched
        lua_pushboolean(L, 0);
        return 1;

    default:
        // got error
        lua_pushboolean(L, 0);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }
}

int poll_event_unwatch_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);

    switch (poll_unwatch_event(L, ev)) {
    case POLL_OK:
        // success
        lua_pushboolean(L, 1);
        return 1;

    case POLL_EALREADY:
        // already unwatched
        lua_pushboolean(L, 0);
        return 1;

    default:
        // got error
        lua_pushboolean(L, 0);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int poll_event_is_enabled_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);
    lua_pushboolean(L, ev->enabled);
    return 1;
}

int poll_event_is_level_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);
    lua_pushboolean(
        L, !(ev->reg_evt.events & ~(EPOLLIN | EPOLLOUT | EPOLLEXCLUSIVE)));
    return 1;
}

int poll_event_as_level_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);

    if (ev->enabled) {
        // event is in use
        errno = EINPROGRESS;
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }

    // treat event as level-triggered event
    ev->reg_evt.events &= ~(EV_ONESHOT | EV_CLEAR);
    ev->reg_evt.events |= EPOLLEXCLUSIVE;
    lua_settop(L, 1);
    return 1;
}

int poll_event_is_edge_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);
    lua_pushboolean(L, ev->reg_evt.events & EV_CLEAR);
    return 1;
}

int poll_event_as_edge_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);

    if (ev->enabled) {
        // event is in use
        errno = EINPROGRESS;
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }

    // treat event as edge-triggered event
    ev->reg_evt.events &= ~EV_ONESHOT;
    ev->reg_evt.events |= (EV_CLEAR | EPOLLEXCLUSIVE);
    lua_settop(L, 1);
    return 1;
}

int poll_event_is_oneshot_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);
    lua_pushboolean(L, ev->reg_evt.events & EV_ONESHOT);
    return 1;
}

int poll_event_as_oneshot_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);

    if (ev->enabled) {
        // event is in use
        errno = EINPROGRESS;
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }

    // treat event as oneshot event
    ev->reg_evt.events &= ~(EV_CLEAR | EPOLLEXCLUSIVE);
    ev->reg_evt.events |= EV_ONESHOT;
    lua_settop(L, 1);
    return 1;
}

int poll_event_ident_lua(lua_State *L, const char *tname)
{
    poll_event_t *ev = luaL_checkudata(L, 1, tname);
    lua_pushinteger(L, ev->ident);
    return 1;
}

int poll_event_udata_lua(lua_State *L, const char *tname)
{
    int narg         = lua_gettop(L);
    poll_event_t *ev = luaL_checkudata(L, 1, tname);

    if (ev->ref_udata == LUA_NOREF) {
        lua_pushnil(L);
    } else {
        pushref(L, ev->ref_udata);
    }

    if (narg > 1) {
        if (lua_isnoneornil(L, 2)) {
            // release udata reference
            ev->ref_udata = unref(L, ev->ref_udata);
        } else {
            // replace new udata
            int ref       = getrefat(L, 2);
            ev->ref_udata = unref(L, ev->ref_udata);
            ev->ref_udata = ref;
        }
    }

    return 1;
}

static int push_event(lua_State *L, poll_event_t *ev, event_t evt)
{
    int edge    = evt.events & EV_CLEAR;
    int oneshot = evt.events & EV_ONESHOT;
    int eof     = evt.events & EV_EOF;

    // push event
    lua_createtable(L, 0, 5);
    pushref(L, ev->ref_udata);
    lua_setfield(L, -2, "udata");
    lua_pushinteger(L, ev->ident);
    lua_setfield(L, -2, "ident");
    if (edge) {
        lua_pushboolean(L, edge);
        lua_setfield(L, -2, "edge");
    }
    if (oneshot) {
        lua_pushboolean(L, oneshot);
        lua_setfield(L, -2, "oneshot");
    }
    if (eof) {
        lua_pushboolean(L, eof);
        lua_setfield(L, -2, "eof");
    }

    return 1;
}

int poll_event_getinfo_lua(lua_State *L, const char *tname)
{
    static const char *const opts[] = {
        "registered",
        "occurred",
        NULL,
    };
    poll_event_t *ev = luaL_checkudata(L, 1, tname);
    int selected     = luaL_checkoption(L, 2, NULL, opts);

    switch (selected) {
    case 0:
        return push_event(L, ev, ev->reg_evt);
    default:
        return push_event(L, ev, ev->occ_evt);
    }
}
