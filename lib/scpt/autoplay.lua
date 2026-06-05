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
