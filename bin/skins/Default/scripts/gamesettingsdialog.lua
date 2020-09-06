function clamp(x, min, max) 
    if x < min then
        x = min
    end
    if x > max then
        x = max
    end

    return x
end

function smootherstep(edge0, edge1, x) 
    -- Scale, and clamp x to 0..1 range
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    -- Evaluate polynomial
    return x * x * x * (x * (x * 6 - 15) + 10)
end
  
function to_range(val, start, stop)
    return start + (stop - start) * val
end

Animation = {
    start = 0,
    stop = 0,
    progress = 0,
    duration = 1,
    smoothStart = false
}

function Animation:new(o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

function Animation:restart(start, stop, duration)
    self.progress = 0
    self.start = start
    self.stop = stop
    self.duration = duration
end

function Animation:tick(deltaTime)
    self.progress = math.min(1, self.progress + deltaTime / self.duration)
    if self.progress == 1 then return self.stop end
    if self.smoothStart then
        return to_range(smootherstep(0, 1, self.progress), self.start, self.stop)
    else
        return to_range(smootherstep(-1, 1, self.progress) * 2 - 1, self.start, self.stop)
    end
end

local yScale = Animation:new()
local diagWidth = 600
local diagHeight = 400
local tabStroke = {start=0, stop=1}
local tabStrokeAnimation = {start=Animation:new(), stop=Animation:new()}
local settingsStrokeAnimation = {x=Animation:new(), y=Animation:new()}
local prevTab = -1
local prevSettingStroke = {x=0, y=0}
local settingStroke = {x=0, y=0}
local prevVis = false

function render(deltaTime, visible)
    if visible and not prevVis then
        yScale:restart(0, 1, 0.25)
    elseif not visible and prevVis then
        yScale:restart(1, 0, 0.25)
    end

    if not visible and yScale:tick(0) < 0.05 then return end

    local posX = SettingsDiag.posX or 0.5
    local posY = SettingsDiag.posY or 0.5
    local message_1 = "Press both FXs to open/close. Use the Start button to press buttons."
    local message_2 = "Use FX keys to navigate tabs. Use arrow keys to navigate and modify settings."

    resX, resY = game.GetResolution()
    local scale = resY / 1080
    gfx.ResetTransform()
    gfx.Translate(math.floor(diagWidth/2 + posX*(resX-diagWidth)), math.floor(diagHeight/2 + posY*(resY-diagHeight)))
    gfx.Scale(scale, scale)
    gfx.Scale(1.0, smootherstep(0, 1, yScale:tick(deltaTime)))
    gfx.BeginPath()
    gfx.Rect(-diagWidth/2, -diagHeight/2, diagWidth, diagHeight)
    gfx.FillColor(50,50,50)
    gfx.Fill()
    gfx.FillColor(255,255,255)
    
    gfx.FontSize(20)
    
    local m_xmin, m_ymin, m_xmax, m_ymax = gfx.TextBounds(0, 0, message_1)
    gfx.Text(message_1, diagWidth/2 - m_xmax, diagHeight/2 - m_ymax - 20)
    
    m_xmin, m_ymin, m_xmax, m_ymax = gfx.TextBounds(0, 0, message_2)
    gfx.Text(message_2, diagWidth/2 - m_xmax, diagHeight/2 - m_ymax)

    tabStroke.start = tabStrokeAnimation.start:tick(deltaTime)
    tabStroke.stop = tabStrokeAnimation.stop:tick(deltaTime)

    settingStroke.x = settingsStrokeAnimation.x:tick(deltaTime)
    settingStroke.y = settingsStrokeAnimation.y:tick(deltaTime)

    local tabBarHeight = 0
    local nextTabX = 5

    gfx.TextAlign(gfx.TEXT_ALIGN_TOP + gfx.TEXT_ALIGN_LEFT)
    gfx.FontSize(35)
    gfx.Save() --draw tab bar
    gfx.Translate(-diagWidth / 2, -diagHeight / 2)
    for ti, tab in ipairs(SettingsDiag.tabs) do
        local xmin,ymin, xmax,ymax = gfx.TextBounds(nextTabX, 5, tab.name)

        if ti == SettingsDiag.currentTab and SettingsDiag.currentTab ~= prevTab then 
            tabStrokeAnimation.start:restart(tabStroke.start, nextTabX, 0.1)
            tabStrokeAnimation.stop:restart(tabStroke.stop, xmax, 0.1)
        end
        tabBarHeight = math.max(tabBarHeight, ymax + 5)
        gfx.Text(tab.name, nextTabX, 5)
        nextTabX = xmax + 10
    end
    gfx.BeginPath()
    gfx.MoveTo(0, tabBarHeight)
    gfx.LineTo(diagWidth, tabBarHeight)
    gfx.StrokeWidth(2)
    gfx.StrokeColor(0,127,255)
    gfx.Stroke()
    gfx.BeginPath()
    gfx.MoveTo(tabStroke.start, tabBarHeight)
    gfx.LineTo(tabStroke.stop, tabBarHeight)
    gfx.StrokeColor(255, 127, 0)
    gfx.Stroke()
    gfx.Restore() --draw tab bar end

    gfx.FontSize(30)
    gfx.Save() --draw current tab
    gfx.Translate(-diagWidth / 2, -diagHeight / 2)
    gfx.Translate(5, tabBarHeight + 5)

    gfx.BeginPath()
    gfx.MoveTo(0, settingStroke.y)
    gfx.LineTo(settingStroke.x, settingStroke.y)
    gfx.StrokeWidth(2)
    gfx.StrokeColor(255, 127, 0)
    gfx.Stroke()

    local settingHeight = 30
    local tab = SettingsDiag.tabs[SettingsDiag.currentTab]
    for si, setting in ipairs(tab.settings) do
        local disp = ""
        if setting.type == "enum" then
            disp = string.format("%s: %s", setting.name, setting.options[setting.value])
        elseif setting.type == "int" then
            disp = string.format("%s: %d", setting.name, setting.value)
        elseif setting.type == "float" then
            disp = string.format("%s: %.2f", setting.name, setting.value)
            if setting.max == 1 and setting.min == 0 then --draw slider
                disp = setting.name .. ": "
                local xmin,ymin, xmax,ymax = gfx.TextBounds(0, 0, disp)
                local width = diagWidth - 20 - xmax
                gfx.BeginPath()
                gfx.MoveTo(xmax + 5, 20)
                gfx.LineTo(xmax + 5 + width, 20)
                gfx.StrokeColor(0,127,255)
                gfx.StrokeWidth(2)
                gfx.Stroke()
                gfx.BeginPath()
                gfx.MoveTo(xmax + 5, 20)
                gfx.LineTo(xmax + 5 + width * setting.value, 20)
                gfx.StrokeColor(255,127,0)
                gfx.StrokeWidth(2)
                gfx.Stroke() 
            end
        elseif setting.type == "button" then
            disp = string.format("%s", setting.name)
            local xmin, ymin, xmax,ymax = gfx.TextBounds(0, 0, disp)
            gfx.BeginPath()
            gfx.Rect(-2, 3, 4+xmax-xmin, 28)
            gfx.FillColor(0, 64, 128)
            if si == SettingsDiag.currentSetting then
                gfx.StrokeColor(255, 127, 0)
            else
                gfx.StrokeColor(0,127,255)
            end
            gfx.StrokeWidth(2)
            gfx.Fill()
            gfx.Stroke()
            gfx.FillColor(255,255,255)
        else
            disp = string.format("%s:", setting.name)
            local xmin,ymin, xmax,ymax = gfx.TextBounds(0, 0, disp)
            gfx.BeginPath()
            gfx.Rect(xmax + 5, 5, 20,20)
            gfx.FillColor(255, 127, 0, setting.value and 255 or 0)
            gfx.StrokeColor(0,127,255)
            gfx.StrokeWidth(2)
            gfx.Fill()
            gfx.Stroke()
            gfx.FillColor(255,255,255)
        end
        gfx.Text(disp, 0 ,0)
        if si == SettingsDiag.currentSetting then
            local setting_name = setting.name .. ":"
            if setting.type == "button" then
                setting_name = setting.name
            end
            local xmin,ymin, xmax,ymax = gfx.TextBounds(0, 0, setting_name)
            ymax = ymax + settingHeight * (si - 1)
            if xmax ~= prevSettingStroke.x or ymax ~= prevSettingStroke.y then
                settingsStrokeAnimation.x:restart(settingStroke.x, xmax, 0.1)
                settingsStrokeAnimation.y:restart(settingStroke.y, ymax, 0.1)
            end

            prevSettingStroke.x = xmax
            prevSettingStroke.y = ymax
        end
        gfx.Translate(0, settingHeight)
    end


    gfx.Restore() --draw current tab end
    prevTab = SettingsDiag.currentTab
    prevVis = visible

end