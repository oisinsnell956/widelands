push_textdomain("tribes")

dirname = path.dirname(__file__)

tribes:new_ware_type {
   name = "helmet_mask",
   -- TRANSLATORS: This is a ware name used in lists of wares
   descname = pgettext("ware", "Mask"),
   icon = dirname .. "menu.png",

   animations = {
      idle = {
         pictures = path.list_files(dirname .. "idle.png"),
         hotspot = { 7, 10 },
      },
   }
}

pop_textdomain()
