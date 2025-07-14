-- init.lua
--
-- Loaded by the C function dinklua_initialize(). Expected to contain the
-- tables "script" and "routine", and the functions "resume_script",
-- "run_script", "proc_exists" and "load_script". For more details on their use
-- and expected functionality and parameters, see the file "dinklua.c".
--
-- Be careful when editing this file. Mistakes in this file are hard to catch,
-- because the Dink Engine often has trouble reporting where the problem lies.
-- Make small, incremental edits and run Dink often in order to catch mistakes
-- early. Lots of productive time has been lost simply due to a forgotten
-- 'then' in an if statement...
--
-- The tables "object", "yield", "choice_menu" and "dink" are set in the file
-- "dinklua_bindings.c", in the method named "dinklua_bind_init", and they
-- contain pointers to the C functions that are used to communicate with the
-- Dink Engine from Lua.
--
-- "object" functions are meant to be used by objects, such as the "sprite"
-- object or the "soundbank" object. These are not made directly available
-- to the user, but accessible through said objects. These functions typically
-- have very long and descriptive names, and may also contain the word "yield",
-- in which case they are yielding functions (described in more detail below),
-- such as the function "yield_sprite_object_say_stop_npc". As they are neatly
-- wrapped in objects, the final appearance to the scripter would simply be
-- "sprite_object:say_stop_npc(...)"
--
-- "yield" functions are functions that require the script to yield (i.e. return
-- control back to the Dink Engine) after they have run. This is accomplished in
-- Lua using the coroutine.yield() functionality. A typical example of a
-- yielding function is the "wait" function.
--
-- "dink" functions are functions that are meant to be used more or less
-- directly by scripters. These are made available to scripters as the "dink"
-- table in script environments. A few of these are modified before being handed
-- over to the scripters, and many of the "yield" functions get placed in here
-- as well, albeit with the coroutine.yield() wrapper.
--
-- "choice_menu" is a special set of functions meant to be used by the choice
-- menu object. Because the way the choice menu works in DinkC is so odd,
-- its implementation had to differ quite substantially in Lua.
--
-- Most code in this file has comments to help you figure out what it does, so
-- for a more detailed explanation, read those.

-- bundled libraries
-- https://github.com/pkulchenko/serpent
local serpent = require("serpent")
-- https://github.com/WetDesertRock/vivid
local vivid = require "vivid"
-- https://github.com/rxi/lume
local lume = require "lume"
-- https://github.com/automattf/vector.lua
local vector = require "vector"
-- https://github.com/jagt/pprint.lua
pprint = require "pprint"

-- Helper functions
local function decode_sprite(sprite, custom_error)
  local sprite_number
  if sprite == nil then
    sprite_number = 0
  elseif type(sprite) == "number" then
    sprite_number = sprite
  elseif debug.getmetatable(sprite) == sprite_metatable then
    sprite_number = sprite.sprite_number
  else
    if custom_error == nil then
      error("sprite parameter must be nil, a sprite number or a sprite object", 3)
    else
      error(custom_error, 3)
    end
  end
  return sprite_number
end

-- Because Lua 5.2 got rid of getfenv we need to make our own
-- Taken from <http://lua-users.org/lists/lua-l/2010-06/msg00313.html>
local function getfenv(f)
  f = (type(f) == 'function' and f or debug.getinfo(f + 1, 'f').func)
  local name, val
  local up = 0
  repeat
      up = up + 1
      name, val = debug.getupvalue(f, up)
  until name == '_ENV' or name == nil
  return val
end

