-- plugin/see_code.lua
local M = {}
local socket_path = "/data/data/com.termux/files/usr/tmp/see_code_socket"

-- Dependency checking
local function check_dependencies()
    if vim.fn.executable('git') == 0 then
        vim.notify("see_code: git is not installed", vim.log.levels.ERROR)
        return false
    end

    -- Check if we're in a git repository
    local git_dir = vim.fn.system("git rev-parse --git-dir 2>/dev/null")
    if vim.v.shell_error ~= 0 then
        vim.notify("see_code: Not in a git repository", vim.log.levels.ERROR)
        return false
    end

    return true
end

-- Check if GUI is running by attempting to connect to the socket
local function check_gui_connection()
    local uv = vim.loop
    local pipe = uv.new_pipe()
    if not pipe then
        vim.notify("see_code: Failed to create pipe", vim.log.levels.ERROR)
        return false
    end

    local connected = false
    -- Use a callback to check connection status
    pipe:connect(socket_path, function(err)
        if not err then
            connected = true
        end
        -- Close the pipe immediately after checking
        pipe:close()
    end)

    -- Give it a brief moment to attempt the connection
    vim.wait(100, function() return connected end, 10)

    if not connected then
        vim.notify("see_code: GUI application not running. Please start it first.", vim.log.levels.ERROR)
        return false
    end
    return true
end

-- Parse diff text into a structured format
local function parse_diff_to_hunks(diff_text)
    if not diff_text or diff_text == "" then
        return {}
    end

    local files = {}
    local current_file = nil
    local current_hunk = nil

    for line in diff_text:gmatch("[^\r\n]+") do
        -- New file detection
        local path_a, path_b = line:match("^diff %-\\-git a/(.+) b/(.+)$")
        if path_a or path_b then
            -- Save previous file and hunk
            if current_file then
                if current_hunk then
                    table.insert(current_file.hunks, current_hunk)
                end
                table.insert(files, current_file)
            end
            -- Start new file
            current_file = {
                path = path_b or path_a,
                hunks = {}
            }
            current_hunk = nil
        -- Hunk header detection
        elseif line:match("^@%@") and current_file then
            if current_hunk then
                table.insert(current_file.hunks, current_hunk)
            end
            -- Start new hunk
            current_hunk = {
                header = line,
                lines = {}
            }
        -- Content lines (only add if we have a current hunk)
        elseif current_hunk then
            -- Include diff lines (+, -, space) and context
            if line:match("^[+%- ]") or line:match("^\\ No newline") then
                table.insert(current_hunk.lines, line)
            end
        end
    end

    -- Don't forget the last hunk and file
    if current_hunk and current_file then
        table.insert(current_file.hunks, current_hunk)
    end
    if current_file then
        table.insert(files, current_file)
    end

    return files
end

-- Send data to the GUI application
local function send_to_gui(data)
    local uv = vim.loop
    local json_str = vim.fn.json_encode(data)
    local json_bytes = string.len(json_str)

    if json_bytes > 50 * 1024 * 1024 then -- 50MB limit from config.h
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

        -- Send the JSON data directly (without length prefix for simplicity with current C server)
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

-- Main function to collect and send diff
function M.send_diff()
    -- Pre-flight checks
    if not check_dependencies() then
        return
    end
    if not check_gui_connection() then
        return
    end

    vim.notify("see_code: Collecting diff data...", vim.log.levels.INFO)

    -- Get git diff
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

    -- Parse diff into structured format
    local parsed_files = parse_diff_to_hunks(diff_text)

    if #parsed_files == 0 then
        vim.notify("see_code: No valid diff hunks found", vim.log.levels.WARN)
        return
    end

    -- Create the payload expected by the C application
    -- The C app expects an array of files directly, not wrapped in a 'files' key
    local payload = parsed_files

    -- Send to GUI
    if send_to_gui(payload) then
        vim.notify(string.format("see_code: Successfully sent %d files with changes", #parsed_files))
    end
end

-- Status check function
function M.status()
    print("=== see_code Status ===")
    local deps_ok = check_dependencies()
    print("Dependencies (git, repo): " .. (deps_ok and "OK" or "MISSING"))
    local gui_ok = check_gui_connection()
    print("GUI Connection: " .. (gui_ok and "OK" or "FAILED"))
    
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
            print("  - Start the see_code GUI application")
        end
    end
end

-- Setup function
function M.setup()
    -- Create user commands
    vim.api.nvim_create_user_command('SeeCodeDiff', M.send_diff, {
        desc = 'Send git diff to see_code GUI'
    })
    vim.api.nvim_create_user_command('SeeCodeStatus', M.status, {
        desc = 'Check see_code system status'
    })

    -- Set up a default keymap
    vim.keymap.set('n', '<Leader>sd', M.send_diff, { desc = 'see_code: Send diff' })
    vim.keymap.set('n', '<Leader>ss', M.status, { desc = 'see_code: Check status' })

    vim.notify("see_code: Plugin loaded. Use :SeeCodeDiff to send changes.", vim.log.levels.INFO)
end

-- Automatically call setup when the plugin is sourced
M.setup()

return M
