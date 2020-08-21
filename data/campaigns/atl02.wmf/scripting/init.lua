-- =======================================================================
--                      Atlanteans Tutorial Mission 02
-- =======================================================================
include "scripting/coroutine.lua"
include "scripting/infrastructure.lua"
include "scripting/messages.lua"
include "scripting/objective_utils.lua"
include "scripting/table.lua"
include "scripting/ui.lua"


-- ===================
-- Constants & Config
-- ===================
set_textdomain("scenario_atl02.wmf")

game = wl.Game()
map = game.map
p1 = game.players[1]
Kalitath = game.players[3]
Maletus = game.players[2]

-- =================
-- global variables and functions
-- =================

include "map:scripting/texts.lua"
include "map:scripting/init_Kalitath.lua"
include "map:scripting/init_Maletus.lua"

-- =================
-- Helper functions
-- =================

-- Show many message boxes
function msg_boxes(boxes_descr)
   for idx,box_descr in ipairs(boxes_descr) do
     campaign_message_box(box_descr)
   end
end

-- Return the total number of items in warehouses of the given
-- ware.
function count_in_warehouses(ware)
   local whs = array_combine(
      p1:get_buildings("atlanteans_headquarters"),
      p1:get_buildings("atlanteans_warehouse")
   )
   local rv = 0
   for idx,wh in ipairs(whs) do
      rv = rv + wh:get_wares(ware)
   end
   return rv
end

-- ====================
-- Starting the main thread
-- ====================

-- starting conditions are included later in the intro
include "map:scripting/mission_thread.lua"