-- Sprite metatable: Handles sprite property assignment/reading
sprite_metatable =
{
  __newindex = function(sprite, key, value)

    -- Properties specific to Dink
    if sprite.sprite_number == 1 then
      if key == "can_walk_off_screen" then
        object.dink_object_can_walk_off_screen(value)
        return
      elseif key == "base_push" then
        object.set_dink_base_push(value)
        return
      end
    end

    -- Properties that take sprite objects as a value
    if key == "target" or key == "follow" then
      value = decode_sprite(value, "target value must be a sprite number or a sprite object")
    end

    local ok, err = pcall(object.set_sprite_value, sprite.sprite_number, key, value)
    if not ok then error(err, 2) end
  end,
  __index = function(sprite, key)

    -- Return methods and properties specific to Dink
    if sprite.sprite_number == 1 then
      if key == "set_speed" then
        return function(self, speed)
          object.dink_object_set_speed(speed)
        end
      elseif key == "get_speed" then
        local ok, value = pcall(object.dink_object_get_speed)
        return value
      end
    end

    -- Return methods specific to sprites
    if key == "kill_wait" then
      return function(self)
        local ok, err = pcall(object.sprite_object_kill_wait, self.sprite_number)
        if not ok then error(err, 2) end
      end
    elseif key == "freeze" then
      return function(self)
        local ok, err = pcall(object.sprite_object_freeze, self.sprite_number)
        if not ok then error(err, 2) end
      end
    elseif key == "unfreeze" then
      return function(self)
        local ok, err = pcall(object.sprite_object_unfreeze, self.sprite_number)
        if not ok then error(err, 2) end
      end
    elseif key == "say" then
      return function(self, text)
        local ok, err = pcall(object.sprite_object_say, text, self.sprite_number)
        if not ok then error(err, 2) end
      end
    elseif key == "say_stop" then
      return function(self, text)
        local ok, err = pcall(object.yield_sprite_object_say_stop, text, self.sprite_number)
        if not ok then error(err, 2) end
        coroutine.yield()
      end
    elseif key == "say_stop_npc" then
      return function(self, text)
        local ok, err = pcall(object.yield_sprite_object_say_stop_npc, text, self.sprite_number)
        if not ok then error(err, 2) end
        coroutine.yield()
      end
    elseif key == "move" then
      return function(self, direction, destination, ignore_hardness)
        local ok, err = pcall(object.sprite_object_move, self.sprite_number, direction, destination, ignore_hardness)
        if not ok then error(err, 2) end
      end
    elseif key == "move_stop" then
      return function(self, direction, destination, ignore_hardness)
        local ok, err = pcall(object.yield_sprite_object_move_stop, self.sprite_number, direction, destination, ignore_hardness)
        if not ok then error(err, 2) end
        coroutine.yield()
      end
    elseif key == "draw_hard" then
      return function(self)
        local ok, err = pcall(object.sprite_object_draw_hard_sprite, self.sprite_number)
        if not ok then error(err, 2) end
      end
    elseif key == "kill_shadow" then
      return function(self)
        local ok, err = pcall(object.sprite_object_kill_shadow, self.sprite_number)
        if not ok then error(err, 2) end
      end
    elseif key == "hurt" then
      return function(self, damage)
        local ok, err = pcall(object.sprite_object_hurt, self.sprite_number, damage)
        if not ok then error(err, 2) end
      end
    elseif key == "compare_script" then
      return function(self, script)
        local ok, err = pcall(object.compare_sprite_script, self.sprite_number, script)
        if not ok then error(err, 2) end
      end
    end

    -- Special properties
    if key == "editor_sprite" then
      local editor_num = object.get_sprite_value(sprite.sprite_number, "editor_num")
      if editor_num == 0 then
        return nil
      else
        return dink.get_editor_sprite(editor_num)
      end
    elseif key == "busy" then
      local text_sprite_num = object.sprite_object_busy(sprite.sprite_number)
      if text_sprite_num == 0 then
        return nil
      else
        return dink.get_sprite(text_sprite_num)
      end
    elseif key == "script_attached" then
      return dink.is_script_attached(sprite.sprite_number)
    end

    local ok, value = pcall(object.get_sprite_value, sprite.sprite_number, key)
    if not ok then error(value, 2) end

    -- Properties that return sprite objects
    if key == "target" then
      if value > 0 then
        value = dink.get_sprite(value)
      else
        value = nil
      end
    end

    return value
  end,
  __metatable = false
}

-- Sprite Custom Metatable: Makes it so you can use the sp_custom feature in a
-- more natural way in Lua: sprite.custom["whatnot"] = 5, or alternatively,
-- sprite.custom.whatnot = 5
sprite_custom_metatable =
{
  -- Warning: Only "sprite_number" property available on this sprite object
  __newindex = function(sprite, key, value)
    object.sp_custom(sprite.sprite_number, key, value)
  end,
  -- Warning: Only "sprite_number" property available on this sprite object
  __index = function(sprite, key)
    return object.sp_custom(sprite.sprite_number, key, -1)
  end,
  __metatable = false
}

