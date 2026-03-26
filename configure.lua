---
--- This script is used as a before_build hook for luarocks-build-hooks.
--- It checks whether the current platform is supported and expands glob
--- patterns in rockspec.build.modules[*].sources so the LuaRocks builtin
--- backend (which does not support wildcards natively) can compile them.
---
local rockspec = ...

local configh = require('configh')
local supported = true
local cc = os.getenv('CC') or
               (rockspec and rockspec.variables and rockspec.variables.CC)
local cfgh = configh(cc)
cfgh:output_status(true)
for header, funcs in pairs({
    ['sys/epoll.h'] = {
        'epoll_create',
        'epoll_create1',
        'epoll_wait',
        'epoll_pwait',
    },
    ['sys/signalfd.h'] = {
        'signalfd',
    },
    ['sys/timerfd.h'] = {
        'timerfd_create',
        'timerfd_settime',
    },
}) do
    if not cfgh:check_header(header) then
        supported = false
    else
        for _, func in ipairs(funcs) do
            cfgh:check_func(header, func)
        end
    end
end
assert(cfgh:flush('src/config.h'))

-- create symbolic link to src/ directory
local function create_symlink(srcdir)
    os.remove('./impl')
    local cmd = ('ln -sf %s impl'):format(srcdir)
    print('cretate symlink: ' .. cmd)
    assert(os.execute(cmd))
end
create_symlink(supported and 'src/' or 'nosup/')

-- expand glob patterns in modules.epoll.sources so the builtin backend
-- can compile them (LuaRocks builtin does not support wildcards natively)
if rockspec and rockspec.build and rockspec.build.modules then
    local epoll = rockspec.build.modules.epoll
    local sources = epoll and epoll.sources
    if type(sources) == 'string' then
        sources = {
            sources,
        }
    end
    if type(sources) == 'table' then
        local expanded = {}
        for _, src in ipairs(sources) do
            if src:find('[*?]') then
                local pipe = assert(io.popen('ls ' .. src))
                for file in pipe:lines() do
                    expanded[#expanded + 1] = file
                end
                pipe:close()
            else
                expanded[#expanded + 1] = src
            end
        end
        epoll.sources = expanded
    end
end
