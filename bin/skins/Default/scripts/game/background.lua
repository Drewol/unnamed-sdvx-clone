-- Names in ksh: cloudy, cyber, deepsea, desert, fantasy, flame, grass, mars, night, ocean, sky, space, sunset
function get_bg_file(bg_name)
  if bg_name == "flame" then
    return "backgrounds/bg2.png", "background.fs"
  end
end

-- Names in ksh: arrow, sakura, smoke, snow, techno, wave
function get_fg_file(layer_name)
  if layer_name == "techno" then
    return "backgrounds/smoke.gif", "foreground.fs"
  end
end
