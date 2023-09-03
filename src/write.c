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

#define MODULE_MT POLL_WRITE_MT

static int getinfo_lua(lua_State *L)
{
    return poll_event_getinfo_lua(L, MODULE_MT);
}

static int udata_lua(lua_State *L)
{
    return poll_event_udata_lua(L, MODULE_MT);
}

static int ident_lua(lua_State *L)
{
    return poll_event_ident_lua(L, MODULE_MT);
}

static int as_oneshot_lua(lua_State *L)
{
    return poll_event_as_oneshot_lua(L, MODULE_MT);
}

static int is_oneshot_lua(lua_State *L)
{
    return poll_event_is_oneshot_lua(L, MODULE_MT);
}

static int as_edge_lua(lua_State *L)
{
    return poll_event_as_edge_lua(L, MODULE_MT);
}

static int is_edge_lua(lua_State *L)
{
    return poll_event_is_edge_lua(L, MODULE_MT);
}

static int as_level_lua(lua_State *L)
{
    return poll_event_as_level_lua(L, MODULE_MT);
}

static int is_level_lua(lua_State *L)
{
    return poll_event_is_level_lua(L, MODULE_MT);
}

static int is_enabled_lua(lua_State *L)
{
    return poll_event_is_enabled_lua(L, MODULE_MT);
}

static int unwatch_lua(lua_State *L)
{
    return poll_event_unwatch_lua(L, MODULE_MT);
}

static int watch_lua(lua_State *L)
{
    return poll_event_watch_lua(L, MODULE_MT);
}

static int revert_lua(lua_State *L)
{
    return poll_event_revert_lua(L, MODULE_MT);
}

static int renew_lua(lua_State *L)
{
    return poll_event_renew_lua(L, MODULE_MT);
}

static int type_lua(lua_State *L)
{
    lua_pushliteral(L, "write");
    return 1;
}

static int tostring_lua(lua_State *L)
{
    return poll_event_tostring_lua(L, MODULE_MT);
}

static int gc_lua(lua_State *L)
{
    return poll_event_gc_lua(L);
}

int poll_write_new(lua_State *L)
{
    poll_event_t *ev = luaL_checkudata(L, 1, POLL_EVENT_MT);
    int fd           = luaL_checkinteger(L, 2);
    int dupfd        = fd;

    if (poll_evset_getflag(L, ev->p->ref_evset_write, fd)) {
        // already registered
        errno = EEXIST;
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    } else if (poll_evset_getflag(L, ev->p->ref_evset_read, fd)) {
        // NOTE: epoll does not support to watch both read and write events on
        // the same fd. so, duplicate fd.
        dupfd = dup(fd);
        if (dupfd == -1) {
            // failed to duplicate fd
            lua_pushnil(L);
            lua_pushstring(L, strerror(errno));
            lua_pushinteger(L, errno);
            return 3;
        }
    }

    ev->ident  = fd;
    ev->filter = EVFILT_WRITE;
    ev->reg_evt.events |= EPOLLOUT;
    ev->reg_evt.data.fd = dupfd;
    if (poll_watch_event(L, ev, 1) != POLL_OK) {
        if (dupfd != fd) {
            close(dupfd);
        }
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }
    // keep udata reference
    if (!lua_isnoneornil(L, 3)) {
        ev->ref_udata = getrefat(L, 3);
    }

    lua_settop(L, 1);
    luaL_getmetatable(L, MODULE_MT);
    lua_setmetatable(L, -2);
    return 1;
}

void libopen_poll_write(lua_State *L)
{
    struct luaL_Reg mmethod[] = {
        {"__gc",       gc_lua      },
        {"__tostring", tostring_lua},
        {NULL,         NULL        }
    };
    struct luaL_Reg method[] = {
        {"type",       type_lua      },
        {"renew",      renew_lua     },
        {"revert",     revert_lua    },
        {"watch",      watch_lua     },
        {"unwatch",    unwatch_lua   },
        {"is_enabled", is_enabled_lua},
        {"is_level",   is_level_lua  },
        {"as_level",   as_level_lua  },
        {"is_edge",    is_edge_lua   },
        {"as_edge",    as_edge_lua   },
        {"is_oneshot", is_oneshot_lua},
        {"as_oneshot", as_oneshot_lua},
        {"ident",      ident_lua     },
        {"udata",      udata_lua     },
        {"getinfo",    getinfo_lua   },
        {NULL,         NULL          }
    };

    // create metatable
    luaL_newmetatable(L, MODULE_MT);
    // metamethods
    for (struct luaL_Reg *ptr = mmethod; ptr->name; ptr++) {
        lua_pushcfunction(L, ptr->func);
        lua_setfield(L, -2, ptr->name);
    }
    // methods
    lua_newtable(L);
    for (struct luaL_Reg *ptr = method; ptr->name; ptr++) {
        lua_pushcfunction(L, ptr->func);
        lua_setfield(L, -2, ptr->name);
    }
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
}
