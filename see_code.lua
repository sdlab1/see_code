-- lua/see_code/init.lua
local M = {}

local socket_path = "/data/data/com.termux/files/usr/tmp/see_code_socket"
local uv = vim.loop

-- Dependency checking
local function check_dependencies()
    local missing = {}
    
    if vim.fn.executable('git') == 0 then
        table.insert(missing, 'git')
    end
    
    -- Check if we're in a git repository
    local git_dir = vim.fn.system("git rev-parse --git-dir 2>/dev/null")
    if vim.v.shell_error ~= 0 then
        vim.notify("see_code: Not in a git repository", vim.log.levels.ERROR)
        return false
    end
    
    if #missing > 0 then
        vim.notify(string.format("see_code: Missing dependencies: %s", table.concat(missing, ", ")), vim.log.levels.ERROR)
        vim.notify("Install with: pkg install " .. table.concat(missing, " "), vim.log.levels.INFO)
        return false
    end
    
    return true
end

-- Check if GUI is running
local function check_gui_connection()
    local client = uv.new_tcp()
    if not client then
        vim.notify("see_code: Failed to create TCP client", vim.log.levels.ERROR)
        return false
    end
    
    local pipe = uv.new_pipe()
    if not pipe then
        client:close()
        vim.notify("see_code: Failed to create pipe", vim.log.levels.ERROR)
        return false
    end
    
    local connected = false
    pipe:connect(socket_path, function(err)
        if not err then
            connected = true
        end
        pipe:close()
    end)
    
    -- Wait briefly for connection attempt
    vim.wait(100)
    
    client:close()
    
    if not connected then
        vim.notify("see_code: GUI not running. Start with: ./see_code &", vim.log.levels.ERROR)
        return false
    end
    
    return true
end

-- Enhanced diff parsing with better error handling
local function parse_diff_to_hunks(diff_text)
    if not diff_text or diff_text == "" then
        return {}
    end
    
    local files = {}
    local current_file = nil
    local current_hunk = nil
    
    for line in diff_text:gmatch("[^\r\n]+") do
        -- New file detection
        local git_match = line:match("^diff --git a/(.+) b/(.+)$")
        if git_match then
            -- Save previous file and hunk
            if current_file then
                if current_hunk then
                    table.insert(current_file.hunks, current_hunk)
                end
                table.insert(files, current_file)
            end
            
            current_file = {
                path = git_match,
                hunks = {}
            }
            current_hunk = nil
            
        -- Hunk header detection
        elseif line:match("^@@") and current_file then
            if current_hunk then
                table.insert(current_file.hunks, current_hunk)
            end
            
            current_hunk = {
                id = #current_file.hunks,
                header = line,
                lines = {},
                is_collapsed = true
            }
            
        -- Content lines (only add if we have a current hunk)
        elseif current_hunk then
            -- Only include actual diff content lines
            if line:match("^[+%-]") or line:match("^%s") then
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

-- Improved data transmission with length prefix
local function send_to_gui(data)
    local json_str = vim.fn.json_encode(data)
    local json_bytes = string.len(json_str)
    
    if json_bytes > 10485760 then  -- 10MB limit
        vim.notify(string.format("see_code: Data too large (%d bytes). Try smaller diff.", json_bytes), vim.log.levels.ERROR)
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
        
        -- Send length first (4 bytes, big endian)
        local length_bytes = string.pack(">I4", json_bytes)
        
        pipe:write(length_bytes, function(err)
            if err then
                vim.notify("see_code: Failed to send length: " .. tostring(err), vim.log.levels.ERROR)
                pipe:close()
                return
            end
            
            -- Send actual JSON data
            pipe:write(json_str, function(err)
                if err then
                    vim.notify("see_code: Failed to send data: " .. tostring(err), vim.log.levels.ERROR)
                else
                    vim.notify(string.format("see_code: Sent %d bytes to GUI successfully", json_bytes), vim.log.levels.INFO)
                end
                pipe:close()
            end)
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
    
    -- Get git diff with proper error handling
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
    
    local payload = {
        type = "structured_diff",
        files = parsed_files,
        meta = {
            repo = vim.fn.getcwd(),
            timestamp = os.time(),
            commit = vim.trim(vim.fn.system("git rev-parse --short HEAD")),
            branch = vim.trim(vim.fn.system("git branch --show-current"))
        }
    }
    
    -- Send to GUI
    if send_to_gui(payload) then
        vim.notify(string.format("see_code: Successfully sent %d files with changes", #parsed_files))
    end
end

-- Status check function
function M.status()
    print("=== see_code Status ===")
    
    -- Check dependencies
    local deps_ok = check_dependencies()
    print("Dependencies: " .. (deps_ok and "OK" or "MISSING"))
    
    -- Check GUI connection
    local gui_ok = check_gui_connection()
    print("GUI Connection: " .. (gui_ok and "OK" or "FAILED"))
    
    -- Check git repository
    local git_status = vim.fn.system("git status --porcelain")
    local has_changes = git_status and git_status ~= ""
    print("Git Changes: " .. (has_changes and "YES" or "NONE"))
    
    if deps_ok and gui_ok then
        print("Status: READY")
    else
        print("Status: NOT READY")
        if not deps_ok then
            print("  - Install missing dependencies")
        end
        if not gui_ok then
            print("  - Start GUI with: ./see_code &")
        end
    end
end

-- Auto-diff on file save (optional)
function M.auto_diff_on_save()
    local group = vim.api.nvim_create_augroup("see_code_auto", { clear = true })
    
    vim.api.nvim_create_autocmd("BufWritePost", {
        group = group,
        pattern = "*",
        callback = function()
            -- Only auto-diff if GUI is running
            if check_gui_connection() then
                vim.defer_fn(function()
                    M.send_diff()
                end, 500)  -- Small delay to let git status update
            end
        end,
    })
    
    vim.notify("see_code: Auto-diff on save enabled", vim.log.levels.INFO)
end

-- Setup function with enhanced configuration
function M.setup(opts)
    opts = opts or {}
    
    -- Create user commands
    vim.api.nvim_create_user_command('SeeCodeDiff', M.send_diff, {
        desc = 'Send git diff to see_code GUI'
    })
    
    vim.api.nvim_create_user_command('SeeCodeStatus', M.status, {
        desc = 'Check see_code system status'
    })
    
    vim.api.nvim_create_user_command('SeeCodeAutoSave', M.auto_diff_on_save, {
        desc = 'Enable auto-diff on file save'
    })
    
    -- Set up key mappings
    if opts.keymaps ~= false then
        local keymap_opts = { desc = 'see_code: Send diff to GUI' }
        vim.keymap.set('n', '<leader>sd', M.send_diff, keymap_opts)
        
        keymap_opts.desc = 'see_code: Check status'
        vim.keymap.set('n', '<leader>ss', M.status, keymap_opts)
        
        keymap_opts.desc = 'see_code: Toggle auto-save'
        vim.keymap.set('n', '<leader>sa', M.auto_diff_on_save, keymap_opts)
    end
    
    -- Initial status check
    vim.defer_fn(function()
        local deps_ok = check_dependencies()
        if deps_ok then
            vim.notify("see_code: Plugin loaded successfully", vim.log.levels.INFO)
            vim.notify("Use :SeeCodeStatus to check system status", vim.log.levels.INFO)
        end
    end, 100)
end

return M
