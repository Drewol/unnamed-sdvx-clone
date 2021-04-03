json = require "json"

local resX,resY = game.GetResolution()

local mposx = 0;
local mposy = 0;
local hovered = nil;
local buttonWidth = resX*(3/4);
local buttonHeight = 75;
local buttonBorder = 2;
local portrait

game.LoadSkinSample("click-02")
game.LoadSkinSample("click-01")
game.LoadSkinSample("menu_click")

local loading = true;
local rooms = {};
local lobby_users = {};
local selected_room = nil;
local user_id = nil;
local jacket = 0;
local all_ready;
local user_ready;
local hard_mode = false;
local rotate_host = false;
local start_game_soon = false;
local host = nil;
local owner = nil;
local missing_song = false;
local placeholderJacket = gfx.CreateSkinImage("song_select/loading.png", 0)
local did_exit = false;

local diffColors = {{0,0,255}, {0,255,0}, {255,0,0}, {255, 0, 255}}

local grades = {
    {["max"] = 6999999, ["image"] = gfx.CreateSkinImage("score/D.png", 0)},
    {["max"] = 7999999, ["image"] = gfx.CreateSkinImage("score/C.png", 0)},
    {["max"] = 8699999, ["image"] = gfx.CreateSkinImage("score/B.png", 0)},
    {["max"] = 8999999, ["image"] = gfx.CreateSkinImage("score/A.png", 0)},
    {["max"] = 9299999, ["image"] = gfx.CreateSkinImage("score/A+.png", 0)},
    {["max"] = 9499999, ["image"] = gfx.CreateSkinImage("score/AA.png", 0)},
    {["max"] = 9699999, ["image"] = gfx.CreateSkinImage("score/AA+.png", 0)},
    {["max"] = 9799999, ["image"] = gfx.CreateSkinImage("score/AAA.png", 0)},
    {["max"] = 9899999, ["image"] = gfx.CreateSkinImage("score/AAA+.png", 0)},
    {["max"] = 99999999, ["image"] = gfx.CreateSkinImage("score/S.png", 0)}
  }

local badges = {
    gfx.CreateSkinImage("badges/played.png", 0),
    gfx.CreateSkinImage("badges/clear.png", 0),
    gfx.CreateSkinImage("badges/hard-clear.png", 0),
    gfx.CreateSkinImage("badges/full-combo.png", 0),
    gfx.CreateSkinImage("badges/perfect.png", 0)
}

local user_name_key = game.GetSkinSetting('multi.user_name_key')
if user_name_key == nil then
  user_name_key = 'nick'
end
local name = game.GetSkinSetting(user_name_key)
if name == nil or name == '' then
    name = 'Guest'
end

local normal_font = game.GetSkinSetting('multi.normal_font')
if normal_font == nil then
    normal_font = 'NotoSans-Regular.ttf'
end
local mono_font = game.GetSkinSetting('multi.mono_font')
if mono_font == nil then
    mono_font = 'NovaMono.ttf'
end

local SERVER = game.GetSkinSetting("multi.server")





mouse_clipped = function(x,y,w,h)
    return mposx > x and mposy > y and mposx < x+w and mposy < y+h;
end;

draw_room = function(name, x, y, selected, hoverindex)
    local buttonWidth = resX*(3/4);
    local rx = x - (buttonWidth / 2);
    local ty = y - (buttonHeight / 2);
    local roomButtonBorder = buttonBorder;
    gfx.BeginPath();
    gfx.FillColor(0,128,255);
    if selected then
       gfx.FillColor(0,255,0);
       roomButtonBorder = 4;
    end
    if mouse_clipped(rx,ty, buttonWidth, buttonHeight) then
       hovered = hoverindex;
       gfx.FillColor(255,128,0);
    end
    gfx.Rect(rx - roomButtonBorder,
        ty - roomButtonBorder,
        buttonWidth + (roomButtonBorder * 2),
        buttonHeight + (roomButtonBorder * 2));
    gfx.Fill();
    gfx.BeginPath();
    gfx.FillColor(40,40,40);
    gfx.Rect(rx, ty, buttonWidth, buttonHeight);
    gfx.Fill();
    gfx.BeginPath();
    gfx.FillColor(255,255,255);
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE);
    gfx.FontSize(40);
    gfx.Text(name, x, y);
