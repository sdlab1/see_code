-- plugin/see_code.lua
local M = {}
-- Убедитесь, что see_code установлен в $PATH
local socket_path = "/data/data/com.termux/files/usr/tmp/see_code_socket"
local see_code_binary = "see_code"

-- Проверка зависимостей
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

-- Проверка, запущен ли GUI, путем подключения к сокету
local function check_gui_connection()
    local uv = vim.loop
    local pipe = uv.new_pipe()
    if not pipe then
        return false
    end

    local connected = false
    pipe:connect(socket_path, function(err)
        if not err then
            connected = true
        end
        pipe:close()
    end)

    vim.wait(100, function() return connected end, 10)

    return connected
end

-- Запуск сервера see_code
local function start_gui_server()
    local uv = vim.loop

    if check_gui_connection() then
        vim.notify("see_code: Server is already running.", vim.log.levels.INFO)
        return true
    end

    vim.notify("see_code: Starting GUI server...", vim.log.levels.INFO)

    local handle, pid_or_err = uv.spawn(see_code_binary, {
        args = { "--verbose" },
        stdio = { nil, nil, nil }
    }, function(code, signal)
        vim.schedule(function()
            if code ~= 0 and code ~= nil then
                vim.notify(string.format("see_code: Server process exited (code: %d, signal: %d)", code or -1, signal or -1), vim.log.levels.WARN)
            end
        end)
    end)

    if not handle then
        vim.notify("see_code: Failed to spawn server process: " .. tostring(pid_or_err), vim.log.levels.ERROR)
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
        vim.notify("see_code: GUI server started successfully (PID: " .. tostring(pid_or_err) .. ").", vim.log.levels.INFO)
        return true
    else
        vim.notify("see_code: GUI server might have started, but connection failed.", vim.log.levels.WARN)
        vim.wait(500)
        if check_gui_connection() then
             vim.notify("see_code: Connection successful after extra wait.", vim.log.levels.INFO)
             return true
        else
             vim.notify("see_code: Could not connect to server after startup.", vim.log.levels.ERROR)
             return false
        end
    end
end

-- Парсинг diff текста
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
                table.insert(current_hunk.lines, line)
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

-- Отправка данных в GUI
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
        vim.notify("see_code: Failed to create pipe for sending data", vim.log.levels.ERROR)
        return false
    end

    pipe:connect(socket_path, function(err)
        if err then
            vim.notify("see_code: Failed to connect to GUI: " .. tostring(err), vim.log.levels.ERROR)
            pipe:close()
            return
        end

        pipe:write(json_str, function(err)
            if err then
                vim.notify("see_code: Failed to send data: " .. tostring(err), vim.log.levels.ERROR)
            else
                vim.notify(string.format("see_code: Sent %d bytes to GUI successfully", json_bytes), vim.log.levels.INFO)
            end
            pipe:close()
        end)
    end)

    return true
end

-- Основная функция отправки diff
function M.send_diff()
    if not check_dependencies() then
        return
    end

    if not check_gui_connection() then
        vim.notify("see_code: GUI server not running, attempting to start...", vim.log.levels.INFO)
        if not start_gui_server() then
            vim.notify("see_code: Failed to start GUI server.", vim.log.levels.ERROR)
            return
        end
        if not check_gui_connection() then
             vim.notify("see_code: Still cannot connect to GUI server after startup attempt.", vim.log.levels.ERROR)
             return
        end
    end

    vim.notify("see_code: Collecting diff data...", vim.log.levels.INFO)

    local diff_cmd = "git diff --unified=3 --no-color HEAD"
    local diff_output = vim.fn.systemlist(diff_cmd)

    if vim.v.shell_error ~= 0 then
        vim.notify("see_code: Git diff failed. Check git status.", vim.log.levels.ERROR)
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
        vim.notify(string.format("see_code: Successfully sent %d files with changes", #parsed_files))
    end
end

-- Функция проверки статуса
function M.status()
    print("=== see_code Status ===")
    local deps_ok = check_dependencies()
    print("Dependencies (git, repo): " .. (deps_ok and "OK" or "MISSING"))
    local gui_ok = check_gui_connection()
    print("GUI Connection: " .. (gui_ok and "OK" or "FAILED"))

    local ps_output = vim.fn.system("pgrep -f '^" .. see_code_binary .. "'")
    local server_running = ps_output and ps_output ~= ""
    local server_pid = server_running and ps_output:match("%d+") or "N/A"
    print("Server Process: " .. (server_running and "RUNNING (PID: " .. server_pid .. ")" or "NOT RUNNING"))

    local git_status = vim.fn.system("git status --porcelain")
    local has_changes = git_status and git_status ~= ""
    print("Git Changes: " .. (has_changes and "YES" or "NONE"))

    if deps_ok and gui_ok then
        print("Status: READY")
    else
        print("Status: NOT READY")
        if not deps_ok then
            print("  - Ensure git is installed and you are in a git repo")
        end
        if not gui_ok then
            if not server_running then
                print("  - Server is not running. It should start automatically with :SeeCodeDiff.")
            else
                print("  - Server is running but not accepting connections. Check logs.")
            end
        end
    end
end

-- Функция установки
function M.setup()
    vim.api.nvim_create_user_command('SeeCodeDiff', M.send_diff, {
        desc = 'Send git diff to see_code GUI (starts server if needed)'
    })
    vim.api.nvim_create_user_command('SeeCodeStatus', M.status, {
        desc = 'Check see_code system status'
    })

    vim.keymap.set('n', '<Leader>sd', M.send_diff, { desc = 'see_code: Send diff' })
    vim.keymap.set('n', '<Leader>ss', M.status, { desc = 'see_code: Check status' })

    vim.notify("see_code: Plugin loaded. Use :SeeCodeDiff to send changes.", vim.log.levels.INFO)
end

M.setup()

return M
