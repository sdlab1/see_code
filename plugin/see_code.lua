-- plugin/see_code.lua
local M = {}

-- Default configuration (no cjson related settings needed)
local default_config = {
    socket_path = "/data/data/com.termux/files/usr/tmp/see_code_socket",
    see_code_binary = "see_code",
    font_chain = {
        "/system/fonts/Roboto-Regular.ttf",
        "/system/fonts/DroidSansMono.ttf",
        "/data/data/com.termux/files/usr/share/fonts/liberation/LiberationMono-Regular.ttf"
    },
    fallback_behavior = "termux_gui",
    auto_start_server = true,
    verbose = false
}

local user_config = {}

local function load_user_config()
    local config_file = vim.fn.stdpath('config') .. '/see_code_config.lua'
    local ok, module = pcall(dofile, config_file)
    if ok and type(module) == "table" then
        user_config = vim.tbl_deep_extend("force", {}, default_config, module)
        if user_config.verbose then
            vim.notify("see_code: User configuration loaded.", vim.log.levels.INFO)
        end
    else
        if user_config.verbose or default_config.verbose then
            vim.notify("see_code: No user config found or error, using defaults.", vim.log.levels.WARN)
        end
        user_config = default_config
    end
end

local function get_config(key)
    return user_config[key] or default_config[key]
end

local function check_dependencies()
    if vim.fn.executable('git') == 0 then
        vim.notify("see_code: git is not installed", vim.log.levels.ERROR)
        return false
    end

    local git_dir = vim.fn.system("git rev-parse --git-dir 2>/dev/null")
    if vim.v.shell_error ~= 0 then
        vim.notify("see_code: Not in a git repository", vim.log.levels.ERROR)
        return false
    end

    return true
end

local function check_gui_connection()
    local uv = vim.loop
    local pipe = uv.new_pipe()
    if not pipe then
        return false
    end

    local connected = false
    pipe:connect(get_config("socket_path"), function(err)
        if not err then
            connected = true
        end
        pipe:close()
    end)

    vim.wait(100, function() return connected end, 10)
    return connected
end

local function start_gui_server()
    if not get_config("auto_start_server") then
        vim.notify("see_code: Auto-start disabled.", vim.log.levels.WARN)
        return false
    end

    local uv = vim.loop

    if check_gui_connection() then
        vim.notify("see_code: Server is already running.", vim.log.levels.INFO)
        return true
    end

    vim.notify("see_code: Starting GUI server...", vim.log.levels.INFO)

    local handle, pid_or_err = uv.spawn(get_config("see_code_binary"), {
        args = { "--verbose" },
        stdio = { nil, nil, nil }
    }, function(code, signal)
        vim.schedule(function()
            if code ~= 0 and code ~= nil then
                vim.notify(string.format("see_code: Server exited (code: %d, signal: %d)", code or -1, signal or -1), vim.log.levels.WARN)
            end
        end)
    end)

    if not handle then
        vim.notify("see_code: Failed to spawn server: " .. tostring(pid_or_err), vim.log.levels.ERROR)
        return false
    end

    local started = false
    for i = 1, 30 do
        if check_gui_connection() then
            started = true
            break
        end
        vim.wait(100)
    end

    if started then
        vim.notify("see_code: GUI server started (PID: " .. tostring(pid_or_err) .. ").", vim.log.levels.INFO)
        return true
    else
        vim.notify("see_code: Server might have started, but connection failed.", vim.log.levels.WARN)
        vim.wait(500)
        if check_gui_connection() then
             vim.notify("see_code: Connection successful after extra wait.", vim.log.levels.INFO)
             return true
        else
             vim.notify("see_code: Could not connect to server.", vim.log.levels.ERROR)
             return false
        end
    end
end

-- --- CHANGED FUNCTION ---
-- Send raw diff data to the GUI application
local function send_to_gui(data_buffer)
    -- data_buffer is expected to be a Lua string containing the raw bytes
    local data_size = #data_buffer -- Length in bytes

    if data_size > 50 * 1024 * 1024 then -- 50MB limit
        vim.notify(string.format("see_code: Data too large (%d bytes).", data_size), vim.log.levels.ERROR)
        return false
    end

    local uv = vim.loop
    local pipe = uv.new_pipe()
    if not pipe then
        vim.notify("see_code: Failed to create pipe for sending data", vim.log.levels.ERROR)
        return false
    end

    pipe:connect(get_config("socket_path"), function(err)
        if err then
            vim.notify("see_code: Failed to connect to GUI: " .. tostring(err), vim.log.levels.ERROR)
            pipe:close()
            return
        end

        -- Send the raw data buffer directly
        pipe:write(data_buffer, function(err)
            if err then
                vim.notify("see_code: Failed to send raw data: " .. tostring(err), vim.log.levels.ERROR)
            else
                if get_config("verbose") then
                    vim.notify(string.format("see_code: Sent %d raw bytes to GUI successfully", data_size), vim.log.levels.INFO)
                end
            end
            pipe:close()
        end)
    end)

    return true
end
-- --- END CHANGE ---