end;

draw_button = function(name, x, y, buttonWidth, hoverindex)
    draw_button_color(name, x, y, buttonWidth, hoverindex, 40,40,40, 0,128,255)
end

draw_button_color = function(name, x, y, buttonWidth, hoverindex,r,g,b, olr,olg,olb)
    local rx = x - (buttonWidth / 2);
    local ty = y - (buttonHeight / 2);
    gfx.BeginPath();
    gfx.FillColor(olr, olg, olb);
    if mouse_clipped(rx,ty, buttonWidth, buttonHeight) then
       hovered = hoverindex;
       gfx.FillColor(255,128,0);
    end
    gfx.Rect(rx - buttonBorder,
        ty - buttonBorder,
        buttonWidth + (buttonBorder * 2),
        buttonHeight + (buttonBorder * 2));
    gfx.Fill();
    gfx.BeginPath();
    gfx.FillColor(r,g,b);
    gfx.Rect(rx, ty, buttonWidth, buttonHeight);
    gfx.Fill();
    gfx.BeginPath();
    gfx.FillColor(255,255,255);
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE);
    gfx.FontSize(40);
    gfx.Text(name, x, y);
end;

draw_checkbox = function(text, x, y, hoverindex, current, can_click)
    local rx = x - (buttonWidth / 2);
    local ty = y - (buttonHeight / 2);
    gfx.BeginPath();
    
    if can_click then
        gfx.FillColor(255,255,255);
    else
        gfx.FillColor(150,100,100);
    end
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE);
    gfx.FontSize(35);
    gfx.Text(text, x, y)

    local xmin,ymin,xmax,ymax = gfx.TextBounds(x, y, text);

    local sx = xmin - 40;
    local sy = y - 15;

    gfx.StrokeColor(0,128,255);
    if can_click and (mouse_clipped(sx, sy, 31, 30) or mouse_clipped(xmin-10, ymin, xmax-xmin, ymax-ymin)) then
        hovered = hoverindex;
        gfx.StrokeColor(255,128,0);
     end

    gfx.Rect(sx, y - 15, 30, 30)
    gfx.StrokeWidth(2)
    gfx.Stroke()

    if current then
        -- Draw checkmark
        gfx.BeginPath();
        gfx.MoveTo(sx+5, sy+10);
        gfx.LineTo(sx+15, y+5);
        gfx.LineTo(sx+35, y-15);
        gfx.StrokeWidth(5)
        gfx.StrokeColor(0,255,0);
        gfx.Stroke()

    end
end;

local userHeight = 100

