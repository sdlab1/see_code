-- plugin/see_code.lua
local M = {}

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

local function parse_diff_to_hunks(diff_text)
    if not diff_text or diff_text == "" then
        return {}
    end

    local files = {}
    local current_file = nil
    local current_hunk = nil

    for line in diff_text:gmatch("[^\r\n]+") do
        local path_a, path_b = line:match("^diff %-\\-git a/(.+) b/(.+)$")
        if path_a or path_b then
            if current_file then
                if current_hunk then
                    table.insert(current_file.hunks, current_hunk)
                end
                table.insert(files, current_file)
            end
            current_file = {
                path = path_b or path_a,
                hunks = {}
            }
            current_hunk = nil
        elseif line:match("^@%@") and current_file then
            if current_hunk then
                table.insert(current_file.hunks, current_hunk)
            end
            current_hunk = {
                header = line,
                lines = {}
            }
        elseif current_hunk then
            if line:match("^[+%- ]") or line:match("^\\ No newline") then
                local line_type = "context"
                if line:sub(1,1) == "+" then
                    line_type = "add"
                elseif line:sub(1,1) == "-" then
                    line_type = "delete"
                end
                table.insert(current_hunk.lines, { content = line, type = line_type })
            end
        end
    end

    if current_hunk and current_file then
        table.insert(current_file.hunks, current_hunk)
    end
    if current_file then
        table.insert(files, current_file)
    end

    return files
end

local function send_to_gui(data)
    local uv = vim.loop
    local json_str = vim.fn.json_encode(data)
    local json_bytes = string.len(json_str)

    if json_bytes > 50 * 1024 * 1024 then
        vim.notify(string.format("see_code: Data too large (%d bytes).", json_bytes), vim.log.levels.ERROR)
        return false
    end

    local pipe = uv.new_pipe()
    if not pipe then
        vim.notify("see_code: Failed to create pipe.", vim.log.levels.ERROR)
        return false
    end

    pipe:connect(get_config("socket_path"), function(err)
        if err then
            vim.notify("see_code: Failed to connect: " .. tostring(err), vim.log.levels.ERROR)
            pipe:close()
            return
        end

        pipe:write(json_str, function(err)
            if err then
                vim.notify("see_code: Failed to send: " .. tostring(err), vim.log.levels.ERROR)
            else
                if get_config("verbose") then
                    vim.notify(string.format("see_code: Sent %d bytes.", json_bytes), vim.log.levels.INFO)
                end
            end
            pipe:close()
        end)
    end)

    return true
end

function M.send_diff()
    if not user_config.socket_path then load_user_config() end

    if not check_dependencies() then
        return
    end

    if not check_gui_connection() then
        if get_config("auto_start_server") then
            vim.notify("see_code: GUI server not running, attempting to start...", vim.log.levels.INFO)
            if not start_gui_server() then
                vim.notify("see_code: Failed to start GUI server.", vim.log.levels.ERROR)
                return
            end
            if not check_gui_connection() then
                 vim.notify("see_code: Still cannot connect after startup.", vim.log.levels.ERROR)
                 return
            end
        else
             vim.notify("see_code: GUI server not running and auto-start is disabled.", vim.log.levels.ERROR)
             return
        end
    end

    if get_config("verbose") then
        vim.notify("see_code: Collecting diff data...", vim.log.levels.INFO)
    end

    local diff_cmd = "git diff --unified=3 --no-color HEAD"
    local diff_output = vim.fn.systemlist(diff_cmd)

    if vim.v.shell_error ~= 0 then
        vim.notify("see_code: Git diff failed.", vim.log.levels.ERROR)
        return
    end

    local diff_text = table.concat(diff_output, "\n")

    if diff_text == "" or diff_text:match("^%s*$") then
        vim.notify("see_code: No changes detected", vim.log.levels.WARN)
        return
    end

    local parsed_files = parse_diff_to_hunks(diff_text)

    if #parsed_files == 0 then
        vim.notify("see_code: No valid diff hunks found", vim.log.levels.WARN)
        return
    end

    local payload = parsed_files

    if send_to_gui(payload) then
        vim.notify(string.format("see_code: Sent %d files with changes", #parsed_files))
    end
end

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
                print("  - Server is not running. Try :SeeCodeDiff to start it.")
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
    vim.api.nvim_create_user_command('SeeCodeStatus', M.status, {
        desc = 'Check see_code system and config status'
    })

    vim.keymap.set('n', '<Leader>sd', M.send_diff, { desc = 'see_code: Send diff', silent = true })
    vim.keymap.set('n', '<Leader>ss', M.status, { desc = 'see_code: Check status' })

    vim.notify("see_code: Plugin loaded. Use :SeeCodeDiff.", vim.log.levels.INFO)
end

M.setup()
return M