-- Main function to collect and send diff
function M.send_diff()
    if not user_config.socket_path then load_user_config() end

    if not check_dependencies() then
        return
    end

    -- [ИЗМЕНЕНО] Проверяем и запускаем сервер, даже если diff пустой
    if not check_gui_connection() then
        if get_config("auto_start_server") then
            vim.notify("see_code: GUI server not running, attempting to start...", vim.log.levels.INFO)
            if not start_gui_server() then
                vim.notify("see_code: Failed to start GUI server.", vim.log.levels.ERROR)
                return
            end
            -- Небольшая задержка после запуска
            vim.wait(200)
            if not check_gui_connection() then
                 vim.notify("see_code: Still cannot connect after startup.", vim.log.levels.WARN)
                 -- Продолжаем, так как сервер может быть запущен, но еще не готов
            end
        else
             vim.notify("see_code: GUI server not running and auto-start is disabled.", vim.log.levels.ERROR)
             return
        end
    end

    if get_config("verbose") then
        vim.notify("see_code: Collecting diff data...", vim.log.levels.INFO)
    end

    -- --- CHANGED: Get raw diff text ---
    local diff_cmd = "git diff --unified=3 --no-color HEAD"
    local diff_output = vim.fn.systemlist(diff_cmd)
    -- --- END CHANGE ---

    if vim.v.shell_error ~= 0 then
        vim.notify("see_code: Git diff failed.", vim.log.levels.ERROR)
        return
    end

    -- --- CHANGED: Concatenate lines into a single string (raw buffer) ---
    local diff_text = table.concat(diff_output, "\n")
    -- --- END CHANGE ---

    -- [ИЗМЕНЕНО] Отправляем данные, даже если diff_text пустой
    -- if diff_text == "" or diff_text:match("^%s*$") then
    --     vim.notify("see_code: No changes detected", vim.log.levels.WARN)
    --     return
    -- end

    -- --- CHANGED: Send the raw text buffer ---
    -- The C application now expects raw bytes, not JSON.
    if send_to_gui(diff_text) then -- Отправляем всегда
        vim.notify(string.format("see_code: Successfully sent %d bytes of raw diff data", #diff_text))
    end
    -- --- END CHANGE ---
end

-- [НОВАЯ ФУНКЦИЯ] Принудительный запуск сервера
function M.start_server()
    if not user_config.socket_path then load_user_config() end

    if check_gui_connection() then
        vim.notify("see_code: Server is already running.", vim.log.levels.INFO)
        return
    end

    vim.notify("see_code: Starting GUI server...", vim.log.levels.INFO)
    if start_gui_server() then
        vim.notify("see_code: Server started successfully.", vim.log.levels.INFO)
    else
        vim.notify("see_code: Failed to start server.", vim.log.levels.ERROR)
    end
end

-- Status check function (remains largely the same)
function M.status()
    if not user_config.socket_path then load_user_config() end

    print("=== see_code Status ===")
    local deps_ok = check_dependencies()
    print("Dependencies (git, repo): " .. (deps_ok and "OK" or "MISSING"))
    local gui_ok = check_gui_connection()
    print("GUI Connection: " .. (gui_ok and "OK" or "FAILED"))

    local ps_output = vim.fn.system("pgrep -f '^" .. get_config("see_code_binary") .. "'")
    local server_running = ps_output and ps_output ~= ""
    local server_pid = server_running and ps_output:match("%d+") or "N/A"
    print("Server Process: " .. (server_running and "RUNNING (PID: " .. server_pid .. ")" or "NOT RUNNING"))

    local git_status = vim.fn.system("git status --porcelain")
    local has_changes = git_status and git_status ~= ""
    print("Git Changes: " .. (has_changes and "YES" or "NONE"))

    print("\n--- Configuration ---")
    print("Socket Path: " .. get_config("socket_path"))
    print("Binary: " .. get_config("see_code_binary"))
    print("Auto Start Server: " .. (get_config("auto_start_server") and "YES" or "NO"))
    print("Fallback Behavior: " .. get_config("fallback_behavior"))
    print("Font Chain:")
    for i, font in ipairs(get_config("font_chain")) do
        print("  " .. i .. ". " .. font)
    end

    if deps_ok and gui_ok then
        print("\nStatus: READY")
    else
        print("\nStatus: NOT READY")
        if not deps_ok then
            print("  - Ensure git is installed and you are in a git repo")
        end
        if not gui_ok then
            if not server_running then
                print("  - Server is not running. Try :SeeCodeDiff or :SeeCodeStart to start it.")
            else
                print("  - Server is running but not accepting connections.")
            end
        end
    end
end

function M.setup()
    load_user_config()

    vim.api.nvim_create_user_command('SeeCodeDiff', M.send_diff, {
        desc = 'Send git diff to see_code GUI'
    })
    -- [НОВАЯ КОМАНДА]
    vim.api.nvim_create_user_command('SeeCodeStart', M.start_server, {
        desc = 'Start the see_code GUI server'
    })
    vim.api.nvim_create_user_command('SeeCodeStatus', M.status, {
        desc = 'Check see_code system and config status'
    })

    vim.keymap.set('n', '<Leader>sd', M.send_diff, { desc = 'see_code: Send diff', silent = true })
    vim.keymap.set('n', '<Leader>ss', M.status, { desc = 'see_code: Check status' })

    vim.notify("see_code: Plugin loaded. Use :SeeCodeDiff or :SeeCodeStart.", vim.log.levels.INFO)
end

M.setup()
return M