draw_user = function(user, x, y, buttonWidth, rank)
    local buttonHeight = userHeight;
    local rx = x - (buttonWidth / 2);
    local ty = y - (buttonHeight / 2);
    gfx.BeginPath();
    gfx.FillColor(256,128,255);

    gfx.Rect(rx - buttonBorder,
        ty - buttonBorder,
        buttonWidth + (buttonBorder * 2),
        buttonHeight + (buttonBorder * 2));
    gfx.Fill();
    gfx.BeginPath();
    if host == user.id then
        gfx.FillColor(80,0,0);
    else
        gfx.FillColor(0,0,40);
    end
    gfx.Rect(rx, ty, buttonWidth, buttonHeight);
    gfx.Fill();
    gfx.BeginPath();
    gfx.FillColor(255,255,255);
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_MIDDLE);
    gfx.FontSize(40);
    local name = user.name;
    if user.id == user_id then
        name = name
    end
    if user.id == host then
        name = name..' (host)'
    elseif user.missing_map then
        name = name..' (NO CHART)'
    elseif user.ready then
        name = name..' (ready)'
    end
    if user.score ~= nil then
        name = '#'..rank..' '..name
    end
    first_y = y - 28
    second_y = y + 28
    gfx.Text(name, x - buttonWidth/2 + 5, first_y);
    if user.score ~= nil then
        gfx.FillColor(255,255,0)
        gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT + gfx.TEXT_ALIGN_MIDDLE);
        local combo_text = '  '..user.combo..'x'
        gfx.Text(combo_text, x+buttonWidth/2 - 5, second_y-5);
        local xmin,ymin,xmax,ymax = gfx.TextBounds(x+buttonWidth/2 - 5, second_y-5, combo_text);

        
        local score_text = '  '..string.format("%08d",user.score);
        gfx.FillColor(255,255,255)
        gfx.Text(score_text, xmin, second_y-5);
        xmin,ymin,xmax,ymax = gfx.TextBounds(xmin, second_y-5, score_text);

        if user.grade == nil then
            for i,v in ipairs(grades) do
                if v.max > user.score then
                    user.grade = v.image
                    break
                end
            end
        end
        if user.badge == nil and user.clear > 1 then
            user.badge = badges[user.clear];
        end

        gfx.BeginPath()
        local iw, ih = gfx.ImageSize(user.grade)
        local iar = iw/ih
        local grade_height = buttonHeight/2 - 10
        gfx.ImageRect(xmin - iar * grade_height, second_y - buttonHeight/4 , iar * grade_height, grade_height, user.grade, 1, 0)
    end
    if user.level ~= 0 then
        gfx.FillColor(255,255,0)
        gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_MIDDLE);
        local level_text = 'Lvl '..user.level..' '
        gfx.Text(level_text, x-buttonWidth/2 + 5, second_y-5)
        local xmin,ymin,xmax,ymax = gfx.TextBounds(x-buttonWidth/2 + 5, second_y-5, level_text);


        if user.badge then
            gfx.BeginPath() 
            local iw, ih = gfx.ImageSize(user.badge)
            local iar = iw/ih;
            local badge_height = buttonHeight/2 - 10
            gfx.ImageRect(xmax+5, second_y - buttonHeight/4 , iar * badge_height, badge_height, user.badge, 1, 0)


        end
    end
end;

function render_loading()
    if not loading then return end
    gfx.Save()
    gfx.ResetTransform()
    gfx.BeginPath()
    gfx.MoveTo(resX, resY)
    gfx.LineTo(resX - 350, resY)
    gfx.LineTo(resX - 300, resY - 50)
    gfx.LineTo(resX, resY - 50)
    gfx.ClosePath()
    gfx.FillColor(33,33,33)
    gfx.Fill()
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_RIGHT, gfx.TEXT_ALIGN_BOTTOM)
    gfx.FontSize(70)
    gfx.Text("LOADING...", resX - 20, resY - 3)
    gfx.Restore()
end

function render_info()
    gfx.Save()
    gfx.ResetTransform()
    gfx.BeginPath()
    gfx.MoveTo(0, resY)
    gfx.LineTo(550, resY)
    gfx.LineTo(500, resY - 65)
    gfx.LineTo(0, resY - 65)
    gfx.ClosePath()
    gfx.FillColor(33,33,33)
    gfx.Fill()
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_LEFT, gfx.TEXT_ALIGN_BOTTOM)
    gfx.FontSize(70)
    gfx.Text("Multiplayer", 3, resY - 15)
    local xmin,ymin,xmax,ymax = gfx.TextBounds(3, resY - 3, "Multiplayer")
    gfx.FontSize(20)
    gfx.Text(MULTIPLAYER_VERSION, xmax + 13, resY - 15)
    --gfx.Text('Server: '..'', xmax + 13, resY - 15)
    draw_button_color("Settings", 500-60, resY-25, 120, function()
        mpScreen.OpenSettings();
    end, 33,33,33, 33,33,33)
    gfx.Restore()

	if searchStatus then
		gfx.BeginPath()
		gfx.FillColor(255,255,255)
		gfx.FontSize(20);
		gfx.TextAlign(gfx.TEXT_ALIGN_LEFT + gfx.TEXT_ALIGN_TOP)
		gfx.Text(searchStatus, 3, 3)
    end
    



end

draw_diff_icon = function(diff, x, y, w, h, selected)
    local shrinkX = w/4
    local shrinkY = h/4
    if selected then
      gfx.FontSize(h/2)
      shrinkX = w/6
      shrinkY = h/6
    else
      gfx.FontSize(math.floor(h / 3))
    end
    gfx.BeginPath()
    gfx.RoundedRectVarying(x+shrinkX,y+shrinkY,w-shrinkX*2,h-shrinkY*2,0,0,0,0)
    gfx.FillColor(15,15,15)
    gfx.StrokeColor(table.unpack(diffColors[diff.difficulty + 1]))
    gfx.StrokeWidth(2)
    gfx.Fill()
    gfx.Stroke()
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_MIDDLE + gfx.TEXT_ALIGN_CENTER)
    gfx.FastText(tostring(diff.level), x+(w/2),y+(h/2))
