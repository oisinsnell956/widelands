dirname = path.dirname(__file__)

tribes:new_productionsite_type {
   msgctxt = "atlanteans_building",
   name = "atlanteans_crystalmine",
   -- TRANSLATORS: This is a building name used in lists of buildings
   descname = pgettext("atlanteans_building", "Crystal Mine"),
   helptext_script = dirname .. "helptexts.lua",
   icon = dirname .. "menu.png",
   size = "mine",

   buildcost = {
      log = 7,
      planks = 4,
      spidercloth = 1
   },
   return_on_dismantle = {
      log = 3,
      planks = 2
   },

   animations = {
      idle = {
         pictures = path.list_files(dirname .. "idle_??.png"),
         hotspot = { 50, 56 },
      },
      working = {
         pictures = path.list_files(dirname .. "idle_??.png"), -- TODO(GunChleoc): No animation yet.
         hotspot = { 50, 56 },
      },
      empty = {
         pictures = path.list_files(dirname .. "empty_??.png"),
         hotspot = { 50, 56 },
      },
   },

   aihints = {
      mines = "stones",
      basic_amount = 1,
      prohibited_till = 600
   },

   working_positions = {
      atlanteans_miner = 3
   },

   inputs = {
      { name = "smoked_fish", amount = 10 },
      { name = "smoked_meat", amount = 6 },
      { name = "atlanteans_bread", amount = 10 }
   },
   outputs = {
      "diamond",
      "quartz",
      "granite"
   },

   programs = {
      work = {
         -- TRANSLATORS: Completed/Skipped/Did not start working because ...
         descname = _"working",
         actions = {
            "call=mine_granite",
            "call=mine_quartz",
            "call=mine_diamond",
            "return=skipped"
         }
      },
      mine_granite = {
         -- TRANSLATORS: Completed/Skipped/Did not start mining granite because ...
         descname = _"mining granite",
         actions = {
            "return=skipped unless economy needs granite",
            "sleep=40000",
            "consume=smoked_fish,smoked_meat:2 atlanteans_bread:2",
            "call=mine_produce_granite",
            "call=mine_produce_granite",
            "call=mine_produce_granite",
            "call=mine_produce_quartz",
            "call=mine_produce_granite",
            "call=mine_produce_granite",
            "return=skipped"
         }
      },
      mine_quartz = {
         -- TRANSLATORS: Completed/Skipped/Did not start mining quartz because ...
         descname = _"mining quartz",
         actions = {
            "return=skipped unless economy needs quartz",
            "sleep=30000",
            "consume=smoked_fish,smoked_meat:2 atlanteans_bread:2",
            "call=mine_produce_quartz",
            "call=mine_produce_granite",
            "call=mine_produce_granite",
            "call=mine_produce_granite",
            "call=mine_produce_quartz",
            "call=mine_produce_quartz",
            "return=skipped"
         }
      },
      mine_diamond = {
         -- TRANSLATORS: Completed/Skipped/Did not start mining diamonds because ...
         descname = _"mining diamonds",
         actions = {
            "return=skipped unless economy needs diamond",
            "sleep=40000",
            "consume=smoked_fish,smoked_meat:2 atlanteans_bread:2",
            "call=mine_produce_diamond",
            "call=mine_produce_granite",
            "call=mine_produce_diamond",
            "return=skipped"
         }
      },
      mine_produce_granite = {
         -- TRANSLATORS: Completed/Skipped/Did not start mining and producing because ...
         descname = _"mining and producing",
         actions = {
            "animate=working 15000",
            "mine=stones 4 100 5 2",
            "produce=granite",
         }
      },
      mine_produce_quartz = {
         -- TRANSLATORS: Completed/Skipped/Did not start mining and producing because ...
         descname = _"mining and producing",
         actions = {
            "animate=working 10000",
            "mine=stones 4 100 5 2",
            "produce=quartz",
         }
      },
      mine_produce_diamond = {
         -- TRANSLATORS: Completed/Skipped/Did not start mining and producing because ...
         descname = _"mining and producing",
         actions = {
            "animate=working 25000",
            "mine=stones 4 100 5 2",
            "produce=diamond",
         }
      },
   },
   out_of_resource_notification = {
      -- Translators: Short for "Out of ..." for a resource
      title = _"No Crystal",
      heading = _"Main Crystal Vein Exhausted",
      message =
         pgettext("atlanteans_building", "This crystal mine’s main vein is exhausted. Expect strongly diminished returns on investment. You should consider dismantling or destroying it."),
   },
}
