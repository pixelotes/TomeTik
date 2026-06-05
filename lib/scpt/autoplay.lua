-- AuToME -- tunable auto-play policy (loaded by init.lua).
--
-- The C engine (src/cmd1.c) consults these functions but ALWAYS has its own
-- fallback, so editing this file re-tunes the bot WITHOUT recompiling. Return
-- a negative value to mean "no opinion, use the C default".

-- ---------------------------------------------------------------- config knobs
-- How many of each supply to keep, gold/turn thresholds, etc. Tweak freely.
autoplay_config_table =
{
	want_cure         = 5,    -- cure-wounds potions to keep
	want_food         = 5,    -- staple food to keep
	want_id           = 5,    -- Scrolls of Identify to keep
	want_wor          = 3,    -- Word of Recall scrolls to keep
	want_oil          = 3,    -- flasks of oil (when wielding a lantern)
	want_torch        = 2,    -- spare torches (when wielding a torch)
	min_gold          = 100,  -- below this, don't bother shopping (keep diving)
	chase_turns       = 15,   -- give up chasing a foe after this many turns
	resupply_cooldown = 3000, -- turns to dive before another town trip
	avoid_paralyze    = 1,    -- 1 = don't melee paralysers without Free Action
	                          --     (floating eyes etc. -- a melee death trap)
	want_ammo         = 40,   -- missiles to keep for the launcher
	-- threat model (real danger = expected damage/turn):
	danger_turns      = 2,    -- avoid meleeing a foe that could kill us in this
	                          --   many turns (single-monster "too dangerous")
	pack_flee_turns   = 4,    -- flee a PACK if its combined damage/turn * this >= HP
	cluster_range     = 8,    -- tiles around us that count as the same pack
}

function autoplay_config(key)
	local v = autoplay_config_table[key]
	if (v == nil) then return -1 end
	return v
end

-- -------------------------------------------------------------- threat / avoid
-- Per-monster combat policy, by exact race name:
--   1 = avoid (don't pick the fight), 0 = always fight, absent = C heuristic.
-- Friendly/quest NPCs the bot shouldn't be hammering go here.
autoplay_avoid_table =
{
	["Farmer Maggot"] = 1,
}

function autoplay_avoid(m)
	local nm = monster_race_desc(m.r_idx, m.ego)
	local v = autoplay_avoid_table[nm]
	if (v ~= nil) then return v end
	return -1
end

-- ------------------------------------------------------------- dungeon route
-- AuToME's strategic plan: an ordered list of dungeons to clear / raid, in the
-- order the bot should attempt them. The C engine (autoplay_objective) walks
-- this top-to-bottom and picks the FIRST entry it hasn't finished yet
-- (max_dlv[dungeon] < depth) AND is strong enough for (player level >= plev).
-- When none qualifies it keeps grinding the deepest principal dungeon it can
-- enter. Reorder, delete, or re-gate entries freely -- it's just data.
--
--   dungeon = d_info index (see lib/edit/d_info.txt)
--   plev    = minimum CHARACTER level before attempting. This is a *survival*
--             gate, deliberately stricter than d_info's own min_plev ("can
--             enter"): it's usually set near the dungeon's bottom depth so the
--             bot grinds itself up before raiding a deep boss lair.
--   depth   = dive target; reaching it marks the entry done. For guardian /
--             artifact dungeons that's the bottom (maxdepth), where the boss is.
--   name    = label shown by the Oracle / recall messages (and the prize).
--
-- The four PRINCIPAL dungeons (Barrow-Downs -> Mirkwood -> Mordor -> Angband)
-- are the spine: a contiguous depth 1..127 ladder. The rest are optional
-- side-raids, slotted in by level, for their boss (FINAL_GUARDIAN) and loot
-- (FINAL_ARTIFACT / FINAL_OBJECT).
autoplay_route =
{
	{ dungeon =  4, plev =  1, depth =  10, name = "the Barrow-Downs" },             -- spine
	{ dungeon = 19, plev = 18, depth =  22, name = "the Orc Cave (Azog, Wand of Thrain)" },
	{ dungeon =  1, plev =  5, depth =  33, name = "Mirkwood" },                     -- spine
	{ dungeon = 21, plev = 22, depth =  25, name = "the Old Forest (Old Man Willow)" },
	{ dungeon = 27, plev = 28, depth =  30, name = "the Sandworm Lair (Queen's armour)" },
	{ dungeon = 10, plev = 34, depth =  36, name = "the Heart of the Earth (Golgarach)" },
	{ dungeon = 18, plev = 35, depth =  37, name = "the Maze (Helm of Hammerhand)" },
	{ dungeon = 29, plev = 38, depth =  40, name = "the Helcaraxe (White Balrog)" },
	{ dungeon = 26, plev = 38, depth =  40, name = "the Land of Rhun (Ulfang)" },
	{ dungeon =  9, plev = 48, depth =  50, name = "Cirith Ungol (Shelob)" },
	{ dungeon = 22, plev = 50, depth =  50, name = "Moria (Durin's Bane)" },
	{ dungeon =  2, plev = 15, depth =  66, name = "Mordor" },                       -- spine
	{ dungeon =  7, plev = 48, depth =  50, name = "the Submerged Ruins (Ar-Pharazon)" },
	{ dungeon = 17, plev = 50, depth =  52, name = "the Illusory Castle (Helm of Knowledge)" },
	{ dungeon = 16, plev = 68, depth =  70, name = "the Paths of the Dead (Feagwath, Doomcaller)" },
	{ dungeon = 25, plev = 68, depth =  70, name = "the Sacred Land of Mountains (Trone)" },
	{ dungeon = 23, plev = 70, depth =  70, name = "Dol Guldur (the Necromancer)" },
	{ dungeon = 20, plev = 70, depth =  72, name = "Erebor (Glaurung)" },
	{ dungeon =  3, plev = 30, depth = 127, name = "the Pits of Angband" },          -- spine (endgame)
	{ dungeon =  5, plev = 85, depth =  99, name = "Mount Doom" },
	{ dungeon = 11, plev = 99, depth = 150, name = "the Void (Melkor)" },
}

-- 1-based index -> dungeon, plev, depth (three ints). Returns dungeon = -1 past
-- the end of the list, which the C side uses as the stop sentinel (Lua 4.0 has
-- no table.getn, so we don't expose a length -- we walk until the sentinel).
function autoplay_route_at(i)
	local e = autoplay_route[i]
	if (e == nil) then return -1, 0, 0 end
	return e.dungeon, e.plev, e.depth
end

-- 1-based index -> label for the Oracle / messages ("" past the end).
function autoplay_route_name(i)
	local e = autoplay_route[i]
	if (e == nil) then return "" end
	return e.name
end
