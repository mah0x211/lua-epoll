local testcase = require('testcase')
local socketpair = require('testcase.socketpair')
local epoll = require('epoll')
local errno = require('errno')
local signal = require('signal')

if not epoll.usable() then
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

function testcase.renew()
    local ep1 = assert(epoll.new())
    local ep2 = assert(epoll.new())
    local ev = ep1:new_event()

    -- test that renew event with other epoll
    assert(ev:renew(ep2))

    -- test that throws an error if invalid argument
    local err = assert.throws(function()
        ev:renew('invalid')
    end)
    assert.match(err, 'epoll expected')
end

function testcase.as_oneshot_is_oneshot()
    local ep = assert(epoll.new())
    local ev = ep:new_event()

    -- test that set the oneshot flags
    assert.is_false(ev:is_oneshot())
    assert.equal(ev:as_oneshot(), ev)
    assert.is_true(ev:is_oneshot())
end

function testcase.as_read_as_write()
    local ep = assert(epoll.new())
    local ev1 = ep:new_event()
    local ev2 = ep:new_event()
    local udata = {
        'context',
    }

    -- test that convert event to read event
    assert(ev1:as_read(Reader:fd(), udata))
    assert.match(ev1, '^epoll%.read: ', false)
    assert.equal(ev1:udata(), udata)
    assert.equal(#ep, 1)

    -- test that convert event to write event
    assert(ev2:as_write(Reader:fd(), udata))
    assert.match(ev2, '^epoll%.write: ', false)
    assert.equal(#ep, 2)

    -- test that return an error if fd is already registered
    local ev3 = ep:new_event()
    local _, err, errnum = ev3:as_read(Reader:fd(), udata)
    assert.is_nil(_)
    assert.equal(err, errno.EEXIST.message)
    assert.equal(errnum, errno.EEXIST.code)

    _, err, errnum = ev3:as_write(Reader:fd(), udata)
    assert.is_nil(_)
    assert.equal(err, errno.EEXIST.message)
    assert.equal(errnum, errno.EEXIST.code)

    -- test that can register same fd after unwatched
    assert(ev1:unwatch())
    assert.equal(#ep, 1)
    assert(ev3:as_read(Reader:fd(), udata))
end

function testcase.as_signal()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    local udata = {
        'context',
    }

    -- test that convert event to signal event
    assert(ev:as_signal(signal.SIGINT, udata))
    assert.match(ev, '^epoll%.signal: ', false)
    assert.equal(ev:udata(), udata)
    assert.equal(#ep, 1)

    -- test that return an error if signal is already registered
    local ev2 = ep:new_event()
    local _, err, errnum = ev2:as_signal(signal.SIGINT, udata)
    assert.is_nil(_)
    assert.equal(err, errno.EEXIST.message)
    assert.equal(errnum, errno.EEXIST.code)

    -- test that return error if unsupported signal number
    assert(ev:revert())
    _, err, errnum = ev:as_signal(1234567890, udata)
    assert.is_nil(_)
    assert.equal(err, errno.EINVAL.message)
    assert.equal(errnum, errno.EINVAL.code)

    -- test that throws an error if invalid signal
    err = assert.throws(function()
        ev:as_signal('invalid', udata)
    end)
    assert.match(err, 'number expected')
end

function testcase.as_timer()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    local udata = {
        'context',
    }

    -- test that convert event to timer event
    assert(ev:as_timer(123, 10, udata))
    assert.match(ev, '^epoll%.timer: ', false)
    assert.equal(ev:udata(), udata)
    assert.equal(#ep, 1)

    -- test that return an error if ident is already registered
    local ev2 = ep:new_event()
    local _, err, errnum = ev2:as_timer(123, 10, udata)
    assert.is_nil(_)
    assert.equal(err, errno.EEXIST.message)
    assert.equal(errnum, errno.EEXIST.code)

    -- test that return error if invalid msec
    assert(ev:revert())
    _, err, errnum = ev:as_timer(123, -1, udata)
    assert.is_nil(_)
    assert.equal(err, errno.EINVAL.message)
    assert.equal(errnum, errno.EINVAL.code)

    -- test that throws an error if invalid timer
    err = assert.throws(function()
        ev:as_timer('invalid', udata)
    end)
    assert.match(err, 'number expected')
end
