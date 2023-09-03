local testcase = require('testcase')
local socketpair = require('testcase.socketpair')
local epoll = require('epoll')
local errno = require('errno')

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

function testcase.type()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))

    -- test that get the event type
    assert.equal(ev:type(), 'read')
end

function testcase.renew()
    local ep1 = assert(epoll.new())
    local ep2 = assert(epoll.new())
    local ev = ep1:new_event()
    assert(ev:as_read(Reader:fd()))

    -- test that renew event with other epoll
    assert(ev:renew(ep2))

    -- test that throws an error if invalid argument
    local err = assert.throws(function()
        ev:renew('invalid')
    end)
    assert.match(err, 'epoll expected')
end

function testcase.revert()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))
    assert.match(ev, '^epoll%.read: ', false)

    -- test that revert event to initial state
    assert(ev:revert())
    assert.match(ev, '^epoll%.event: ', false)
end

function testcase.revert_after_fd_closed()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))
    assert.match(ev, '^epoll%.read: ', false)

    -- test that revert event to initial state
    Reader:close()
    assert(ev:revert())
    assert.match(ev, '^epoll%.event: ', false)
end

function testcase.watch_unwatch()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))

    -- test that return false without error if event is already watched
    local ok, err, errnum = ev:watch()
    assert.is_false(ok)
    assert.is_nil(err)
    assert.is_nil(errnum)

    -- test that event occurs when fd is readable
    assert(Writer:write('test'))
    local nevt = assert(ep:wait())
    assert.equal(nevt, 1)
    local oev = assert(ep:consume())
    assert.equal(oev, ev)

    -- test that return true if event is watched
    ok, err, errnum = ev:unwatch()
    assert.is_true(ok)
    assert.is_nil(err)
    assert.is_nil(errnum)

    -- test that return false without error if event is already unwatched
    ok, err, errnum = ev:unwatch()
    assert.is_false(ok)
    assert.is_nil(err)
    assert.is_nil(errnum)

    -- test that return true if event will be watched
    ok, err, errnum = ev:watch()
    assert.is_true(ok)
    assert.is_nil(err)
    assert.is_nil(errnum)
end

function testcase.is_enabled()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))

    -- test that return true if event is enabled
    assert.is_true(ev:is_enabled())

    -- test that return false if event is not enabled
    assert(ev:unwatch())
    assert.is_false(ev:is_enabled())
end

function testcase.as_level_is_level()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))
    ev:unwatch()

    -- test that remove the oneshot flags
    assert(ev:as_oneshot())
    assert.is_false(ev:is_level())
    assert(ev:as_level())
    assert.is_true(ev:is_level())

    -- test that remove the edge flag
    assert(ev:as_edge())
    assert.is_false(ev:is_level())
    assert(ev:as_level())
    assert.is_true(ev:is_level())

    -- test that return error if event is in-progress
    assert(ev:watch())
    local err, errnum
    ev, err, errnum = ev:as_level()
    assert.is_nil(ev)
    assert.equal(err, errno.EINPROGRESS.message)
    assert.equal(errnum, errno.EINPROGRESS.code)
end

function testcase.as_edge_is_edge()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))
    ev:unwatch()

    -- test that set the edge flags
    assert.is_false(ev:is_edge())
    assert(ev:as_edge())
    assert.is_true(ev:is_edge())

    -- test that return error if event is in-progress
    assert(ev:watch())
    local err, errnum
    ev, err, errnum = ev:as_edge()
    assert.is_nil(ev)
    assert.equal(err, errno.EINPROGRESS.message)
    assert.equal(errnum, errno.EINPROGRESS.code)
end

function testcase.as_oneshot_is_oneshot()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))
    ev:unwatch()

    -- test that set the oneshot flags
    assert.is_false(ev:is_oneshot())
    assert(ev:as_oneshot())
    assert.is_true(ev:is_oneshot())

    -- test that return error if event is in-progress
    assert(ev:watch())
    local err, errnum
    ev, err, errnum = ev:as_oneshot()
    assert.is_nil(ev)
    assert.equal(err, errno.EINPROGRESS.message)
    assert.equal(errnum, errno.EINPROGRESS.code)
end

function testcase.ident()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))

    -- test that return ident
    assert.equal(ev:ident(), Reader:fd())
end

function testcase.udata()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd(), 'test'))

    -- test that set udata and return previous udata
    assert.equal(ev:udata(nil), 'test')

    -- test that return nil
    assert.is_nil(ev:udata())
end

function testcase.getinfo()
    local ep = assert(epoll.new())
    local ev = ep:new_event()
    assert(ev:as_read(Reader:fd()))

    -- test that get info of registered event
    assert.is_table(ev:getinfo('registered'))

    -- test that get info of occurred event
    assert.is_table(ev:getinfo('occurred'))

    -- test that throws an error if invalid info name
    local err = assert.throws(function()
        ev:getinfo('invalid')
    end)
    assert.match(err, 'invalid option')
end

