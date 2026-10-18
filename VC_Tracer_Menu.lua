script_name('VC Tracer Menu')
script_author('HEXED Studio')
script_version('2.0')

-- All-in-one menu for the VC bullet tracer.
-- Needs: libVC_Tracer_Nezuko.so (renderer) + libVCTracerMenu.so (this all-in-one mod).
-- Open with /vctracer.  Settings are saved to VCTracer.ini.

local imgui  = require 'mimgui'
local ffi    = require 'ffi'
local inicfg = require 'inicfg'

local new = imgui.new

local ini = inicfg.load({
    main = { enabled = true, thick = 1.0, usecolor = false, r = 1.0, g = 1.0, b = 1.0, style = 0 }
}, 'VCTracer.ini')
inicfg.save(ini, 'VCTracer.ini')

local window   = new.bool(false)
local enabled  = new.bool(ini.main.enabled)
local useColor = new.bool(ini.main.usecolor)
local thick    = new.float(ini.main.thick)
local style    = new.int(ini.main.style or 0)   -- 0 = textured (VC), 1 = solid line
local col      = new.float[3](ini.main.r, ini.main.g, ini.main.b)

-- ---------------------------------------------------------------- native link
-- VCTracerCfg (float[8]): 0 enabled, 1 thick, 2 usecolor, 3-5 rgb, 6 status, 7 style
ffi.cdef[[
    void* dlopen(const char* name, int flags);
    void* dlsym(void* handle, const char* symbol);
]]

local cfgPtr = nil

local function linkNative()
    if cfgPtr ~= nil then return true end
    -- already-loaded lib by name (RTLD_NOLOAD | RTLD_NOW = 4 | 2)
    local ok, h = pcall(function() return ffi.C.dlopen('libVCTracerMenu.so', 6) end)
    if ok and h ~= nil then
        local sym = ffi.C.dlsym(h, 'VCTracerCfg')
        if sym ~= nil then cfgPtr = ffi.cast('volatile float*', sym) return true end
    end
    return false
end

local function apply()
    if not linkNative() then return end
    cfgPtr[0] = enabled[0] and 1 or 0
    cfgPtr[1] = thick[0]
    cfgPtr[2] = useColor[0] and 1 or 0
    cfgPtr[3] = col[0]
    cfgPtr[4] = col[1]
    cfgPtr[5] = col[2]
    cfgPtr[7] = style[0]
end

local function save()
    ini.main.enabled  = enabled[0]
    ini.main.usecolor = useColor[0]
    ini.main.thick    = thick[0]
    ini.main.style    = style[0]
    ini.main.r, ini.main.g, ini.main.b = col[0], col[1], col[2]
    inicfg.save(ini, 'VCTracer.ini')
end

local function setColor(r, g, b)
    col[0], col[1], col[2] = r, g, b
    useColor[0] = true
    apply() save()
end

local function styleButton(label, id)
    local active = style[0] == id
    if active then
        imgui.PushStyleColor(imgui.Col.Button, imgui.GetStyle().Colors[imgui.Col.ButtonActive])
    end
    local clicked = imgui.Button(label)
    if active then imgui.PopStyleColor() end
    if clicked then style[0] = id end
    return clicked
end

-- ---------------------------------------------------------------- menu
imgui.OnFrame(function() return window[0] end, function()
    imgui.SetNextWindowSize(imgui.ImVec2(460, 0), imgui.Cond.FirstUseEver)
    imgui.Begin('VC Tracer - HEXED Studio', window, imgui.WindowFlags.AlwaysAutoResize)

    local changed = false
    if imgui.Checkbox('Enabled', enabled) then changed = true end

    imgui.Separator()
    imgui.Text('Style')
    if styleButton('Textured (VC)', 0) then changed = true end imgui.SameLine()
    if styleButton('Solid line', 1)    then changed = true end

    imgui.Separator()
    imgui.Text('Thickness')
    imgui.PushItemWidth(-1)
    if imgui.SliderFloat('##thick', thick, 0.2, 6.0, '%.2fx') then changed = true end
    imgui.PopItemWidth()

    imgui.Separator()
    if imgui.Checkbox('Custom color', useColor) then changed = true end
    imgui.PushItemWidth(-1)
    if imgui.ColorEdit3('##col', col) then useColor[0] = true changed = true end
    imgui.PopItemWidth()

    if imgui.Button('White')  then setColor(1, 1, 1) end imgui.SameLine()
    if imgui.Button('Red')    then setColor(1, 0.1, 0.1) end imgui.SameLine()
    if imgui.Button('Green')  then setColor(0.1, 1, 0.2) end imgui.SameLine()
    if imgui.Button('Blue')   then setColor(0.2, 0.5, 1) end imgui.SameLine()
    if imgui.Button('Yellow') then setColor(1, 0.9, 0.1) end

    imgui.Separator()
    if imgui.Button('Reset to default') then
        enabled[0], useColor[0], thick[0], style[0] = true, false, 1.0, 0
        col[0], col[1], col[2] = 1, 1, 1
        changed = true
    end

    if changed then apply() save() end

    -- status line (helps when something isn't hooked)
    imgui.Separator()
    if cfgPtr ~= nil then
        local st = math.floor(cfgPtr[6] + 0.5)
        imgui.TextDisabled(('Mod linked | hooks: Transform:%s Draw:%s | tracer drawn: %s'):format(
            (st % 2 >= 1) and 'OK' or 'NO',
            (st % 4 >= 2) and 'OK' or 'NO',
            (st % 8 >= 4) and 'yes' or 'not yet'))
    else
        imgui.TextDisabled('libVCTracerMenu.so not loaded')
    end

    imgui.End()
end)

-- ---------------------------------------------------------------- main
function main()
    while not isSampAvailable() do wait(100) end
    sampRegisterChatCommand('vctracer', function() window[0] = not window[0] end)
    while true do
        wait(500)
        apply()   -- keeps settings applied even if the mod loads late
    end
end
