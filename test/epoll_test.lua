local testcase = require('testcase')
local socketpair = require('testcase.socketpair')
local epoll = require('epoll')

if not epoll.usable() then
    function testcase.usable()
        -- test that return true on supported platform
        assert.is_false(epoll.usable())
    end

    function testcase.new()
        -- test that create a new epoll
        local err = assert.throws(epoll.new)
        assert.match(err, 'epoll is not supported on this platform')
    end

    return
end

local Reader
local Writer

function testcase.before_each()
    for _, sock in pairs({
        Reader,
        Writer,
    }) do
        if sock then
            sock:close()
        end
    end

    Reader, Writer = assert(socketpair())
end

function testcase.usable()
    -- test that return true on supported platform
    assert.is_true(epoll.usable())
end

function testcase.new()
    -- test that create a new epoll
    local ep = assert(epoll.new())
    assert.match(ep, '^epoll: ', false)
end

function testcase.renew()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_oneshot())
    assert(ev:as_write(Writer:fd()))

    local nevt = assert(ep:wait())
    assert.equal(nevt, 1)
    assert.is_true(ev:is_enabled())

    -- test that renew a epoll file descriptor and unconsumed events will be disabled
    assert(ep:renew())
    assert.is_false(ev:is_enabled())
    assert.is_nil(ep:consume())
end

function testcase.new_event()
    local ep = assert(epoll.new())

    -- test that create a new event
    local ev = ep:new_event()
    assert.match(ev, '^epoll%.event: ', false)
end

function testcase.len()
    local ep = assert(epoll.new())
    local ev = ep:new_event()

    -- test that return number of registered events
    assert.equal(#ep, 0)
    assert(ev:as_read(Reader:fd()))
    assert.equal(#ep, 1)
    ev:unwatch()
    assert.equal(#ep, 0)
end

function testcase.wait()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    local ctx = {
        'context',
    }
    assert(Writer:write('test'))

    -- test that return 0 if no events registered
    local nevt = assert(ep:wait())
    assert.equal(nevt, 0)

    -- test that return 1
    assert(ev:as_read(Reader:fd(), ctx))
    nevt = assert(ep:wait())
    assert.equal(nevt, 1)
    Reader:read()

    -- test that return 0 if timeout
    nevt = assert(ep:wait(0.01))
    assert.equal(nevt, 0)

    -- test that event occurred repeatedly as default
    assert(Writer:write('test'))
    nevt = assert(ep:wait())
    assert.equal(nevt, 1)
end

function testcase.unconsumed_events_will_be_consumed_in_wait()
    local ep = assert(epoll.new())
    local ev1 = ep:new_event()
    local ev2 = ep:new_event()
    assert(Writer:write('test'))

    assert(ev1:as_oneshot())
    assert(ev1:as_read(Reader:fd()))
    assert(ev2:as_oneshot())
    assert(ev2:as_write(Writer:fd()))
    -- epoll remove descriptor if it is closed

    local nevt = assert(ep:wait())
    assert.equal(nevt, 2)
    assert.is_true(ev1:is_enabled())
    assert.is_true(ev2:is_enabled())

    -- test that wait() will consume unconsumed events
    nevt = assert(ep:wait())
    assert.equal(nevt, 0)
    assert.is_false(ev1:is_enabled())
    assert.is_false(ev2:is_enabled())
end

function testcase.consume()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(Writer:write('test'))
    assert(ev:as_read(Reader:fd(), {
        'context',
    }))

    -- test that return number of occurred events
    assert.equal(assert(ep:wait()), 1)
    local oev, ctx, disabled = assert(ep:consume())
    assert.equal(oev, ev)
    assert.equal(ctx, {
        'context',
    })
    assert.is_nil(disabled)

    -- test that return nil if consumed all events
    oev = ep:consume()
    assert.is_nil(oev)
end

function testcase.eof_event_will_be_disabled_in_consume()
    local ep = assert(epoll.new())
    assert(Writer:write('test'))
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd(), {
        'context',
    }))

    -- test that return number of occurred events
    Writer:close()
    assert.equal(assert(ep:wait()), 1)
    local oev, ctx, disabled = assert(ep:consume())
    assert.equal(oev, ev)
    assert.equal(ctx, {
        'context',
    })
    assert.is_true(disabled)
    assert.equal(#ep, 0)
    assert.equal(ev:getinfo('occurred'), {
        ident = Reader:fd(),
        eof = true,
        udata = {
            'context',
        },
    })

    -- test that return nil if consumed all events
    ev = ep:consume()
    assert.is_nil(ev)
end

function testcase.oneshot_event_will_be_disabled_in_consume()
    local ep = assert(epoll.new())
    assert(Writer:write('test'))
    local ev = ep:new_event()
    ev:as_oneshot()
    assert(ev:as_read(Reader:fd(), {
        'context',
    }))

    -- test that oneshot-trigger event
    assert.equal(assert(ep:wait()), 1)
    local oev, ctx, disabled = assert(ep:consume())
    assert.equal(oev, ev)
    assert.equal(ctx, {
        'context',
    })
    assert.is_true(disabled)
    assert.equal(#ep, 0)
    assert.equal(ev:getinfo('occurred'), {
        ident = Reader:fd(),
        udata = {
            'context',
        },
    })

    -- test that onshot-event will be deleted after event occurred
    assert.equal(#ep, 0)
    assert.equal(assert(ep:wait(0.01)), 0)

    -- test that fd will be deleted from epoll after event occurred
    ev = ep:new_event()
    ev:as_oneshot()
    assert(ev:as_read(Reader:fd(), {
        'context',
    }))
end

function testcase.edge_triggered_event_will_not_repeat()
    local ep = assert(epoll.new())
    assert(Writer:write('test'))
    local ev = ep:new_event()
    ev:as_edge()
    assert(ev:as_read(Reader:fd(), {
        'context',
    }))

    -- test that edge-trigger event
    assert.equal(assert(ep:wait()), 1)
    local oev, ctx, disabled = assert(ep:consume())
    assert.equal(oev, ev)
    assert.equal(ctx, {
        'context',
    })
    assert.is_nil(disabled)
    assert.equal(ev:getinfo('occurred'), {
        ident = Reader:fd(),
        udata = {
            'context',
        },
    })
    assert.equal(#ep, 1)

    -- test that event does not occur repeatedly
    assert.equal(assert(ep:wait(0.01)), 0)

    -- test that event occur if descriptor has changed
    assert(Writer:write('test'))
    assert.equal(assert(ep:wait()), 1)
end

