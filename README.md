# lua-epoll

[![test](https://github.com/mah0x211/lua-epoll/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-epoll/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-epoll/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-epoll)

epoll bindings for lua


## Installation

```
luarocks install epoll
```


## ok = epoll.usable()

it returns `true` if the epoll is usable.

**Returns**

- `ok:boolean`: `true` on if the epoll is usable.


## ep, err = epoll.new()

create a new epoll instance.

**Returns**

- `ep:epoll`: epoll instance.
- `err:string`: error string.
- `errno:number`: error number.


## Metamethods of epoll instance

### __len

return the number of registered events.

### __tostring

return the string representation of the epoll instance.

### __gc

close the epoll descriptor that holds by the epoll instance.
free the event-list that holds by the epoll instance.


## ok, err, errno = ep:renew()

disabled any events that have occurred and renew the file descriptor held by the epoll instance.

**NOTE:** this method should be called after forking the process. additionally, you need to invoke the `renew` method of the event instance that was created by the `epoll` instance.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error string.
- `errno:number`: error number.


## ev = ep:new_event()

create a new `epoll.event` instance.

**Returns**

- `ev:epoll.event`: `epoll.event` instance.


## n, err, errno = ep:wait( [msec] )

wait for events. it consumes all remaining events before waiting for new events.

**Parameters**

- `msec:number`: timeout in milliseconds. if the value is `nil` or `<=0` then it waits forever.

**Returns**

- `n:number`: the number of events.
- `err:string`: error string.
- `errno:number`: error number.

**Example**

```lua
local epoll = require('epoll')
local ep = assert(epoll.new())
-- register a new event for the file descriptor 0 (stdin)
local ev = assert(ep:new_event())
assert(ev:as_read(0))
-- wait until stdin is readable
local n, err, errno = ep:wait()
if err then
    print(err, errno)
    return
end
print('n:', n)
```


## ev, udata, errno = ep:consume()

consume the occurred event.

**NOTE:** if the `EV_EOF` flag is present in the event flags, the event is automatically unregisterd.

**Returns**

- `ev:epoll.event`: `epoll.event` instance.
- `udata:any`: udata will be treated as the following.
    - `any`: the event is occurred and the `udata` is set.
    - `nil`: the event is not occurred.
    - `string`: error message on error.
- `errno:number`: error number on error.

**Example**

```lua
local epoll = require('epoll')
local ep = assert(epoll.new())
-- register a new event for the file descriptor 0 (stdin)
local ev = assert(ep:new_event())
assert(ev:as_read(0, 'hello'))
-- wait until stdin is readable
local n, err, errno = ep:wait()
if err then
    print('error:', err, errno)
    return
end

print('n:', n)
-- consume the event
local occurred, udata, errno = ep:consume()
if errno then
    print('error:', udata, errno)
elseif ocurred then
    print('event occurred:', ocurred, udata)
end
```


## `epoll.event` instance

`epoll.event` instance is used to register the following events.

- Read event: it watches the file descriptor until it becomes readable.
- Write event: it watches the file descriptor until it becomes writable.
- Signal event: it watches the signal until it becomes occurred.
- Timer event: it watches the timer until it becomes expired.


## ok, err, errno = ev:renew( [ep] )

rewatch the event. if the `ep` is specified then it rewatch the event with the specified epoll instance.

**Parameters**

- `ep:epoll`: `epoll` instance.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error string.
- `errno:number`: error number.


## ok = ev:is_level()

return `true` if the event trigger is level trigger.

**Returns**

- `ok:boolean`: `true` if the event trigger is level trigger.


## ok = ev:as_level()

change the event trigger to level trigger.

**NOTE:** the level trigger is a trigger that is activated while the state of the target is active.


**Returns**

- `ok:boolean`: `true` on success.


## ok = ev:is_edge()

return `true` if the event trigger is edge trigger.

**Returns**

- `ok:boolean`: `true` if the event trigger is edge trigger.


## ok = ev:as_edge()

change the event trigger to edge trigger.

**NOTE:** the edge trigger is a trigger that is activated only when the state of the target changes from inactive to active.

**Returns**

- `ok:boolean`: `true` on success.


## ok = ev:is_oneshot()

return `true` if the event type is one-shot event.

**Returns**

- `ok:boolean`: `true` if the event type is one-shot event.


## ok = ev:as_oneshot()

change the event type to one-shot event.

**NOTE:** the one-shot event is a event that is automatically removed after the event is activated.

**Returns**

- `ok:boolean`: `true` on success.


## ev, err, errno = ev:as_read( fd [, udata] )

register a event that watches the file descriptor until it becomes readable.

this method is change the meta-table of the `ev` to `epoll.read`.


**Parameters**

- `fd:number`: file descriptor.
- `udata:any`: user data.

**Returns**

- `ev:epoll.read`: `epoll.read` instance that is changed the meta-table of the `ev`.
- `err:string`: error string.
- `errno:number`: error number.

**Example**

```lua
local epoll = require('epoll')
local ep = assert(epoll.new())

-- register a new event for the file descriptor 0 (stdin)
local ev = assert(ep:new_event())
assert(ev:as_read(0, 'stdin is readable'))

-- wait until stdin is readable
local n, err, errno = ep:wait()
if err then
    print(err, errno)
    return
end
print('n:', n)

-- consume the event
while true do
    local occurred, udata, errno = ep:consume()
    if errno then
        print('error:', udata, errno)
    elseif not occurred then
        break
    end
    print('event occurred:', occurred, udata)
end
```


## ev, err, errno = ev:as_write( fd [, udata] )

register a event that watches the file descriptor until it becomes writable.

this method is change the meta-table of the `ev` to `epoll.write`.

**Parameters**

- `fd:number`: file descriptor.
- `udata:any`: user data.

**Returns**

- `ev:epoll.write`: `epoll.write` instance that is changed the meta-table of the `ev`.
- `err:string`: error string.
- `errno:number`: error number.

**Example**

```lua
local epoll = require('epoll')
local ep = assert(epoll.new())

-- register a new event for the file descriptor 1 (stdout)
local ev = assert(ep:new_event())
assert(ev:as_write(1, 'stdout is writable'))

-- wait until stdout is writable
local n, err, errno = ep:wait()
if err then
    print(err, errno)
    return
end
print('n:', n)

-- consume the event
while true do
    local occurred, udata, errno = ep:consume()
    if errno then
        print('error:', udata, errno)
    elseif not occurred then
        break
    end
    print('event occurred:', occurred, udata)
end
```

## ev, err, errno = ev:as_signal( signo [, udata] )

register a event that watches the signal until it becomes occurred.

this method is change the meta-table of the `ev` to `epoll.signal`.

**Parameters**

- `signo:number`: signal number.
- `udata:any`: user data.

**Returns**

- `ev:epoll.signal`: `epoll.signal` instance that is changed the meta-table of the `ev`.
- `err:string`: error string.
- `errno:number`: error number.

**Example**

```lua
local signal = require('signal')
local epoll = require('epoll')
local ep = assert(epoll.new())

-- register a new event for the signal SIGINT
local ev = assert(ep:new_event())
assert(ev:as_signal(signal.SIGINT, 'SIGINT occurred'))

-- wait until SIGINT is occurred
signal.block(signal.SIGINT)
local n, err, errno = ep:wait()
if err then
    print(err, errno)
    return
end
print('n:', n)

-- consume the event
while true do
    local occurred, udata, errno = ep:consume()
    if errno then
        print('error:', udata, errno)
    elseif not occurred then
        break
    end
    print('event occurred:', occurred, udata)
end
```

## ev, err, errno = ev:as_timer( ident, msec [, udata] )

register a event that watches the timer until it becomes expired.

this method is change the meta-table of the `ev` to `epoll.timer`.

**Parameters**

- `ident:number`: timer identifier.
- `msec:number`: timer interval in milliseconds.
- `udata:any`: user data.

**Returns**

- `ev:epoll.timer`: `epoll.timer` instance that is changed the meta-table of the `ev`.
- `err:string`: error string.
- `errno:number`: error number.

**Example**

```lua
local epoll = require('epoll')
local ep = assert(epoll.new())

-- register a new event for the timer
local ev = assert(ep:new_event())
assert(ev:as_timer(123, 150, 'timer expired after 150 milliseconds'))

-- wait until the timer is expired
local n, err, errno = ep:wait()
if err then
    print(err, errno)
    return
end
print('n:', n)

-- consume the event
while true do
    local occurred, udata, errno = ep:consume()
    if errno then
        print('error:', udata, errno)
    elseif not occurred then
        break
    end
    print('event occurred:', occurred, udata)
end
```


## Common Methods

the following methods are common methods of the `epoll.read`, `epoll.write`, `epoll.signal` and `epoll.timer` instances.


## ok, err, errno = ev:renew( [ep] )

rewatch the event. if the `ep` is specified then it rewatch the event with the specified epoll instance.

**Parameters**

- `ep:epoll`: `epoll` instance.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error string.
- `errno:number`: error number.


## ok, err, errno = ev:renew( [ep] )

rewatch the event. if the `ep` is specified then it rewatch the event with the specified epoll instance.

**Parameters**

- `ep:epoll`: `epoll` instance.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error string.
- `errno:number`: error number.


## ev, err, errno = ev:revert()

revert the event to the `epoll.event` instance. if the event is enabled then it disable the event.

**Returns**

- `ev:epoll.event`: `epoll.event` instance that is reverted the meta-table of the `ev`.
- `err:string`: error string.
- `errno:number`: error number.


## ok, err, errno = ev:watch()

watch the event.

**NOTE:** the event is managed by its type and a unique identifier pair. If this pair has already been watched, then the method will return `false`.

- `epoll.read`: file descriptor used as the identifier.
- `epoll.write`: file descriptor used as the identifier.
- `epoll.signal`: signal number used as the identifier.
- `epoll.timer`: timer identifier used as the identifier.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error string.
- `errno:number`: error number.


## ok, err, errno = ev:unwatch()

unwatch the event.

**NOTE:** if the event is enabled then it disable the event.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error string.
- `errno:number`: error number.


## ok = ev:is_enabled()

return `true` if the event is enabled (watching).

**Returns**

- `ok:boolean`: `true` if the event is enabled (watching).


## ok = ev:is_level()

return `true` if the event trigger is level trigger.

**Returns**

- `ok:boolean`: `true` if the event trigger is level trigger.


## ok, err, errno = ev:as_level()

change the event trigger to level trigger.

**NOTE:** if the event is enabled, it can not be changed.

**Returns**

- `ok:boolean`: `true` on success.
- `err:string`: error string.
- `errno:number`: error number.


## ok = ev:is_edge()

return `true` if the event trigger is edge trigger.

**Returns**

- `ok:boolean`: `true` if the event trigger is edge trigger.


## ok = ev:as_edge()

change the event trigger to edge trigger.

**NOTE:** if the event is enabled, it can not be changed.

**Returns**

- `ok:boolean`: `true` on success.


## ok = ev:is_oneshot()

return `true` if the event type is one-shot event.

**Returns**

- `ok:boolean`: `true` if the event type is one-shot event.


## ok = ev:as_oneshot()

change the event type to one-shot event.

**NOTE:** if the event is enabled, it can not be changed.

**Returns**

- `ok:boolean`: `true` on success.


## ident = ev:ident()

return the identifier of the event.

**Returns**

- `ident:number`: identifier of the event.


## udata = ev:udata( [udata] )

set or return the user data of the event. 

if the `udata` is specified then it set the user data of the event and return the previous user data.

**Returns**

- `udata:any`: user data of the event.


## info, err, errno = ev:getinfo( event )

get the information of the specified event.

**NOTE:** if the `EV_ERROR` flag is present in the flags, then the `data` treated as the `errno`.

**Parameters**

- `event:string`: event name as follows.
  - `registered`: return the information of the event that is registered.
  - `occurred`: return the information of the event that is occurred.
  
**Returns**

- `info:table`: information of the event.
  - `ident:number`: identifier of the event.
  - `udata:any`: user data of the event.
  - `edge:boolean`: `true` if the event trigger is edge trigger.
  - `oneshot:boolean`: `true` if the event type is one-shot event.
- `err:string`: error string.
- `errno:number`: error number.