end

local doffset = 0;
local timer = 0;

draw_cursor = function(x,y,rotation,width)
	gfx.Save()
    gfx.BeginPath();
    gfx.Translate(x,y)
    gfx.Rotate(rotation)
    gfx.StrokeColor(255,128,0)
    gfx.StrokeWidth(4)
    gfx.Rect(-width/2, -width/2, width, width)
    gfx.Stroke()
    gfx.Restore()
end

draw_diffs = function(diffs, x, y, w, h, selectedDiff)
    local diffWidth = w/2.5
    local diffHeight = w/2.5
    local diffCount = #diffs
    gfx.Scissor(x,y,w,h)
    for i = math.max(selectedDiff - 2, 1), math.max(selectedDiff - 1,1) do
      local diff = diffs[i]
      local xpos = x + ((w/2 - diffWidth/2) + (selectedDiff - i + doffset)*(-0.8*diffWidth))
      if  i ~= selectedDiff then
        draw_diff_icon(diff, xpos, y, diffWidth, diffHeight, false)
      end
    end

    --after selected
  for i = math.min(selectedDiff + 2, diffCount), selectedDiff + 1,-1 do
      local diff = diffs[i]
      local xpos = x + ((w/2 - diffWidth/2) + (selectedDiff - i + doffset)*(-0.8*diffWidth))
      if  i ~= selectedDiff then
        draw_diff_icon(diff, xpos, y, diffWidth, diffHeight, false)
      end
    end
    local diff = diffs[selectedDiff]
    local xpos = x + ((w/2 - diffWidth/2) + (doffset)*(-0.8*diffWidth))
  draw_diff_icon(diff, xpos, y, diffWidth, diffHeight, true)
  gfx.BeginPath()
  gfx.FillColor(0,128,255)
  gfx.Rect(x,y+10,2,diffHeight-h/6)
  gfx.Fill()
  gfx.BeginPath()
  gfx.Rect(x+w-2,y+10,2,diffHeight-h/6)
  gfx.Fill()
  gfx.ResetScissor()
  draw_cursor(x + w/2, y +diffHeight/2, timer * math.pi, diffHeight / 1.5)
end

set_diff = function(oldDiff, newDiff)
    game.PlaySample("click-02")
    doffset = doffset + oldDiff - newDiff
end;

local selected_room_index = 1;
local ioffset = 0;