--[[

Called with the desired sprite number, this function returns a
lua table with special metatable values that effectively creates
"properties" on the sprite objects, in such a way that you can do
things like:

some_sprite = dink.get_sprite(15)
some_sprite.strength = 10

Generally, though, you'll probably not want to use this function.
The variable "current_sprite" is always available to every script,
as is "player" (sprite #1), so any other need will probably want to
use another mechanism than a hardcoded number to share sprites.

]]--
function dink.get_sprite(sprite_number)
  return setmetatable({
    sprite_number = sprite_number,
    custom = setmetatable({sprite_number = sprite_number}, sprite_custom_metatable)
  }, sprite_metatable)
end

-- Editor sprite metatable: Handles editor sprite property assignment/reading
editor_sprite_metatable =
{
  __newindex = function(editor_sprite, key, value)
    local ok, err = pcall(object.set_editor_value, editor_sprite.editor_number, key, value)

    -- error level 2 means that the error will be signaled on 2nd stack frame,
    -- that is the line at which the key has been set, rather than from here
    if not ok then error(err, 2) end
  end,
  __index = function(editor_sprite, key)

    -- Special properties
    if key == "sprite" then
      local sprite_num = object.get_editor_value(editor_sprite.editor_number, "sprite")
      if sprite_num == 0 then
        return nil
      else
        return dink.get_sprite(sprite_num)
      end
    end

    local ok, value = pcall(object.get_editor_value, editor_sprite.editor_number, key)
    if not ok then error(value, 2) end
    return value
  end,
  __metatable = false
}

--[[

Called with the desired editor number, this function returns a
lua table with special metatable values that effectively creates
"properties" on the editor sprite objects, in such a way that you can do
things like:

local hold = current_sprite.editor_sprite
hold.editor_type = editor_type.KILL_COMPLETELY

Generally, though, you'll probably not want to use this function.
Sprite objects have a property editor_sprite that will give you the proper
editor sprite for the sprite.

]]--
function dink.get_editor_sprite(editor_number)
  return setmetatable({editor_number = editor_number}, editor_sprite_metatable)
end

-- Sound metatable: Handles soundbank property assignment/reading
soundbank_metatable =
{
  __newindex = function(t, key, value)
    local ok, err = pcall(object.set_soundbank_value, t.soundbank_number, key, value)
    if not ok then error(err, 2) end
  end,
  __index = function(t, key)
    -- Return methods specific to soundbanks
    if key == "kill" then
      return function(self)
        local ok, err = pcall(object.soundbank_object_set_kill, self.soundbank_number)
        if not ok then error(err, 2) end
      end
    elseif key == "fade_vol" then
      return function(self, vol, time)
        local ok, err = pcall(object.soundbank_object_fade_vol, self.soundbank_number, vol, time)
        if not ok then error(err, 2) end
      end
    elseif key == "fade_pan" then
      return function(self, pan, time)
        local ok, err = pcall(object.soundbank_object_fade_pan, self.soundbank_number, pan, time)
        if not ok then error(err, 2) end
      end
    elseif key == "fade_speed" then
      return function(self, speed, time)
        local ok, err = pcall(object.soundbank_object_fade_speed, self.soundbank_number, speed, time)
        if not ok then error(err, 2) end
      end
    elseif key == "schedule_pause" then
      return function(self, time)
        local ok, err = pcall(object.soundbank_object_schedule_pause, self.soundbank_number, time)
        if not ok then error(err, 2) end
      end
    elseif key == "schedule_stop" then
      return function(self, time)
        local ok, err = pcall(object.soundbank_object_schedule_stop, self.soundbank_number, time)
        if not ok then error(err, 2) end
      end
    elseif key == "oscillate_pan" then
      return function(self, from, to, time)
        local ok, err = pcall(object.soundbank_object_oscillate_pan, self.soundbank_number, from, to, time)
        if not ok then error(err, 2) end
      end
    elseif key == "oscillate_vol" then
      return function(self, from, to, time)
        local ok, err = pcall(object.soundbank_object_oscillate_vol, self.soundbank_number, from, to, time)
        if not ok then error(err, 2) end
      end
    elseif key == "oscillate_speed" then
      return function(self, from, to, time)
        local ok, err = pcall(object.soundbank_object_oscillate_speed, self.soundbank_number, from, to, time)
        if not ok then error(err, 2) end
      end
    end
    -- get the parameter asked for
    local ok, value = pcall(object.get_soundbank_value, t.soundbank_number, key)
    if not ok then error(value, 2) end
    return value
  end,
  __metatable = false
}

--[[

Called with the desired sound bank number, this function returns a
lua table with special metatable values that effectively creates
"properties" on the sound bank objects, in such a way that you can do
things like:

sound = dink.playsound(123, 22050, 0, some_sprite_object, false)
sound.vol = -1500

]]--
function get_soundbank(soundbank_number)
  return setmetatable({soundbank_number = soundbank_number}, soundbank_metatable)
end

-- Global metatable: Handles returning/setting global DinkC variables
global_metatable = {
  __newindex = function(t, key, value)
    local ok, err = pcall(object.set_global_variable_value, key, value)
    if not ok then error(err, 2) end
  end,
  __index = function(t, key)
    if key == "create" then
      return object.make_global_int
    else
      local ok, value = pcall(object.get_global_variable_value, key)
      if not ok then error(value, 2) end
      return value
    end
  end,
  __metatable = false
}

-- Wrappers around DinkC functions that return or accept sprite numbers
-- so that they instead return or accept lua sprite objects

function dink.create_sprite(x, y, brain, sequence, frame)
  local ok, result = pcall(object.create_sprite, x, y, brain, sequence, frame)
  if not ok then error(result, 2) end
  local sprite_number = result
  if sprite_number == 0 then
    return nil
  else
    return dink.get_sprite(sprite_number)
  end
end

function dink.get_sprite_with_this_brain(brain, active_sprite_ignore)
  local active_sprite_number = decode_sprite(active_sprite_ignore)
  local ok, result = pcall(object.get_sprite_with_this_brain, brain, active_sprite_number)
  if not ok then error(result, 2) end
  local sprite_number = result
  if sprite_number == 0 then
    return nil
  else
    return dink.get_sprite(sprite_number)
  end
end

function dink.get_rand_sprite_with_this_brain(brain, active_sprite_ignore)
  local active_sprite_number = decode_sprite(active_sprite_ignore)
  local ok, result = pcall(object.get_rand_sprite_with_this_brain, brain, active_sprite_number)
  if not ok then error(result, 2) end
  local sprite_number = result
  if sprite_number == 0 then
    return nil
  else
    return dink.get_sprite(sprite_number)
  end
end

function dink.get_next_sprite_with_this_brain(brain, active_sprite_ignore, active_sprite_start_with)
  local active_sprite_number = decode_sprite(active_sprite_ignore)
  local active_sprite_start_number = decode_sprite(active_sprite_start_with)
  local ok, result = pcall(object.get_next_sprite_with_this_brain, brain, active_sprite_number, active_sprite_start_number)
  if not ok then error(result, 2) end
  local sprite_number = result
  if sprite_number == 0 then
    return nil
  else
    return dink.get_sprite(sprite_number)
  end
end

dink.say_xy = function(text, x, y)
  local ok, result = pcall(object.say_xy, text, x, y)
  if not ok then error(result, 2) end
  local sprite_number = result
  if sprite_number == 0 then
    return nil
  else
    return dink.get_sprite(sprite_number)
  end
end

local add_exp = dink.add_exp
dink.add_exp = function(experience, sprite)
  local sprite_number = decode_sprite(sprite)
  local ok, result = pcall(add_exp, experience, sprite_number)
  if not ok then error(result, 2) end
end

-- Wrappers around functions that yield

function dink.show_bmp(...)
  local ok, err = pcall(yield.show_bmp, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.wait_for_button(...)
  local ok, err = pcall(yield.wait_for_button, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.say_stop_xy(...)
  local ok, err = pcall(yield.say_stop_xy, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.kill_cur_item(...)
  local ok, err = pcall(yield.kill_cur_item, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.kill_cur_magic(...)
  local ok, err = pcall(yield.kill_cur_magic, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.restart_game(...)
  local ok, err = pcall(yield.restart_game, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.wait(...)
  local ok, err = pcall(yield.wait, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.activate_bow(...)
  local ok, err = pcall(yield.activate_bow, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.fade_up(...)
  local ok, err = pcall(yield.fade_up, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.fade_down(...)
  local ok, err = pcall(yield.fade_down, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.kill_game(...)
  local ok, err = pcall(yield.kill_game, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.kill_this_task(...)
  local ok, err = pcall(yield.kill_this_task, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

function dink.load_game(...)
  local ok, err = pcall(yield.load_game, ...)
  if not ok then error(err, 2) end
  coroutine.yield()
end

-- Functions that yield conditionally

function dink.draw_screen(...)
  local ok, result = pcall(yield.draw_screen, ...)
  if not ok then error(result, 2) end
  if result then
    coroutine.yield()
  end
end

-- Other wrappers

-- Adapt playsound to accept sprite objects as well as sprite numbers,
-- and to return a soundbank object rather than a soundbank number.
local playsound = dink.playsound
dink.playsound = function(sound_number, min_speed, rand_speed_to_add, sprite, repeat_p)
  -- everything except sound slot is optional
  local min_speed = min_speed or 0
  local rand_speed_to_add = rand_speed_to_add or 0
  local sprite = sprite or 0
  local repeat_p = repeat_p or false
  local sprite_number = decode_sprite(sprite) or 0
  local ok, result = pcall(playsound, sound_number, min_speed, rand_speed_to_add, sprite_number, repeat_p)
  if not ok then error(result, 2) end
  local soundbank_number = result
  return get_soundbank(soundbank_number)
end

-- playmidi with optional fade in timer
local playmidi = dink.playmidi
dink.playmidi = function(filename, fadein)
  local fadein = fadein or 0
  local ok, result = pcall(playmidi, filename, fadein)
  if not ok then error(result, 2) end
end

-- Adapt game_exist, load_game, save_game and set_button to accept choice menu
-- objects directly
local game_exist = dink.game_exist
dink.game_exist = function(choice_object)
  if type(choice_object) == "number" then
    return game_exist(choice_object)
  elseif debug.getmetatable(choice_object) == choice_metatable then
    return game_exist(choice_object._index)
  else
    error("game_exist takes a game number or a choice object", 2)
  end
end

local load_game = dink.load_game
dink.load_game = function(choice_object)
  if type(choice_object) == "number" then
    return load_game(choice_object)
  elseif debug.getmetatable(choice_object) == choice_metatable then
    return load_game(choice_object._index)
  else
    error("load_game takes a game number or a choice object", 2)
  end
end

local save_game = dink.save_game
dink.save_game = function(choice_object)
  if type(choice_object) == "number" then
    return save_game(choice_object)
  elseif debug.getmetatable(choice_object) == choice_metatable then
    return save_game(choice_object._index)
  else
    error("save_game takes a game number or a choice object", 2)
  end
end

local set_button = dink.set_button
dink.set_button = function(button_choice, action_choice)
  local button_index, action_index

  if type(button_choice) == "number" then
    button_index = button_choice
  elseif debug.getmetatable(button_choice) == choice_metatable then
    button_index = button_choice._index
  else
    error("set_button takes a button number or a choice object", 2)
  end

  if type(action_choice) == "number" then
    action_index = action_choice
  elseif debug.getmetatable(action_choice) == choice_metatable then
    action_index = action_choice._index
  else
    error("set_button takes an action number or a choice object", 2)
  end

  set_button(button_index, action_index)
end

-- Adapt script_attach and force_vision to update the current_sprite value
local script_attach = dink.script_attach
dink.script_attach = function(sprite_number)
  local ok, err = pcall(script_attach, sprite_number)
  if not ok then error(err, 2) end

  --[[
    Since the DinkLua scripts run in a different environment than we (the engine) do,
    we can't just access things like `current_sprite` directly here, but `getfenv(2)`
    gives us access to the environment we need to modify.
  ]]
  local env = getfenv(2)
  env.current_sprite = dink.get_sprite(sprite_number)
end

local force_vision = dink.force_vision
dink.force_vision = function(vision)
  local ok, err = pcall(force_vision, vision)
  if not ok then error(err, 2) end

  --[[
    Since the DinkLua scripts run in a different environment than we (the engine) do,
    we can't just access things like `current_sprite` directly here, but `getfenv(2)`
    gives us access to the environment we need to modify.
  ]]
  local env = getfenv(2)
  env.current_sprite = dink.get_sprite(1000)
end

-- `screenlock` from DinkC is weird. We can do better!
local screenlock = dink.screenlock
dink.screenlock = nil
dink.lock_screen = function()
  screenlock(1)
end
dink.unlock_screen = function()
  screenlock(0)
end
dink.is_screen_locked = function()
  return screenlock(-1)
end

-- Choice menu stuff

choice_metatable = {
  __newindex = function(t, key, value)
    if key == "condition" then
      rawset(t, "_condition", value)
    elseif key == "on_success" then
      rawset(t, "_success", value)
    else
      error("Choices do not have a "..key.." property", 2)
    end
  end,
  __index = function(t, key)
    if key == "condition" then
      return rawget(t, "_condition")
    elseif key == "on_success" then
      rawget(t, "_success")
    else
      error("Choices do not have a "..key.." property", 2)
    end
  end,
  __metatable = false
}

function create_choice(index, text, condition)
  return setmetatable({_index = index, _text = text, _condition = condition, _success = nil}, choice_metatable)
end

choice_menu_metatable = {
  __newindex = function(t, key, value)
    if key == "title" then
      rawset(t, "_title", value)
    elseif key == "y" then
      rawset(t, "_y", value)
    elseif key == "title_color" then
      rawset(t, "_title_color", value)
    else
      error("Choice menus do not have a "..key.." property", 2)
    end
  end,
  __index = function(t, key)
    if key == "title" then
      return rawget(t, "_title")
    elseif key == "y" then
      return rawget(t, "_y")
    elseif key == "title_color" then
      return rawget(t, "_title_color")
    elseif key == "add_choice" then
      return function(self, choice_text, choice_condition)
        local choice_index = #self._choices + 1
        self._choices[choice_index] = create_choice(choice_index, choice_text, choice_condition)
        return self._choices[choice_index]
      end
    elseif key == "add_savegame_choice" then
      return function(self)
        local choice_index = #self._choices + 1
        self._choices[choice_index] = create_choice(choice_index, "&savegameinfo")
        return self._choices[choice_index]
      end
    elseif key == "add_button_choice" then
      return function(self)
        local choice_index = #self._choices + 1
        self._choices[choice_index] = create_choice(choice_index, "Button "..choice_index.." - &buttoninfo")
        return self._choices[choice_index]
      end
    elseif key == "show" then
      return function(self)
        if #self._choices == 0 then
          error("There are no choices in the choice menu", 2)
        end

        -- TODO: Error handling when invalid values for title/y/title_color/conditions
        choice_menu.prepare(self._title, self._y, self._title_color)
        local choices_added = 0
        for _, c in ipairs(self._choices) do
          local condition = rawget(c, "_condition")
          local condition_result = true
          if condition ~= nil then
            if type(condition) == "boolean" then
              condition_result = condition
            elseif type(condition) == "function" then
              condition_result = condition()
            end
          end

          if condition_result then
            choice_menu.add_choice(c._index, c._text)
            choices_added = choices_added + 1
          end
        end

        if choices_added == 0 then
          error("There are no choices in the choice menu", 2)
        end

        choice_menu.show()
        coroutine.yield()

        -- The Dink engine places the result of a choice menu selection in the
        -- &result variable, so we'll dig it out of there and return the
        -- corresponding choice object.
        local result_index = object.get_global_variable_value("result")
        for _, c in ipairs(self._choices) do
          if c._index == result_index then
            -- If the scripter has set an on_success event, it runs before the
            -- show() method returns
            local success = rawget(c, "_success")
            if success ~= nil and type(success) == "function" then
              success()
            end
            return c
          end
        end

        -- Should never be reached, but let's just make sure we know if it does.
        error("Choice menu allowed non-existent choice (result = "..result_index..")")
      end
    else
      error("Choice menus do not have a "..key.." property", 2)
    end
  end,
  __metatable = false
}

function dink.create_choice_menu(choices, title, y, title_color)
  title = title or ""
  y = y or -5000
  title_color = title_color or 0

  local choice_menu = setmetatable({_choices = {}, _title = "", _y = -5000, _title_color = 0}, choice_menu_metatable)

  if type(choices) == "table" then
    for _, choice in pairs(choices) do
      if type(choice) == "string" then
        choice_menu:add_choice(choice)
      elseif type(choice) == "table" then
        choice_menu:add_choice(choice[2], choice[1])
      end
    end
  end

  return choice_menu
end

-- yeolde: serialisation

function dink.todisk(mytable, filename)
  local ok, destpath = pcall(savepath, filename, "wb")
  if not ok then error(destpath, 2) end
  local f = assert(io.open(destpath, "wb"))
  f:write(serpent.dump(mytable))
  f:close()
end

function dink.fromdisk(filename)
  local ok, destpath = pcall(savepath, filename, "rb")
  if not ok then error(destpath, 2) end
  local f = assert(io.open(destpath, "rb"))
  local ok, copy = serpent.load(f:read("a"))
  f:close()
  return copy
end

-- Table accessible by all scripts. Can be used for any purpose, though as its
-- name implies, it's volatile storage, and is not saved between sessions.
volatile = {}

-- We set this metatable on user-created coroutines so we can tell them apart
-- from engine-created ones.
coroutine_metatable = {
  __mode = "v",
  __metatable = false,
}
-- Lets us map real Lua threads to our user ones
thread_map = setmetatable({}, {__mode = "k"})

function create_environment(script_number, path, sprite_number)
  --[[

  Creates the environment we run the each Dink lua scripts in.

  Each script runs in its own environment, so that they can
  have the same function names and variable names without interfering
  with each other. If scripts need to communicate, facilities
  are provided for that purpose, for instance through the volatile table.

  ]]--
  local dink_env
  dink_env =
  {
    -- For debugging purposes
    print = dink.debug,
    -- inbuilt global objects
    sfx = sfx,
    music = music,
    palette = palette,
    timer = timer,
    game = game,

    -- Allow access to the safe Lua features
    assert = assert,
    error = error,
    getmetatable = getmetatable,
    ipairs = ipairs,
    next = next,
    pairs = pairs,
    pcall = pcall,
    select = select,
    setmetatable = setmetatable,
    tonumber = tonumber,
    tostring = tostring,
    type = type,
    xpcall = xpcall,
    coroutine = {
      close = function(co)
        if debug.getmetatable(co) ~= coroutine_metatable then
          error("Can only coroutine.close DinkLua coroutines", 2)
        end
        return coroutine.close(co._thread)
      end,
      create = function(f)
        local t = coroutine.create(f)
        local ut = setmetatable({_thread = t}, coroutine_metatable)
        thread_map[t] = ut
        return ut
      end,
      isyieldable = function(co)
        if co == nil then
          co = coroutine.running()
          co = thread_map[co]
        end
        if co == nil or debug.getmetatable(co) ~= coroutine_metatable then
          -- error("Can only use coroutine.isyieldable on/in DinkLua coroutines", 2)
          return false
        end
        return coroutine.isyieldable(co._thread)
      end,
      resume = function(co, ...)
        if debug.getmetatable(co) ~= coroutine_metatable then
          -- error("Can only use coroutine.resume on DinkLua coroutines", 2)
          return false, "can only resume DinkLua coroutines"
        end
        return coroutine.resume(co._thread, ...)
      end,
      running = function()
        local co, main = coroutine.running()
        co = thread_map[co]
        if co == nil or debug.getmetatable(co) ~= coroutine_metatable then
          error("Can only use coroutine.running in DinkLua coroutines", 2)
        end

        return co, main
      end,
      status = function(co)
        if debug.getmetatable(co) ~= coroutine_metatable then
          error("Can only use coroutine.status on DinkLua coroutines", 2)
        end
        return coroutine.status(co._thread)
      end,
      yield = function(...)
        local co, main = coroutine.running()
        co = thread_map[co]
        if co == nil or debug.getmetatable(co) ~= coroutine_metatable then
          error("Can only use coroutine.yield in DinkLua coroutines", 2)
        end
        return coroutine.yield(...)
      end
    },
    string = {
      byte = string.byte,
      char = string.char,
      find = string.find,
      format = string.format,
      gmatch = string.gmatch,
      gsub = string.gsub,
      len = string.len,
      lower = string.lower,
      match = string.match,
      rep = string.rep,
      reverse = string.reverse,
      sub = string.sub,
      upper = string.upper
    },
    table = table,
    math = math,
    -- include our bundled libraries
    vivid = vivid,
    lume = lume,
    vector = vector,

    os = {
      clock = os.clock,
      date = os.date,
      difftime = os.difftime,
      time = os.time
    },
    -- blacklist io and debug library
    io = nil,
    debug = nil,
    -- Volatile table. Can be used for any sort of volatile storage for the
    -- duration of the dmod. Gets wiped when Dink restarts
    volatile = volatile,

    -- Rather than allowing scripters to access the dink table directly, we give them an empty table and
    -- read-only access through a meta-table.
    dink = setmetatable({}, {__index = dink, __newindex = function() error("Cannot add or change members in dink", 2) end, __metatable = false}),

    -- Global allow access to the global DinkC variables through global.<variable_name>.
    global = setmetatable({}, global_metatable),

    -- Player is a convenience variable given to each script, is the sprite with sprite number 1 (usually Dink)
    player = dink.get_sprite(1),

    -- Current sprite is a convenience variable given to each script; should be equivalent to &current_sprite in DinkC
    current_sprite = dink.get_sprite(sprite_number),

    -- Brain value enumerable, allows doing things like
    --
    -- sprite.brain = brain.DUCK
    brain = setmetatable({}, {__index = {
      NONE = 0,
      PLAYER = 1,
      DUMB_SPRITE_BOUNCER = 2,
      DUCK = 3,
      PIG = 4,
      KILL_SEQ_DONE_LEAVE_LAST_FRAME = 5,
      REPEAT = 6,
      KILL_SEQ_DONE = 7,
      TEXT_SPRITE = 8,
      MONSTER_DIAGONAL = 9,
      MONSTER_NONDIAGONAL = 10,
      MISSILE = 11,
      BRAIN_PARM_SIZE_MATCH = 12,
      MOUSE = 13,
      BUTTON = 14,
      SHADOW = 15,
      SMART_PEOPLE = 16,
      MISSILE_KILL_SEQ_DONE = 17
    }, __newindex = function() error("Cannot add or change members in brain", 2) end, __metatable = false}),

    -- Direction enumerable, allows doing things like
    --
    -- sprite.dir = direction.NORTH
    direction = setmetatable({}, {__index = {
      SOUTH_WEST = 1,
      SOUTH = 2,
      SOUTH_EAST = 3,
      WEST = 4,
      EAST = 6,
      NORTH_WEST = 7,
      NORTH = 8,
      NORTH_EAST = 9
    }, __newindex = function() error("Cannot add or change members in direction", 2) end, __metatable = false}),

    -- Editor type enumerable, allows doing things like
    --
    -- editor.type = editor_type.KILL_COMPLETELY
    editor_type = setmetatable({}, {__index = {
      KILL_COMPLETELY = 1,
      DRAW_WITHOUT_HARDNESS = 2,
      DRAW_BACKGROUND_WITHOUT_HARDNESS = 3,
      DRAW_WITH_HARDNESS = 4,
      DRAW_BACKGROUND_WITH_HARDNESS = 5,
      KILL_RETURN_AFTER_FIVE_MINUTES = 6,
      KILL_RETURN_AFTER_THREE_MINUTES = 7,
      KILL_RETURN_AFTER_ONE_MINUTE = 8
    }, __newindex = function() error("Cannot add or change members in editor_type", 2) end, __metatable = false}),

    -- Include function, lets you include another Lua script as a table.
    include = function(script_name) return include(path, script_name, script_number) end,

    -- Auto-include feature. Does an automatic include() in every loaded script.
    autoinclude = function(include_name, script_name)
      autoincludes[include_name] = script_name
    end
  }

  --[[
    Because `missile_target` changes for each invocation of the script, we look it up each time
    instead of setting it once above. Ideally we'd be able to do the same for `current_sprite`,
    but because it's a pseudo-variable, we instead have to track changes to it ourselves.
    {{
      TECHNICALLY we could modify the engine itself to deal with current_sprite instead,
      but editing init.lua means I don't have to recompile the engine, so... ;)
    }}
    Luckily, only `sprite_attach` and `force_vision` modifies it, so we've only had to override
    those.
  ]]
  return setmetatable(dink_env, {
    __index = function(_, key)
      if key == "missile_target" then
        local missile_target = object.get_global_variable_value("missile_target")
        if missile_target == 0 then
          return nil
        else
          return dink.get_sprite(missile_target)
        end
      elseif key == "enemy_sprite" then
        local enemy_sprite = object.get_global_variable_value("enemy_sprite")
        if enemy_sprite == 0 then
          return nil
        else
          return dink.get_sprite(enemy_sprite)
        end
      elseif key == "missile_source" or key == "missle_source" then
        -- Unfortunately, this is misspelled in the engine, but since we're new, we can support
        -- the right spelling also.
        local missle_source = object.get_global_variable_value("missle_source")
        if missle_source == 0 then
          return nil
        else
          return dink.get_sprite(missle_source)
        end
      end
    end,
    __metatable = false
  })
end

function include(path, script_name, script_number)
  local script_path = engine.find_script(script_name)
  if script_path == nil then
    error("Could not include script "..script_name..".lua: not found", 2)
  end

  -- This loads the script in its own environment, but gives it access to the
  -- environment of the script it is being loaded from as well.
  local include_env = setmetatable({}, {__index = script[script_number], __metatable = false})
  local chunk, message = loadfile(script_path, 't', include_env)
  if not chunk then
    error("Could not include script "..script_name..".lua: "..message, 2)
  end
  local success, pmessage = pcall(chunk)
  if not success then
    error("Could not include script "..script_name..".lua: "..pmessage, 2)
  end
  return include_env
end

-- Script cache
script = {}

-- Co-routine cache. Holds a script's co-routines between resumes,
-- to properly yield when Dink expects a script to yield, e.g.
-- when calling the wait() function or the *_stop() functions.
routine = {}

-- Autoinclude list
autoincludes = {}

function load_script(script_number, path, sprite_number)
  script[script_number] = create_environment(script_number, path, sprite_number)
  local chunk, message = loadfile(path, 't', script[script_number])
  if not chunk then return nil, message end
  local load_results = table.pack(pcall(chunk))

  if not load_results[1] then
    return table.unpack(load_results)
  else
     -- Autoinclude scripts
     for include_name, script_name in pairs(autoincludes) do
       script[script_number][include_name] = include(path, script_name, script_number)
     end
  end
end

-- The function kill_script is implemented entirely in C, as it misbehaves if
-- implemented in Lua.

function proc_exists(script_number, proc)
  return script[script_number] ~= nil and type(script[script_number][proc]) == "function"
end

function run_script(script_number, proc)
  if not proc_exists(script_number, proc) then
    -- Silently ignore
    return
    --return false, "run_script cannot run function "..proc.." in script "..script_number..". No such function exists."
  end

  -- Kill off any currently running routine
  if routine[script_number] ~= nil then
    routine[script_number] = nil
  end
  routine[script_number] = coroutine.create(script[script_number][proc])

  if proc == "hit" then
    -- Call the hit routine with the sprite that attacked last as a parameter
    return coroutine.resume(routine[script_number], dink.get_sprite(object.get_global_variable_value("enemy_sprite")))
  else
    return coroutine.resume(routine[script_number])
  end
end

function resume_script(script_number)
  if routine[script_number] == nil then
    return false, "resume_script cannot resume script "..script_number..". No function currently running."
  end

  if coroutine.status(routine[script_number]) == "dead" then
    return false, "resume_script cannot resume script "..script_number..". Routine already completed."
  end
  return coroutine.resume(routine[script_number])
end
