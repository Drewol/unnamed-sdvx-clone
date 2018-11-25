
local ini = { }

local function CheckFileExists(fileName)
   local f = io.open(fileName, "r")
   if f ~= nil then io.close(f) return true else return false end
end

local function GetIniPath(fileName) return "skins/" .. game.GetSkin() .. "/" .. fileName .. ".ini" end

function ini.Load(fileName)
   local path = GetIniPath(fileName)

   if not CheckFileExists(path) then
      return { }
   end

   local result = { }
   local section = result

   for line in io.lines(path) do
      line = line:trim()
      if #line ~= 0 then
         if line:sub(1, 1) == '[' then
            if line:sub(-1, -1) == ']' then
               -- section header
               section = { }
               result[line:sub(2, -2)] = section
            else
               -- TODO(local): complain of malformed section header I guess
            end
         else
            local index = line:find('=')
            if index ~= nil then
               -- section entry
               local key = line:sub(1, index - 1)
               local value = line:sub(index + 1)
               section[key] = value
            else
               -- TODO(local): complain of malformed line or something I dunno
            end
         end
      end
   end

   return result
end

function ini.Save(fileName, table)
   local path = GetIniPath(fileName)

   local res = ''
   local sectionsLeft = { }

   for k, v in next, table do
      if type(v) == "table" then
         sectionsLeft[k] = v
      else
         res = res .. tostring(k) .. "=" .. tostring(v) .. "\n"
      end
   end

   for header, section in next, sectionsLeft do
      res = res .. "[" .. header .. "]" .. "\n"
      for k, v in next, section do
         res = res .. tostring(k) .. "=" .. tostring(v) .. "\n"
      end
   end

   local file = io.open(path, "w")
   file:write(res)
   file:close()
end

return ini