function draw_rooms(y, h)
    if #rooms == 0 then
        return
    end
    local num_rooms_visible = math.floor(h / (buttonHeight + 10))

    local first_half_rooms = math.floor(num_rooms_visible/2)
    local second_half_rooms = math.ceil(num_rooms_visible/2) - 1

    local start_offset = math.max(selected_room_index - first_half_rooms, 1);
    local end_offset = math.min(selected_room_index + second_half_rooms + 2, #rooms);

    local start_index_offset = 1;

    -- If our selection is near the start or end we have to offset
    if selected_room_index <= first_half_rooms then
        start_index_offset = 0;
        end_offset = math.min(#rooms, num_rooms_visible + 1)
    end
    if selected_room_index >= #rooms - second_half_rooms then
        start_offset = math.max(1, #rooms - num_rooms_visible)
        end_offset = #rooms
    end

    for i = start_offset, end_offset do
        local room = rooms[i];
        -- if selected room < halfvis then we start at 1
        -- if sel > #rooms - halfvis then we start at -halfvis
        local offset_index = (start_offset + first_half_rooms) - i + start_index_offset

        local offsetY = (offset_index + ioffset) * (buttonHeight + 10);
        local ypos = y + (h/2) - offsetY;
        local status = room.current..'/'..room.max
        if room.ingame then
            status = status..' (In Game)'
        end
        if room.password then
            status = status..' <P>'
        end
        draw_room(room.name .. ':  '.. status, resX / 2, ypos, i == selected_room_index, function()
            join_room(room)
        end)
    end
end

change_selected_room = function(off)

    local new_index = selected_room_index + off;
    --selected_room_index = 2;
    if new_index < 1 or new_index > #rooms then
        return;
    end

    local h = resY - 290;

    local num_rooms_visible = math.floor(h / (buttonHeight + 10))

    local first_half_rooms = math.floor(num_rooms_visible/2)
    local second_half_rooms = math.ceil(num_rooms_visible/2) - 1

    if off > 0 and (selected_room_index < first_half_rooms or selected_room_index >= #rooms - second_half_rooms - 1) then
    elseif off < 0 and (selected_room_index <= first_half_rooms or selected_room_index >= #rooms - second_half_rooms) then 
    else
        ioffset = ioffset - new_index + selected_room_index;
    end

    game.PlaySample("menu_click")

    selected_room_index = new_index;
end

function render_lobby(deltaTime)

    local jacket_size;

    -- split is how the screen is split or not
    if portrait then
        split = resX
        jacket_size = math.min(resX/3, resY/3);
        user_y_off = 375+jacket_size + 70
        song_x_off = 0;
    else
        split = resX/2
        jacket_size = math.min(resX/2, resY/2);
        user_y_off = 0
        song_x_off = 1/2*resX;
    end

    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER, gfx.TEXT_ALIGN_BOTTOM)
    gfx.FontSize(70)
    gfx.Text(selected_room.name, resX/2, 50)

    -- === Users ===

    gfx.Text("Users", split/2, user_y_off+100)

    buttonY = user_y_off + 125 + userHeight/2
    for i, user in ipairs(lobby_users) do
        draw_user(user, split/2, buttonY, split*3/4, i)
        local side_button_off = 0
        if owner == user_id and user.id ~= user_id then
            draw_button("K",split/2 + split*3/8+10+25, buttonY, 50, function()
                kick_user(user);
            end)
            side_button_off = 60;
        end
        if (owner == user_id or host == user_id) and user.id ~= host then
            draw_button("H",split/2 + split*3/8+10+25+side_button_off, buttonY, 50, function()
                change_host(user);
            end)
        end
        buttonY = buttonY + userHeight
    end
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_MIDDLE);
    gfx.FillColor(255,255,255)


    -- === song select ===

    gfx.FontSize(60)
    gfx.Text("Selected Song:", split/2 + song_x_off, 100)
    gfx.FontSize(40)
    if selected_song == nil then
        if host == user_id then
            gfx.Text("Select song:", split/2 + song_x_off, 175)
        else
            if missing_song then
                gfx.Text("Missing song!!!!", split/2 + song_x_off, 175)
            else
                gfx.Text("Host is selecting song", split/2 + song_x_off, 175)
            end
        end
        if jacket == 0 then
            jacket = placeholderJacket
        end
    else
        gfx.Text(selected_song.title, split/2 + song_x_off, 175)
        draw_diffs(selected_song.all_difficulties, split/2 + song_x_off - 150, 200, 300, 100, selected_song.diff_index+1)
        
        if selected_song.jacket == nil or selected_song.jacket == placeholderJacket then
            selected_song.jacket = gfx.LoadImageJob(selected_song.jacketPath, placeholderJacket)
            jacket = selected_song.jacket
        end
    end
    gfx.Save()
    gfx.BeginPath()
    gfx.Translate(split/2 + song_x_off, 325+jacket_size/2)
    gfx.ImageRect(-jacket_size/2,-jacket_size/2,jacket_size,jacket_size,jacket,1,0)
    
    if mouse_clipped(split/2 + song_x_off-jacket_size/2, 325, jacket_size,jacket_size) and host == user_id then
        hovered = function() 
            missing_song = false
            mpScreen.SelectSong()
        end
    end
    gfx.Restore()
    if start_game_soon then
        draw_button("Game starting...", split/2 + song_x_off, 375+jacket_size, 600, function() end);
    else
        if host == user_id then
            if selected_song == nil or not selected_song.self_picked then
                draw_button_color("Select song", split/2 + song_x_off, 375+jacket_size, 600, function() 
                    missing_song = false
                    mpScreen.SelectSong()
                end, 0, math.min(255, 128 + math.floor(32 * math.cos(timer * math.pi))), 0, 0,128,255);
            elseif user_ready and all_ready then
                draw_button("Start game", split/2 + song_x_off, 375+jacket_size, 600, start_game)
            elseif user_ready and not all_ready then
                draw_button("Waiting for others", split/2 + song_x_off, 375+jacket_size, 600, function() 
                    missing_song = false
                    mpScreen.SelectSong()
                end)
            else
                draw_button("Ready", split/2 + song_x_off, 375+jacket_size, 600, ready_up);
            end
        elseif host == nil then
            draw_button("Waiting for game to end", split/2 + song_x_off, 375+jacket_size, 600, function() end);
        elseif missing_song then
            draw_button("Missing Song!", split/2 + song_x_off, 375+jacket_size, 600, function() end);
        elseif selected_song ~= nil then
            if user_ready then
                draw_button("Cancel", split/2 + song_x_off, 375+jacket_size, 600, ready_up);
            else
                draw_button("Ready", split/2 + song_x_off, 375+jacket_size, 600, ready_up);
            end
        else
            draw_button("Waiting for host", split/2 + song_x_off, 375+jacket_size, 600, function() end);
        end
    end

    draw_checkbox("Excessive", split/2 + song_x_off - 150, 375+jacket_size + 70, toggle_hard, hard_mode, not start_game_soon)
    draw_checkbox("Mirror", split/2 + song_x_off, 375+jacket_size + 70, toggle_mirror, mirror_mode, not start_game_soon)
    
    draw_checkbox("Rotate Host", split/2 + song_x_off + 150 + 20, 375+jacket_size + 70, toggle_rotate, do_rotate,
                    (owner == user_id or host == user_id) and not start_game_soon)

    if selected_song ~= nil then
        gfx.FillColor(255,255,255)
        gfx.FontSize(20);
        gfx.TextAlign(gfx.TEXT_ALIGN_CENTER + gfx.TEXT_ALIGN_TOP)
        if selected_song.min_bpm ~= selected_song.max_bpm then
            gfx.Text(string.format("BPM: %.0f-%.0f, Start BPM: %.0f, Hispeed: %.0f x %.1f = %.0f",
                selected_song.min_bpm, selected_song.max_bpm, selected_song.start_bpm,
                selected_song.speed_bpm, selected_song.hispeed, selected_song.speed_bpm * selected_song.hispeed),
                split/2 + song_x_off, 375+jacket_size + 70 + 30)
        else
            gfx.Text(string.format("BPM: %.0f, Hispeed: %.0f x %.1f = %.0f",
                selected_song.min_bpm,
                selected_song.speed_bpm, selected_song.hispeed, selected_song.speed_bpm * selected_song.hispeed),
                split/2 + song_x_off, 375+jacket_size + 70 + 30)
        end
    end
end

function render_room_list(deltaTime)
    draw_rooms(175, resY - 290);

    -- Draw cover for rooms out of view
    gfx.BeginPath()
    gfx.FillColor(20, 20, 20)
    gfx.Rect(0, 0, resX, 145)
    gfx.Rect(0, resY-170, resX, 170)
    gfx.Fill()
    
    gfx.BeginPath()
    gfx.FillColor(60, 60, 60)
    gfx.Rect(0, 145, resX, 2)
    gfx.Rect(0, resY-170-2, resX, 2)
    gfx.Fill()

    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER, gfx.TEXT_ALIGN_BOTTOM)
    gfx.FontSize(70)
    gfx.Text("Multiplayer Rooms", resX/2, 100)


    if not loading then
        draw_button("Create new room", resX/2, resY-40-buttonHeight, resX*(3/4), new_room);
    end
end


passwordErrorOffset = 0;
function render_password_screen(deltaTime)
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER, gfx.TEXT_ALIGN_BOTTOM)
    gfx.FontSize(70)
    gfx.Text("Joining "..selected_room.name.."...", resX/2, resY/4)

    gfx.FillColor(50,50,50)
    gfx.BeginPath() 
    gfx.Rect(0, resY/2-10, resX, 40)
    gfx.Fill(); 

    gfx.FillColor(255,255,255)
    gfx.Text("Please enter room password:", resX/2, resY/2-40)
    gfx.Text(string.rep("*",#textInput.text), resX/2, resY/2+40) 
    if passwordError then
        
        gfx.FillColor(255,50,50)
        gfx.FontSize(60 + math.floor(passwordErrorOffset*20))
        gfx.Text("Invalid password", resX/2, resY/2+80) 
    end
    draw_button("Join", resX/2, resY*3/4, resX/2,  mpScreen.JoinWithPassword);
end

function render_new_room_password(delta_time)
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER, gfx.TEXT_ALIGN_BOTTOM)
    gfx.FontSize(70)
    gfx.Text("Create New Room", resX/2, resY/4)

    gfx.FillColor(50,50,50)
    gfx.BeginPath() 
    gfx.Rect(0, resY/2-10, resX, 40)
    gfx.Fill(); 

    gfx.FillColor(255,255,255)
    gfx.Text("Enter room password:", resX/2, resY/2-40)
    gfx.Text(string.rep("*",#textInput.text), resX/2, resY/2+40) 
    draw_button("Create Room", resX/2, resY*3/4, resX/2, mpScreen.NewRoomStep);
end

function render_new_room_name(deltaTime)
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER, gfx.TEXT_ALIGN_BOTTOM)
    gfx.FontSize(70)
    gfx.Text("Create New Room", resX/2, resY/4)

    gfx.FillColor(50,50,50)
    gfx.BeginPath() 
    gfx.Rect(0, resY/2-10, resX, 60)
    gfx.Fill(); 

    gfx.FillColor(255,255,255)
    gfx.Text("Please enter room name:", resX/2, resY/2-40)
    gfx.Text(textInput.text, resX/2, resY/2+40) 
    draw_button("Next", resX/2, resY*3/4, resX/2, mpScreen.NewRoomStep);
end

function render_set_username(deltaTime)
    gfx.FillColor(255,255,255)
    gfx.TextAlign(gfx.TEXT_ALIGN_CENTER, gfx.TEXT_ALIGN_BOTTOM)
    gfx.FontSize(70)
    gfx.Text("First things first...", resX/2, resY/4)

    gfx.FillColor(50,50,50)
    gfx.BeginPath() 
    gfx.Rect(0, resY/2-10, resX, 60)
    gfx.Fill(); 

    gfx.FillColor(255,255,255)
    gfx.Text("Enter a display name:", resX/2, resY/2-40)
    gfx.Text(textInput.text, resX/2, resY/2+40) 
    draw_button("Join Multiplayer", resX/2, resY*3/4, resX/2, function()
        loading = true;
        mpScreen.SaveUsername()
    end);

end

render = function(deltaTime)
    resX,resY = game.GetResolution();
	buttonWidth = resX*(3/4);
    mposx,mposy = game.GetMousePos();
    portrait = resY > resX


    doffset = doffset * 0.9
    ioffset = ioffset * 0.9
    passwordErrorOffset = passwordErrorOffset * 0.9
    timer = (timer + deltaTime)
    timer = timer % 2
    
    hovered = nil;

    gfx.LoadSkinFont(normal_font);

    do_sounds(deltaTime);

    -- Room Listing View
    if screenState == "inRoom" then
        render_lobby(deltaTime);
    elseif screenState == "roomList" then
        render_room_list(deltaTime);
    elseif screenState == "passwordScreen" then
        render_password_screen(deltaTime);
    elseif screenState == "newRoomName" then
        render_new_room_name()
    elseif screenState == "newRoomPassword" then
        render_new_room_password()
    elseif screenState == "setUsername" then
        loading = false;
        render_set_username()
    end
    render_loading();
    render_info();

end

-- Ready up to play
function ready_up()
    Tcp.SendLine(json.encode({topic="user.ready.toggle"}))
end

-- Toggle hard gauage
function toggle_hard()
    Tcp.SendLine(json.encode({topic="user.hard.toggle"}))
end

-- Toggle hard gauage
function toggle_mirror()
    Tcp.SendLine(json.encode({topic="user.mirror.toggle"}))
end

function new_room()
    host = user_id
    owner = user_id
    mpScreen.NewRoomStep()
end

-- Toggle host rotation
function toggle_rotate()
    Tcp.SendLine(json.encode({topic="room.option.rotation.toggle"}))
end

-- Change lobby host
function change_host(user)
    Tcp.SendLine(json.encode({topic="room.host.set", host=user.id}))
end

-- Kick user
function kick_user(user)
    Tcp.SendLine(json.encode({topic="room.kick", id=user.id}))
end

-- Tell the server to start the game
function start_game()
    selected_song.self_picked = false
    if (selected_song == nil) then
        return
    end
    if (start_game_soon) then
        return
    end
    
    Tcp.SendLine(json.encode({topic="room.game.start"}))
end

-- Join a given room
function join_room(room)
    host = user_id
    selected_room = room;
    if room.password then
        mpScreen.JoinWithPassword(room.id)
    else
        mpScreen.JoinWithoutPassword(room.id)
    end
end

-- Handle button presses to advance the UI
button_released = function(button)
    if button == game.BUTTON_STA then
        if start_game_soon then
            return
        end
        if screenState == "roomList" then
            if #rooms == 0 then
                new_room()
            else
                -- TODO navigate room selection
                join_room(rooms[selected_room_index]) 
            end
        elseif screenState == "inRoom" then
            if host == user_id then
                if selected_song and selected_song.self_picked then
                    if all_ready then
                        start_game()
                    else
                        missing_song = false
                        mpScreen.SelectSong()
                    end
                else
                    missing_song = false
                    mpScreen.SelectSong()
                end
            else
                ready_up()
            end
        end
    end
    if button == game.BUTTON_FXL then
        toggle_hard();
    end
    if button == game.BUTTON_FXR then
        toggle_mirror();
    end
end

-- Handle the escape key around the UI
function key_pressed(key)
    if key == 27 then --escape pressed
        if screenState == "roomList" then
            did_exit = true;
            mpScreen.Exit();
            return
        end

        -- Reset room data
        screenState = "roomList" -- have to update here
        selected_room = nil;
        rooms = {};
        selected_song = nil
        selected_song_index = 1;
        jacket = 0;
    end

end

-- Handle mouse clicks in the UI
mouse_pressed = function(button)
    if hovered then
        hovered()
    end
    return 0
end

function init_tcp()
Tcp.SetTopicHandler("server.info", function(data)
    loading = false
    user_id = data.userid
end)
-- Update the list of rooms as well as get user_id for the client
Tcp.SetTopicHandler("server.rooms", function(data)

    rooms = {}
    for i, room in ipairs(data.rooms) do
        table.insert(rooms, room)
    end
end)

Tcp.SetTopicHandler("server.room.joined", function(data)
    selected_room = data.room
end)

local sound_time = 0;
local sound_clip = nil;
local sounds_left = 0;
local sound_interval = 0;

function repeat_sound(clip, times, interval)
    sound_clip = clip;
    sound_time = 0;
    sounds_left = times - 1;
    sound_interval = interval;
    game.PlaySample(clip)
end

function do_sounds(deltaTime)
    if sound_clip == nil then
        return
    end

    sound_time = sound_time + deltaTime;
    if sound_time > sound_interval then
        sound_time = sound_time - sound_interval;
        game.PlaySample(sound_clip);
        sounds_left = sounds_left - 1
        if sounds_left <= 0 then
            sound_clip = nil
        end
    end
end

local last_song = nil

-- Update the current lobby
Tcp.SetTopicHandler("room.update", function(data)
    -- Update the users in the lobby
    lobby_users = {}
    local prev_all_ready = all_ready;
    all_ready = true
    for i, user in ipairs(data.users) do
        table.insert(lobby_users, user)
        if user.id == user_id then
            user_ready = user.ready
        end
        if not user.ready then
            all_ready = false
        end
    end

    if user_id == host and #data.users > 1 and all_ready and not prev_all_ready then
        repeat_sound("click-02", 3, .1)
    end

    if data.host == user_id and host ~= user_id then
        repeat_sound("click-02", 3, .1)
    end

    if data.song ~=nil and last_song ~=data.song then
        game.PlaySample("menu_click")
        last_song = data.song
    end
    host = data.host
    if data.owner then
        owner = data.owner
    else
        owner = host
    end
    hard_mode = data.hard_mode
    mirror_mode = data.mirror_mode
    do_rotate = data.do_rotate
    if data.start_soon and not start_game_soon then
        repeat_sound("click-01", 5, 1)
    end
    start_game_soon = data.start_soon

end)
end
