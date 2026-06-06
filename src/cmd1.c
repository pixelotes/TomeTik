/* File: cmd1.c */

/* Purpose: Movement commands (part 1) */

/*
 * Copyright (c) 1989 James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#include "angband.h"
#define MAX_VAMPIRIC_DRAIN 100


/*
 * Determine if the player "hits" a monster (normal combat).
 * Note -- Always miss 5%, always hit 5%, otherwise random.
 */
bool test_hit_fire(int chance, int ac, int vis)
{
	int k;


	/* Percentile dice */
	k = rand_int(100);

	/* Hack -- Instant miss or hit */
	if (k < 10) return (k < 5);

	/* Never hit */
	if (chance <= 0) return (FALSE);

	/* Invisible monsters are harder to hit */
	if (!vis) chance = (chance + 1) / 2;

	/* Power competes against armor */
	if (rand_int(chance + luck( -10, 10)) < (ac * 3 / 4)) return (FALSE);

	/* Assume hit */
	return (TRUE);
}



/*
 * Determine if the player "hits" a monster (normal combat).
 *
 * Note -- Always miss 5%, always hit 5%, otherwise random.
 */
bool test_hit_norm(int chance, int ac, int vis)
{
	int k;


	/* Percentile dice */
	k = rand_int(100);

	/* Hack -- Instant miss or hit */
	if (k < 10) return (k < 5);

	/* Wimpy attack never hits */
	if (chance <= 0) return (FALSE);

	/* Penalize invisible targets */
	if (!vis) chance = (chance + 1) / 2;

	/* Power must defeat armor */
	if (rand_int(chance + luck( -10, 10)) < (ac * 3 / 4)) return (FALSE);

	/* Assume hit */
	return (TRUE);
}



/*
 * Critical hits (from objects thrown by player)
 * Factor in item weight, total plusses, and player level.
 */
s16b critical_shot(int weight, int plus, int dam)
{
	int i, k;


	/* Extract "shot" power */
	i = (weight + ((p_ptr->to_h + plus) * 4) +
	     get_skill_scale(SKILL_ARCHERY, 100));
	i += 50 * p_ptr->xtra_crit;
	i += luck( -100, 100);

	/* Critical hit */
	if (randint(5000) <= i)
	{
		k = weight + randint(500);

		if (k < 500)
		{
			msg_print("It was a good hit!");
			dam = 2 * dam + 5;
		}
		else if (k < 1000)
		{
			msg_print("It was a great hit!");
			dam = 2 * dam + 10;
		}
		else
		{
			msg_print("It was a superb hit!");
			dam = 3 * dam + 15;
		}
	}

	return (dam);
}

/*
 * Critical hits (by player)
 *
 * Factor in weapon weight, total plusses, player level.
 */
s16b critical_norm(int weight, int plus, int dam, int weapon_tval, bool *done_crit)
{
	int i, k, num = randint(5000);

	*done_crit = FALSE;

	/* Extract "blow" power */
	i = (weight + ((p_ptr->to_h + plus) * 5) +
	     get_skill_scale(p_ptr->melee_style, 150));
	i += 50 * p_ptr->xtra_crit;
	if ((weapon_tval == TV_SWORD) && (weight < 50) && get_skill(SKILL_CRITS))
	{
		i += get_skill_scale(SKILL_CRITS, 40 * 50);
	}
	i += luck( -100, 100);

	/* Force good strikes */
	if (p_ptr->tim_deadly)
	{
		set_tim_deadly(p_ptr->tim_deadly - 1);
		msg_print("It was a *GREAT* hit!");
		dam = 3 * dam + 20;
		*done_crit = TRUE;
	}

	/* Chance */
	else if (num <= i)
	{
		k = weight + randint(650);
		if ((weapon_tval == TV_SWORD) && (weight < 50) && get_skill(SKILL_CRITS))
		{
			k += get_skill_scale(SKILL_CRITS, 400);
		}

		if (k < 400)
		{
			msg_print("It was a good hit!");
			dam = 2 * dam + 5;
		}
		else if (k < 700)
		{
			msg_print("It was a great hit!");
			dam = 2 * dam + 10;
		}
		else if (k < 900)
		{
			msg_print("It was a superb hit!");
			dam = 3 * dam + 15;
		}
		else if (k < 1300)
		{
			msg_print("It was a *GREAT* hit!");
			dam = 3 * dam + 20;
		}
		else
		{
			msg_print("It was a *SUPERB* hit!");
			dam = ((7 * dam) / 2) + 25;
		}
		*done_crit = TRUE;
	}

	return (dam);
}



/*
 * Extract the "total damage" from a given object hitting a given monster.
 *
 * Note that "flasks of oil" do NOT do fire damage, although they
 * certainly could be made to do so.  XXX XXX
 *
 * Note that most brands and slays are x3, except Slay Animal (x2),
 * Slay Evil (x2), and Kill dragon (x5).
 */
s16b tot_dam_aux(object_type *o_ptr, int tdam, monster_type *m_ptr,
                 s32b *special)
{
	int mult = 1;

	monster_race *r_ptr = race_inf(m_ptr);

	u32b f1, f2, f3, f4, f5, esp;


	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3, &f4, &f5, &esp);

	/* Some "weapons" and "ammo" do extra damage */
	switch (o_ptr->tval)
	{
	case TV_SHOT:
	case TV_ARROW:
	case TV_BOLT:
	case TV_BOOMERANG:
	case TV_HAFTED:
	case TV_POLEARM:
	case TV_SWORD:
	case TV_AXE:
	case TV_DIGGING:
		{
			/* Slay Animal */
			if ((f1 & (TR1_SLAY_ANIMAL)) && (r_ptr->flags3 & (RF3_ANIMAL)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_ANIMAL);
				}

				if (mult < 2) mult = 2;
			}

			/* Slay Evil */
			if ((f1 & (TR1_SLAY_EVIL)) && (r_ptr->flags3 & (RF3_EVIL)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_EVIL);
				}

				if (mult < 2) mult = 2;
			}

			/* Slay Undead */
			if ((f1 & (TR1_SLAY_UNDEAD)) && (r_ptr->flags3 & (RF3_UNDEAD)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_UNDEAD);
				}

				if (mult < 3) mult = 3;
			}

			/* Slay Demon */
			if ((f1 & (TR1_SLAY_DEMON)) && (r_ptr->flags3 & (RF3_DEMON)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_DEMON);
				}

				if (mult < 3) mult = 3;
			}

			/* Slay Orc */
			if ((f1 & (TR1_SLAY_ORC)) && (r_ptr->flags3 & (RF3_ORC)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_ORC);
				}

				if (mult < 3) mult = 3;
			}

			/* Slay Troll */
			if ((f1 & (TR1_SLAY_TROLL)) && (r_ptr->flags3 & (RF3_TROLL)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_TROLL);
				}

				if (mult < 3) mult = 3;
			}

			/* Slay Giant */
			if ((f1 & (TR1_SLAY_GIANT)) && (r_ptr->flags3 & (RF3_GIANT)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_GIANT);
				}

				if (mult < 3) mult = 3;
			}

			/* Slay Dragon  */
			if ((f1 & (TR1_SLAY_DRAGON)) && (r_ptr->flags3 & (RF3_DRAGON)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_DRAGON);
				}

				if (mult < 3) mult = 3;
			}

			/* Execute Dragon */
			if ((f1 & (TR1_KILL_DRAGON)) && (r_ptr->flags3 & (RF3_DRAGON)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_DRAGON);
				}

				if (mult < 5) mult = 5;
			}

			/* Execute Undead */
			if ((f5 & (TR5_KILL_UNDEAD)) && (r_ptr->flags3 & (RF3_UNDEAD)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_UNDEAD);
				}

				if (mult < 5) mult = 5;
			}

			/* Execute Demon */
			if ((f5 & (TR5_KILL_DEMON)) && (r_ptr->flags3 & (RF3_DEMON)))
			{
				if (m_ptr->ml)
				{
					r_ptr->r_flags3 |= (RF3_DEMON);
				}

				if (mult < 5) mult = 5;
			}


			/* Brand (Acid) */
			if (f1 & (TR1_BRAND_ACID))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_IM_ACID))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags3 |= (RF3_IM_ACID);
					}
				}

				/* Notice susceptibility */
				else if (r_ptr->flags9 & (RF9_SUSCEP_ACID))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags9 |= (RF9_SUSCEP_ACID);
					}
					if (mult < 6) mult = 6;
				}

				/* Otherwise, take the damage */
				else
				{
					if (mult < 3) mult = 3;
				}
			}

			/* Brand (Elec) */
			if (f1 & (TR1_BRAND_ELEC))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_IM_ELEC))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags3 |= (RF3_IM_ELEC);
					}
				}

				/* Notice susceptibility */
				else if (r_ptr->flags9 & (RF9_SUSCEP_ELEC))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags9 |= (RF9_SUSCEP_ELEC);
					}
					if (mult < 6) mult = 6;
				}

				/* Otherwise, take the damage */
				else
				{
					if (mult < 3) mult = 3;
				}
			}

			/* Brand (Fire) */
			if (f1 & (TR1_BRAND_FIRE))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_IM_FIRE))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags3 |= (RF3_IM_FIRE);
					}
				}

				/* Notice susceptibility */
				else if (r_ptr->flags3 & (RF3_SUSCEP_FIRE))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags3 |= (RF3_SUSCEP_FIRE);
					}
					if (mult < 6) mult = 6;
				}

				/* Otherwise, take the damage */
				else
				{
					if (mult < 3) mult = 3;
				}
			}

			/* Brand (Cold) */
			if (f1 & (TR1_BRAND_COLD))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_IM_COLD))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags3 |= (RF3_IM_COLD);
					}
				}

				/* Notice susceptibility */
				else if (r_ptr->flags3 & (RF3_SUSCEP_COLD))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags3 |= (RF3_SUSCEP_COLD);
					}
					if (mult < 6) mult = 6;
				}

				/* Otherwise, take the damage */
				else
				{
					if (mult < 3) mult = 3;
				}
			}

			/* Brand (Poison) */
			if (f1 & (TR1_BRAND_POIS) || (p_ptr->tim_poison))
			{
				/* Notice immunity */
				if (r_ptr->flags3 & (RF3_IM_POIS))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags3 |= (RF3_IM_POIS);
					}
				}

				/* Notice susceptibility */
				else if (r_ptr->flags9 & (RF9_SUSCEP_POIS))
				{
					if (m_ptr->ml)
					{
						r_ptr->r_flags9 |= (RF9_SUSCEP_POIS);
					}
					if (mult < 6) mult = 6;
					if (magik(95)) *special |= SPEC_POIS;
				}

				/* Otherwise, take the damage */
				else
				{
					if (mult < 3) mult = 3;
					if (magik(50)) *special |= SPEC_POIS;
				}
			}

			/* Wounding */
			if (f5 & (TR5_WOUNDING))
			{
				/* Notice immunity */
				if (r_ptr->flags8 & (RF8_NO_CUT))
				{
					if (m_ptr->ml)
					{
						r_info[m_ptr->r_idx].r_flags8 |= (RF8_NO_CUT);
					}
				}

				/* Otherwise, take the damage */
				else
				{
					if (magik(50)) *special |= SPEC_CUT;
				}
			}
			break;
		}
	}


	/* Return the total damage */
	return (tdam * mult);
}


/*
 * Search for hidden things
 */
void search(void)
{
	int y, x, chance;

	s16b this_o_idx, next_o_idx = 0;

	cave_type *c_ptr;


	/* Start with base search ability */
	chance = p_ptr->skill_srh;

	/* Penalize various conditions */
	if (p_ptr->blind || no_lite()) chance = chance / 10;
	if (p_ptr->confused || p_ptr->image) chance = chance / 10;

	/* Search the nearby grids, which are always in bounds */
	for (y = (p_ptr->py - 1); y <= (p_ptr->py + 1); y++)
	{
		for (x = (p_ptr->px - 1); x <= (p_ptr->px + 1); x++)
		{
			/* Sometimes, notice things */
			if (rand_int(100) < chance)
			{
				/* Access the grid */
				c_ptr = &cave[y][x];

				/* Invisible trap */
				if ((c_ptr->t_idx != 0) && !(c_ptr->info & CAVE_TRDT))
				{
					/* Pick a trap */
					pick_trap(y, x);

					/* Message */
					msg_print("You have found a trap.");

					/* Disturb */
					disturb(0, 0);
				}

				/* Secret door */
				if (c_ptr->feat == FEAT_SECRET)
				{
					/* Message */
					msg_print("You have found a secret door.");

					/* Pick a door XXX XXX XXX */
					cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
					cave[y][x].mimic = 0;
					lite_spot(y, x);

					/* Disturb */
					disturb(0, 0);
				}

				/* Scan all objects in the grid */
				for (this_o_idx = c_ptr->o_idx; this_o_idx;
				                this_o_idx = next_o_idx)
				{
					object_type * o_ptr;

					/* Acquire object */
					o_ptr = &o_list[this_o_idx];

					/* Acquire next object */
					next_o_idx = o_ptr->next_o_idx;

					/* Skip non-chests */
					if (o_ptr->tval != TV_CHEST) continue;

					/* Skip non-trapped chests */
					if (!o_ptr->pval) continue;

					/* Identify once */
					if (!object_known_p(o_ptr))
					{
						/* Message */
						msg_print("You have discovered a trap on the chest!");

						/* Know the trap */
						object_known(o_ptr);

						/* Notice it */
						disturb(0, 0);
					}
				}
			}
		}
	}
}




/*
 * Player "wants" to pick up an object or gold.
 * Note that we ONLY handle things that can be picked up.
 * See "move_player()" for handling of other things.
 */
void carry(int pickup)
{
	if (!p_ptr->disembodied)
	{
		py_pickup_floor(pickup);
	}
}


/*
 * Handle player hitting a real trap
 */
static void hit_trap(void)
{
	bool ident = FALSE;

	cave_type *c_ptr;


	/* Disturb the player */
	disturb(0, 0);

	/* Get the cave grid */
	c_ptr = &cave[p_ptr->py][p_ptr->px];
	if (c_ptr->t_idx != 0)
	{
		ident = player_activate_trap_type(p_ptr->py, p_ptr->px, NULL, -1);
		if (ident)
		{
			t_info[c_ptr->t_idx].ident = TRUE;
			msg_format("You identified the trap as %s.",
			           t_name + t_info[c_ptr->t_idx].name);
		}
	}
}


void touch_zap_player(monster_type *m_ptr)
{
	int aura_damage = 0;

	monster_race *r_ptr = race_inf(m_ptr);


	if (r_ptr->flags2 & (RF2_AURA_FIRE))
	{
		if (!(p_ptr->immune_fire))
		{
			char aura_dam[80];

			aura_damage =
			        damroll(1 + (m_ptr->level / 26), 1 + (m_ptr->level / 17));

			/* Hack -- Get the "died from" name */
			monster_desc(aura_dam, m_ptr, 0x88);

			msg_print("You are suddenly very hot!");

			if (p_ptr->oppose_fire) aura_damage = (aura_damage + 2) / 3;
			if (p_ptr->resist_fire) aura_damage = (aura_damage + 2) / 3;
			if (p_ptr->sensible_fire) aura_damage = (aura_damage + 2) * 2;

			take_hit(aura_damage, aura_dam);
			r_ptr->r_flags2 |= RF2_AURA_FIRE;
			handle_stuff();
		}
	}


	if (r_ptr->flags2 & (RF2_AURA_ELEC))
	{
		if (!(p_ptr->immune_elec))
		{
			char aura_dam[80];

			aura_damage =
			        damroll(1 + (m_ptr->level / 26), 1 + (m_ptr->level / 17));

			/* Hack -- Get the "died from" name */
			monster_desc(aura_dam, m_ptr, 0x88);

			if (p_ptr->oppose_elec) aura_damage = (aura_damage + 2) / 3;
			if (p_ptr->resist_elec) aura_damage = (aura_damage + 2) / 3;

			msg_print("You get zapped!");
			take_hit(aura_damage, aura_dam);
			r_ptr->r_flags2 |= RF2_AURA_ELEC;
			handle_stuff();
		}
	}
}

#if 0 /* not used, eliminates a compiler warning */
static void natural_attack(s16b m_idx, int attack, bool *fear, bool *mdeath)
{
	int k, bonus, chance;

	int n_weight = 0;
	bool done_crit;

	monster_type *m_ptr = &m_list[m_idx];

	char m_name[80];

	int dss, ddd;

	char *atk_desc;


	switch (attack)
	{
	default:
		{
			dss = ddd = n_weight = 1;
			atk_desc = "undefined body part";

			break;
		}
	}

	/* Extract monster name (or "it") */
	monster_desc(m_name, m_ptr, 0);


	/* Calculate the "attack quality" */
	bonus = p_ptr->to_h;
	chance = (p_ptr->skill_thn + (bonus * BTH_PLUS_ADJ));

	/* Test for hit */
	if (test_hit_norm(chance, m_ptr->ac, m_ptr->ml))
	{
		/* Sound */
		sound(SOUND_HIT);

		msg_format("You hit %s with your %s.", m_name, atk_desc);

		k = damroll(ddd, dss);
		k = critical_norm(n_weight, p_ptr->to_h, k, -1, &done_crit);

		/* Apply the player damage bonuses */
		k += p_ptr->to_d;

		/* No negative damage */
		if (k < 0) k = 0;

		/* Complex message */
		if (wizard)
		{
			msg_format("You do %d (out of %d) damage.", k, m_ptr->hp);
		}

		switch (is_friend(m_ptr))
		{
		case 1:
			{
				msg_format("%^s gets angry!", m_name);
				change_side(m_ptr);

				break;
			}

		case 0:
			{
				msg_format("%^s gets angry!", m_name);
				m_ptr->status = MSTATUS_NEUTRAL_M;

				break;
			}
		}

		/* Damage, check for fear and mdeath */
		switch (attack)
		{
		default:
			{
				*mdeath = mon_take_hit(m_idx, k, fear, NULL);
				break;
			}
		}

		touch_zap_player(m_ptr);
	}

	/* Player misses */
	else
	{
		/* Sound */
		sound(SOUND_MISS);

		/* Message */
		msg_format("You miss %s.", m_name);
	}
}

#endif

/*
 * Carried monster can attack too.
 * Based on monst_attack_monst.
 */
static void carried_monster_attack(s16b m_idx, bool *fear, bool *mdeath,
                                   int x, int y)
{
	monster_type *t_ptr = &m_list[m_idx];

	monster_race *r_ptr;

	monster_race *tr_ptr = race_inf(t_ptr);

	cave_type *c_ptr;

	int ap_cnt;

	int ac, rlev, pt;

	char t_name[80];

	cptr sym_name = symbiote_name(TRUE);

	char temp[80];

	bool blinked = FALSE, touched = FALSE;

	byte y_saver = t_ptr->fy;

	byte x_saver = t_ptr->fx;

	object_type *o_ptr;


	/* Get the carried monster */
	o_ptr = &p_ptr->inventory[INVEN_CARRY];
	if (!o_ptr->k_idx) return;

	c_ptr = &cave[y][x];

	r_ptr = &r_info[o_ptr->pval];

	/* Not allowed to attack */
	if (r_ptr->flags1 & RF1_NEVER_BLOW) return;

	/* Total armor */
	ac = t_ptr->ac;

	/* Extract the effective monster level */
	rlev = ((r_ptr->level >= 1) ? r_ptr->level : 1);

	/* Get the monster name (or "it") */
	monster_desc(t_name, t_ptr, 0);

	/* Assume no blink */
	blinked = FALSE;

	if (!t_ptr->ml)
	{
		msg_print("You hear noise.");
	}

	/* Scan through all four blows */
	for (ap_cnt = 0; ap_cnt < 4; ap_cnt++)
	{
		bool visible = FALSE;
		bool obvious = FALSE;

		int power = 0;
		int damage = 0;

		cptr act = NULL;

		/* Extract the attack infomation */
		int effect = r_ptr->blow[ap_cnt].effect;
		int method = r_ptr->blow[ap_cnt].method;
		int d_dice = r_ptr->blow[ap_cnt].d_dice;
		int d_side = r_ptr->blow[ap_cnt].d_side;

		/* Stop attacking if the target dies! */
		if (t_ptr->fx != x_saver || t_ptr->fy != y_saver)
			break;

		/* Hack -- no more attacks */
		if (!method) break;

		if (blinked)			/* Stop! */
		{
			/* break; */
		}

		/* Extract visibility (before blink) */
		visible = TRUE;

#if 0

		/* Extract visibility from carrying lite */
		if (r_ptr->flags9 & RF9_HAS_LITE) visible = TRUE;

#endif /* 0 */

		/* Extract the attack "power" */
		switch (effect)
		{
		case RBE_HURT:
			{
				power = 60;
				break;
			}
		case RBE_POISON:
			{
				power = 5;
				break;
			}
		case RBE_UN_BONUS:
			{
				power = 20;
				break;
			}
		case RBE_UN_POWER:
			{
				power = 15;
				break;
			}
		case RBE_EAT_GOLD:
			power = 5;
			break;
		case RBE_EAT_ITEM:
			power = 5;
			break;
		case RBE_EAT_FOOD:
			power = 5;
			break;
		case RBE_EAT_LITE:
			power = 5;
			break;
		case RBE_ACID:
			power = 0;
			break;
		case RBE_ELEC:
			power = 10;
			break;
		case RBE_FIRE:
			power = 10;
			break;
		case RBE_COLD:
			power = 10;
			break;
		case RBE_BLIND:
			power = 2;
			break;
		case RBE_CONFUSE:
			power = 10;
			break;
		case RBE_TERRIFY:
			power = 10;
			break;
		case RBE_PARALYZE:
			power = 2;
			break;
		case RBE_LOSE_STR:
			power = 0;
			break;
		case RBE_LOSE_DEX:
			power = 0;
			break;
		case RBE_LOSE_CON:
			power = 0;
			break;
		case RBE_LOSE_INT:
			power = 0;
			break;
		case RBE_LOSE_WIS:
			power = 0;
			break;
		case RBE_LOSE_CHR:
			power = 0;
			break;
		case RBE_LOSE_ALL:
			power = 2;
			break;
		case RBE_SHATTER:
			power = 60;
			break;
		case RBE_EXP_10:
			power = 5;
			break;
		case RBE_EXP_20:
			power = 5;
			break;
		case RBE_EXP_40:
			power = 5;
			break;
		case RBE_EXP_80:
			power = 5;
			break;
		case RBE_DISEASE:
			power = 5;
			break;
		case RBE_TIME:
			power = 5;
			break;
		case RBE_SANITY:
			power = 60;
			break;
		case RBE_HALLU:
			power = 10;
			break;
		case RBE_PARASITE:
			power = 5;
			break;
		case RBE_ABOMINATION:
			power = 20;
			break;
		}


		/* Monster hits */
		if (!effect || check_hit2(power, rlev, ac))
		{
			/* Always disturbing */
			disturb(1, 0);

			/* Describe the attack method */
			switch (method)
			{
			case RBM_HIT:
				{
					act = "hits %s.";
					touched = TRUE;
					break;
				}

			case RBM_TOUCH:
				{
					act = "touches %s.";
					touched = TRUE;
					break;
				}

			case RBM_PUNCH:
				{
					act = "punches %s.";
					touched = TRUE;
					break;
				}

			case RBM_KICK:
				{
					act = "kicks %s.";
					touched = TRUE;
					break;
				}

			case RBM_CLAW:
				{
					act = "claws %s.";
					touched = TRUE;
					break;
				}

			case RBM_BITE:
				{
					act = "bites %s.";
					touched = TRUE;
					break;
				}

			case RBM_STING:
				{
					act = "stings %s.";
					touched = TRUE;
					break;
				}

			case RBM_XXX1:
				{
					act = "XXX1's %s.";
					break;
				}

			case RBM_BUTT:
				{
					act = "butts %s.";
					touched = TRUE;
					break;
				}

			case RBM_CRUSH:
				{
					act = "crushes %s.";
					touched = TRUE;
					break;
				}

			case RBM_ENGULF:
				{
					act = "engulfs %s.";
					touched = TRUE;
					break;
				}

			case RBM_CHARGE:
				{
					act = "charges %s.";
					touched = TRUE;
					break;
				}

			case RBM_CRAWL:
				{
					act = "crawls on %s.";
					touched = TRUE;
					break;
				}

			case RBM_DROOL:
				{
					act = "drools on %s.";
					touched = FALSE;
					break;
				}

			case RBM_SPIT:
				{
					act = "spits on %s.";
					touched = FALSE;
					break;
				}

			case RBM_GAZE:
				{
					act = "gazes at %s.";
					touched = FALSE;
					break;
				}

			case RBM_WAIL:
				{
					act = "wails at %s.";
					touched = FALSE;
					break;
				}

			case RBM_SPORE:
				{
					act = "releases spores at %s.";
					touched = FALSE;
					break;
				}

			case RBM_XXX4:
				{
					act = "projects XXX4's at %s.";
					touched = FALSE;
					break;
				}

			case RBM_BEG:
				{
					act = "begs %s for money.";
					touched = FALSE;
					t_ptr->csleep = 0;
					break;
				}

			case RBM_INSULT:
				{
					act = "insults %s.";
					touched = FALSE;
					t_ptr->csleep = 0;
					break;
				}

			case RBM_MOAN:
				{
					act = "moans at %s.";
					touched = FALSE;
					t_ptr->csleep = 0;
					break;
				}

			case RBM_SHOW:
				{
					act = "sings to %s.";
					touched = FALSE;
					t_ptr->csleep = 0;
					break;
				}
			}

			/* Message */
			if (act)
			{
				strfmt(temp, act, t_name);
				if (t_ptr->ml)
					msg_format("%s %s", sym_name, temp);

			}

			/* Hack -- assume all attacks are obvious */
			obvious = TRUE;

			/* Roll out the damage */
			damage = damroll(d_dice, d_side);

			pt = GF_MISSILE;

			/* Apply appropriate damage */
			switch (effect)
			{
			case 0:
				{
					damage = 0;
					pt = 0;
					break;
				}

			case RBE_HURT:
			case RBE_SANITY:
				{
					damage -= (damage * ((ac < 150) ? ac : 150) / 250);
					break;
				}

			case RBE_POISON:
			case RBE_DISEASE:
				{
					pt = GF_POIS;
					break;
				}

			case RBE_UN_BONUS:
			case RBE_UN_POWER:
			case RBE_ABOMINATION:
				{
					pt = GF_DISENCHANT;
					break;
				}

			case RBE_EAT_FOOD:
			case RBE_EAT_LITE:
				{
					pt = damage = 0;
					break;
				}

			case RBE_EAT_ITEM:
			case RBE_EAT_GOLD:
				{
					pt = damage = 0;
					if (randint(2) == 1) blinked = TRUE;
					break;
				}

			case RBE_ACID:
				{
					pt = GF_ACID;
					break;
				}

			case RBE_ELEC:
				{
					pt = GF_ELEC;
					break;
				}

			case RBE_FIRE:
				{
					pt = GF_FIRE;
					break;
				}

			case RBE_COLD:
				{
					pt = GF_COLD;
					break;
				}

			case RBE_BLIND:
				{
					break;
				}

			case RBE_CONFUSE:
			case RBE_HALLU:
				{
					pt = GF_CONFUSION;
					break;
				}

			case RBE_TERRIFY:
				{
					pt = GF_TURN_ALL;
					break;
				}

			case RBE_PARALYZE:
				{
					pt = GF_OLD_SLEEP; 	/* sort of close... */
					break;
				}

			case RBE_LOSE_STR:
			case RBE_LOSE_INT:
			case RBE_LOSE_WIS:
			case RBE_LOSE_DEX:
			case RBE_LOSE_CON:
			case RBE_LOSE_CHR:
			case RBE_LOSE_ALL:
			case RBE_PARASITE:
				{
					break;
				}

			case RBE_SHATTER:
				{
					if (damage > 23)
					{
						/* Prevent destruction of quest levels and town */
						if (!is_quest(dun_level) && dun_level)
							earthquake(p_ptr->py, p_ptr->px, 8);
					}
					break;
				}

			case RBE_EXP_10:
			case RBE_EXP_20:
			case RBE_EXP_40:
			case RBE_EXP_80:
				{
					pt = GF_NETHER;
					break;
				}

			case RBE_TIME:
				{
					pt = GF_TIME;
					break;
				}

			default:
				{
					pt = 0;
					break;
				}
			}

			if (pt)
			{
				/* Do damage if not exploding */
				project(0, 0, t_ptr->fy, t_ptr->fx,
				        (pt == GF_OLD_SLEEP ? r_ptr->level : damage), pt,
				        PROJECT_KILL | PROJECT_STOP);

				if (touched)
				{
					/* Aura fire */
					if ((tr_ptr->flags2 & RF2_AURA_FIRE) &&
					                !(r_ptr->flags3 & RF3_IM_FIRE))
					{
						if (t_ptr->ml)
						{
							blinked = FALSE;
							msg_format("You are suddenly very hot!");
							if (t_ptr->ml)
								tr_ptr->r_flags2 |= RF2_AURA_FIRE;
						}
						project(m_idx, 0, p_ptr->py, p_ptr->px,
						        damroll(1 + ((t_ptr->level) / 26),
						                1 + ((t_ptr->level) / 17)),
						        GF_FIRE, PROJECT_KILL | PROJECT_STOP);
					}

					/* Aura elec */
					if ((tr_ptr->flags2 & (RF2_AURA_ELEC)) &&
					                !(r_ptr->flags3 & (RF3_IM_ELEC)))
					{
						if (t_ptr->ml)
						{
							blinked = FALSE;
							msg_format("You get zapped!");
							if (t_ptr->ml)
								tr_ptr->r_flags2 |= RF2_AURA_ELEC;
						}
						project(m_idx, 0, p_ptr->py, p_ptr->px,
						        damroll(1 + ((t_ptr->level) / 26),
						                1 + ((t_ptr->level) / 17)),
						        GF_ELEC, PROJECT_KILL | PROJECT_STOP);
					}
				}
			}
		}

		/* Monster missed player */
		else
		{
			/* Analyze failed attacks */
			switch (method)
			{
			case RBM_HIT:
			case RBM_TOUCH:
			case RBM_PUNCH:
			case RBM_KICK:
			case RBM_CLAW:
			case RBM_BITE:
			case RBM_STING:
			case RBM_XXX1:
			case RBM_BUTT:
			case RBM_CRUSH:
			case RBM_ENGULF:
			case RBM_CHARGE:
				{
					/* Disturb */
					disturb(1, 0);

					/* Message */
					msg_format("%s misses %s.", sym_name, t_name);
					break;
				}
			}
		}


		/* Analyze "visible" monsters only */
		if (visible)
		{
			/* Count "obvious" attacks (and ones that cause damage) */
			if (obvious || damage || (r_ptr->r_blows[ap_cnt] > 10))
			{
				/* Count attacks of this type */
				if (r_ptr->r_blows[ap_cnt] < MAX_UCHAR)
				{
					r_ptr->r_blows[ap_cnt]++;
				}
			}
		}
	}

	/* Blink away */
	if (blinked)
	{
		msg_format("You and %s flee laughing!", symbiote_name(FALSE));

		teleport_player(MAX_SIGHT * 2 + 5);
	}
}

/*
 * Carried monster can attack too.
 * Based on monst_attack_monst.
 */
static void incarnate_monster_attack(s16b m_idx, bool *fear, bool *mdeath,
                                     int x, int y)
{
	monster_type *t_ptr = &m_list[m_idx];

	monster_race *r_ptr;

	monster_race *tr_ptr = race_inf(t_ptr);

	cave_type *c_ptr;

	int ap_cnt;

	int ac, rlev, pt;

	char t_name[80];

	char temp[80];

	bool blinked = FALSE, touched = FALSE;

	byte y_saver = t_ptr->fy;

	byte x_saver = t_ptr->fx;


	if (!p_ptr->body_monster) return;

	c_ptr = &cave[y][x];

	r_ptr = race_info_idx(p_ptr->body_monster, 0);

	/* Not allowed to attack */
	if (r_ptr->flags1 & RF1_NEVER_BLOW) return;

	/* Total armor */
	ac = t_ptr->ac;

	/* Extract the effective monster level */
	rlev = ((r_ptr->level >= 1) ? r_ptr->level : 1);

	/* Get the monster name (or "it") */
	monster_desc(t_name, t_ptr, 0);

	/* Assume no blink */
	blinked = FALSE;

	if (!t_ptr->ml)
	{
		msg_print("You hear noise.");
	}

	/* Scan through all four blows */
	for (ap_cnt = 0; ap_cnt < (p_ptr->num_blow > 4) ? 4 : p_ptr->num_blow;
	                ap_cnt++)
	{
		bool visible = FALSE;
		bool obvious = FALSE;

		int power = 0;
		int damage = 0;

		cptr act = NULL;

		/* Extract the attack infomation */
		int effect = r_ptr->blow[ap_cnt].effect;
		int method = r_ptr->blow[ap_cnt].method;
		int d_dice = r_ptr->blow[ap_cnt].d_dice;
		int d_side = r_ptr->blow[ap_cnt].d_side;

		/* Stop attacking if the target dies! */
		if (t_ptr->fx != x_saver || t_ptr->fy != y_saver)
			break;

		/* Hack -- no more attacks */
		if (!method) break;

		if (blinked)			/* Stop! */
		{
			/* break; */
		}

		/* Extract visibility (before blink) */
		visible = TRUE;

#if 0

		/* Extract visibility from carrying lite */
		if (r_ptr->flags9 & RF9_HAS_LITE) visible = TRUE;

#endif /* 0 */

		/* Extract the attack "power" */
		switch (effect)
		{
		case RBE_HURT:
			power = 60;
			break;
		case RBE_POISON:
			power = 5;
			break;
		case RBE_UN_BONUS:
			power = 20;
			break;
		case RBE_UN_POWER:
			power = 15;
			break;
		case RBE_EAT_GOLD:
			power = 5;
			break;
		case RBE_EAT_ITEM:
			power = 5;
			break;
		case RBE_EAT_FOOD:
			power = 5;
			break;
		case RBE_EAT_LITE:
			power = 5;
			break;
		case RBE_ACID:
			power = 0;
			break;
		case RBE_ELEC:
			power = 10;
			break;
		case RBE_FIRE:
			power = 10;
			break;
		case RBE_COLD:
			power = 10;
			break;
		case RBE_BLIND:
			power = 2;
			break;
		case RBE_CONFUSE:
			power = 10;
			break;
		case RBE_TERRIFY:
			power = 10;
			break;
		case RBE_PARALYZE:
			power = 2;
			break;
		case RBE_LOSE_STR:
			power = 0;
			break;
		case RBE_LOSE_DEX:
			power = 0;
			break;
		case RBE_LOSE_CON:
			power = 0;
			break;
		case RBE_LOSE_INT:
			power = 0;
			break;
		case RBE_LOSE_WIS:
			power = 0;
			break;
		case RBE_LOSE_CHR:
			power = 0;
			break;
		case RBE_LOSE_ALL:
			power = 2;
			break;
		case RBE_SHATTER:
			power = 60;
			break;
		case RBE_EXP_10:
			power = 5;
			break;
		case RBE_EXP_20:
			power = 5;
			break;
		case RBE_EXP_40:
			power = 5;
			break;
		case RBE_EXP_80:
			power = 5;
			break;
		case RBE_DISEASE:
			power = 5;
			break;
		case RBE_TIME:
			power = 5;
			break;
		case RBE_SANITY:
			power = 60;
			break;
		case RBE_HALLU:
			power = 10;
			break;
		case RBE_PARASITE:
			power = 5;
			break;
		}


		/* Monster hits */
		if (!effect || check_hit2(power, rlev, ac))
		{
			/* Always disturbing */
			disturb(1, 0);

			/* Describe the attack method */
			switch (method)
			{
			case RBM_HIT:
				{
					act = "hit %s.";
					touched = TRUE;
					break;
				}

			case RBM_TOUCH:
				{
					act = "touch %s.";
					touched = TRUE;
					break;
				}

			case RBM_PUNCH:
				{
					act = "punch %s.";
					touched = TRUE;
					break;
				}

			case RBM_KICK:
				{
					act = "kick %s.";
					touched = TRUE;
					break;
				}

			case RBM_CLAW:
				{
					act = "claw %s.";
					touched = TRUE;
					break;
				}

			case RBM_BITE:
				{
					act = "bite %s.";
					touched = TRUE;
					break;
				}

			case RBM_STING:
				{
					act = "sting %s.";
					touched = TRUE;
					break;
				}

			case RBM_XXX1:
				{
					act = "XXX1's %s.";
					break;
				}

			case RBM_BUTT:
				{
					act = "butt %s.";
					touched = TRUE;
					break;
				}

			case RBM_CRUSH:
				{
					act = "crush %s.";
					touched = TRUE;
					break;
				}

			case RBM_ENGULF:
				{
					act = "engulf %s.";
					touched = TRUE;
					break;
				}

			case RBM_CHARGE:
				{
					act = "charge %s.";
					touched = TRUE;
					break;
				}

			case RBM_CRAWL:
				{
					act = "crawl on %s.";
					touched = TRUE;
					break;
				}

			case RBM_DROOL:
				{
					act = "drool on %s.";
					touched = FALSE;
					break;
				}

			case RBM_SPIT:
				{
					act = "spit on %s.";
					touched = FALSE;
					break;
				}

			case RBM_GAZE:
				{
					act = "gaze at %s.";
					touched = FALSE;
					break;
				}

			case RBM_WAIL:
				{
					act = "wail at %s.";
					touched = FALSE;
					break;
				}

			case RBM_SPORE:
				{
					act = "release spores at %s.";
					touched = FALSE;
					break;
				}

			case RBM_XXX4:
				{
					act = "project XXX4's at %s.";
					touched = FALSE;
					break;
				}

			case RBM_BEG:
				{
					act = "beg %s for money.";
					touched = FALSE;
					t_ptr->csleep = 0;
					break;
				}

			case RBM_INSULT:
				{
					act = "insult %s.";
					touched = FALSE;
					t_ptr->csleep = 0;
					break;
				}

			case RBM_MOAN:
				{
					act = "moan at %s.";
					touched = FALSE;
					t_ptr->csleep = 0;
					break;
				}

			case RBM_SHOW:
				{
					act = "sing to %s.";
					touched = FALSE;
					t_ptr->csleep = 0;
					break;
				}
			}

			/* Message */
			if (act)
			{
				strfmt(temp, act, t_name);
				if (t_ptr->ml)
					msg_format("You %s", temp);

			}

			/* Hack -- assume all attacks are obvious */
			obvious = TRUE;

			/* Roll out the damage */
			damage = damroll(d_dice, d_side) + p_ptr->to_d;

			pt = GF_MISSILE;

			/* Apply appropriate damage */
			switch (effect)
			{
			case 0:
				{
					damage = 0;
					pt = 0;
					break;
				}

			case RBE_HURT:
			case RBE_SANITY:
				{
					damage -= (damage * ((ac < 150) ? ac : 150) / 250);
					break;
				}

			case RBE_POISON:
			case RBE_DISEASE:
				{
					pt = GF_POIS;
					break;
				}

			case RBE_UN_BONUS:
			case RBE_UN_POWER:
				{
					pt = GF_DISENCHANT;
					break;
				}

			case RBE_EAT_FOOD:
			case RBE_EAT_LITE:
				{
					pt = damage = 0;
					break;
				}

			case RBE_EAT_ITEM:
			case RBE_EAT_GOLD:
				{
					pt = damage = 0;
					if (randint(2) == 1) blinked = TRUE;
					break;
				}

			case RBE_ACID:
				{
					pt = GF_ACID;
					break;
				}

			case RBE_ELEC:
				{
					pt = GF_ELEC;
					break;
				}

			case RBE_FIRE:
				{
					pt = GF_FIRE;
					break;
				}

			case RBE_COLD:
				{
					pt = GF_COLD;
					break;
				}

			case RBE_BLIND:
				{
					break;
				}

			case RBE_HALLU:
			case RBE_CONFUSE:
				{
					pt = GF_CONFUSION;
					break;
				}

			case RBE_TERRIFY:
				{
					pt = GF_TURN_ALL;
					break;
				}

			case RBE_PARALYZE:
				{
					pt = GF_OLD_SLEEP; 	/* sort of close... */
					break;
				}

			case RBE_LOSE_STR:
			case RBE_LOSE_INT:
			case RBE_LOSE_WIS:
			case RBE_LOSE_DEX:
			case RBE_LOSE_CON:
			case RBE_LOSE_CHR:
			case RBE_LOSE_ALL:
			case RBE_PARASITE:
				{
					break;
				}

			case RBE_SHATTER:
				{
					if (damage > 23)
					{
						/* Prevent destruction of quest levels and town */
						if (!is_quest(dun_level) && dun_level)
							earthquake(p_ptr->py, p_ptr->px, 8);
					}
					break;
				}

			case RBE_EXP_10:
			case RBE_EXP_20:
			case RBE_EXP_40:
			case RBE_EXP_80:
				{
					pt = GF_NETHER;
					break;
				}

			case RBE_TIME:
				{
					pt = GF_TIME;
					break;
				}

			default:
				{
					pt = 0;
					break;
				}
			}

			if (pt)
			{
				/* Do damage if not exploding */
				project(0, 0, t_ptr->fy, t_ptr->fx,
				        (pt == GF_OLD_SLEEP ? p_ptr->lev * 2 : damage), pt,
				        PROJECT_KILL | PROJECT_STOP);

				if (touched)
				{
					/* Aura fire */
					if ((tr_ptr->flags2 & RF2_AURA_FIRE) &&
					                !(r_ptr->flags3 & RF3_IM_FIRE))
					{
						if (t_ptr->ml)
						{
							blinked = FALSE;
							msg_format("You are suddenly very hot!");
							if (t_ptr->ml)
								tr_ptr->r_flags2 |= RF2_AURA_FIRE;
						}
						project(m_idx, 0, p_ptr->py, p_ptr->px,
						        damroll(1 + ((t_ptr->level) / 26),
						                1 + ((t_ptr->level) / 17)),
						        GF_FIRE, PROJECT_KILL | PROJECT_STOP);
					}

					/* Aura elec */
					if ((tr_ptr->flags2 & (RF2_AURA_ELEC)) &&
					                !(r_ptr->flags3 & (RF3_IM_ELEC)))
					{
						if (t_ptr->ml)
						{
							blinked = FALSE;
							msg_format("You get zapped!");
							if (t_ptr->ml)
								tr_ptr->r_flags2 |= RF2_AURA_ELEC;
						}
						project(m_idx, 0, p_ptr->py, p_ptr->px,
						        damroll(1 + ((t_ptr->level) / 26),
						                1 + ((t_ptr->level) / 17)),
						        GF_ELEC, PROJECT_KILL | PROJECT_STOP);
					}

				}
			}
		}

		/* Monster missed player */
		else
		{
			/* Analyze failed attacks */
			switch (method)
			{
			case RBM_HIT:
			case RBM_TOUCH:
			case RBM_PUNCH:
			case RBM_KICK:
			case RBM_CLAW:
			case RBM_BITE:
			case RBM_STING:
			case RBM_XXX1:
			case RBM_BUTT:
			case RBM_CRUSH:
			case RBM_ENGULF:
			case RBM_CHARGE:
				{
					/* Disturb */
					disturb(1, 0);

					/* Message */
					msg_format("You miss %s.", t_name);

					break;
				}
			}
		}


		/* Analyze "visible" monsters only */
		if (visible)
		{
			/* Count "obvious" attacks (and ones that cause damage) */
			if (obvious || damage || (r_ptr->r_blows[ap_cnt] > 10))
			{
				/* Count attacks of this type */
				if (r_ptr->r_blows[ap_cnt] < MAX_UCHAR)
				{
					r_ptr->r_blows[ap_cnt]++;
				}
			}
		}
	}

	/* Blink away */
	if (blinked)
	{
		msg_print("You flee laughing!");

		teleport_player(MAX_SIGHT * 2 + 5);
	}
}


/*
 * Fetch an attack description from dam_*.txt files.
 */

static void flavored_attack(int percent, char *output)
{
	int insanity = (p_ptr->msane - p_ptr->csane) * 100 / p_ptr->msane;
	bool insane = (rand_int(100) < insanity);

	if (percent < 5)
	{
		if (!insane)
			strcpy(output, "You scratch %s.");
		else
			get_rnd_line("dam_none.txt", output);

	}
	else if (percent < 30)
	{
		if (!insane)
			strcpy(output, "You hit %s.");
		else
			get_rnd_line("dam_med.txt", output);
	}
	else if (percent < 60)
	{
		if (!insane)
			strcpy(output, "You wound %s.");
		else
			get_rnd_line("dam_lots.txt", output);
	}
	else if (percent < 95)
	{
		if (!insane)
			strcpy(output, "You cripple %s.");
		else
			get_rnd_line("dam_huge.txt", output);

	}
	else
	{
		if (!insane)
			strcpy(output, "You demolish %s.");
		else
			get_rnd_line("dam_xxx.txt", output);
	}
}


/*
 * Apply the special effects of an attack
 */
void attack_special(monster_type *m_ptr, s32b special, int dam)
{
	char m_name[80];

	monster_race *r_ptr = race_inf(m_ptr);


	/* Extract monster name (or "it") */
	monster_desc(m_name, m_ptr, 0);

	/* Special - Cut monster */
	if (special & SPEC_CUT)
	{
		/* Cut the monster */
		if (r_ptr->flags8 & (RF8_NO_CUT))
		{
			if (m_ptr->ml)
			{
				r_info[m_ptr->r_idx].r_flags8 |= (RF8_NO_CUT);
			}
		}
		else if (rand_int(100) >= r_ptr->level)
		{
			/* Already partially poisoned */
			if (m_ptr->bleeding) msg_format("%^s is bleeding more strongly.",
				                                m_name);
			/* Was not poisoned */
			else
				msg_format("%^s is bleeding.", m_name);

			m_ptr->bleeding += dam * 2;
		}
	}

	/* Special - Poison monster */
	if (special & SPEC_POIS)
	{
		/* Poison the monster */
		if (r_ptr->flags3 & (RF3_IM_POIS))
		{
			if (m_ptr->ml)
			{
				r_ptr->r_flags3 |= (RF3_IM_POIS);
			}
		}
		/* Notice susceptibility */
		else if (r_ptr->flags9 & (RF9_SUSCEP_POIS))
		{
			if (m_ptr->ml)
			{
				r_ptr->r_flags9 |= (RF9_SUSCEP_POIS);
			}
			/* Already partially poisoned */
			if (m_ptr->poisoned) msg_format("%^s is more poisoned.", m_name);
			/* Was not poisoned */
			else
				msg_format("%^s is poisoned.", m_name);

			m_ptr->poisoned += dam * 2;
		}
		else if (rand_int(100) >= r_ptr->level)
		{
			/* Already partially poisoned */
			if (m_ptr->poisoned) msg_format("%^s is more poisoned.", m_name);
			/* Was not poisoned */
			else
				msg_format("%^s is poisoned.", m_name);

			m_ptr->poisoned += dam;
		}
	}
}


/*
 * Bare handed attacks
 */
static void py_attack_hand(int *k, monster_type *m_ptr, s32b *special)
{
	s16b special_effect = 0, stun_effect = 0, times = 0;
	martial_arts *ma_ptr, *old_ptr, *blow_table = ma_blows;
	int resist_stun = 0, max = MAX_MA;
	monster_race *r_ptr = race_inf(m_ptr);
	char m_name[80];
	bool desc = FALSE;
	bool done_crit;
	int plev = p_ptr->lev;

	if ((!p_ptr->body_monster) && (p_ptr->mimic_form == resolve_mimic_name("Bear")) &&
	                (p_ptr->melee_style == SKILL_BEAR))
	{
		blow_table = bear_blows;
		max = MAX_BEAR;
		plev = get_skill(SKILL_BEAR);
	}
	if (p_ptr->melee_style == SKILL_HAND)
	{
		blow_table = ma_blows;
		max = MAX_MA;
		plev = get_skill(SKILL_HAND);
	}
	ma_ptr = &blow_table[0];
	old_ptr = &blow_table[0];

	/* Extract monster name (or "it") */
	monster_desc(m_name, m_ptr, 0);

	if (r_ptr->flags1 & RF1_UNIQUE) resist_stun += 88;
	if (r_ptr->flags3 & RF3_NO_CONF) resist_stun += 44;
	if (r_ptr->flags3 & RF3_NO_SLEEP) resist_stun += 44;
	if ((r_ptr->flags3 & RF3_UNDEAD) ||
	                (r_ptr->flags3 & RF3_NONLIVING)) resist_stun += 88;

	/* Attempt 'times' */
	for (times = 0; times < (plev < 7 ? 1 : plev / 7); times++)
	{
		do
		{
			ma_ptr = &blow_table[(randint(max)) - 1];
		}
		while ((ma_ptr->min_level > plev) || (randint(plev) < ma_ptr->chance));

		/* keep the highest level attack available we found */
		if ((ma_ptr->min_level > old_ptr->min_level) &&
		                !(p_ptr->stun || p_ptr->confused))
		{
			old_ptr = ma_ptr;

			if (wizard && cheat_xtra)
			{
				msg_print("Attack re-selected.");
			}
		}
		else
		{
			ma_ptr = old_ptr;
		}
	}

	*k = damroll(ma_ptr->dd, ma_ptr->ds);

	if (ma_ptr->effect & MA_KNEE)
	{
		if (r_ptr->flags1 & RF1_MALE)
		{
			if (!desc) msg_format("You hit %s in the groin with your knee!",
				                      m_name);
			sound(SOUND_PAIN);
			special_effect = MA_KNEE;
		}
		else if (!desc) msg_format(ma_ptr->desc, m_name);

		desc = TRUE;
	}
	if (ma_ptr->effect & MA_FULL_SLOW)
	{
		special_effect = MA_SLOW;
		if (!desc) msg_format(ma_ptr->desc, m_name);

		desc = TRUE;
	}
	if (ma_ptr->effect & MA_SLOW)
	{
		if (!
		                ((r_ptr->flags1 & RF1_NEVER_MOVE) ||
		                 strchr("UjmeEv$,DdsbBFIJQSXclnw!=?", r_ptr->d_char)))
		{
			if (!desc) msg_format("You kick %s in the ankle.", m_name);
			special_effect = MA_SLOW;
		}
		else if (!desc) msg_format(ma_ptr->desc, m_name);

		desc = TRUE;
	}
	if (ma_ptr->effect & MA_STUN)
	{
		if (ma_ptr->power)
		{
			stun_effect = (ma_ptr->power / 2) + randint(ma_ptr->power / 2);
		}

		if (!desc) msg_format(ma_ptr->desc, m_name);
		desc = TRUE;
	}
	if (ma_ptr->effect & MA_WOUND)
	{
		if (magik(ma_ptr->power))
		{
			*special |= SPEC_CUT;
		}
		if (!desc) msg_format(ma_ptr->desc, m_name);
		desc = TRUE;
	}

	*k = critical_norm(plev * (randint(10)), ma_ptr->min_level, *k, -1, &done_crit);

	if ((special_effect & MA_KNEE) && ((*k + p_ptr->to_d) < m_ptr->hp))
	{
		msg_format("%^s moans in agony!", m_name);
		stun_effect = 7 + randint(13);
		resist_stun /= 3;
	}
	if (((special_effect & MA_FULL_SLOW) || (special_effect & MA_SLOW)) &&
	                ((*k + p_ptr->to_d) < m_ptr->hp))
	{
		if (!(r_ptr->flags1 & RF1_UNIQUE) &&
		                (randint(plev) > m_ptr->level) && m_ptr->mspeed > 60)
		{
			msg_format("%^s starts limping slower.", m_name);
			m_ptr->mspeed -= 10;
		}
	}

	if (stun_effect && ((*k + p_ptr->to_d) < m_ptr->hp))
	{
		if (plev > randint(m_ptr->level + resist_stun + 10))
		{
			if (m_ptr->stunned)
				msg_format("%^s is still stunned.", m_name);
			else
				msg_format("%^s is stunned.", m_name);

			m_ptr->stunned += (stun_effect);
		}
	}
}


/*
 * Apply nazgul effects
 */
void do_nazgul(int *k, int *num, int num_blow, int weap, monster_race *r_ptr,
               object_type *o_ptr)
{
	u32b f1, f2, f3, f4, f5, esp;

	bool mundane;
	bool allow_shatter = TRUE;

	/* Extract mundane-ness of the current weapon */
	object_flags(o_ptr, &f1, &f2, &f3, &f4, &f5, &esp);

	/* It should be Slay Evil, Slay Undead, or *Slay Undead* */
	mundane = !(f1 & TR1_SLAY_EVIL) && !(f1 & TR1_SLAY_UNDEAD) &&
	          !(f5 & TR5_KILL_UNDEAD);

	/* Some blades can resist shattering */
	if (f5 & TR5_RES_MORGUL)
		allow_shatter = FALSE;

	/* Mega Hack -- Hitting Nazgul is REALY dangerous (ideas from Akhronath) */
	if (r_ptr->flags7 & RF7_NAZGUL)
	{
		if ((!o_ptr->name2) && (!artifact_p(o_ptr)) && allow_shatter)
		{
			msg_print("Your weapon *DISINTEGRATES*!");
			*k = 0;
			inven_item_increase(INVEN_WIELD + weap, -1);
			inven_item_optimize(INVEN_WIELD + weap);

			/* To stop attacking */
			*num = num_blow;
		}
		else if (o_ptr->name2)
		{
			if (mundane)
			{
				msg_print
				("The Ringwraith is IMPERVIOUS to the mundane weapon.");
				*k = 0;
			}

			/* 25% chance of getting destroyed */
			if (magik(25) && allow_shatter)
			{
				msg_print("Your weapon is destroyed !");
				inven_item_increase(INVEN_WIELD + weap, -1);
				inven_item_optimize(INVEN_WIELD + weap);

				/* To stop attacking */
				*num = num_blow;
			}
		}
		else if (artifact_p(o_ptr))
		{
			if (mundane)
			{
				msg_print
				("The Ringwraith is IMPERVIOUS to the mundane weapon.");
				*k = 0;
			}

			apply_disenchant(INVEN_WIELD + weap);

			/* 1/1000 chance of getting destroyed */
			if (!rand_int(1000) && allow_shatter)
			{
				msg_print("Your weapon is destroyed !");
				inven_item_increase(INVEN_WIELD + weap, -1);
				inven_item_optimize(INVEN_WIELD + weap);

				/* To stop attacking */
				*num = num_blow;
			}
		}

		/* If any damage is done, then 25% chance of getting the Black Breath */
		if (*k)
		{
			if (magik(25))
			{
				msg_print("Your foe calls upon your soul!");
				msg_print
				("You feel the Black Breath slowly draining you of life...");
				p_ptr->black_breath = TRUE;
			}
		}
	}
}


/*
 * Player attacks a (poor, defenseless) creature        -RAK-
 *
 * If no "weapon" is available, then "punch" the monster one time.
 */
void py_attack(int y, int x, int max_blow)
{
	int num = 0, k, bonus, chance;

	s32b special = 0;

	cave_type *c_ptr = &cave[y][x];

	monster_type *m_ptr = &m_list[c_ptr->m_idx];

	monster_race *r_ptr = race_inf(m_ptr);

	object_type *o_ptr;

	char m_name[80];

	bool fear = FALSE;

	bool mdeath = FALSE;

	bool backstab = FALSE;

	bool vorpal_cut = FALSE;

	int chaos_effect = 0;

	bool stab_fleeing = FALSE;

	bool do_quake = FALSE;

	bool done_crit = FALSE;

	bool drain_msg = TRUE;

	int drain_result = 0, drain_heal = 0;

	int drain_left = MAX_VAMPIRIC_DRAIN;

	/* A massive hack -- life-draining weapons */
	u32b f1, f2, f3, f4, f5, esp;

	bool no_extra = FALSE;

	int weap;

	/* Disturb the player */
	disturb(0, 0);

	if (r_info[p_ptr->body_monster].flags1 & RF1_NEVER_BLOW)
	{
		msg_print("You cannot attack in this form!");
		return;
	}

	if (get_skill(SKILL_BACKSTAB))
	{
		if ((m_ptr->csleep) && (m_ptr->ml))
		{
			/* Can't backstab creatures that we can't see, right? */
			backstab = TRUE;
		}
		else if ((m_ptr->monfear) && (m_ptr->ml))
		{
			stab_fleeing = TRUE;
		}
	}

	/* Disturb the monster */
	m_ptr->csleep = 0;


	/* Extract monster name (or "it") */
	monster_desc(m_name, m_ptr, 0);

	/* Dont even bother */
	if (r_ptr->flags7 & RF7_IM_MELEE)
	{
		msg_format("%^s is immune to melee attacks.");
		return;
	}

	/* Auto-Recall if possible and visible */
	if (m_ptr->ml) monster_race_track(m_ptr->r_idx, m_ptr->ego);

	/* Track a new monster */
	if (m_ptr->ml) health_track(c_ptr->m_idx);

	/* Stop if friendly */
	if ((is_friend(m_ptr) >= 0) &&
	                !(p_ptr->stun || p_ptr->confused || p_ptr->image ||
	                  !(m_ptr->ml)))
	{
		if (!(p_ptr->inventory[INVEN_WIELD].art_name))
		{
			msg_format("You stop to avoid hitting %s.", m_name);
			return;
		}

		if (!
		                (streq
		                 (quark_str(p_ptr->inventory[INVEN_WIELD].art_name), "'Stormbringer'")))
		{
			msg_format("You stop to avoid hitting %s.", m_name);
			return;
		}

		msg_format("Your black blade greedily attacks %s!", m_name);
	}

	/* Break goi/manashield */
	if (p_ptr->invuln)
	{
		set_invuln(0);
	}
	if (p_ptr->disrupt_shield)
	{
		set_disrupt_shield(0);
	}

	/* Handle player fear */
	if (p_ptr->afraid)
	{
		/* Message */
		if (m_ptr->ml)
			msg_format("You are too afraid to attack %s!", m_name);
		else
			msg_format("There is something scary in your way!");

		/* Done */
		return;
	}

	/* Monsters can use barehanded combat, but not weapon combat */
	if ((p_ptr->body_monster) &&
	                (!r_info[p_ptr->body_monster].body_parts[BODY_WEAPON]) &&
	                !(p_ptr->melee_style == SKILL_HAND))
	{
		incarnate_monster_attack(c_ptr->m_idx, &fear, &mdeath, y, x);
	}
	/* Otherwise use your weapon(s) */
	else
	{
		int weapons;
		if (p_ptr->melee_style == SKILL_MASTERY)
			weapons = r_info[p_ptr->body_monster].body_parts[BODY_WEAPON];
		else /* SKILL_HAND */
			weapons = 1;

		/* Attack with ALL the weapons !!!!! -- ooh that's gonna hurt YOU */
		for (weap = 0; weap < weapons; ++weap)
		{
			/* Monster is already dead ? oh :( */
			if (mdeath) break;

			/* Reset the blows counter */
			num = 0;

			/* Access the weapon */
			o_ptr = &p_ptr->inventory[INVEN_WIELD + weap];

			/* Calculate the "attack quality" */
			bonus = p_ptr->to_h + p_ptr->to_h_melee + o_ptr->to_h;
			chance = p_ptr->skill_thn + (bonus * BTH_PLUS_ADJ);

			object_flags(o_ptr, &f1, &f2, &f3, &f4, &f5, &esp);

			if (!(f4 & TR4_NEVER_BLOW))
			{
				int num_blow = p_ptr->num_blow;

				/* Restrict to max_blow(if max_blow >= 0) */
				if ((max_blow >= 0) &&
				                (num_blow > max_blow)) num_blow = max_blow;

				/* Attack once for each legal blow */
				while (num++ < num_blow)
				{
					/* Test for hit */
					if (test_hit_norm(chance, m_ptr->ac, m_ptr->ml))
					{
						/* Sound */
						sound(SOUND_HIT);

						/* Hack -- bare hands do one damage */
						k = 1;

						/* Select a chaotic effect (50% chance) */
						if ((f1 & TR1_CHAOTIC) && (rand_int(2) == 0))
						{
							if (randint(5) < 3)
							{
								/* Vampiric (20%) */
								chaos_effect = 1;
							}
							else if (rand_int(250) == 0)
							{
								/* Quake (0.12%) */
								chaos_effect = 2;
							}
							else if (rand_int(10))
							{
								/* Confusion (26.892%) */
								chaos_effect = 3;
							}
							else if (rand_int(2) == 0)
							{
								/* Teleport away (1.494%) */
								chaos_effect = 4;
							}
							else
							{
								/* Polymorph (1.494%) */
								chaos_effect = 5;
							}
						}

						/* Vampiric drain */
						if ((f1 & TR1_VAMPIRIC) || (chaos_effect == 1))
						{
							if (!
							                ((r_ptr->flags3 & RF3_UNDEAD) ||
							                 (r_ptr->flags3 & RF3_NONLIVING)))
								drain_result = m_ptr->hp;
							else
								drain_result = 0;
						}

						if (f1 & TR1_VORPAL && (randint(6) == 1))
							vorpal_cut = TRUE;
						else
							vorpal_cut = FALSE;

						/* Should we attack with hands or not ? */
						if (p_ptr->melee_style != SKILL_MASTERY)
						{
							py_attack_hand(&k, m_ptr, &special);
						}
						/* Handle normal weapon */
						else if (o_ptr->k_idx)
						{
							k = damroll(o_ptr->dd, o_ptr->ds);
							k = tot_dam_aux(o_ptr, k, m_ptr, &special);

							if (backstab)
							{
								k += (k *
								      get_skill_scale(SKILL_BACKSTAB,
								                      100)) / 100;
							}
							else if (stab_fleeing)
							{
								k += (k * get_skill_scale(SKILL_BACKSTAB, 70)) /
								     100;
							}

							if ((p_ptr->impact && ((k > 50) || randint(7) == 1))
							                || (chaos_effect == 2))
							{
								do_quake = TRUE;
							}

							k = critical_norm(o_ptr->weight, o_ptr->to_h, k, o_ptr->tval, &done_crit);

							/* Stunning blow */
							if (magik(get_skill(SKILL_STUN)) && (o_ptr->tval == TV_HAFTED) && (o_ptr->weight > 50) && done_crit)
							{
								if (!(r_ptr->flags4 & (RF4_BR_SOUN)) && !(r_ptr->flags4 & (RF4_BR_WALL)) && k)
								{
									int tmp;

									/* Get stunned */
									if (m_ptr->stunned)
									{
										msg_format("%^s is more dazed.", m_name);
										tmp = m_ptr->stunned + get_skill_scale(SKILL_STUN, 30) + 10;
									}
									else
									{
										msg_format("%^s is dazed.", m_name);
										tmp = get_skill_scale(SKILL_STUN, 60) + 20;
									}

									/* Apply stun */
									m_ptr->stunned = (tmp < 200) ? tmp : 200;
								}
							}

							if (vorpal_cut)
							{
								int step_k = k;

								msg_format("Your weapon cuts deep into %s!",
								           m_name);
								do
								{
									k += step_k;
								}
								while (randint(4) == 1);
							}

							PRAY_GOD(GOD_TULKAS)
							{
								if (magik(wisdom_scale(130) - m_ptr->level) && (p_ptr->grace > 1000))
								{
									msg_print("You feel the hand of Tulkas helping your blow.");
									k += (o_ptr->to_d + p_ptr->to_d_melee) * 2;
								}
								else k += o_ptr->to_d + p_ptr->to_d_melee;
							}
							else k += o_ptr->to_d;

							/* Project some more nasty stuff? */
							if (p_ptr->tim_project)
							{
								project(0, p_ptr->tim_project_rad, y, x, p_ptr->tim_project_dam, p_ptr->tim_project_gf, p_ptr->tim_project_flag | PROJECT_JUMP);
								if (!c_ptr->m_idx)
								{
									mdeath = TRUE;
									break;
								}
							}

							do_nazgul(&k, &num, num_blow, weap, r_ptr, o_ptr);

						}

						/* Melkor can cast curse for you*/
						PRAY_GOD(GOD_MELKOR)
						{
							int lv = exec_lua("return get_level(MELKOR_CURSE, 100)");

							if (lv >= 10)
							{
								int chance = (wisdom_scale(30) * lv) / ((m_ptr->level < 1) ? 1 : m_ptr->level);

								if (chance < 1) chance = 1;
								if ((p_ptr->grace > 5000) && magik(chance))
								{
									exec_lua(format("do_melkor_curse(%d)", c_ptr->m_idx));
								}
							}
						}

						/* May it clone the monster ? */
						if ((f4 & TR4_CLONE) && magik(30))
						{
							msg_format("Oh no ! Your weapon clones %^s!",
							           m_name);
							multiply_monster(c_ptr->m_idx, FALSE, TRUE);
						}

						/* Apply the player damage bonuses */
						k += p_ptr->to_d + p_ptr->to_d_melee;

						/* No negative damage */
						if (k < 0) k = 0;

						/* Message */
						if (!(backstab || stab_fleeing))
						{
							/* These monsters never have flavoured combat msgs */
							if (strchr("vwjmelX,.*", r_ptr->d_char))
							{
								msg_format("You hit %s.", m_name);
							}

							/* Print flavoured messages if requested */
							else
							{
								char buff[255];

								flavored_attack((100 * k) / m_ptr->maxhp, buff);
								msg_format(buff, m_name);
							}
						}
						else if (backstab)
						{
							char buf[80];

							monster_race_desc(buf, m_ptr->r_idx, m_ptr->ego);

							backstab = FALSE;

							msg_format
							("You cruelly stab the helpless, sleeping %s!",
							 buf);
						}
						else
						{
							char buf[80];

							monster_race_desc(buf, m_ptr->r_idx, m_ptr->ego);

							msg_format("You backstab the fleeing %s!", buf);
						}

						/* Complex message */
						if (wizard)
						{
							msg_format("You do %d (out of %d) damage.", k,
							           m_ptr->hp);
						}

						if (special) attack_special(m_ptr, special, k);

						/* Damage, check for fear and death */
						if (mon_take_hit(c_ptr->m_idx, k, &fear, NULL))
						{
							/* Hack -- High-level warriors can spread their attacks out
							 * among weaker foes.
							 */
							if ((has_ability(AB_SPREAD_BLOWS)) && (num < num_blow) &&
							                (energy_use))
							{
								energy_use = energy_use * num / num_blow;
							}
							mdeath = TRUE;
							break;
						}

						switch (is_friend(m_ptr))
						{
						case 1:
							msg_format("%^s gets angry!", m_name);
							change_side(m_ptr);
							break;
						case 0:
							msg_format("%^s gets angry!", m_name);
							m_ptr->status = MSTATUS_NEUTRAL_M;
							break;
						}

						touch_zap_player(m_ptr);

						/* Are we draining it?  A little note: If the monster is
						   dead, the drain does not work... */

						if (drain_result)
						{
							drain_result -= m_ptr->hp; 	/* Calculate the difference */

							if (drain_result > 0)	/* Did we really hurt it? */
							{
								drain_heal = damroll(4, (drain_result / 6));

								if (cheat_xtra)
								{
									msg_format("Draining left: %d", drain_left);
								}

								if (drain_left)
								{
									if (drain_heal < drain_left)
									{
										drain_left -= drain_heal;
									}
									else
									{
										drain_heal = drain_left;
										drain_left = 0;
									}

									if (drain_msg)
									{
										msg_format
										("Your weapon drains life from %s!",
										 m_name);
										drain_msg = FALSE;
									}

									hp_player(drain_heal);
									/* We get to keep some of it! */
								}
							}
						}

						/* Confusion attack */
						if ((p_ptr->confusing) || (chaos_effect == 3))
						{
							/* Cancel glowing hands */
							if (p_ptr->confusing)
							{
								p_ptr->confusing = FALSE;
								msg_print("Your hands stop glowing.");
							}

							/* Confuse the monster */
							if (r_ptr->flags3 & (RF3_NO_CONF))
							{
								if (m_ptr->ml)
								{
									r_ptr->r_flags3 |= (RF3_NO_CONF);
								}

								msg_format("%^s is unaffected.", m_name);
							}
							else if (rand_int(100) < m_ptr->level)
							{
								msg_format("%^s is unaffected.", m_name);
							}
							else
							{
								msg_format("%^s appears confused.", m_name);
								m_ptr->confused +=
								        10 + rand_int(get_skill(SKILL_COMBAT)) / 5;
							}
						}

						else if (chaos_effect == 4)
						{
							msg_format("%^s disappears!", m_name);
							teleport_away(c_ptr->m_idx, 50);
							num = num_blow + 1; 	/* Can't hit it anymore! */
							no_extra = TRUE;
						}

						else if ((chaos_effect == 5) && cave_floor_bold(y, x) &&
						                (randint(90) > m_ptr->level))
						{
							if (!((r_ptr->flags1 & RF1_UNIQUE) ||
							                (r_ptr->flags4 & RF4_BR_CHAO) ||
							                (m_ptr->mflag & MFLAG_QUEST)))
							{
								/* Handle polymorph */
								if (do_poly_monster(y, x))
								{
									/* Polymorph succeeded */
									msg_format("%^s changes!", m_name);

									/* Hack -- Get new monster */
									m_ptr = &m_list[c_ptr->m_idx];

									/* Oops, we need a different name... */
									monster_desc(m_name, m_ptr, 0);

									/* Hack -- Get new race */
									r_ptr = race_inf(m_ptr);

									fear = FALSE;
								}
								else
								{
									msg_format("%^s resists.", m_name);
								}
							}
							else
							{
								msg_format("%^s is unaffected.", m_name);
							}
						}
					}

					/* Player misses */
					else
					{
						/* Sound */
						sound(SOUND_MISS);

						backstab = FALSE; 	/* Clumsy! */

						/* Message */
						msg_format("You miss %s.", m_name);
					}
				}
			}
			else
			{
				msg_print("You can't attack with that weapon.");
			}
		}
	}

	/* Carried monster can attack too */
	if ((!mdeath) && m_list[c_ptr->m_idx].hp)
		carried_monster_attack(c_ptr->m_idx, &fear, &mdeath, y, x);

	/* Hack -- delay fear messages */
	if (fear && m_ptr->ml)
	{
		/* Sound */
		sound(SOUND_FLEE);

		/* Message */
		msg_format("%^s flees in terror!", m_name);
	}

	/* Mega-Hack -- apply earthquake brand */
	if (do_quake)
	{
		/* Prevent destruction of quest levels and town */
		if (!is_quest(dun_level) && dun_level)
			earthquake(p_ptr->py, p_ptr->px, 10);
	}
}



static bool pattern_tile(int y, int x)
{
	return ((cave[y][x].feat <= FEAT_PATTERN_XTRA2) &&
	        (cave[y][x].feat >= FEAT_PATTERN_START));
}


static bool pattern_seq(int c_y, int c_x, int n_y, int n_x)
{
	if (!(pattern_tile(c_y, c_x)) && !(pattern_tile(n_y, n_x)))
		return TRUE;

	if (cave[n_y][n_x].feat == FEAT_PATTERN_START)
	{
		if ((!(pattern_tile(c_y, c_x))) &&
		                !(p_ptr->confused || p_ptr->stun || p_ptr->image))
		{
			if (get_check
			                ("If you start walking the Straight Road, you must walk the whole way. Ok? "))
				return TRUE;
			else
				return FALSE;
		}
		else
			return TRUE;
	}
	else if ((cave[n_y][n_x].feat == FEAT_PATTERN_OLD) ||
	                (cave[n_y][n_x].feat == FEAT_PATTERN_END) ||
	                (cave[n_y][n_x].feat == FEAT_PATTERN_XTRA2))
	{
		if (pattern_tile(c_y, c_x))
		{
			return TRUE;
		}
		else
		{
			msg_print
			("You must start walking the Straight Road from the startpoint.");
			return FALSE;
		}
	}
	else if ((cave[n_y][n_x].feat == FEAT_PATTERN_XTRA1) ||
	                (cave[c_y][c_x].feat == FEAT_PATTERN_XTRA1))
	{
		return TRUE;
	}
	else if (cave[c_y][c_x].feat == FEAT_PATTERN_START)
	{
		if (pattern_tile(n_y, n_x))
			return TRUE;
		else
		{
			msg_print("You must walk the Straight Road in correct order.");
			return FALSE;
		}
	}
	else if ((cave[c_y][c_x].feat == FEAT_PATTERN_OLD) ||
	                (cave[c_y][c_x].feat == FEAT_PATTERN_END) ||
	                (cave[c_y][c_x].feat == FEAT_PATTERN_XTRA2))
	{
		if (!pattern_tile(n_y, n_x))
		{
			msg_print("You may not step off from the Straight Road.");
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}
	else
	{
		if (!pattern_tile(c_y, c_x))
		{
			msg_print
			("You must start walking the Straight Road from the startpoint.");
			return FALSE;
		}
		else
		{
			byte ok_move = FEAT_PATTERN_START;
			switch (cave[c_y][c_x].feat)
			{
			case FEAT_PATTERN_1:
				ok_move = FEAT_PATTERN_2;
				break;
			case FEAT_PATTERN_2:
				ok_move = FEAT_PATTERN_3;
				break;
			case FEAT_PATTERN_3:
				ok_move = FEAT_PATTERN_4;
				break;
			case FEAT_PATTERN_4:
				ok_move = FEAT_PATTERN_1;
				break;
			default:
				if (wizard)
					msg_format("Funny Straight Road walking, %d.",
					           cave[c_y][c_x]);
				return TRUE; 	/* Goof-up */
			}

			if ((cave[n_y][n_x].feat == ok_move) ||
			                (cave[n_y][n_x].feat == cave[c_y][c_x].feat))
				return TRUE;
			else
			{
				if (!pattern_tile(n_y, n_x))
					msg_print("You may not step off from the Straight Road.");
				else
					msg_print
					("You must walk the Straight Road in correct order.");

				return FALSE;
			}
		}
	}
}



bool player_can_enter(byte feature)
{
	bool pass_wall;

	bool only_wall = FALSE;


	/* Player can not walk through "walls" unless in Shadow Form */
	if (p_ptr->wraith_form || (PRACE_FLAG(PR1_SEMI_WRAITH)))
		pass_wall = TRUE;
	else
		pass_wall = FALSE;

	/* Wall mimicry force the player to stay in walls */
	if (p_ptr->mimic_extra & CLASS_WALL)
	{
		only_wall = TRUE;
	}

	/* Don't let the player kill himself with one keystroke */
	if (p_ptr->wild_mode)
	{
		if (feature == FEAT_DEEP_WATER)
		{
			int wt = weight_limit() / 2;

			if ((calc_total_weight() >= wt) && !(p_ptr->ffall))
				return (FALSE);
		}
		else if (feature == FEAT_SHAL_LAVA ||
		                feature == FEAT_DEEP_LAVA)
		{
			if (!(p_ptr->resist_fire ||
			                p_ptr->immune_fire ||
			                p_ptr->oppose_fire ||
			                p_ptr->ffall))
				return (FALSE);
		}
	}

	if (feature == FEAT_TREES)
	{
		if ((p_ptr->fly ||
		                pass_wall ||
		                (has_ability(AB_TREE_WALK)) ||
		                (p_ptr->mimic_form == resolve_mimic_name("Ent")) ||
		                ((p_ptr->grace >= 9000) && (p_ptr->praying) && (p_ptr->pgod == GOD_YAVANNA))))
			return (TRUE);
	}

	if ((p_ptr->climb) && (f_info[feature].flags1 & FF1_CAN_CLIMB))
		return (TRUE);
	if ((p_ptr->fly) &&
	                ((f_info[feature].flags1 & FF1_CAN_FLY) ||
	                 (f_info[feature].flags1 & FF1_CAN_LEVITATE)))
		return (TRUE);
	else if (only_wall && (f_info[feature].flags1 & FF1_FLOOR))
		return (FALSE);
	else if ((p_ptr->ffall) &&
	                (f_info[feature].flags1 & FF1_CAN_LEVITATE))
		return (TRUE);
	else if ((pass_wall || only_wall) &&
	                (f_info[feature].flags1 & FF1_CAN_PASS))
		return (TRUE);
	else if (f_info[feature].flags1 & FF1_NO_WALK)
		return (FALSE);
	else if ((f_info[feature].flags1 & FF1_WEB) &&
	                ((!(r_info[p_ptr->body_monster].flags7 & RF7_SPIDER)) && (p_ptr->mimic_form != resolve_mimic_name("Spider"))))
		return (FALSE);

	return (TRUE);
}

/*
 * Move player in the given direction, with the given "pickup" flag.
 *
 * This routine should (probably) always induce energy expenditure.
 *
 * Note that moving will *always* take a turn, and will *always* hit
 * any monster which might be in the destination grid.  Previously,
 * moving into walls was "free" and did NOT hit invisible monsters.
 */
void move_player_aux(int dir, int do_pickup, int run, bool disarm)
{
	int y, x, tmp;

	cave_type *c_ptr = &cave[p_ptr->py][p_ptr->px];

	monster_type *m_ptr;

	monster_race *r_ptr = &r_info[p_ptr->body_monster], *mr_ptr;

	char m_name[80];

	bool stormbringer = FALSE;

	bool old_dtrap, new_dtrap;

	bool oktomove = TRUE;


	/* Hack - random movement */
	if (p_ptr->disembodied)
		tmp = dir;
	else if ((r_ptr->flags1 & RF1_RAND_25) && (r_ptr->flags1 & RF1_RAND_50))
	{
		if (randint(100) < 75)
			tmp = randint(9);
		else
			tmp = dir;
	}
	else if (r_ptr->flags1 & RF1_RAND_50)
	{
		if (randint(100) < 50)
			tmp = randint(9);
		else
			tmp = dir;
	}
	else if (r_ptr->flags1 & RF1_RAND_25)
	{
		if (randint(100) < 25)
			tmp = randint(9);
		else
			tmp = dir;
	}
	else
	{
		tmp = dir;
	}

	if ((c_ptr->feat == FEAT_ICE) && (!p_ptr->ffall && !p_ptr->fly))
	{
		if (magik(70 - p_ptr->lev))
		{
			tmp = randint(9);
			msg_print("You slip on the icy floor.");
		}
		else
			tmp = dir;
	}

	/* Find the result of moving */
	y = p_ptr->py + ddy[tmp];
	x = p_ptr->px + ddx[tmp];

	/* Examine the destination */
	c_ptr = &cave[y][x];

	/* Change oldpx and oldpy to place the player well when going back to big mode */
	if (p_ptr->wild_mode)
	{
		if (ddy[tmp] > 0) p_ptr->oldpy = 1;
		if (ddy[tmp] < 0) p_ptr->oldpy = MAX_HGT - 2;
		if (ddy[tmp] == 0) p_ptr->oldpy = MAX_HGT / 2;
		if (ddx[tmp] > 0) p_ptr->oldpx = 1;
		if (ddx[tmp] < 0) p_ptr->oldpx = MAX_WID - 2;
		if (ddx[tmp] == 0) p_ptr->oldpx = MAX_WID / 2;
	}

	/* Exit the area */
	if (!dun_level && !p_ptr->wild_mode && !is_quest(dun_level) &&
	                ((x == 0) || (x == cur_wid - 1) || (y == 0) || (y == cur_hgt - 1)))
	{
		/* Can the player enter the grid? */
		if (player_can_enter(c_ptr->mimic))
		{
			/* Hack: move to new area */
			if ((y == 0) && (x == 0))
			{
				p_ptr->wilderness_y--;
				p_ptr->wilderness_x--;
				p_ptr->oldpy = cur_hgt - 2;
				p_ptr->oldpx = cur_wid - 2;
				ambush_flag = FALSE;
			}

			else if ((y == 0) && (x == MAX_WID - 1))
			{
				p_ptr->wilderness_y--;
				p_ptr->wilderness_x++;
				p_ptr->oldpy = cur_hgt - 2;
				p_ptr->oldpx = 1;
				ambush_flag = FALSE;
			}

			else if ((y == MAX_HGT - 1) && (x == 0))
			{
				p_ptr->wilderness_y++;
				p_ptr->wilderness_x--;
				p_ptr->oldpy = 1;
				p_ptr->oldpx = cur_wid - 2;
				ambush_flag = FALSE;
			}

			else if ((y == MAX_HGT - 1) && (x == MAX_WID - 1))
			{
				p_ptr->wilderness_y++;
				p_ptr->wilderness_x++;
				p_ptr->oldpy = 1;
				p_ptr->oldpx = 1;
				ambush_flag = FALSE;
			}

			else if (y == 0)
			{
				p_ptr->wilderness_y--;
				p_ptr->oldpy = cur_hgt - 2;
				p_ptr->oldpx = x;
				ambush_flag = FALSE;
			}

			else if (y == cur_hgt - 1)
			{
				p_ptr->wilderness_y++;
				p_ptr->oldpy = 1;
				p_ptr->oldpx = x;
				ambush_flag = FALSE;
			}

			else if (x == 0)
			{
				p_ptr->wilderness_x--;
				p_ptr->oldpx = cur_wid - 2;
				p_ptr->oldpy = y;
				ambush_flag = FALSE;
			}

			else if (x == cur_wid - 1)
			{
				p_ptr->wilderness_x++;
				p_ptr->oldpx = 1;
				p_ptr->oldpy = y;
				ambush_flag = FALSE;
			}

			p_ptr->leaving = TRUE;

			return;
		}
	}

	/* Some hooks */
	if (process_hooks(HOOK_MOVE, "(d,d)", y, x)) return;

	/* Get the monster */
	m_ptr = &m_list[c_ptr->m_idx];
	mr_ptr = race_inf(m_ptr);

	if (p_ptr->inventory[INVEN_WIELD].art_name)
	{
		if (streq(quark_str(p_ptr->inventory[INVEN_WIELD].art_name), "'Stormbringer'"))
			stormbringer = TRUE;
	}

	/* Hack -- attack monsters */
	if (c_ptr->m_idx && (m_ptr->ml || player_can_enter(c_ptr->feat)))
	{

		/* Attack -- only if we can see it OR it is not in a wall */
		if ((is_friend(m_ptr) > 0) &&
		                !(p_ptr->confused || p_ptr->image || !(m_ptr->ml) || p_ptr->stun) &&
		                (pattern_seq(p_ptr->py, p_ptr->px, y, x)) &&
		                ((player_can_enter(cave[y][x].feat))))
		{
			m_ptr->csleep = 0;

			/* Extract monster name (or "it") */
			monster_desc(m_name, m_ptr, 0);

			/* Auto-Recall if possible and visible */
			if (m_ptr->ml) monster_race_track(m_ptr->r_idx, m_ptr->ego);

			/* Track a new monster */
			if (m_ptr->ml) health_track(c_ptr->m_idx);

			/* displace? */
			if (stormbringer && (randint(1000) > 666))
			{
				py_attack(y, x, -1);
			}
			else if (cave_floor_bold(p_ptr->py, p_ptr->px) ||
			                (mr_ptr->flags2 & RF2_PASS_WALL))
			{
				msg_format("You push past %s.", m_name);
				m_ptr->fy = p_ptr->py;
				m_ptr->fx = p_ptr->px;
				cave[p_ptr->py][p_ptr->px].m_idx = c_ptr->m_idx;
				c_ptr->m_idx = 0;
				update_mon(cave[p_ptr->py][p_ptr->px].m_idx, TRUE);
			}
			else
			{
				msg_format("%^s is in your way!", m_name);
				energy_use = 0;
				oktomove = FALSE;
			}

			/* now continue on to 'movement' */
		}
		else
		{
			py_attack(y, x, -1);
			oktomove = FALSE;
		}
	}

	else if ((c_ptr->feat == FEAT_DARK_PIT) && !p_ptr->ffall)
	{
		msg_print("You can't cross the chasm.");
		running = 0;
		oktomove = FALSE;
	}

#ifdef ALLOW_EASY_DISARM		/* TNB */

	/* Disarm a visible trap */
	else if (easy_disarm && disarm && (c_ptr->info & (CAVE_TRDT)))
	{
		(void)do_cmd_disarm_aux(y, x, tmp, do_pickup);
		return;
	}

#endif /* ALLOW_EASY_DISARM -- TNB */

	/* Player can't enter ? soo bad for him/her ... */
	else if (!player_can_enter(c_ptr->feat))
	{
		oktomove = FALSE;

		/* Disturb the player */
		disturb(0, 0);

		if (p_ptr->prob_travel)
		{
			if (passwall(tmp, TRUE)) return;
		}

		/* Notice things in the dark */
		if (!(c_ptr->info & (CAVE_MARK)) && !(c_ptr->info & (CAVE_SEEN)))
		{
			/* Rubble */
			if (c_ptr->feat == FEAT_RUBBLE)
			{
				msg_print("You feel some rubble blocking your way.");
				c_ptr->info |= (CAVE_MARK);
				lite_spot(y, x);
			}

			/* Closed door */
			else if (c_ptr->feat < FEAT_SECRET)
			{
				msg_print("You feel a closed door blocking your way.");
				c_ptr->info |= (CAVE_MARK);
				lite_spot(y, x);
			}

			/* Wall (or secret door) */
			else
			{
				int feat;

				if (c_ptr->mimic) feat = c_ptr->mimic;
				else
					feat = f_info[c_ptr->feat].mimic;

				msg_format("You feel %s.", f_text + f_info[feat].block);
				c_ptr->info |= (CAVE_MARK);
				lite_spot(y, x);
			}
		}

		/* Notice things */
		else
		{
			/* Rubble */
			if (c_ptr->feat == FEAT_RUBBLE)
			{
				if (!easy_tunnel)
				{
					msg_print("There is rubble blocking your way.");

					if (!(p_ptr->confused || p_ptr->stun || p_ptr->image))
						energy_use = 0;
					/*
					 * Well, it makes sense that you lose time bumping into
					 * a wall _if_ you are confused, stunned or blind; but
					 * typing mistakes should not cost you a turn...
					 */
				}
				else
				{
					do_cmd_tunnel_aux(y, x, dir);
					return;
				}
			}
			/* Closed doors */
			else if ((c_ptr->feat >= FEAT_DOOR_HEAD) && (c_ptr->feat <= FEAT_DOOR_TAIL))
			{
#ifdef ALLOW_EASY_OPEN

				if (easy_open)
				{
					if (easy_open_door(y, x)) return;
				}
				else
#endif /* ALLOW_EASY_OPEN */

				{
					msg_print("There is a closed door blocking your way.");

					if (!(p_ptr->confused || p_ptr->stun || p_ptr->image))
						energy_use = 0;
				}
			}

			/* Wall (or secret door) */
			else
			{
				if (!easy_tunnel)
				{
					int feat;

					if (c_ptr->mimic) feat = c_ptr->mimic;
					else
						feat = f_info[c_ptr->feat].mimic;

					msg_format("There is %s.", f_text + f_info[feat].block);

					if (!(p_ptr->confused || p_ptr->stun || p_ptr->image))
						energy_use = 0;
				}
				else
				{
					do_cmd_tunnel_aux(y, x, dir);
					return;
				}
			}
		}

		/* Sound */
		sound(SOUND_HITWALL);
	}

	/* Normal movement */
	if (!pattern_seq(p_ptr->py, p_ptr->px, y, x))
	{
		if (!(p_ptr->confused || p_ptr->stun || p_ptr->image))
		{
			energy_use = 0;
		}

		disturb(0, 0); 			/* To avoid a loop with running */

		oktomove = FALSE;
	}


	/*
	 * Check trap detection status -- retrieve them here
	 * because they are used by the movement code as well
	 */
	old_dtrap = ((cave[p_ptr->py][p_ptr->px].info & CAVE_DETECT) != 0);
	new_dtrap = ((cave[y][x].info & CAVE_DETECT) != 0);

	/* Normal movement */
	if (oktomove && running && disturb_detect)
	{
		/*
		 * Disturb the player when about to leave the trap detected
		 * area
		 */
		if (old_dtrap && !new_dtrap)
		{
			/* Disturb player */
			disturb(0, 0);

			/* but don't take a turn */
			energy_use = 0;

			/* Tell player why */
			cmsg_print(TERM_VIOLET, "You are about to leave a trap detected zone.");
			/* Flush */
			/* msg_print(NULL); */

			oktomove = FALSE;
		}
	}

	/* Normal movement */
	if (oktomove)
	{
		int oy, ox;
		int feat;

		/* Rooted means no move */
		if (p_ptr->tim_roots) return;

		/* Save old location */
		oy = p_ptr->py;
		ox = p_ptr->px;

		/* Move the player */
		p_ptr->py = y;
		p_ptr->px = x;

		if (cave[p_ptr->py][p_ptr->px].mimic) feat = cave[p_ptr->py][p_ptr->px].mimic;
		else
			feat = cave[p_ptr->py][p_ptr->px].feat;

		/* Some hooks */
		if (process_hooks(HOOK_MOVED, "(d,d)", oy, ox)) return;

		/* Redraw new spot */
		lite_spot(p_ptr->py, p_ptr->px);

		/* Redraw old spot */
		lite_spot(oy, ox);

		/* Sound */
		/* sound(SOUND_WALK); */

		/* Check for new panel (redraw map) */
		verify_panel();

		/* Check detection status */
		if (old_dtrap && !new_dtrap)
		{
			cmsg_print(TERM_VIOLET, "You leave a trap detected zone.");
			if (running) msg_print(NULL);
			p_ptr->redraw |= (PR_DTRAP);
		}
		else if (!old_dtrap && new_dtrap)
		{
			cmsg_print(TERM_L_BLUE, "You enter a trap detected zone.");
			if (running) msg_print(NULL);
			p_ptr->redraw |= (PR_DTRAP);
		}

		/* Update stuff */
		p_ptr->update |= (PU_VIEW | PU_FLOW | PU_MON_LITE);

		/* Update the monsters */
		p_ptr->update |= (PU_DISTANCE);

		/* Window stuff */
		if (!run) p_ptr->window |= (PW_OVERHEAD);

		/* Some feature descs */
		if (f_info[cave[p_ptr->py][p_ptr->px].feat].text > 1)
		{
			/* Mega-hack for dungeon branches */
			if ((feat == FEAT_MORE) && c_ptr->special)
			{
				msg_format("There is %s", d_text + d_info[c_ptr->special].text);
			}
			else
			{
				msg_print(f_text + f_info[feat].text);
			}

			/* Flush message while running */
			if (running) msg_print(NULL);
		}

		/* Spontaneous Searching */
		if ((p_ptr->skill_fos >= 50) || (0 == rand_int(50 - p_ptr->skill_fos)))
		{
			search();
		}

		/* Continuous Searching */
		if (p_ptr->searching)
		{
			search();
		}

		/* Handle "objects" */
		carry(do_pickup);

		/* Handle "store doors" */
		if (c_ptr->feat == FEAT_SHOP)
		{
			/* Disturb */
			disturb(0, 0);

			/* Hack -- Enter store */
			command_new = '_';
		}

#if 0 /* These are noxious -- pelpel */

		/* Handle quest areas -KMW- */
		else if (cave[y][x].feat == FEAT_QUEST_ENTER)
		{
			/* Disturb */
			disturb(0, 0);

			/* Hack -- Enter quest level */
			command_new = '[';
		}

		else if (cave[y][x].feat == FEAT_QUEST_EXIT)
		{
			leaving_quest = p_ptr->inside_quest;

			p_ptr->inside_quest = cave[y][x].special;
			dun_level = 0;
			p_ptr->oldpx = 0;
			p_ptr->oldpy = 0;
			p_ptr->leaving = TRUE;
		}

#endif /* 0 */

		else if (cave[y][x].feat >= FEAT_ALTAR_HEAD &&
		                cave[y][x].feat <= FEAT_ALTAR_TAIL)
		{
			cptr name = f_name + f_info[cave[y][x].feat].name;
			cptr pref = (is_a_vowel(name[0])) ? "an" : "a";

			msg_format("You see %s %s.", pref, name);

			/* Flush message while running */
			if (running) msg_print(NULL);
		}

		/* Discover invisible traps */
		else if ((c_ptr->t_idx != 0) &&
		                !(f_info[cave[y][x].feat].flags1 & FF1_DOOR))
		{
			/* Disturb */
			disturb(0, 0);

			if (!(c_ptr->info & (CAVE_TRDT)))
			{
				/* Message */
				msg_print("You found a trap!");

				/* Pick a trap */
				pick_trap(p_ptr->py, p_ptr->px);
			}

			/* Hit the trap */
			hit_trap();
		}

		/* Execute the inscription */
		else if (c_ptr->inscription)
		{
			/* Disturb */
			disturb(0, 0);

			msg_format("There is an inscription here: %s",
			           inscription_info[c_ptr->inscription].text);
			if (inscription_info[c_ptr->inscription].when & INSCRIP_EXEC_WALK)
			{
				execute_inscription(c_ptr->inscription, p_ptr->py, p_ptr->px);
			}
		}
	}

	/* Update wilderness knowledge */
	if (p_ptr->wild_mode)
	{
		if (wizard) msg_format("y:%d, x:%d", p_ptr->py, p_ptr->px);

		/* Update the known wilderness */
		reveal_wilderness_around_player(p_ptr->py, p_ptr->px, 0, WILDERNESS_SEE_RADIUS);

		/* Walking the wild isnt meaningfull */
		p_ptr->did_nothing = TRUE;
	}
}

void move_player(int dir, int do_pickup)
{
	move_player_aux(dir, do_pickup, 0, TRUE);
}


/*
 * Hack -- Grid-based version of see_obstacle
 */
static int see_obstacle_grid(cave_type *c_ptr)
{
	/*
	 * Hack -- Avoid hitting detected traps, because we cannot rely on
	 * the CAVE_MARK check below, and traps can be set to nearly
	 * everything the player can move on to XXX XXX XXX
	 */
	if (c_ptr->info & (CAVE_TRDT)) return (TRUE);


	/* Hack -- Handle special cases XXX XXX */
	switch (c_ptr->feat)
	{
		/* Require levitation */
	case FEAT_DARK_PIT:
	case FEAT_DEEP_WATER:
	case FEAT_ICE:
		{
			if (p_ptr->ffall || p_ptr->fly) return (FALSE);
		}

		/* Require immunity */
	case FEAT_DEEP_LAVA:
	case FEAT_SHAL_LAVA:
		{
			if (p_ptr->invuln || p_ptr->immune_fire) return (FALSE);
		}
	}


	/* "Safe" floor grids aren't obstacles */
	if (f_info[c_ptr->feat].flags1 & FF1_CAN_RUN) return (FALSE);

	/* Must be known to the player */
	if (!(c_ptr->info & (CAVE_MARK))) return (FALSE);

	/* Default */
	return (TRUE);
}


/*
 * Hack -- Check for a "known wall" or "dangerous" feature (see below)
 */
static int see_obstacle(int dir, int y, int x)
{
	/* Get the new location */
	y += ddy[dir];
	x += ddx[dir];

	/* Illegal grids are not known walls */
	if (!in_bounds2(y, x)) return (FALSE);

	/* Analyse the grid */
	return (see_obstacle_grid(&cave[y][x]));
}


/*
 * Hack -- Check for an "unknown corner" (see below)
 */
static int see_nothing(int dir, int y, int x)
{
	/* Get the new location */
	y += ddy[dir];
	x += ddx[dir];

	/* Illegal grids are unknown */
	if (!in_bounds2(y, x)) return (TRUE);

	/* Memorized grids are always known */
	if (cave[y][x].info & (CAVE_MARK)) return (FALSE);

	/* Non-floor grids are unknown */
	if (!cave_floor_bold(y, x)) return (TRUE);

	/* Viewable door/wall grids are known */
	if (player_can_see_bold(y, x)) return (FALSE);

	/* Default */
	return (TRUE);
}





/*
 * The running algorithm:                       -CJS-
 *
 * In the diagrams below, the player has just arrived in the
 * grid marked as '@', and he has just come from a grid marked
 * as 'o', and he is about to enter the grid marked as 'x'.
 *
 * Of course, if the "requested" move was impossible, then you
 * will of course be blocked, and will stop.
 *
 * Overview: You keep moving until something interesting happens.
 * If you are in an enclosed space, you follow corners. This is
 * the usual corridor scheme. If you are in an open space, you go
 * straight, but stop before entering enclosed space. This is
 * analogous to reaching doorways. If you have enclosed space on
 * one side only (that is, running along side a wall) stop if
 * your wall opens out, or your open space closes in. Either case
 * corresponds to a doorway.
 *
 * What happens depends on what you can really SEE. (i.e. if you
 * have no light, then running along a dark corridor is JUST like
 * running in a dark room.) The algorithm works equally well in
 * corridors, rooms, mine tailings, earthquake rubble, etc, etc.
 *
 * These conditions are kept in static memory:
 * find_openarea         You are in the open on at least one
 * side.
 * find_breakleft        You have a wall on the left, and will
 * stop if it opens
 * find_breakright       You have a wall on the right, and will
 * stop if it opens
 *
 * To initialize these conditions, we examine the grids adjacent
 * to the grid marked 'x', two on each side (marked 'L' and 'R').
 * If either one of the two grids on a given side is seen to be
 * closed, then that side is considered to be closed. If both
 * sides are closed, then it is an enclosed (corridor) run.
 *
 * LL           L
 * @x          LxR
 * RR          @R
 *
 * Looking at more than just the immediate squares is
 * significant. Consider the following case. A run along the
 * corridor will stop just before entering the center point,
 * because a choice is clearly established. Running in any of
 * three available directions will be defined as a corridor run.
 * Note that a minor hack is inserted to make the angled corridor
 * entry (with one side blocked near and the other side blocked
 * further away from the runner) work correctly. The runner moves
 * diagonally, but then saves the previous direction as being
 * straight into the gap. Otherwise, the tail end of the other
 * entry would be perceived as an alternative on the next move.
 *
 * #.#
 * ##.##
 * .@x..
 * ##.##
 * #.#
 *
 * Likewise, a run along a wall, and then into a doorway (two
 * runs) will work correctly. A single run rightwards from @ will
 * stop at 1. Another run right and down will enter the corridor
 * and make the corner, stopping at the 2.
 *
 * #@x    1
 * ########### ######
 * 2        #
 * #############
 * #
 *
 * After any move, the function area_affect is called to
 * determine the new surroundings, and the direction of
 * subsequent moves. It examines the current player location
 * (at which the runner has just arrived) and the previous
 * direction (from which the runner is considered to have come).
 *
 * Moving one square in some direction places you adjacent to
 * three or five new squares (for straight and diagonal moves
 * respectively) to which you were not previously adjacent,
 * marked as '!' in the diagrams below.
 *
 * ...!   ...
 * .o@!   .o.!
 * ...!   ..@!
 * !!!
 *
 * You STOP if any of the new squares are interesting in any way:
 * for example, if they contain visible monsters or treasure.
 *
 * You STOP if any of the newly adjacent squares seem to be open,
 * and you are also looking for a break on that side. (that is,
 * find_openarea AND find_break).
 *
 * You STOP if any of the newly adjacent squares do NOT seem to be
 * open and you are in an open area, and that side was previously
 * entirely open.
 *
 * Corners: If you are not in the open (i.e. you are in a corridor)
 * and there is only one way to go in the new squares, then turn in
 * that direction. If there are more than two new ways to go, STOP.
 * If there are two ways to go, and those ways are separated by a
 * square which does not seem to be open, then STOP.
 *
 * Otherwise, we have a potential corner. There are two new open
 * squares, which are also adjacent. One of the new squares is
 * diagonally located, the other is straight on (as in the diagram).
 * We consider two more squares further out (marked below as ?).
 *
 * We assign "option" to the straight-on grid, and "option2" to the
 * diagonal grid, and "check_dir" to the grid marked 's'.
 *
 * .s
 * @x?
 * #?
 *
 * If they are both seen to be closed, then it is seen that no
 * benefit is gained from moving straight. It is a known corner.
 * To cut the corner, go diagonally, otherwise go straight, but
 * pretend you stepped diagonally into that next location for a
 * full view next time. Conversely, if one of the ? squares is
 * not seen to be closed, then there is a potential choice. We check
 * to see whether it is a potential corner or an intersection/room entrance.
 * If the square two spaces straight ahead, and the space marked with 's'
 * are both blank, then it is a potential corner and enter if find_examine
 * is set, otherwise must stop because it is not a corner.
 */




/*
 * Hack -- allow quick "cycling" through the legal directions
 */
static byte cycle[] = { 1, 2, 3, 6, 9, 8, 7, 4, 1, 2, 3, 6, 9, 8, 7, 4, 1 };

/*
 * Hack -- map each direction into the "middle" of the "cycle[]" array
 */
static byte chome[] = { 0, 8, 9, 10, 7, 0, 11, 6, 5, 4 };

/*
 * The direction we are running
 */
static byte find_current;

/*
 * The direction we came from
 */
static byte find_prevdir;

/*
 * We are looking for open area
 */
static bool find_openarea;

/*
 * We are looking for a break
 */
static bool find_breakright;
static bool find_breakleft;



/*
 * Initialize the running algorithm for a new direction.
 *
 * Diagonal Corridor -- allow diaginal entry into corridors.
 *
 * Blunt Corridor -- If there is a wall two spaces ahead and
 * we seem to be in a corridor, then force a turn into the side
 * corridor, must be moving straight into a corridor here. ???
 *
 * Diagonal Corridor    Blunt Corridor (?)
 *       # #                  #
 *       #x#                 @x#
 *       @p.                  p
 */
static void run_init(int dir)
{
	int row, col, deepleft, deepright;

	int i, shortleft, shortright;


	/* Save the direction */
	find_current = dir;

	/* Assume running straight */
	find_prevdir = dir;

	/* Assume looking for open area */
	find_openarea = TRUE;

	/* Assume not looking for breaks */
	find_breakright = find_breakleft = FALSE;

	/* Assume no nearby walls */
	deepleft = deepright = FALSE;
	shortright = shortleft = FALSE;

	/* Find the destination grid */
	row = p_ptr->py + ddy[dir];
	col = p_ptr->px + ddx[dir];

	/* Extract cycle index */
	i = chome[dir];

	/* Check for walls */
	if (see_obstacle(cycle[i + 1], p_ptr->py, p_ptr->px))
	{
		find_breakleft = TRUE;
		shortleft = TRUE;
	}
	else if (see_obstacle(cycle[i + 1], row, col))
	{
		find_breakleft = TRUE;
		deepleft = TRUE;
	}

	/* Check for walls */
	if (see_obstacle(cycle[i - 1], p_ptr->py, p_ptr->px))
	{
		find_breakright = TRUE;
		shortright = TRUE;
	}
	else if (see_obstacle(cycle[i - 1], row, col))
	{
		find_breakright = TRUE;
		deepright = TRUE;
	}

	/* Looking for a break */
	if (find_breakleft && find_breakright)
	{
		/* Not looking for open area */
		find_openarea = FALSE;

		/* Hack -- allow angled corridor entry */
		if (dir & 0x01)
		{
			if (deepleft && !deepright)
			{
				find_prevdir = cycle[i - 1];
			}
			else if (deepright && !deepleft)
			{
				find_prevdir = cycle[i + 1];
			}
		}

		/* Hack -- allow blunt corridor entry */
		else if (see_obstacle(cycle[i], row, col))
		{
			if (shortleft && !shortright)
			{
				find_prevdir = cycle[i - 2];
			}
			else if (shortright && !shortleft)
			{
				find_prevdir = cycle[i + 2];
			}
		}
	}
}


/*
 * Update the current "run" path
 *
 * Return TRUE if the running should be stopped
 */
static bool run_test(void)
{
	int prev_dir, new_dir, check_dir = 0;

	int row, col;

	int i, max, inv;

	int option = 0, option2 = 0;

	cave_type *c_ptr;


	/* Where we came from */
	prev_dir = find_prevdir;


	/* Range of newly adjacent grids */
	max = (prev_dir & 0x01) + 1;


	/* Look at every newly adjacent square. */
	for (i = -max; i <= max; i++)
	{
		s16b this_o_idx, next_o_idx = 0;


		/* New direction */
		new_dir = cycle[chome[prev_dir] + i];

		/* New location */
		row = p_ptr->py + ddy[new_dir];
		col = p_ptr->px + ddx[new_dir];

		/* Access grid */
		c_ptr = &cave[row][col];


		/* Visible monsters abort running */
		if (c_ptr->m_idx)
		{
			monster_type *m_ptr = &m_list[c_ptr->m_idx];

			/* Visible monster */
			if (m_ptr->ml) return (TRUE);
		}

		/* Visible objects abort running */
		for (this_o_idx = c_ptr->o_idx; this_o_idx; this_o_idx = next_o_idx)
		{
			object_type * o_ptr;

			/* Acquire object */
			o_ptr = &o_list[this_o_idx];

			/* Acquire next object */
			next_o_idx = o_ptr->next_o_idx;

			/* Visible object */
			if (o_ptr->marked) return (TRUE);
		}


		/* Assume unknown */
		inv = TRUE;

		/* Check memorized grids */
		if (c_ptr->info & (CAVE_MARK))
		{
			bool notice = TRUE;

			/*
			 * Examine the terrain -- conditional disturbance
			 * If we had more flags, we could make these customisable too
			 */
			switch (c_ptr->feat)
			{
			case FEAT_DEEP_LAVA:
			case FEAT_SHAL_LAVA:
				{
					/* Ignore */
					if (p_ptr->invuln || p_ptr->immune_fire) notice = FALSE;

					/* Done */
					break;
				}

			case FEAT_DEEP_WATER:
			case FEAT_ICE:
				{
					/* Ignore */
					if (p_ptr->ffall || p_ptr->fly) notice = FALSE;

					/* Done */
					break;
				}

				/* Open doors */
			case FEAT_OPEN:
			case FEAT_BROKEN:
				{
					/* Option -- ignore */
					if (find_ignore_doors) notice = FALSE;

					/* Done */
					break;
				}

				/*
				 * Stairs - too many of them, should find better ways to
				 * handle them (not scripting!, because it can be called
				 * from within the running algo) XXX XXX XXX
				 */
			case FEAT_LESS:
			case FEAT_MORE:
			case FEAT_QUEST_ENTER:
			case FEAT_QUEST_EXIT:
			case FEAT_QUEST_DOWN:
			case FEAT_QUEST_UP:
			case FEAT_SHAFT_UP:
			case FEAT_SHAFT_DOWN:
			case FEAT_WAY_LESS:
			case FEAT_WAY_MORE:
				/* XXX */
			case FEAT_BETWEEN:
			case FEAT_BETWEEN2:
				{
					/* Option -- ignore */
					if (find_ignore_stairs) notice = FALSE;

					/* Done */
					break;
				}
			}

			/* Check the "don't notice running" flag */
			if (f_info[c_ptr->feat].flags1 & FF1_DONT_NOTICE_RUNNING)
			{
				notice = FALSE;
			}

			/* A detected trap is interesting */
			if (c_ptr->info & (CAVE_TRDT)) notice = TRUE;

			/* Interesting feature */
			if (notice) return (TRUE);

			/* The grid is "visible" */
			inv = FALSE;
		}

		/* Mega-Hack -- Maze code removes CAVE_MARK XXX XXX XXX */
		if (c_ptr->info & (CAVE_TRDT)) return (TRUE);

		/* Analyze unknown grids and floors */
		if (inv || cave_floor_bold(row, col))
		{
			/* Looking for open area */
			if (find_openarea)
			{
				/* Nothing */
			}

			/* The first new direction. */
			else if (!option)
			{
				option = new_dir;
			}

			/* Three new directions. Stop running. */
			else if (option2)
			{
				return (TRUE);
			}

			/* Two non-adjacent new directions.  Stop running. */
			else if (option != cycle[chome[prev_dir] + i - 1])
			{
				return (TRUE);
			}

			/* Two new (adjacent) directions (case 1) */
			else if (new_dir & 0x01)
			{
				check_dir = cycle[chome[prev_dir] + i - 2];
				option2 = new_dir;
			}

			/* Two new (adjacent) directions (case 2) */
			else
			{
				check_dir = cycle[chome[prev_dir] + i + 1];
				option2 = option;
				option = new_dir;
			}
		}

		/* Obstacle, while looking for open area */
		else
		{
			if (find_openarea)
			{
				if (i < 0)
				{
					/* Break to the right */
					find_breakright = TRUE;
				}

				else if (i > 0)
				{
					/* Break to the left */
					find_breakleft = TRUE;
				}
			}
		}
	}


	/* Looking for open area */
	if (find_openarea)
	{
		/* Hack -- look again */
		for (i = -max; i < 0; i++)
		{
			new_dir = cycle[chome[prev_dir] + i];

			row = p_ptr->py + ddy[new_dir];
			col = p_ptr->px + ddx[new_dir];

			/* Access grid */
			c_ptr = &cave[row][col];

			/* Unknown grids or non-obstacle */
			if (!see_obstacle_grid(c_ptr))
			{
				/* Looking to break right */
				if (find_breakright)
				{
					return (TRUE);
				}
			}

			/* Obstacle */
			else
			{
				/* Looking to break left */
				if (find_breakleft)
				{
					return (TRUE);
				}
			}
		}

		/* Hack -- look again */
		for (i = max; i > 0; i--)
		{
			new_dir = cycle[chome[prev_dir] + i];

			row = p_ptr->py + ddy[new_dir];
			col = p_ptr->px + ddx[new_dir];

			/* Access grid */
			c_ptr = &cave[row][col];

			/* Unknown grid or non-obstacle */
			if (!see_obstacle_grid(c_ptr))
			{
				/* Looking to break left */
				if (find_breakleft)
				{
					return (TRUE);
				}
			}

			/* Obstacle */
			else
			{
				/* Looking to break right */
				if (find_breakright)
				{
					return (TRUE);
				}
			}
		}
	}


	/* Not looking for open area */
	else
	{
		/* No options */
		if (!option)
		{
			return (TRUE);
		}

		/* One option */
		else if (!option2)
		{
			/* Primary option */
			find_current = option;

			/* No other options */
			find_prevdir = option;
		}

		/* Two options, examining corners */
		else if (find_examine && !find_cut)
		{
			/* Primary option */
			find_current = option;

			/* Hack -- allow curving */
			find_prevdir = option2;
		}

		/* Two options, pick one */
		else
		{
			/* Get next location */
			row = p_ptr->py + ddy[option];
			col = p_ptr->px + ddx[option];

			/* Don't see that it is closed off. */
			/* This could be a potential corner or an intersection. */
			if (!see_obstacle(option, row, col) || !see_obstacle(check_dir, row, col))
			{
				/* Can not see anything ahead and in the direction we */
				/* are turning, assume that it is a potential corner. */
				if (find_examine &&
				                see_nothing(option, row, col) &&
				                see_nothing(option2, row, col))
				{
					find_current = option;
					find_prevdir = option2;
				}

				/* STOP: we are next to an intersection or a room */
				else
				{
					return (TRUE);
				}
			}

			/* This corner is seen to be enclosed; we cut the corner. */
			else if (find_cut)
			{
				find_current = option2;
				find_prevdir = option2;
			}

			/* This corner is seen to be enclosed, and we */
			/* deliberately go the long way. */
			else
			{
				find_current = option;
				find_prevdir = option2;
			}
		}
	}


	/* About to hit a known wall, stop */
	if (see_obstacle(find_current, p_ptr->py, p_ptr->px))
	{
		return (TRUE);
	}


	/* Failure */
	return (FALSE);
}



/*
 * Take one step along the current "run" path
 */
void run_step(int dir)
{
	/* Start running */
	if (dir)
	{
		/* Hack -- do not start silly run */
		if (see_obstacle(dir, p_ptr->py, p_ptr->px) &&
		                (cave[p_ptr->py + ddy[dir]][p_ptr->px + ddx[dir]].feat != FEAT_TREES))
		{
			/* Message */
			msg_print("You cannot run in that direction.");

			/* Disturb */
			disturb(0, 0);

			/* Done */
			return;
		}

		/* Calculate torch radius */
		p_ptr->update |= (PU_TORCH);

		/* Initialize */
		run_init(dir);
	}

	/* Keep running */
	else
	{
		/* Update run */
		if (run_test())
		{
			/* Disturb */
			disturb(0, 0);

			/* Done */
			return;
		}
	}

	/* Decrease the run counter */
	if (--running <= 0) return;

	/* Take time */
	energy_use = 100;


	/* Move the player, using the "pickup" flag */
	move_player_aux(find_current, always_pickup, 1, TRUE);
}


/*
 * Auto-travel ("click to walk"): walk the player along a precomputed A* path,
 * one step per game turn, until the goal is reached or something interrupts.
 *
 * This is the shared engine half of the mouse "go to" feature and the planned
 * base for auto-explore: a frontend (or the explorer) only has to pick a goal
 * grid and call travel_to(); the per-turn stepping, energy use and interruption
 * are handled here, mirroring how run_step()/running work.
 *
 * The current route and our position along it.
 */
static path_result *travel_route = NULL;
static int travel_idx = 0;
static int travel_stuck = 0;   /* pasos seguidos sin movernos (abrir puerta atrancada) */

/* Pause (ms) after each auto-walked step (travel / auto-explore) so the
 * movement is watchable rather than instantaneous. */
#define TRAVEL_STEP_DELAY 150
#define TRAVEL_STUCK_MAX  12   /* si no avanzamos en N intentos, abandona el tramo */

/* TomeTik: ¿hay algún monstruo NO aliado a la vista? (para no autoexplorar con
 * enemigos delante / parar si aparece uno). */
static bool monster_threat_visible(void)
{
	int i;
	for (i = 1; i < m_max; i++)
	{
		monster_type *m_ptr = &m_list[i];
		if (!m_ptr->r_idx) continue;            /* hueco libre */
		if (!m_ptr->ml) continue;               /* no visible */
		if (m_ptr->status >= MSTATUS_FRIEND) continue;  /* aliado/mascota */
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Per-level "ever seen" bitmap.
 *
 * ToME only memorizes (CAVE_MARK) walls/doors and -- with view_perma_grids --
 * lit-room floors; a torch-lit corridor floor is CAVE_SEEN while you stand on
 * it but loses both flags once you leave. So CAVE_MARK alone can't tell "floor
 * I have walked" from "floor I have never seen", which would make both travel
 * (can't path back down a dark corridor) and auto-explore (oscillates over
 * already-seen floor) misbehave.
 *
 * We therefore accumulate our own knowledge: any grid that is currently
 * memorized or in view is recorded as "seen" and stays so for the rest of the
 * level. This is the basis for BOTH walkability (can step onto seen floor) and
 * the explore frontier (a seen grid next to an unseen one). The map is reset
 * per level using old_turn (the turn the level began) as a cheap stamp.
 */
static byte *explore_seen = NULL;
static int explore_seen_n = 0;
static s32b explore_seen_stamp = -1;
static long explore_seen_count = 0;   /* nº de celdas vistas (para medir progreso) */

/* Anti-oscilación: metas (fronteras) que ya intentamos y NO aportaron celdas
 * nuevas; no se vuelven a elegir (evita ir y venir entre fronteras "muertas"). */
static byte *explore_bad = NULL;

/* Celdas que el autoexplore ABANDONA este nivel: una puerta que no logra abrir
 * (atrancada / cerradura imposible), un tramo que se queda bloqueado. Se tratan
 * como intransitables SOLO para el autoexplore, de modo que rodea en vez de
 * pararse. El click-to-move no las consulta (el jugador puede ir a mano). */
static byte *explore_block = NULL;
static byte *ap_searched = NULL;   /* A2: floor cells we've already searched from */
static int ap_search_done = 0;     /* A2: search turns spent on this level (capped) */
#define AP_SEARCH_MAX 60           /*     don't sweep forever before scumming */
static bool ap_detected = FALSE;   /* B1: have we run detection on this level yet? */
static int explore_goal_y = -1, explore_goal_x = -1;
static long explore_goal_count = -1;  /* celdas vistas cuando fijamos la meta */
static bool explore_goal_is_loot = FALSE; /* la meta actual es oro / un objeto */

/* Radio (en pasos) dentro del cual el autoexplore se DESVÍA a recoger oro / un
 * objeto que ya ha visto. Lo más lejos se ignora (no lo perseguimos por todo el
 * mapa). Configurable; 0 desactiva ese tipo de desvío. */
int explore_gold_radius = 5;
int explore_item_radius = 5;

/* Auto-play raises this while its backpack is full: the goal-picker then stops
 * detouring for items. Gold costs no inventory slot, so it is still collected. */
static bool explore_no_items = FALSE;

/* Clase de interés de una celda para el autoexplore (modelo del
 * borg_flow_dark_interesting): se elige la celda de MAYOR prioridad y, a igualdad,
 * la más cercana. El BFS de find_nearest_goal visita en orden de distancia, así
 * que el primer LOOT-en-radio gana y, si no hay, la primera frontera. */
#define EXPLORE_NONE     0   /* nada por lo que desviarse aquí               */
#define EXPLORE_FRONTIER 1   /* celda vista junto a una nunca vista          */
#define EXPLORE_LOOT     2   /* oro/objeto visto dentro de su radio (gana)   */

/* (Re)allocate for the current level if needed, then fold in everything that is
 * memorized or currently visible. Cheap (one pass); call before pathing. */
static void explore_sync_seen(void)
{
	int n = cur_hgt * cur_wid;
	int y, x;

	if ((explore_seen == NULL) || (explore_seen_n != n) ||
	                (explore_seen_stamp != old_turn))
	{
		if (explore_seen) C_FREE(explore_seen, explore_seen_n, byte);
		if (explore_bad) C_FREE(explore_bad, explore_seen_n, byte);
		if (explore_block) C_FREE(explore_block, explore_seen_n, byte);
		if (ap_searched) C_FREE(ap_searched, explore_seen_n, byte);
		explore_seen_n = n;
		C_MAKE(explore_seen, n, byte);
		C_MAKE(explore_bad, n, byte);
		C_MAKE(explore_block, n, byte);
		C_MAKE(ap_searched, n, byte);
		ap_search_done = 0;
		ap_detected = FALSE;
		explore_seen_stamp = old_turn;
		explore_seen_count = 0;
		explore_goal_y = explore_goal_x = -1;
		explore_goal_count = -1;
	}

	explore_seen_count = 0;
	for (y = 0; y < cur_hgt; y++)
	{
		for (x = 0; x < cur_wid; x++)
		{
			if (cave[y][x].info & (CAVE_MARK | CAVE_SEEN))
				explore_seen[y * cur_wid + x] = 1;
			if (explore_seen[y * cur_wid + x]) explore_seen_count++;
		}
	}
}

/* Have we ever seen this grid (this level)? Live flags count even if a sync
 * hasn't folded them in yet. */
static bool explore_is_seen(int y, int x)
{
	if (cave[y][x].info & (CAVE_MARK | CAVE_SEEN)) return (TRUE);
	if (explore_seen && (explore_seen[y * cur_wid + x])) return (TRUE);
	return (FALSE);
}

/* ¿Hay oro (ya visto) en esta celda? Para que el autoexplore lo recoja si está
 * cerca (ver explore_gold_radius). */
static bool cell_has_gold(int y, int x)
{
	s16b this_o_idx, next_o_idx;

	for (this_o_idx = cave[y][x].o_idx; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr = &o_list[this_o_idx];
		next_o_idx = o_ptr->next_o_idx;
		if (o_ptr->marked && (o_ptr->tval == TV_GOLD)) return (TRUE);
	}
	return (FALSE);
}

/* ¿Hay un objeto (ya visto) que NO sea oro en esta celda? Igual que el oro, el
 * autoexplore se desvía a recogerlo si está dentro de explore_item_radius. */
static bool cell_has_item(int y, int x)
{
	s16b this_o_idx, next_o_idx;

	for (this_o_idx = cave[y][x].o_idx; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr = &o_list[this_o_idx];
		next_o_idx = o_ptr->next_o_idx;
		/* Corpses and skeletons are junk for this (melee) bot -- never a loot
		 * goal. (Skeletons/junk CAN be converted to ammo by an archer with the
		 * right skill -- item_tester_hook_convertible -- so a ranged-aware brain
		 * might want them; ours doesn't shoot.) */
		if (o_ptr->marked && (o_ptr->tval != TV_GOLD) &&
		                (o_ptr->tval != TV_CORPSE) && (o_ptr->tval != TV_SKELETON))
		{
			/* Auto-play won't pick up worthless junk (spent torches, broken
			 * sticks, value<=0) -- so it must not treat it as a loot goal either,
			 * or it walks to an item it refuses to take and loops forever. Keep
			 * this in step with py_pickup_floor's auto-play filter. */
			if (autoplaying)
			{
				if ((o_ptr->tval == TV_LITE) &&
				                ((o_ptr->sval == SV_LITE_TORCH) || (o_ptr->sval == SV_LITE_LANTERN)) &&
				                (o_ptr->timeout <= 0))
					continue;
				if (object_value(o_ptr) <= 0)
					continue;
			}
			return (TRUE);
		}
	}
	return (FALSE);
}

/* (B4) Survivable missiles (shots/arrows/bolts) lying here -- the bot's own fired
 * ammo to recover. Flasks of oil shatter when thrown, so they're not counted. */
static bool cell_has_ammo(int y, int x)
{
	s16b this_o_idx, next_o_idx;

	for (this_o_idx = cave[y][x].o_idx; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr = &o_list[this_o_idx];
		next_o_idx = o_ptr->next_o_idx;
		if (!o_ptr->marked) continue;
		if ((o_ptr->tval == TV_SHOT) || (o_ptr->tval == TV_ARROW) ||
		                (o_ptr->tval == TV_BOLT))
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Walkability for the travel A*: a grid may be entered iff we have seen it (so
 * we know it is there) and it is a floor-like feature -- no walls, closed doors
 * or never-seen tiles. Monsters are ignored here (the path may cross a
 * monster's grid); the stepping code stops short instead of bumping them.
 */
static bool travel_walkable_hook(int y, int x, void *user)
{
	int f;
	(void)user;

	if (!explore_is_seen(y, x)) return (FALSE);

	/* Suelo transitable. */
	if (cave_floor_bold(y, x)) return (TRUE);

	/* Puertas cerradas/atrancadas: con easy_open (ON por defecto) el juego las
	 * abre al andar contra ellas, así que las ruteamos como transitables. */
	f = cave[y][x].feat;
	if (easy_open && (f >= FEAT_DOOR_HEAD) && (f <= FEAT_DOOR_TAIL)) return (TRUE);

	return (FALSE);
}

/*
 * Walkability for click-to-move ("go there"). Unlike the auto-explore hook this
 * does NOT require the grid to have been seen: the whole level lives in cave[][]
 * (feat is the true terrain even for un-memorized grids, as the rest of the game
 * already relies on), so we can route over real floor/doors the player hasn't
 * discovered yet and let them walk into the dark toward the clicked goal,
 * uncovering it on the way. travel_step still re-validates each step, so a tile
 * that genuinely turns out blocked stops us cleanly.
 */
static bool travel_walkable_real(int y, int x, void *user)
{
	int f;
	(void)user;

	if (cave_floor_bold(y, x)) return (TRUE);

	f = cave[y][x].feat;
	if (easy_open && (f >= FEAT_DOOR_HEAD) && (f <= FEAT_DOOR_TAIL)) return (TRUE);

	return (FALSE);
}

/*
 * Walkability for auto-explore (frontier search and the legs toward it): the
 * seen-gated policy, plus any grid we have given up on this level (explore_block)
 * is treated as impassable so we route around it instead of halting. Click-to-
 * move does NOT consult this, so the player can still order a trip by hand.
 */
static bool explore_walkable_hook(int y, int x, void *user)
{
	if (explore_block && explore_block[y * cur_wid + x]) return (FALSE);
	return (travel_walkable_hook(y, x, user));
}

/* As explore_walkable_hook / travel_walkable_real, but also treats a grid HELD
 * BY A MONSTER as impassable. Used for auto-play's non-combat movement (explore,
 * delve, walk-to-stairs) so A* routes AROUND creatures -- e.g. a friendly NPC
 * like Farmer Maggot standing in the way -- instead of bumping into them
 * forever. (Combat keeps the monster-ignoring hooks so it can reach its target.) */
static bool explore_walkable_clear(int y, int x, void *user)
{
	if (cave[y][x].m_idx) return (FALSE);
	return (explore_walkable_hook(y, x, user));
}

static bool real_walkable_clear(int y, int x, void *user)
{
	if (cave[y][x].m_idx) return (FALSE);
	return (travel_walkable_real(y, x, user));
}

/* As explore_walkable_clear, but a cell held by a FRIENDLY monster is passable:
 * the player swaps / pushes past friendlies (see move_player_aux), so a friendly
 * NPC stuck in a 1-wide corridor with no detour doesn't dead-end the bot. Hostile
 * (and unpushable) occupants still block -- we won't blunder into a fight just to
 * get past. Used only as a fallback when the monster-avoiding route fails. */
static bool explore_walkable_friend(int y, int x, void *user)
{
	if (cave[y][x].m_idx && (is_friend(&m_list[cave[y][x].m_idx]) <= 0))
		return (FALSE);
	return (explore_walkable_hook(y, x, user));
}

/* Give up on grid (y,x) for the rest of this level's auto-explore: from now on we
 * route around it (and never sit a frontier goal on it, since the BFS no longer
 * reaches it). */
static void explore_block_cell(int y, int x)
{
	if (explore_block && in_bounds2(y, x)) explore_block[y * cur_wid + x] = 1;
}

/*
 * Walkability policy of the leg currently armed: seen-gated (auto-explore),
 * real-terrain (click-to-move), or the explore variant above. travel_plan() sets
 * it from its caller and travel_step() reuses it so re-validation matches how the
 * path was planned.
 */
static astar_walkable_hook travel_hook = travel_walkable_hook;

/*
 * Silent teardown of the travel state. Used internally (arrival, restart) where
 * we either say nothing or print our own, more specific, message.
 */
static void travel_clear(void)
{
	if (travel_route)
	{
		path_free(travel_route);
		travel_route = NULL;
	}
	travel_idx = 0;
	travel_stuck = 0;

	if (travelling)
	{
		travelling = 0;

		/* Mirror running's cleanup. */
		p_ptr->update |= (PU_TORCH);
		p_ptr->redraw |= (PR_STATE);
	}
}

/*
 * Stop auto-travelling. Called from disturb(), so it fires on ANY disturbance
 * (a monster comes into view, damage, a key press, ...). When that interrupts
 * an in-progress trip we report it, since otherwise the character would just
 * halt mid-path for no visible reason. Safe to call when not travelling.
 */
void travel_cancel(void)
{
	bool interrupted = (travelling != 0);
	bool was_exploring = (exploring != 0);

	travel_clear();

	/* Durante autoexplore NO avisamos aquí: un tramo se corta a menudo por algo
	 * benigno (recoger oro/objeto) y explore_step re-planifica y sigue solo. Los
	 * mensajes de parada real (monstruo / "Done") los pone explore_step. */
	if (interrupted && !was_exploring)
		msg_print("You stop travelling.");
}

/*
 * Hard stop from inside travel_step(): end BOTH travel and any auto-explore
 * (so we don't immediately re-pick a goal), then optionally report why.
 */
static void travel_abort(cptr msg)
{
	if (exploring)
	{
		exploring = 0;
		p_ptr->redraw |= (PR_STATE);
	}
	travel_clear();
	if (msg) msg_print(msg);
}

/*
 * Plan and arm a travel route to (gy, gx). Pure mechanics: no disturb(), no
 * messages, no state cancellation -- callers (travel_to for the mouse, and the
 * auto-explorer) wrap it with their own policy. Returns TRUE if a route of at
 * least one step was armed.
 */
static bool travel_plan(int gy, int gx, astar_walkable_hook hook)
{
	path_result *route;
	int sy = p_ptr->py;
	int sx = p_ptr->px;

	/* Remember the policy of this leg so travel_step() re-validates the same
	 * way the path was planned. */
	travel_hook = hook;

	if (!in_bounds2(gy, gx)) return (FALSE);
	if ((gy == sy) && (gx == sx)) return (FALSE);

	/* Goal must be a tile we could actually reach under this leg's policy. */
	if (!hook(gy, gx, NULL)) return (FALSE);

	/* Path over cave[][] without copying it. Allow diagonals to cut corners,
	 * matching how the player can actually move (otherwise paths through
	 * corridors come out as orthogonal staircases). */
	route = astar_find_path_cb(cur_hgt, cur_wid,
	                           hook, NULL,
	                           sy, sx, gy, gx, ASTAR_8DIR_CUT);

	/* steps[0] is the player's own tile, so a real path has length >= 2. */
	if (!route || (route->length < 2))
	{
		path_free(route);
		return (FALSE);
	}

	travel_route = route;
	travel_idx = 1;        /* next tile to step onto (steps[0] is us) */
	travelling = 1;        /* active flag; progress is tracked by travel_idx */

	return (TRUE);
}

/*
 * Begin auto-travelling to grid (gy, gx). Returns TRUE if a path was found and
 * travel started, FALSE otherwise (no path, goal not walkable, can't travel).
 */
bool travel_to(int gy, int gx)
{
	/* Drop any previous route silently (re-clicking a new goal shouldn't say
	 * "you stop travelling"), then cancel running/resting/repeat. */
	travel_clear();
	disturb(0, 0);

	/* States where precise auto-walking makes no sense. */
	if (p_ptr->confused || p_ptr->image || p_ptr->immovable) return (FALSE);

	/* Refresh our "seen" knowledge before pathing. */
	explore_sync_seen();

	/* Off-map / our own tile: nothing to do, quietly. */
	if (!in_bounds2(gy, gx) || ((gy == p_ptr->py) && (gx == p_ptr->px))) return (FALSE);

	/* The clicked tile isn't walkable (a wall, or a near-miss on the edge of the
	 * black): snap to its nearest walkable neighbour so the click still goes
	 * somewhere sensible instead of doing nothing. */
	if (!travel_walkable_real(gy, gx, NULL))
	{
		static const int dy8[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
		static const int dx8[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
		int best = -1, best_d = 0, i;

		for (i = 0; i < 8; i++)
		{
			int ny = gy + dy8[i], nx = gx + dx8[i], d;

			if (!in_bounds2(ny, nx)) continue;
			if (!travel_walkable_real(ny, nx, NULL)) continue;

			d = distance(p_ptr->py, p_ptr->px, ny, nx);
			if ((best < 0) || (d < best_d)) { best = i; best_d = d; }
		}

		if (best < 0) return (FALSE);   /* nothing walkable next to the click */
		gy += dy8[best];
		gx += dx8[best];
		if ((gy == p_ptr->py) && (gx == p_ptr->px)) return (FALSE);
	}

	/* Walkable but unreachable -> say so. Routes over real terrain, so the @
	 * will walk into as-yet-unseen floor toward the goal. */
	if (!travel_plan(gy, gx, travel_walkable_real))
	{
		msg_print("You can't find a path to there.");
		return (FALSE);
	}

	return (TRUE);
}

/*
 * Take one travel step. Called from process_player()'s energy loop while
 * travelling, exactly where running calls run_step(). Stops on arrival or if
 * the world changed under the route (monster in the way, terrain altered),
 * reporting the reason.
 */
void travel_step(void)
{
	int ny, nx, dir, d;
	int oy, ox;

	/* Nothing to do / route consumed. */
	if (!travel_route || (travel_idx >= travel_route->length))
	{
		travel_clear();
		return;
	}

	/* Confusion would scramble the chosen direction. */
	if (p_ptr->confused)
	{
		travel_abort("You are too confused to travel.");
		return;
	}

	ny = travel_route->steps[travel_idx].y;
	nx = travel_route->steps[travel_idx].x;

	/* A monster stepped into our path: stop short instead of bumping it. */
	if (cave[ny][nx].m_idx)
	{
		travel_abort("There is a monster in your way.");
		return;
	}

	/* The next tile is no longer traversable, under this leg's policy. While
	 * auto-exploring don't abandon the sweep: give up on this grid and let
	 * explore_step re-route around it. */
	if (!travel_hook(ny, nx, NULL))
	{
		if (exploring) { explore_block_cell(ny, nx); travel_clear(); return; }
		travel_abort("Your way is blocked.");
		return;
	}

	/* Direction from the player to the next tile (also verifies adjacency). */
	dir = 0;
	for (d = 1; d <= 9; d++)
	{
		if (d == 5) continue;
		if ((p_ptr->py + ddy[d] == ny) && (p_ptr->px + ddx[d] == nx))
		{
			dir = d;
			break;
		}
	}
	if (!dir)
	{
		if (exploring) { explore_block_cell(ny, nx); travel_clear(); return; }
		travel_abort(NULL);
		return;
	}

	/* Take the step: one game turn (monsters act in between). While auto-
	 * exploring we always grab items in passing; plain travel honours the
	 * player's always_pickup option. */
	oy = p_ptr->py;
	ox = p_ptr->px;
	energy_use = 100;
	move_player_aux(dir, (exploring ? TRUE : always_pickup), 1, TRUE);

	/* Animate: draw the new position and pause briefly, so travelling and
	 * auto-explore are watchable instead of teleport-fast. */
	handle_stuff();
	Term_fresh();
	Term_xtra(TERM_XTRA_DELAY, TRAVEL_STEP_DELAY);

	/* move_player_aux may have already cancelled us via disturb() (a trap, a
	 * newly seen monster, ...) and reported it -- respect that. */
	if (!travelling) return;

	/* No nos movimos: normalmente porque easy_open acaba de ABRIR una puerta
	 * (cuesta un turno y no avanzas) o está reintentando forzar una cerradura.
	 * Reintenta el paso el turno siguiente SIN avanzar el índice; así se insiste
	 * en el comando "open" varias veces. Si tras TRAVEL_STUCK_MAX intentos
	 * seguimos clavados (puerta atrancada que no cede, cerradura imposible): en
	 * autoexplore se ABANDONA esa celda y se reencamina (sin parar la exploración);
	 * en travel-a-click se corta el tramo. */
	if ((p_ptr->py == oy) && (p_ptr->px == ox))
	{
		if (++travel_stuck > TRAVEL_STUCK_MAX)
		{
			if (exploring) { explore_block_cell(ny, nx); travel_clear(); return; }
			travel_abort(NULL);
		}
		return;
	}
	travel_stuck = 0;

	/* Auto-explore: stop and announce when we reach something notable, so the
	 * player can decide (DCSS-style). Travel-to-click keeps going. */
	if (exploring)
	{
		cptr note = NULL;

		switch (cave[p_ptr->py][p_ptr->px].feat)
		{
		case FEAT_LESS:        note = "There is a staircase up here."; break;
		case FEAT_MORE:        note = "There is a staircase down here."; break;
		case FEAT_SHOP:        note = "There is a shop entrance here."; break;
		case FEAT_QUEST_ENTER: note = "There is a quest entrance here."; break;
		default:               break;
		}

		if (note)
		{
			travel_abort(note);
			return;
		}
	}

	/* Advance; announce arrival at the goal. During auto-explore each leg ends
	 * silently (only the final "Done exploring." matters). */
	travel_idx++;
	if (travel_idx >= travel_route->length)
	{
		travel_clear();
		if (!exploring) msg_print("You arrive.");
	}
}


/*
 * Mouse "do the right thing" on a click at grid (gy, gx):
 *  - If it holds a VISIBLE monster ADJACENT to the player, queue a single
 *    step toward it. move_player_aux() then does the natural thing: ATTACK a
 *    hostile, or "push past" (swap) a friendly/pet.
 *  - Otherwise, travel there (which stops adjacent to monsters as before).
 *
 * Returns TRUE if a command was queued/started (so the frontend can unblock
 * inkey() with an ESCAPE), FALSE if nothing to do.
 */
bool do_cmd_click(int gy, int gx)
{
	if (in_bounds2(gy, gx) && cave[gy][gx].m_idx)
	{
		monster_type *m_ptr = &m_list[cave[gy][gx].m_idx];
		int dy = gy - p_ptr->py;
		int dx = gx - p_ptr->px;

		/* Visible monster on an adjacent tile -> melee / swap toward it. */
		if (m_ptr->ml && (ABS(dy) <= 1) && (ABS(dx) <= 1) && (dy || dx))
		{
			int d;

			/* Cancel any running/resting/travel first. */
			disturb(0, 0);

			if (p_ptr->confused || p_ptr->image || p_ptr->immovable) return (FALSE);

			for (d = 1; d <= 9; d++)
			{
				if (d == 5) continue;
				if ((p_ptr->py + ddy[d] == gy) && (p_ptr->px + ddx[d] == gx))
				{
					click_dir = d;
					return (TRUE);
				}
			}
			return (FALSE);
		}
	}

	/* Not an adjacent monster: walk there. */
	return (travel_to(gy, gx));
}

/*
 * Perform the one-shot move/attack queued by do_cmd_click(). Called from
 * process_player()'s energy loop, like run_step()/travel_step().
 */
void click_act_step(void)
{
	int dir = click_dir;

	click_dir = 0;
	if (!dir) return;

	/* One game turn: move_player_aux attacks a hostile or pushes past an ally. */
	energy_use = 100;
	move_player_aux(dir, always_pickup, 0, TRUE);
}


/*
 * Auto-explore (DCSS-style): repeatedly travel to the nearest unexplored part
 * of the level until everything reachable is seen or something interrupts.
 *
 * It is a thin layer on top of travel: explore_step() picks a goal and arms a
 * travel leg (travel_plan), the normal travel branch in process_player() walks
 * that leg, and when the leg ends explore_step() picks the next goal. disturb()
 * cancels both, so monsters/damage/key presses stop exploration like running.
 *
 * Find the nearest reachable "frontier": a known, walkable grid that is adjacent
 * to an as-yet-unexplored (un-memorized) grid. A breadth-first flood from the
 * player over walkable known grids returns the closest such grid (and proves it
 * is reachable in one pass). Returns TRUE and fills (*gy,*gx) if one exists.
 */
static int explore_cell_interest(int y, int x, int dist)
{
	static const int dy8[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
	static const int dx8[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
	int d;

	/* Loot within reach: gold or a dropped item we have already seen. Treated as
	 * the highest-priority kind so that, on a grid that is both loot and a
	 * frontier, picking it up wins (matching the old gold-checked-first order). */
	if ((explore_gold_radius > 0) && (dist <= explore_gold_radius) && cell_has_gold(y, x))
		return (EXPLORE_LOOT);
	if (!explore_no_items && (explore_item_radius > 0) &&
	                (dist <= explore_item_radius) && cell_has_item(y, x))
		return (EXPLORE_LOOT);

	/* (B4) Recover our own missiles at a longer range than ordinary loot -- ammo
	 * is a limited resource and worth a small detour to pick back up. */
	if ((explore_item_radius > 0) && (dist <= explore_item_radius * 2) &&
	                cell_has_ammo(y, x))
		return (EXPLORE_LOOT);

	/* Frontier: a seen grid next to one we have never seen -- and not blacklisted
	 * as a dead frontier that revealed nothing when we reached it last time. */
	if (!(explore_bad && explore_bad[y * cur_wid + x]))
	{
		for (d = 0; d < 8; d++)
		{
			int ny = y + dy8[d], nx = x + dx8[d];
			if ((ny < 0) || (ny >= cur_hgt) || (nx < 0) || (nx >= cur_wid)) continue;
			if (!explore_is_seen(ny, nx)) return (EXPLORE_FRONTIER);
		}
	}

	return (EXPLORE_NONE);
}

static bool find_nearest_goal(int *gy, int *gx, int *kind)
{
	/* 8-directional neighbour offsets. */
	static const int dy8[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
	static const int dx8[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };

	int n = cur_hgt * cur_wid;
	byte *seen;
	int *queue, *dist;
	int head = 0, tail = 0;
	int start = p_ptr->py * cur_wid + p_ptr->px;
	bool found = FALSE;

	*kind = EXPLORE_NONE;
	if (n <= 0) return (FALSE);

	C_MAKE(seen, n, byte);
	C_MAKE(queue, n, int);
	C_MAKE(dist, n, int);

	seen[start] = 1;
	dist[start] = 0;
	queue[tail++] = start;

	while (head < tail)
	{
		int cur = queue[head++];
		int cy = cur / cur_wid;
		int cx = cur % cur_wid;
		int d;

		/* The BFS visits in increasing distance, so the first interesting grid we
		 * pop is the nearest one: take it. (The player's own tile never counts as
		 * a goal, but we still expand from it.) */
		if (!((cy == p_ptr->py) && (cx == p_ptr->px)))
		{
			int k = explore_cell_interest(cy, cx, dist[cur]);
			if (k != EXPLORE_NONE)
			{
				*gy = cy; *gx = cx; *kind = k; found = TRUE; break;
			}
		}

		/* Expand to walkable, known, not-yet-visited neighbours. */
		for (d = 0; d < 8; d++)
		{
			int ny = cy + dy8[d];
			int nx = cx + dx8[d];
			int ncell;

			if ((ny < 0) || (ny >= cur_hgt) || (nx < 0) || (nx >= cur_wid)) continue;

			ncell = ny * cur_wid + nx;
			if (seen[ncell]) continue;
			if (!explore_walkable_hook(ny, nx, NULL)) continue;

			seen[ncell] = 1;
			dist[ncell] = dist[cur] + 1;
			queue[tail++] = ncell;
		}
	}

	C_FREE(seen, n, byte);
	C_FREE(queue, n, int);
	C_FREE(dist, n, int);

	return (found);
}

/*
 * Pick the next exploration goal and arm a travel leg toward it, including the
 * anti-oscillation blacklisting of dead frontiers. Shared by auto-explore and
 * auto-play. Assumes explore_sync_seen() has already run this turn. Returns TRUE
 * if a leg was armed, FALSE if nothing reachable is left to uncover.
 */
static bool explore_pick_goal(int *gy, int *gx)
{
	int goal_kind = EXPLORE_NONE;

	/* Anti-oscilación: si LLEGAMOS a una meta-FRONTERA y no descubrió nada nuevo
	 * (frontera "muerta"), la metemos en la lista negra. Solo si de verdad la
	 * alcanzamos (estamos sobre ella): un tramo cortado a medias por recoger oro
	 * no debe descartar una frontera válida. Las metas de loot nunca se blacklistean. */
	if ((explore_goal_y >= 0) && !explore_goal_is_loot && explore_bad &&
	                (p_ptr->py == explore_goal_y) && (p_ptr->px == explore_goal_x) &&
	                (explore_seen_count <= explore_goal_count))
	{
		explore_bad[explore_goal_y * cur_wid + explore_goal_x] = 1;
	}
	explore_goal_y = explore_goal_x = -1;

	/* Nothing reachable left to uncover. */
	if (!find_nearest_goal(gy, gx, &goal_kind)) return (FALSE);

	/* Recordar la meta y el progreso al fijarla (para el chequeo de arriba). */
	explore_goal_y = *gy;
	explore_goal_x = *gx;
	explore_goal_count = explore_seen_count;
	explore_goal_is_loot = (goal_kind == EXPLORE_LOOT);

	return (TRUE);
}

static bool explore_arm_next_leg(astar_walkable_hook hook)
{
	int gy = 0, gx = 0;

	if (!explore_pick_goal(&gy, &gx)) return (FALSE);

	/* Arm the leg toward it (no disturb/messages: that's the caller's job).
	 * Seen-gated and routing around grids we gave up on (explore_block). */
	return (travel_plan(gy, gx, hook));
}

/*
 * One auto-explore decision. Called from process_player()'s energy loop while
 * exploring and not currently mid-leg. Picks the nearest unexplored frontier
 * and arms a travel leg toward it; when none remain, exploration is done.
 */
void explore_step(void)
{
	/* A travel leg is still running: let it finish (travel branch handles it). */
	if (travelling) return;

	/* States where exploring makes no sense. */
	if (p_ptr->confused || p_ptr->image || p_ptr->wild_mode)
	{
		exploring = 0;
		p_ptr->redraw |= (PR_STATE);
		return;
	}

	/* Parar si hay un enemigo a la vista (que el jugador decida). */
	if (monster_threat_visible())
	{
		exploring = 0;
		p_ptr->redraw |= (PR_STATE);
		msg_print("There is a monster nearby; auto-explore stopped.");
		return;
	}

	/* Fold in everything seen so far, then arm the next leg. */
	explore_sync_seen();

	if (!explore_arm_next_leg(explore_walkable_hook))
	{
		exploring = 0;
		p_ptr->redraw |= (PR_STATE);
		msg_print("Done exploring.");
	}
}

/*
 * Start auto-exploring. Bound to a command key (Ctrl-E). The per-turn stepping
 * is driven by process_player(); here we just validate and flip the flag.
 */
void do_cmd_explore(void)
{
	if (p_ptr->immovable) return;

	if (p_ptr->confused)
	{
		msg_print("You are too confused!");
		return;
	}

	if (p_ptr->wild_mode)
	{
		msg_print("You cannot auto-explore the world map.");
		return;
	}

	/* No autoexplorar con un enemigo a la vista. */
	if (monster_threat_visible())
	{
		msg_print("There is a monster nearby.");
		return;
	}

	/* Cancel running/resting/repeat/travel, then begin exploring. The energy
	 * loop in process_player() will call explore_step() to pick the first leg. */
	disturb(0, 0);
	exploring = 1;
	p_ptr->redraw |= (PR_STATE);
}


/*
 * ------------------------------------------------------------------ Auto-play
 *
 * "Play the level for me": built on the auto-explore machinery but, instead of
 * stopping for monsters, it also FIGHTS, FLEES, HEALS, EATS, manages its LIGHT,
 * and -- once a level is cleared -- walks to a known down staircase and descends
 * to keep going. Deliberately NOT a borg: melee only (no spells/ranged tactics),
 * no inventory management, no shopping.
 *
 * Unlike auto-explore it never arms a multi-step travel leg: autoplay_step()
 * takes exactly ONE step/action per turn and re-evaluates its priorities every
 * turn, so it reacts immediately to a new threat or dropping HP.
 */

/* ---- Lua policy bridge (AuToME) ----
 *
 * Tunable policy -- shopping targets, thresholds, a per-monster threat table --
 * lives in lib/scpt/autoplay.lua (loaded by init.lua), so it can be retuned
 * without recompiling. The C engine consults it but ALWAYS has a fallback, so
 * the bot works even if the script is missing. We probe once that the Lua
 * functions exist, because call_lua() errors loudly on a missing global. */
static int autoplay_lua = -1;     /* -1 unknown, 0 absent, 1 present */

static bool autoplay_lua_ok(void)
{
	if (autoplay_lua < 0)
	{
		cptr r = string_exec_lua("return tostring(type(autoplay_config) == 'function')");
		autoplay_lua = (r && !strcmp(r, "true")) ? 1 : 0;
	}
	return (autoplay_lua == 1);
}

/* Integer config knob from Lua autoplay_config(key); returns `def` if Lua is
 * absent or hands back a negative value ("no override, use the C default"). */
static int autoplay_cfg(cptr key, int def)
{
	s32b v = def;
	if (!autoplay_lua_ok()) return (def);
	if (!call_lua("autoplay_config", "(s)", "d", (char *)key, &v)) return (def);
	return (v >= 0) ? ((int)v) : def;
}

/* Chasing state: a monster we have given up closing on (it flees / keeps its
 * distance -- e.g. a fruit bat doing bait-and-switch) and progress tracking, so
 * we don't oscillate after it forever. */
static s16b ap_ignore_idx = 0;    /* monster we stopped chasing (0 = none)   */
static s16b ap_chase_idx  = 0;    /* monster we are currently going after     */
static int  ap_chase_turns = 0;   /* consecutive turns targeting it           */

/* A staircase we have committed to walking toward. Without this, the dead-end
 * scummer re-picks the nearest stair every turn by straight-line distance, so
 * with two stairs it flip-flops which one is "nearest" as it moves and ends up
 * oscillating in place forever. We lock one and keep heading to it until we
 * reach it, take stairs, or it goes stale. (y = -1 means none locked.) */
static int  ap_stair_y = -1, ap_stair_x = -1;

static void autoplay_clear_stair(void) { ap_stair_y = ap_stair_x = -1; }

/* A cell we keep trying to step into but can't actually enter -- normally a door
 * being opened (legit, costs a turn each), but if it never yields (a jammed door
 * we can't force, an obstacle) we'd retry it forever. Mirrors travel_stuck: after
 * AP_STUCK_MAX failed tries we explore_block_cell() it and reroute. */
#define AP_STUCK_MAX 12
static int  ap_stuck_y = -1, ap_stuck_x = -1, ap_stuck_count = 0;

static void autoplay_clear_stuck(void) { ap_stuck_y = ap_stuck_x = -1; ap_stuck_count = 0; }

static bool autoplay_too_dangerous(monster_type *m);   /* fwd: threat verdict */

/* A foe not worth chasing: an erratic bouncer (fruit bats etc. -- RAND_25/50,
 * impossible to corner) or something that simply can't hurt us (no melee blows
 * AND casts nothing). We still hit one that wanders adjacent; we just won't run
 * after it across the level (the silliest chases) or waste ammo on it. */
static bool autoplay_low_value_foe(monster_type *m)
{
	monster_race *r_ptr = &r_info[m->r_idx];
	int b;
	bool has_blow = FALSE;

	if (r_ptr->flags1 & (RF1_RAND_25 | RF1_RAND_50)) return (TRUE);

	for (b = 0; b < 4; b++)
		if (r_ptr->blow[b].method) { has_blow = TRUE; break; }

	if (!has_blow && !r_ptr->freq_inate && !r_ptr->freq_spell) return (TRUE);
	return (FALSE);
}

/* ---- threat model: roughly how much damage a foe deals us per turn ---- */

/* Expected damage/turn this monster can do to us: melee blows (average roll,
 * crude ~60% land factor), a chunk for casters/breathers scaled by their HP and
 * spell frequency, and a bump for being faster than us. Approximate but enough
 * to compare foes and sum a pack's threat. Always >= 1. */
static int autoplay_monster_danger(monster_type *m)
{
	monster_race *r_ptr = &r_info[m->r_idx];
	int b, dmg = 0;

	/* Melee: sum the average damage of each blow. */
	for (b = 0; b < 4; b++)
	{
		if (!r_ptr->blow[b].method) continue;
		dmg += (r_ptr->blow[b].d_dice * (r_ptr->blow[b].d_side + 1)) / 2;
	}
	dmg = (dmg * 3) / 5;        /* not every swing lands (crude, AC-agnostic) */

	/* Casters / breathers: breaths scale with current HP; approximate the extra
	 * damage/turn as a fraction of its HP times how often it acts with magic. */
	if (r_ptr->freq_inate || r_ptr->freq_spell)
		dmg += ((int)(m->maxhp / 6) * (r_ptr->freq_inate + r_ptr->freq_spell)) / 100;

	/* Faster than us -> it gets extra turns to hit us. */
	if (m->mspeed > 110)
		dmg += (dmg * (m->mspeed - 110)) / 20;

	return ((dmg < 1) ? 1 : dmg);
}

/* Sum of danger/turn of the visible hostiles near us (within cluster_range
 * tiles), the count in *foes, and the nearest of them in *nearest. The
 * pack-threat metric: one jackal is nothing, ten are lethal. */
static int autoplay_cluster_danger(int *foes, monster_type **nearest)
{
	int i, total = 0, n = 0, bd = 0;
	int range = autoplay_cfg("cluster_range", 8);
	monster_type *best = NULL;

	for (i = 1; i < m_max; i++)
	{
		monster_type *m = &m_list[i];
		int d;

		if (!m->r_idx || !m->ml) continue;
		if (m->status != MSTATUS_ENEMY) continue;
		if (m->monfear) continue;                 /* a fleeing foe isn't pressing us */
		d = distance(p_ptr->py, p_ptr->px, m->fy, m->fx);
		if (d > range) continue;

		total += autoplay_monster_danger(m);
		n++;
		if (!best || (d < bd)) { best = m; bd = d; }
	}

	if (foes) *foes = n;
	if (nearest) *nearest = best;
	return (total);
}

/* Nearest VISIBLE real enemy worth engaging; NULL if none. Skips a foe we gave
 * up chasing, a frightened one fleeing in the distance, and one judged too
 * dangerous to melee (paralysers, out-of-depth) -- so the bot routes past those
 * rather than throwing itself at them. A merely-frightened/given-up one adjacent
 * is still returned (we just hit it); a too-dangerous one is never a target. */
static monster_type *autoplay_nearest_enemy(int *dist)
{
	int i, bd = 0;
	monster_type *best = NULL;

	/* Forget the give-up target once it's dead or out of sight. */
	if (ap_ignore_idx && ((ap_ignore_idx >= m_max) ||
	                !m_list[ap_ignore_idx].r_idx || !m_list[ap_ignore_idx].ml))
		ap_ignore_idx = 0;

	for (i = 1; i < m_max; i++)
	{
		monster_type *m_ptr = &m_list[i];
		int d;

		if (!m_ptr->r_idx) continue;
		if (!m_ptr->ml) continue;                       /* must be seen      */
		if (m_ptr->status != MSTATUS_ENEMY) continue;   /* only real foes    */

		d = distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);

		/* Ignore the give-up target, frightened fleers, and low-value foes
		 * (erratic bats / harmless things) -- unless they're adjacent. */
		if ((i == ap_ignore_idx) && (d > 1)) continue;
		if (m_ptr->monfear && (d > 1)) continue;
		if (autoplay_low_value_foe(m_ptr) && (d > 1)) continue;

		/* Never melee a foe judged too dangerous (paralyser, out-of-depth): the
		 * bot routes past it instead of dying on its gaze/claws. */
		if (autoplay_too_dangerous(m_ptr)) continue;

		if (!best || (d < bd)) { best = m_ptr; bd = d; }
	}

	if (best && dist) *dist = bd;
	return (best);
}

/* Backpack index of the best healing potion to drink now, or -1. When 'severe'
 * (HP critical) we reach for a big heal first; otherwise spend a cheap cure. */
static int autoplay_find_heal(bool severe)
{
	static const int heavy[] =
	{
		SV_POTION_CURE_CRITICAL, SV_POTION_HEALING, SV_POTION_CURE_SERIOUS,
		SV_POTION_STAR_HEALING, SV_POTION_LIFE, SV_POTION_CURE_LIGHT
	};
	static const int light[] =
	{
		SV_POTION_CURE_LIGHT, SV_POTION_CURE_SERIOUS, SV_POTION_CURE_CRITICAL
	};
	const int *order = severe ? heavy : light;
	int n = severe ? 6 : 3;
	int k, i;

	for (k = 0; k < n; k++)
	{
		for (i = 0; i < INVEN_PACK; i++)
		{
			object_type *o_ptr = &p_ptr->inventory[i];
			if (!o_ptr->k_idx) continue;
			if ((o_ptr->tval == TV_POTION) && (o_ptr->sval == order[k])) return (i);
		}
	}
	return (-1);
}

/* ---- defensive items: lift a crippling status, or teleport out of trouble ---- */

/* Pack index of the first potion whose sval is in svals[0..n-1], else -1. */
static int autoplay_find_potion(const int *svals, int n)
{
	int i, k;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx || (o_ptr->tval != TV_POTION)) continue;
		for (k = 0; k < n; k++) if (o_ptr->sval == svals[k]) return (i);
	}
	return (-1);
}

/* A disabling status is active AND we carry a potion that lifts it: return that
 * potion's pack index and set *what to a label, else -1. Potions can be quaffed
 * while blind/confused, so this self-clears those (which otherwise cripple the
 * bot -- blind = can't see to explore, confused = can't move straight). Priority:
 * confusion > blindness > meaningful poison. (Fear is handled in the combat
 * ladder, where it actually matters.) svals are ordered cheapest-first so we
 * don't burn a big heal on a minor ailment. */
static int autoplay_find_cure(cptr *what)
{
	static const int conf[] =                  /* potions that clear confusion */
	{ SV_POTION_CURE_SERIOUS, SV_POTION_CURE_CRITICAL, SV_POTION_HEALING,
	  SV_POTION_STAR_HEALING, SV_POTION_LIFE };
	static const int blnd[] =                  /* potions that clear blindness */
	{ SV_POTION_CURE_LIGHT, SV_POTION_CURE_SERIOUS, SV_POTION_CURE_CRITICAL,
	  SV_POTION_HEALING, SV_POTION_STAR_HEALING, SV_POTION_LIFE };
	static const int pois[] =                  /* dedicated poison cures only */
	{ SV_POTION_CURE_POISON, SV_POTION_CURING };
	int it;

	if (p_ptr->confused)
	{ it = autoplay_find_potion(conf, 5); if (it >= 0) { *what = "the confusion"; return (it); } }
	if (p_ptr->blind)
	{ it = autoplay_find_potion(blnd, 6); if (it >= 0) { *what = "the blindness"; return (it); } }
	if (p_ptr->poisoned > 10)
	{ it = autoplay_find_potion(pois, 2); if (it >= 0) { *what = "the poison"; return (it); } }
	return (-1);
}

/* Pack index of a potion that removes fear (to face a foe), or -1. */
static int autoplay_find_boldness(void)
{
	static const int f[] = { SV_POTION_BOLDNESS, SV_POTION_HEROISM };
	return (autoplay_find_potion(f, 2));
}

/* Pack index of an escape scroll: prefer Phase Door (short hop), else Teleport.
 * Reading needs eyes, so the caller must ensure we are not blind/confused. */
static int autoplay_find_escape(void)
{
	int i, tele = -1;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx || (o_ptr->tval != TV_SCROLL)) continue;
		if (o_ptr->sval == SV_SCROLL_PHASE_DOOR) return (i);
		if (o_ptr->sval == SV_SCROLL_TELEPORT) tele = i;
	}
	return (tele);
}

/* Read escape scroll in slot 'item': blink (Phase Door) or teleport away, and
 * spend the scroll. One turn. */
static void autoplay_escape(int item)
{
	object_type *o_ptr = &p_ptr->inventory[item];
	int dist = (o_ptr->sval == SV_SCROLL_PHASE_DOOR) ? 10 : 100;

	energy_use = 100;
	teleport_player(dist);
	inven_item_increase(item, -1);
	inven_item_describe(item);
	inven_item_optimize(item);
}

/* ---- ranged attacks: fire a launcher, or throw something, at a foe ---- */

/* TRUE if we wield a usable missile launcher (bow / sling / crossbow). */
static bool autoplay_have_launcher(void)
{
	object_type *bow = &p_ptr->inventory[INVEN_BOW];
	return (bow->k_idx && bow->tval && (bow->tval != TV_INSTRUMENT));
}

/* Index of ammo matching our launcher (prefers the quiver), or -1. */
static int autoplay_find_ammo(void)
{
	object_type *q = &p_ptr->inventory[INVEN_AMMO];
	int i;

	if (q->k_idx && (q->tval == p_ptr->tval_ammo)) return (INVEN_AMMO);
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (o_ptr->k_idx && (o_ptr->tval == p_ptr->tval_ammo)) return (i);
	}
	return (-1);
}

/* Pack index of something worth throwing. v1: flasks of oil (fire damage) -- but
 * not while we burn them as lantern fuel. -1 if nothing suitable. */
/* Rough "expected damage" worth of throwing this item, or 0 if not worth it.
 * Used to pick the best thing to throw (ranged v2: not just flasks of oil). */
static int autoplay_throwable_score(object_type *o_ptr)
{
	if (!o_ptr->k_idx) return (0);

	/* Flasks of oil: reliable fire damage -- but not the ones we burn as lamp
	 * fuel. */
	if (o_ptr->tval == TV_FLASK)
	{
		object_type *lite = &p_ptr->inventory[INVEN_LITE];
		if (lite->k_idx && (lite->tval == TV_LITE) && (lite->sval == SV_LITE_LANTERN))
			return (0);
		return (12 + o_ptr->to_d);
	}

	/* Boulders are strong thrown -- but only with the skill (else heavy & weak). */
	if ((o_ptr->tval == TV_JUNK) && (o_ptr->sval == SV_BOULDER))
	{
		if (get_skill(SKILL_BOULDER) <= 0) return (0);
		return (20 + (int)get_skill_scale(SKILL_BOULDER, 80) +
		        (o_ptr->dd * o_ptr->ds));
	}

	/* Ammo with no launcher to fire it: throwing is better than nothing. If we DO
	 * wield a matching launcher we fire it instead (handled separately), so don't
	 * offer that ammo as a throwable. */
	if ((o_ptr->tval == TV_SHOT) || (o_ptr->tval == TV_ARROW) || (o_ptr->tval == TV_BOLT))
	{
		if (autoplay_have_launcher() && (o_ptr->tval == p_ptr->tval_ammo)) return (0);
		return ((o_ptr->dd * o_ptr->ds) + o_ptr->to_d);
	}

	return (0);
}

/* Pack index of the best thing to throw (highest autoplay_throwable_score), or -1. */
static int autoplay_find_throwable(void)
{
	int i, best = -1, best_score = 0;

	for (i = 0; i < INVEN_PACK; i++)
	{
		int s = autoplay_throwable_score(&p_ptr->inventory[i]);
		if (s > best_score) { best_score = s; best = i; }
	}
	return (best);
}

/* Any hostile right next to us? Then deal with the melee threat before shooting. */
static bool autoplay_adjacent_enemy(void)
{
	int i;
	for (i = 1; i < m_max; i++)
	{
		monster_type *m_ptr = &m_list[i];
		if (!m_ptr->r_idx || !m_ptr->ml) continue;
		if (m_ptr->status != MSTATUS_ENEMY) continue;
		if (distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) <= 1) return (TRUE);
	}
	return (FALSE);
}

/* Nearest VISIBLE hostile that is NOT adjacent and we have a clear shot at --
 * INCLUDING foes too dangerous to melee (floating eyes etc.), which is exactly
 * what shooting them from range is for. NULL if none. */
static monster_type *autoplay_nearest_ranged(void)
{
	int i, bd = 0;
	monster_type *best = NULL;

	for (i = 1; i < m_max; i++)
	{
		monster_type *m_ptr = &m_list[i];
		int d;

		if (!m_ptr->r_idx || !m_ptr->ml) continue;
		if (m_ptr->status != MSTATUS_ENEMY) continue;
		d = distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);
		if (d <= 1) continue;                  /* adjacent: melee/flee handles it */
		if (m_ptr->monfear) continue;          /* let fleers flee */
		if (autoplay_low_value_foe(m_ptr)) continue;   /* don't waste ammo on bats/harmless */
		if (!projectable(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx)) continue;
		if (!best || (d < bd)) { best = m_ptr; bd = d; }
	}
	return (best);
}

/* Point the game's target at a monster so get_aim_dir() returns "use target". */
static void autoplay_aim_at(monster_type *m_ptr)
{
	target_who = (s16b)(m_ptr - m_list);
	target_row = m_ptr->fy;
	target_col = m_ptr->fx;
}

/* Fire ammo 'item' / throw 'item' at m_ptr. autoplay_force_item feeds get_item()
 * (so no picker) and the aimed target feeds get_aim_dir(). do_cmd_fire/throw set
 * energy_use themselves. */
static void autoplay_shoot(int item, monster_type *m_ptr)
{
	autoplay_aim_at(m_ptr);
	autoplay_force_item = item; autoplay_force_item_on = TRUE;
	do_cmd_fire();
	autoplay_force_item_on = FALSE;
}

static void autoplay_throw_at(int item, monster_type *m_ptr)
{
	autoplay_aim_at(m_ptr);
	autoplay_force_item = item; autoplay_force_item_on = TRUE;
	do_cmd_throw();
	autoplay_force_item_on = FALSE;
}

/* ---- (B) use beneficial devices/potions: detect, haste, restore, uncurse ---- */

/* Kinds of "device" the bot can use without a picker, via autoplay_force_item. */
#define APDEV_STAFF  1
#define APDEV_ROD    2
#define APDEV_SCROLL 3

/* Use staff/rod/scroll in pack slot 'item' (force_item feeds get_item; the
 * do_cmd_* set energy_use themselves). */
static void autoplay_use_device(int item, int kind)
{
	autoplay_force_item = item; autoplay_force_item_on = TRUE;
	switch (kind)
	{
	case APDEV_STAFF:  do_cmd_use_staff(); break;
	case APDEV_ROD:    do_cmd_zap_rod(); break;
	case APDEV_SCROLL: do_cmd_read_scroll(); break;
	}
	autoplay_force_item_on = FALSE;
}

/* Find a usable detection item (monsters/traps/mapping) with charges. Sets *kind
 * (APDEV_*) and returns its pack index, or -1. Staves/scrolls never prompt for a
 * direction; rods only if unaware/aimed, so we only take AWARE detection rods. */
static int autoplay_find_detection(int *kind)
{
	static const int staves[] =
	{ SV_STAFF_SENSE_MONSTER, SV_STAFF_REVEAL_WAYS, SV_STAFF_SENSE_HIDDEN,
	  SV_STAFF_VISION, SV_STAFF_MITHRANDIR };
	static const int rods[] =
	{ SV_ROD_DETECTION, SV_ROD_MAPPING, SV_ROD_DETECT_TRAP, SV_ROD_DETECT_DOOR };
	static const int scrolls[] =
	{ SV_SCROLL_DETECT_INVIS, SV_SCROLL_DETECT_TRAP, SV_SCROLL_DETECT_DOOR };
	int i, k;

	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) continue;

		if ((o_ptr->tval == TV_STAFF) && (o_ptr->pval > 0))
			for (k = 0; k < 5; k++)
				if (o_ptr->sval == staves[k]) { *kind = APDEV_STAFF; return (i); }

		if ((o_ptr->tval == TV_ROD) && (o_ptr->timeout <= 0) && object_aware_p(o_ptr))
			for (k = 0; k < 4; k++)
				if (o_ptr->sval == rods[k]) { *kind = APDEV_ROD; return (i); }

		if (o_ptr->tval == TV_SCROLL)
			for (k = 0; k < 3; k++)
				if (o_ptr->sval == scrolls[k]) { *kind = APDEV_SCROLL; return (i); }
	}
	return (-1);
}

/* A worn item is cursed (so we'd want a Remove Curse scroll). */
static bool autoplay_wearing_cursed(void)
{
	int i;
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
		if (p_ptr->inventory[i].k_idx && cursed_p(&p_ptr->inventory[i])) return (TRUE);
	return (FALSE);
}

/* Pack index of a Remove Curse scroll, or -1. */
static int autoplay_find_remove_curse(void)
{
	int i;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (o_ptr->k_idx && (o_ptr->tval == TV_SCROLL) &&
		                ((o_ptr->sval == SV_SCROLL_REMOVE_CURSE) ||
		                 (o_ptr->sval == SV_SCROLL_STAR_REMOVE_CURSE)))
			return (i);
	}
	return (-1);
}

/* Pack index of a Potion of Speed, or -1. */
static int autoplay_find_speed(void)
{
	int i;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (o_ptr->k_idx && (o_ptr->tval == TV_POTION) && (o_ptr->sval == SV_POTION_SPEED))
			return (i);
	}
	return (-1);
}

/* A drained stat (stat_cur < stat_max) with a matching Restore potion in the
 * pack: return that potion's slot (and set *stat to the index), else -1. */
static int autoplay_find_restore(int *stat)
{
	static const int restore_sval[6] =
	{ SV_POTION_RES_STR, SV_POTION_RES_INT, SV_POTION_RES_WIS,
	  SV_POTION_RES_DEX, SV_POTION_RES_CON, SV_POTION_RESTORE_MANA /* CHR has none here */ };
	int s, i;

	for (s = 0; s < 6; s++)
	{
		if (s == 5) continue;                       /* CHR: no restore potion */
		if (p_ptr->stat_cur[s] >= p_ptr->stat_max[s]) continue;   /* not drained */
		for (i = 0; i < INVEN_PACK; i++)
		{
			object_type *o_ptr = &p_ptr->inventory[i];
			if (o_ptr->k_idx && (o_ptr->tval == TV_POTION) &&
			                (o_ptr->sval == restore_sval[s]))
			{ *stat = s; return (i); }
		}
	}
	return (-1);
}

/* Backpack index of an edible staple food, or -1. */
static int autoplay_find_food(void)
{
	int i;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) continue;
		if (o_ptr->tval != TV_FOOD) continue;
		switch (o_ptr->sval)
		{
		case SV_FOOD_RATION:
		case SV_FOOD_BISCUIT:
		case SV_FOOD_JERKY:
		case SV_FOOD_WAYBREAD:
		case SV_FOOD_SLIME_MOLD:
			return (i);
		}
	}
	return (-1);
}

/* Backpack index of a usable spare torch (with fuel left), or -1. */
static int autoplay_find_torch(void)
{
	int i;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) continue;
		if ((o_ptr->tval == TV_LITE) && (o_ptr->sval == SV_LITE_TORCH) &&
		                (o_ptr->timeout > 0)) return (i);
	}
	return (-1);
}

/* Backpack index of a flask of oil, or -1. */
static int autoplay_find_oil(void)
{
	int i;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) continue;
		if (o_ptr->tval == TV_FLASK) return (i);
	}
	return (-1);
}

/* Drink the potion in backpack slot 'item' (one turn). */
static void autoplay_quaff(int item)
{
	object_type *o_ptr = &p_ptr->inventory[item];

	energy_use = 100;
	(void)quaff_potion(o_ptr->tval, o_ptr->sval, o_ptr->pval, o_ptr->pval2);
	inven_item_increase(item, -1);
	inven_item_describe(item);
	inven_item_optimize(item);
}

/* Move the torch/lantern in backpack slot 'item' into the light slot. The light
 * currently worn is dropped on the floor (drop_old) or returned to the pack. */
static void autoplay_equip_lite(int item, bool drop_old)
{
	object_type forge;

	/* Keep a copy, then remove the new light from the pack. */
	object_copy(&forge, &p_ptr->inventory[item]);
	forge.number = 1;
	inven_item_increase(item, -1);
	inven_item_optimize(item);

	/* Take off whatever is currently lit (to floor or pack). */
	if (p_ptr->inventory[INVEN_LITE].k_idx)
		(void)inven_takeoff(INVEN_LITE, 255, drop_old);

	/* Wear the new one. */
	object_copy(&p_ptr->inventory[INVEN_LITE], &forge);
	equip_cnt++;

	p_ptr->update |= (PU_BONUS | PU_TORCH | PU_HP);
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER);
	energy_use = 100;
}

/* Tend the light source. Returns TRUE if it spent the turn doing so. */
static bool autoplay_manage_light(void)
{
	object_type *lite = &p_ptr->inventory[INVEN_LITE];
	int t, oil;

	/* No light at all: wield a spare torch if we have one. */
	if (!lite->k_idx)
	{
		t = autoplay_find_torch();
		if (t >= 0)
		{
			autoplay_equip_lite(t, FALSE);
			msg_print("Autoplay: equipping a torch.");
			return (TRUE);
		}
		return (FALSE);
	}

	if (lite->tval != TV_LITE) return (FALSE);

	/* A burnt-out torch: drop it and light a fresh one. */
	if (lite->sval == SV_LITE_TORCH)
	{
		if (lite->timeout <= 0)
		{
			t = autoplay_find_torch();
			if (t >= 0)
			{
				autoplay_equip_lite(t, TRUE);
				msg_print("Autoplay: replacing the spent torch.");
				return (TRUE);
			}
		}
		return (FALSE);
	}

	/* A lantern: refuel from oil when low; if dead and out of oil, fall back to
	 * a torch (keeping the lamp for later). */
	if (lite->sval == SV_LITE_LANTERN)
	{
		if (lite->timeout < 500)
		{
			oil = autoplay_find_oil();
			if (oil >= 0)
			{
				object_type *o_ptr = &p_ptr->inventory[oil];

				lite->timeout += o_ptr->pval;
				if (lite->timeout >= FUEL_LAMP) lite->timeout = FUEL_LAMP;
				inven_item_increase(oil, -1);
				inven_item_optimize(oil);
				p_ptr->update |= (PU_TORCH);
				energy_use = 50;
				msg_print("Autoplay: refuelling the lamp.");
				return (TRUE);
			}
			if (lite->timeout <= 0)
			{
				t = autoplay_find_torch();
				if (t >= 0)
				{
					autoplay_equip_lite(t, FALSE);
					msg_print("Autoplay: out of oil, switching to a torch.");
					return (TRUE);
				}
			}
		}
		return (FALSE);
	}

	return (FALSE);
}

/* Read-only: would autoplay_manage_light() do something right now? (Mirrors its
 * act-conditions so the decision layer / oracle can check without acting.) */
static bool autoplay_light_needs(void)
{
	object_type *lite = &p_ptr->inventory[INVEN_LITE];

	if (!lite->k_idx) return (autoplay_find_torch() >= 0);
	if (lite->tval != TV_LITE) return (FALSE);

	if (lite->sval == SV_LITE_TORCH)
		return ((lite->timeout <= 0) && (autoplay_find_torch() >= 0));

	if (lite->sval == SV_LITE_LANTERN)
	{
		if (lite->timeout >= 500) return (FALSE);
		if (autoplay_find_oil() >= 0) return (TRUE);
		return ((lite->timeout <= 0) && (autoplay_find_torch() >= 0));
	}
	return (FALSE);
}

/* No usable spare light anywhere: no fresh torch in the pack, and (if wielding a
 * lantern) no oil to refuel it. */
static bool autoplay_no_spare_light(void)
{
	object_type *lite = &p_ptr->inventory[INVEN_LITE];

	if (autoplay_find_torch() >= 0) return (FALSE);          /* a spare torch */
	if (lite->k_idx && (lite->tval == TV_LITE) &&
	                (lite->sval == SV_LITE_LANTERN) && (autoplay_find_oil() >= 0))
		return (FALSE);                                       /* oil for the lamp */
	return (TRUE);
}

/* In the dark with no way to relight: the worn light gives nothing (no light
 * worn, or a torch/lantern burnt to 0) AND there's no spare/oil to fix it.
 * Diving like this is suicide -- and with nothing lit the frontier search finds
 * no goal, so the bot would otherwise just stall. (Permanent/artifact lights,
 * which never run out, are never "dark".) */
static bool autoplay_light_dark(void)
{
	object_type *lite = &p_ptr->inventory[INVEN_LITE];
	bool dark;

	if (!lite->k_idx) dark = TRUE;
	else if (lite->tval != TV_LITE) dark = FALSE;
	else if ((lite->sval == SV_LITE_TORCH) || (lite->sval == SV_LITE_LANTERN))
		dark = (lite->timeout <= 0);
	else dark = FALSE;     /* permanent light (phial, star, ...) */

	return (dark && autoplay_no_spare_light());
}

/* Take one step toward (gy,gx) over terrain accepted by 'hook', via
 * move_player_aux -- so stepping onto an adjacent monster ATTACKS it. Returns
 * TRUE if a step/attack was issued (a turn spent). The seen-gated
 * explore_walkable_hook keeps to known ground; travel_walkable_real pushes into
 * as-yet-unseen real floor (delving into the dark). */
static bool autoplay_step_towards_hook(int gy, int gx, bool do_pickup,
                                       astar_walkable_hook hook)
{
	path_result *route;
	int dir = 0, d, ny, nx;

	if ((gy == p_ptr->py) && (gx == p_ptr->px)) return (FALSE);

	route = astar_find_path_cb(cur_hgt, cur_wid, hook, NULL,
	                           p_ptr->py, p_ptr->px, gy, gx, ASTAR_8DIR_CUT);
	if (!route || (route->length < 2))
	{
		if (route) path_free(route);
		return (FALSE);
	}

	ny = route->steps[1].y;
	nx = route->steps[1].x;
	path_free(route);

	for (d = 1; d <= 9; d++)
	{
		if (d == 5) continue;
		if ((p_ptr->py + ddy[d] == ny) && (p_ptr->px + ddx[d] == nx)) { dir = d; break; }
	}
	if (!dir) return (FALSE);

	{
		int oy = p_ptr->py, ox = p_ptr->px;

		energy_use = 100;
		move_player_aux(dir, do_pickup, 1, TRUE);

		/* Stuck-on-a-cell give-up. If we issued a step but didn't actually move,
		 * and the blocking cell holds NO monster (so it's terrain -- a door being
		 * forced, an obstacle -- not a melee attack, which legitimately stays in
		 * place), count the failures. A normal closed door opens in a turn or two;
		 * if a cell resists AP_STUCK_MAX tries it won't yield, so block it and let
		 * A* reroute (and drop any stair lock that was leading us into it) rather
		 * than bumping it forever. */
		if ((p_ptr->py == oy) && (p_ptr->px == ox) && !cave[ny][nx].m_idx)
		{
			if ((ny == ap_stuck_y) && (nx == ap_stuck_x)) ap_stuck_count++;
			else { ap_stuck_y = ny; ap_stuck_x = nx; ap_stuck_count = 1; }

			if (ap_stuck_count > AP_STUCK_MAX)
			{
				explore_block_cell(ny, nx);
				autoplay_clear_stair();
				autoplay_clear_stuck();
			}
		}
		else autoplay_clear_stuck();   /* moved (or it was a melee bump): reset */
	}
	return (TRUE);
}

/* Default step toward known ground (auto-explore policy). */
static bool autoplay_step_towards(int gy, int gx, bool do_pickup)
{
	return (autoplay_step_towards_hook(gy, gx, do_pickup, explore_walkable_hook));
}

/* Nearest still-unseen real-floor cell reachable over real terrain (ignoring
 * what we've seen), for delving into the dark when the seen-gated frontier
 * search comes up empty. Returns TRUE and fills (*gy,*gx). */
static bool autoplay_blind_goal(int *gy, int *gx)
{
	static const int dy8[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
	static const int dx8[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
	int n = cur_hgt * cur_wid, head = 0, tail = 0;
	int start = p_ptr->py * cur_wid + p_ptr->px;
	byte *seen;
	int *queue;
	bool found = FALSE;

	if (n <= 0) return (FALSE);
	C_MAKE(seen, n, byte);
	C_MAKE(queue, n, int);

	seen[start] = 1;
	queue[tail++] = start;

	while (head < tail)
	{
		int cur = queue[head++], cy = cur / cur_wid, cx = cur % cur_wid, d;
		for (d = 0; d < 8; d++)
		{
			int ny = cy + dy8[d], nx = cx + dx8[d], nc;
			if ((ny < 0) || (ny >= cur_hgt) || (nx < 0) || (nx >= cur_wid)) continue;
			nc = ny * cur_wid + nx;
			if (seen[nc]) continue;
			if (!real_walkable_clear(ny, nx, NULL)) continue;   /* real floor, no monster */

			/* A walkable cell we have not yet uncovered: head there. */
			if (!explore_is_seen(ny, nx)) { *gy = ny; *gx = nx; found = TRUE; break; }

			seen[nc] = 1;
			queue[tail++] = nc;
		}
		if (found) break;
	}

	C_FREE(seen, n, byte);
	C_FREE(queue, n, int);
	return (found);
}

/* Step directly away from the threat at (ty,tx) over known floor with no monster
 * on it. Returns TRUE only if a step that increases the distance was taken. */
static bool autoplay_flee_from(int ty, int tx)
{
	int cur = distance(p_ptr->py, p_ptr->px, ty, tx);
	int best = 0, bestd = cur, d;

	for (d = 1; d <= 9; d++)
	{
		int ny, nx, dd;

		if (d == 5) continue;
		ny = p_ptr->py + ddy[d];
		nx = p_ptr->px + ddx[d];
		if (!in_bounds2(ny, nx)) continue;
		if (!cave_floor_bold(ny, nx)) continue;
		if (cave[ny][nx].m_idx) continue;

		dd = distance(ny, nx, ty, tx);
		if (dd > bestd) { bestd = dd; best = d; }
	}

	if (!best) return (FALSE);

	energy_use = 100;
	move_player_aux(best, TRUE, 1, TRUE);
	return (TRUE);
}

/* (Staircase selection lives in autoplay_pick_stair, below -- it commits to one
 * stair and filters by real A* reachability, replacing the old straight-line
 * nearest-stair finders that made the scummer oscillate between equidistant
 * stairs.) */

/* Descend the stairs under the player; auto-play continues on the new level. */
static void autoplay_descend(void)
{
	bool saved = confirm_stairs;

	confirm_stairs = FALSE;     /* don't block on "Really leave the level?" */
	msg_print("Autoplay: descending.");
	do_cmd_go_down();
	confirm_stairs = saved;
	autoplay_clear_stair();     /* new level: any locked stair / stuck cell is gone */
	autoplay_clear_stuck();
}

/* Ascend the stairs under the player. A no-way-down level gets escaped by going
 * up and coming back down -- the level regenerates (level scumming). */
static void autoplay_ascend(void)
{
	bool saved = confirm_stairs;

	confirm_stairs = FALSE;
	msg_print("Autoplay: taking the stairs up (regenerating the level).");
	do_cmd_go_up();
	confirm_stairs = saved;
	autoplay_clear_stair();     /* new level: any locked stair / stuck cell is gone */
	autoplay_clear_stuck();
}

/* Is the backpack full (no free general slot)? The pack is kept compact, so a
 * full pack means the last general slot is occupied. */
static bool autoplay_pack_full(void)
{
	return (p_ptr->inventory[INVEN_PACK - 1].k_idx != 0);
}

/* Is monster m too dangerous to melee right now (out-of-depth / much higher
 * native level than us)? When so, auto-play tries to flee/avoid rather than
 * trade blows -- a crude stand-in for the borg's per-monster danger model;
 * the exact policy is a good thing to push into Lua later. */
static bool autoplay_too_dangerous(monster_type *m)
{
	monster_race *r_ptr = &r_info[m->r_idx];
	int plev = p_ptr->lev;

	/* Let the Lua threat table decide first: 1 = avoid, 0 = fight, anything else
	 * (e.g. -1) = no opinion, use the C heuristic below. */
	if (autoplay_lua_ok())
	{
		s32b avoid = -1;
		if (call_lua("autoplay_avoid", "(M)", "d", m, &avoid))
		{
			if (avoid == 1) return (TRUE);
			if (avoid == 0) return (FALSE);
		}
	}

	/* A foe that can PARALYSE us while we lack Free Action: meleeing it is the
	 * classic way the bot dies (floating eyes -- you bump it, it gazes, you're
	 * paralysed, you die). Don't engage; route past it instead. */
	if (!p_ptr->free_act && autoplay_cfg("avoid_paralyze", 1))
	{
		int b;
		for (b = 0; b < 4; b++)
			if (r_ptr->blow[b].effect == RBE_PARALYZE) return (TRUE);
	}

	/* A unique noticeably above our level: don't pick the fight. */
	if ((r_ptr->flags1 & RF1_UNIQUE) && (r_ptr->level > plev + 3)) return (TRUE);

	/* Any monster whose native level roughly doubles ours (and is well above
	 * it in absolute terms): treat as out-of-depth and avoid. */
	if ((r_ptr->level >= plev * 2) && (r_ptr->level > plev + 5)) return (TRUE);

	/* Real threat: a single foe whose expected damage/turn could kill us in a
	 * couple of rounds -- don't trade melee with it, route around / shoot / flee. */
	if (autoplay_monster_danger(m) * autoplay_cfg("danger_turns", 2) >= p_ptr->chp)
		return (TRUE);

	return (FALSE);
}

/* ---- world navigation (Phase 1: recall-based town <-> dungeon travel) ----
 *
 * Recall is the shortcut that lets the bot move between town and a dungeon
 * without walking the wilderness: a town->dungeon recall drops us in
 * p_ptr->recall_dungeon at depth max_dlv[that]. We can aim it from code. */

static bool autoplay_in_town(void)
{
	return ((dun_level == 0) && !p_ptr->wild_mode && !p_ptr->inside_quest);
}

/* Backpack index of a Word of Recall scroll, or -1. */
static int autoplay_find_recall_scroll(void)
{
	int i;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) continue;
		if ((o_ptr->tval == TV_SCROLL) && (o_ptr->sval == SV_SCROLL_WORD_OF_RECALL))
			return (i);
	}
	return (-1);
}

/* Aim a town->dungeon recall at (dungeon, depth), clamped to the dungeon's range. */
static void autoplay_set_recall_target(int dungeon, int depth)
{
	if (dungeon <= 0) dungeon = DUNGEON_BARROW_DOWNS;
	if (depth < 1) depth = 1;
	if (depth > d_info[dungeon].maxdepth) depth = d_info[dungeon].maxdepth;

	p_ptr->recall_dungeon = dungeon;
	if (max_dlv[dungeon] < depth) max_dlv[dungeon] = depth;
}

/* ---- strategic route (Paso C): which dungeon to be in ----
 *
 * The plan lives in lib/scpt/autoplay.lua as autoplay_route (an ordered list of
 * {dungeon, plev, depth}). We walk it and take the first entry we haven't
 * finished (max_dlv[dungeon] < target depth) and are strong enough for
 * (p_ptr->lev >= plev). Entries we're too weak for are skipped now and retried
 * once we level up; finished ones are skipped for good. If Lua is absent or the
 * list is exhausted we fall back to the deepest PRINCIPAL dungeon we may enter
 * and haven't bottomed (Barrow -> Mirkwood -> Mordor -> Angband).
 *
 * Fills *dungeon and *depth (the depth to aim a town->dungeon recall at:
 * wherever we left off, clamped into [mindepth, target]). If `name` is non-NULL
 * it gets a human-readable label for the objective (Oracle / recall messages). */

/* Is the route's objective for dungeon dn (target depth tgt) finished? A dungeon
 * with a FINAL_GUARDIAN (the quest lairs) is "done" only when that guardian unique
 * is dead (r_info[g].max_num == 0) -- reaching the bottom isn't enough; you must
 * kill the boss. The principal ladder (no guardian) is done at the target depth. */
static bool autoplay_dungeon_done(int dn, int tgt)
{
	int g = d_info[dn].final_guardian;

	if ((g > 0) && (g < max_r_idx)) return (r_info[g].max_num == 0);
	return (max_dlv[dn] >= tgt);
}

/* Can we plausibly beat this dungeon's final guardian right now? Lets the route
 * SKIP a quest whose boss is still out of our league (we move on and come back
 * when stronger) instead of locking onto an unwinnable fight forever. TRUE if
 * there's no guardian, it's already dead, or it looks beatable. */
static bool autoplay_guardian_ok(int g)
{
	monster_race *r_ptr;

	if ((g <= 0) || (g >= max_r_idx)) return (TRUE);
	r_ptr = &r_info[g];
	if (r_ptr->max_num == 0) return (TRUE);          /* already slain */

	/* A paralyser we can't safely melee without Free Action: not yet. */
	if (!p_ptr->free_act && autoplay_cfg("avoid_paralyze", 1))
	{
		int b;
		for (b = 0; b < 4; b++)
			if (r_ptr->blow[b].effect == RBE_PARALYZE) return (FALSE);
	}

	/* Out of our depth -- wait until we're roughly its level. */
	if (r_ptr->level > p_ptr->lev + 5) return (FALSE);
	return (TRUE);
}

static void autoplay_objective(int *dungeon, int *depth, char *name)
{
	int i, best = -1, best_depth = 0, best_ridx = -1;

	if (autoplay_lua_ok())
	{
		for (i = 1; i <= 200; i++)   /* 1-based; guard against a runaway list */
		{
			s32b dn = -1, plev = 0, tgt = 0;
			if (!call_lua("autoplay_route_at", "(d)", "ddd", i, &dn, &plev, &tgt))
				break;
			if (dn < 0) break;                              /* end of the list */
			if ((dn <= 0) || (dn >= max_d_idx)) continue;   /* wilderness/bogus */
			if (autoplay_dungeon_done((int)dn, (int)tgt)) continue;  /* cleared / boss dead */
			if (p_ptr->lev < plev) continue;                /* too weak: retry later */
			if (!autoplay_guardian_ok(d_info[dn].final_guardian)) continue;  /* boss too tough yet */
			best = (int)dn; best_depth = (int)tgt; best_ridx = i;
			break;
		}
	}

	if (best < 0)
	{
		/* Fallback: deepest principal dungeon we may enter and haven't bottomed. */
		for (i = 0; i < max_d_idx; i++)
		{
			if (!(d_info[i].flags1 & DF1_PRINCIPAL)) continue;
			if (i == DUNGEON_WILDERNESS) continue;
			if (p_ptr->lev < d_info[i].min_plev) continue;
			if (autoplay_dungeon_done(i, d_info[i].maxdepth)) continue;
			if ((best < 0) || (d_info[i].mindepth > d_info[best].mindepth)) best = i;
		}
		if (best < 0) best = DUNGEON_BARROW_DOWNS;
		best_depth = d_info[best].maxdepth;
	}

	*dungeon = best;

	/* Aim the recall where we left off, but never above the target or below the
	 * dungeon's entrance. */
	{
		int d = max_dlv[best];
		if (d < d_info[best].mindepth) d = d_info[best].mindepth;
		if (d > best_depth) d = best_depth;
		if (d < 1) d = 1;
		*depth = d;
	}

	if (name)
	{
		cptr nm = NULL;
		if ((best_ridx > 0) && autoplay_lua_ok())
			(void)call_lua("autoplay_route_name", "(d)", "s", best_ridx, &nm);
		if (nm && nm[0]) strnfmt(name, 80, "%s", nm);
		else strnfmt(name, 80, "%s", d_name + d_info[best].name);
	}
}

/* Begin a recall: read a Word of Recall scroll if we have one, otherwise invoke
 * the recall directly (bot convenience -- the shopping phase will keep scrolls
 * stocked). Returns TRUE if a recall is now pending. The caller must ensure none
 * is already pending (recall_player toggles an active one off). */
static bool autoplay_start_recall(void)
{
	int s = autoplay_find_recall_scroll();

	energy_use = 100;
	recall_player(21, 15);

	if ((s >= 0) && (p_ptr->word_recall > 0))
	{
		inven_item_increase(s, -1);
		inven_item_describe(s);
		inven_item_optimize(s);
	}
	return (p_ptr->word_recall > 0);
}

/* ---- inventory upkeep: identify unknowns, equip the best gear ---- */

/* Backpack index of a Scroll of Identify, or -1. */
static int autoplay_find_identify_scroll(void)
{
	int i;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) continue;
		if ((o_ptr->tval == TV_SCROLL) && (o_ptr->sval == SV_SCROLL_IDENTIFY))
			return (i);
	}
	return (-1);
}

/* Worth spending a Scroll of Identify on? Only gear and magic devices -- not
 * potions/scrolls/food, which become known through use. */
static bool autoplay_id_worthy(object_type *o_ptr)
{
	if (object_known_p(o_ptr)) return (FALSE);
	if (wield_slot(o_ptr) >= INVEN_WIELD) return (TRUE);
	if ((o_ptr->tval == TV_WAND) || (o_ptr->tval == TV_STAFF) ||
	                (o_ptr->tval == TV_ROD)) return (TRUE);
	return (FALSE);
}

/* Read-only: if we have a Scroll of Identify and an unidentified piece of
 * gear/device, return that item's pack index (else -1). */
static int autoplay_find_unknown_id(void)
{
	int i;
	if (autoplay_find_identify_scroll() < 0) return (-1);
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) continue;
		if (autoplay_id_worthy(o_ptr)) return (i);
	}
	return (-1);
}

/* Identify pack item 'item' and spend one Scroll of Identify. One turn. */
static void autoplay_do_identify(int item)
{
	int sc = autoplay_find_identify_scroll();
	object_type *o_ptr = &p_ptr->inventory[item];

	/* Identify the item (flags only; reorder is deferred, so sc stays valid). */
	object_aware(o_ptr);
	object_known(o_ptr);
	o_ptr->ident |= (IDENT_MENTAL);

	if (sc >= 0)
	{
		inven_item_increase(sc, -1);
		inven_item_optimize(sc);
	}

	p_ptr->update |= (PU_BONUS);
	p_ptr->notice |= (PN_COMBINE | PN_REORDER);
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER);
	energy_use = 100;
}

/* Slots the bot will auto-equip into (armour + weapon/shield). Rings, amulets,
 * bow and ammo are left out for now -- their value/slot logic is situational. */
static bool autoplay_slot_autoequippable(int slot)
{
	return ((slot == INVEN_WIELD) || (slot == INVEN_BODY) ||
	        (slot == INVEN_OUTER) || (slot == INVEN_ARM) ||
	        (slot == INVEN_HEAD)  || (slot == INVEN_HANDS) ||
	        (slot == INVEN_FEET));
}

/* Equip the item in backpack slot 'item' into its wield slot; the gear it
 * replaces goes back to the pack. One turn. */
static void autoplay_wield(int item)
{
	object_type forge;
	int slot = wield_slot(&p_ptr->inventory[item]);

	if (slot < INVEN_WIELD) return;

	object_copy(&forge, &p_ptr->inventory[item]);
	forge.number = 1;
	inven_item_increase(item, -1);
	inven_item_optimize(item);

	if (p_ptr->inventory[slot].k_idx)
		(void)inven_takeoff(slot, 255, FALSE);

	object_copy(&p_ptr->inventory[slot], &forge);
	equip_cnt++;

	p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS | PU_TORCH);
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER);
	energy_use = 100;
}

/* Read-only: pack index of an item strictly more valuable than what occupies its
 * slot (a worthwhile upgrade), or -1. Only items we have some knowledge of (known
 * or sensed) and that aren't cursed, so we never blindly don an unknown cursed
 * item. (object_value is 0 for cursed, so they never win anyway.) */
static int autoplay_find_upgrade(void)
{
	int i;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		int slot;

		if (!o_ptr->k_idx) continue;
		if (!(object_known_p(o_ptr) || (o_ptr->ident & IDENT_SENSE))) continue;
		if (cursed_p(o_ptr)) continue;

		slot = wield_slot(o_ptr);
		if (!autoplay_slot_autoequippable(slot)) continue;

		if (object_value(o_ptr) > object_value(&p_ptr->inventory[slot])) return (i);
	}
	return (-1);
}

/* ---- shopping & resupply (Phase 2b): tour the town's shops, sell junk, buy
 * supplies, then recall back down. Targets below are tunable. ---- */

#define AP_WANT_WOR    3   /* Word of Recall scrolls to keep */
#define AP_WANT_CURE   5   /* cure-wounds potions to keep    */
#define AP_WANT_FOOD   5   /* staple food to keep            */
#define AP_WANT_ID     5   /* Scroll of Identify to keep     */
#define AP_WANT_OIL    3   /* flasks of oil (fuel and/or throwing) */
#define AP_WANT_TORCH  2   /* spare torches (if torch)       */
#define AP_WANT_AMMO  40   /* missiles to keep for the launcher    */

static bool autoplay_shopping = FALSE;     /* mid town-shopping trip */
static byte autoplay_shop_visited[32];     /* by store index, this trip */
static s32b autoplay_no_resupply_until = 0; /* don't go back to town before this turn */

/* Units of (tval[,sval]) in the backpack. sval < 0 matches any sval. */
static int autoplay_inv_count(int tval, int sval)
{
	int i, n = 0;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) continue;
		if (o_ptr->tval != tval) continue;
		if ((sval >= 0) && (o_ptr->sval != sval)) continue;
		n += o_ptr->number;
	}
	return (n);
}

static bool autoplay_is_cure(int sval)
{
	return ((sval == SV_POTION_CURE_LIGHT) || (sval == SV_POTION_CURE_SERIOUS) ||
	        (sval == SV_POTION_CURE_CRITICAL) || (sval == SV_POTION_HEALING) ||
	        (sval == SV_POTION_STAR_HEALING) || (sval == SV_POTION_LIFE));
}

static bool autoplay_is_staple(int sval)
{
	return ((sval == SV_FOOD_RATION) || (sval == SV_FOOD_BISCUIT) ||
	        (sval == SV_FOOD_JERKY) || (sval == SV_FOOD_WAYBREAD) ||
	        (sval == SV_FOOD_SLIME_MOLD));
}

static int autoplay_count_cure(void)
{
	int i, n = 0;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (o_ptr->k_idx && (o_ptr->tval == TV_POTION) && autoplay_is_cure(o_ptr->sval))
			n += o_ptr->number;
	}
	return (n);
}

static int autoplay_count_food(void)
{
	int i, n = 0;
	for (i = 0; i < INVEN_PACK; i++)
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (o_ptr->k_idx && (o_ptr->tval == TV_FOOD) && autoplay_is_staple(o_ptr->sval))
			n += o_ptr->number;
	}
	return (n);
}

/* Keep this pack item (never sell it)? Equippable gear (a possible upgrade or
 * worth identifying), our consumables, and lamp oil are kept; everything else is
 * fair game to sell (aggressive). */
static bool autoplay_keep_item(object_type *o_ptr)
{
	if (wield_slot(o_ptr) >= INVEN_WIELD) return (TRUE);

	switch (o_ptr->tval)
	{
	case TV_POTION: return (autoplay_is_cure(o_ptr->sval));
	case TV_SCROLL: return ((o_ptr->sval == SV_SCROLL_WORD_OF_RECALL) ||
		                        (o_ptr->sval == SV_SCROLL_IDENTIFY));
	case TV_FOOD:   return (autoplay_is_staple(o_ptr->sval));
	case TV_FLASK:  return (TRUE);
	}
	return (FALSE);
}

/* Sell every non-keep pack item this store will buy. */
static void autoplay_shop_sell_junk(int town, int store)
{
	int i, guard = 0;
	for (i = 0; i < INVEN_PACK; )
	{
		object_type *o_ptr = &p_ptr->inventory[i];
		if (!o_ptr->k_idx) { i++; continue; }
		if (autoplay_keep_item(o_ptr)) { i++; continue; }
		if (store_bot_sell(town, store, i) <= 0) { i++; continue; }
		/* sold one unit; stay on i to clear the rest of the stack */
		if (++guard > 500) break;
	}
}

/* Buy the shortfall of one supply type if this store stocks it. */
static void autoplay_buy_one(int town, int store, int tval, int sval, int target, int have)
{
	int need = target - have, idx;
	if (need <= 0) return;
	idx = store_bot_find(town, store, tval, sval);
	if (idx >= 0) (void)store_bot_buy(town, store, idx, need);
}

/* Buy the single best gear upgrade this store offers that we can afford while
 * keeping a gold reserve: scan the stock for a wieldable, non-cursed item worth
 * more (object_value) than what sits in its slot, pick the biggest gain, buy it.
 * Returns TRUE if it bought something (caller loops to grab several). The bought
 * piece lands in the pack; the post-shopping equip pass wields it. */
static bool autoplay_buy_best_gear(int town, int store)
{
	store_type *st = &town_info[town].store[store];
	int i, best = -1, reserve = autoplay_cfg("min_gold", 100);
	s32b best_gain = 0;
	char nm[80];

	for (i = 0; i < st->stock_num; i++)
	{
		object_type *o_ptr = &st->stock[i];
		int slot = wield_slot(o_ptr);
		s32b gain, price;

		if (!autoplay_slot_autoequippable(slot)) continue;   /* only gear we auto-wear */
		if (cursed_p(o_ptr)) continue;

		gain = object_value(o_ptr) - object_value(&p_ptr->inventory[slot]);
		if (gain <= 0) continue;                              /* not an upgrade */

		price = store_bot_price(town, store, i);
		if ((price <= 0) || (p_ptr->au - price < reserve)) continue;  /* can't afford + reserve */

		if (gain > best_gain) { best_gain = gain; best = i; }
	}

	if (best < 0) return (FALSE);

	object_desc(nm, &st->stock[best], TRUE, 1);
	if (store_bot_buy(town, store, best, 1) > 0)
	{
		msg_format("Autoplay: bought %s.", nm);
		return (TRUE);
	}
	return (FALSE);
}

static void autoplay_shop_buy_needs(int town, int store)
{
	object_type *lite = &p_ptr->inventory[INVEN_LITE];
	int want_cure = autoplay_cfg("want_cure", AP_WANT_CURE);
	int want_food = autoplay_cfg("want_food", AP_WANT_FOOD);

	autoplay_buy_one(town, store, TV_SCROLL, SV_SCROLL_WORD_OF_RECALL,
	                 autoplay_cfg("want_wor", AP_WANT_WOR),
	                 autoplay_inv_count(TV_SCROLL, SV_SCROLL_WORD_OF_RECALL));
	autoplay_buy_one(town, store, TV_SCROLL, SV_SCROLL_IDENTIFY,
	                 autoplay_cfg("want_id", AP_WANT_ID),
	                 autoplay_inv_count(TV_SCROLL, SV_SCROLL_IDENTIFY));

	/* Cure wounds: buy whatever kind the store stocks, toward the combined target. */
	autoplay_buy_one(town, store, TV_POTION, SV_POTION_CURE_LIGHT, want_cure, autoplay_count_cure());
	autoplay_buy_one(town, store, TV_POTION, SV_POTION_CURE_SERIOUS, want_cure, autoplay_count_cure());
	autoplay_buy_one(town, store, TV_POTION, SV_POTION_CURE_CRITICAL, want_cure, autoplay_count_cure());

	/* Food: any staple this store carries. */
	autoplay_buy_one(town, store, TV_FOOD, SV_FOOD_RATION, want_food, autoplay_count_food());
	autoplay_buy_one(town, store, TV_FOOD, SV_FOOD_BISCUIT, want_food, autoplay_count_food());
	autoplay_buy_one(town, store, TV_FOOD, SV_FOOD_JERKY, want_food, autoplay_count_food());
	autoplay_buy_one(town, store, TV_FOOD, SV_FOOD_WAYBREAD, want_food, autoplay_count_food());

	{
		bool lantern = (lite->k_idx && (lite->tval == TV_LITE) &&
		                (lite->sval == SV_LITE_LANTERN));

		/* Torch users keep spare torches for light. */
		if (!lantern)
			autoplay_buy_one(town, store, TV_LITE, SV_LITE_TORCH,
			                 autoplay_cfg("want_torch", AP_WANT_TORCH),
			                 autoplay_inv_count(TV_LITE, SV_LITE_TORCH));

		/* Flasks of oil: lamp fuel for lantern users, throwing ammo for everyone
		 * else (ranged v2 -- the bot now throws oil, so stock it on purpose). */
		autoplay_buy_one(town, store, TV_FLASK, -1, autoplay_cfg("want_oil", AP_WANT_OIL),
		                 autoplay_inv_count(TV_FLASK, -1));

		/* Ammo for our launcher, so the bot keeps firing instead of running dry. */
		if (autoplay_have_launcher())
			autoplay_buy_one(town, store, p_ptr->tval_ammo, -1,
			                 autoplay_cfg("want_ammo", AP_WANT_AMMO),
			                 autoplay_inv_count(p_ptr->tval_ammo, -1));
	}

	/* With consumables covered, spend surplus gold on the best gear upgrades this
	 * shop has (weapons/armour/etc.), keeping a reserve. Loop to grab several;
	 * the post-shopping equip pass then wields them. */
	{
		int guard = 0;
		while ((guard++ < 8) && autoplay_buy_best_gear(town, store)) ;
	}
}

/* Nearest town shop tile we haven't done this trip (skipping the Home). */
static bool autoplay_find_shop(int *sy, int *sx, int *sidx)
{
	int y, x, bestd = -1;
	for (y = 0; y < cur_hgt; y++)
	{
		for (x = 0; x < cur_wid; x++)
		{
			int idx, d;
			if (cave[y][x].feat != FEAT_SHOP) continue;
			idx = cave[y][x].special;
			if ((idx < 0) || (idx >= 32)) continue;
			if (idx == STORE_HOME) continue;
			if (autoplay_shop_visited[idx]) continue;
			d = distance(p_ptr->py, p_ptr->px, y, x);
			if ((bestd < 0) || (d < bestd)) { bestd = d; *sy = y; *sx = x; *sidx = idx; }
		}
	}
	return (bestd >= 0);
}

/* A walkable floor tile next to the shop entrance (we transact from there rather
 * than step onto the entrance, which would open the interactive store UI). */
static bool autoplay_shop_neighbor(int sy, int sx, int *ny, int *nx)
{
	static const int dy8[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
	static const int dx8[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
	int d, bestd = -1;
	for (d = 0; d < 8; d++)
	{
		int y = sy + dy8[d], x = sx + dx8[d], dd;
		if (!in_bounds2(y, x)) continue;
		if (!explore_walkable_clear(y, x, NULL)) continue;   /* free floor, no NPC */
		dd = distance(p_ptr->py, p_ptr->px, y, x);
		if ((bestd < 0) || (dd < bestd)) { bestd = dd; *ny = y; *nx = x; }
	}
	return (bestd >= 0);
}

/* One step of the town routine: walk to the next shop and trade there; when all
 * shops are done, recall back into the dungeon. Always spends the turn. */
static bool autoplay_town_step(void)
{
	int town = p_ptr->town_num;
	int sy = 0, sx = 0, sidx = -1, ny = 0, nx = 0, guard;

	if (!autoplay_shopping)
	{
		int k;
		autoplay_shopping = TRUE;
		for (k = 0; k < 32; k++) autoplay_shop_visited[k] = 0;
	}

	/* Recall down already armed: pass turns until we're yanked. */
	if (p_ptr->word_recall > 0) { energy_use = 100; return (TRUE); }

	for (guard = 0; guard < 40; guard++)
	{
		int dy, dx;
		if (!autoplay_find_shop(&sy, &sx, &sidx)) break;

		dy = sy - p_ptr->py; dx = sx - p_ptr->px;
		if ((ABS(dy) <= 1) && (ABS(dx) <= 1))
		{
			/* Standing next to the shop: trade. */
			store_bot_refresh(town, sidx);
			autoplay_shop_sell_junk(town, sidx);
			autoplay_shop_buy_needs(town, sidx);
			autoplay_shop_visited[sidx] = 1;
			energy_use = 100;
			msg_print("Autoplay: shopping.");
			return (TRUE);
		}

		if (autoplay_shop_neighbor(sy, sx, &ny, &nx) &&
		                autoplay_step_towards_hook(ny, nx, FALSE, explore_walkable_clear))
			return (TRUE);

		/* Can't reach it: skip and try the next shop. */
		autoplay_shop_visited[sidx] = 1;
	}

	/* All shops done -> gear up (it's safe here, no foes), then recall into the
	 * dungeon. Doing the identify/equip/light here means we descend already
	 * kitted out, instead of arriving with our weapon/armour/torch still in the
	 * pack (the in-dungeon upkeep only runs when no enemy is in sight, so a foe by
	 * the entrance could otherwise keep us unequipped). */
	{
		int it, guard = 0;
		while (((it = autoplay_find_unknown_id()) >= 0) && (guard++ < 40))
			autoplay_do_identify(it);
		guard = 0;
		while (((it = autoplay_find_upgrade()) >= 0) && (guard++ < 40))
			autoplay_wield(it);
		(void)autoplay_manage_light();
	}

	/* Hold off on the next town trip for a while so we actually dive rather than
	 * yo-yo to the shops (and don't thrash if we couldn't buy what we wanted). */
	autoplay_shopping = FALSE;
	autoplay_no_resupply_until = turn + autoplay_cfg("resupply_cooldown", 3000);

	/* Strategic plan (Paso C): recall to wherever the route says we should be,
	 * not just back to the last dungeon. */
	{
		int obj_dn = 0, obj_depth = 0;
		char obj_name[80];
		autoplay_objective(&obj_dn, &obj_depth, obj_name);
		autoplay_set_recall_target(obj_dn, obj_depth);

		if (autoplay_start_recall())
		{
			msg_format("Autoplay: done shopping, recalling to %s (L%d).",
			           obj_name, obj_depth);
			return (TRUE);
		}
	}

	autoplaying = 0;
	p_ptr->redraw |= (PR_STATE);
	msg_print("Autoplay: stuck in town.");
	return (TRUE);
}

/* In the dungeon, time to head back to town to restock? */
static bool autoplay_needs_resupply(void)
{
	/* Cooldown after a town trip: stay in the dungeon for a while. */
	if (turn < autoplay_no_resupply_until) return (FALSE);

	/* A full pack always warrants a trip: we go to sell (which also makes gold)
	 * and free space. */
	if (autoplay_pack_full()) return (TRUE);

	/* Running low on light with no spare/oil: go restock before we end up stuck
	 * in the dark. Checked before the gold gate -- torches are cheap and going
	 * dark is lethal, so we make the trip even near-broke. */
	{
		object_type *lite = &p_ptr->inventory[INVEN_LITE];
		if (autoplay_no_spare_light() && lite->k_idx && (lite->tval == TV_LITE) &&
		                ((lite->sval == SV_LITE_TORCH) || (lite->sval == SV_LITE_LANTERN)) &&
		                (lite->timeout < 500))
			return (TRUE);
	}

	/* Buying needs gold; a near-broke character should keep diving and earning
	 * rather than trudge to town for supplies it can't afford. */
	if (p_ptr->au < autoplay_cfg("min_gold", 100)) return (FALSE);

	/* NB: we do NOT trip on missing Word of Recall -- the bot recalls for free
	 * (autoplay_start_recall falls back to recall_player), so it never needs the
	 * scrolls. Triggering on WoR sent it town<->dungeon forever, never playing. */
	if (autoplay_count_cure() < 1) return (TRUE);
	if (autoplay_count_food() < 1) return (TRUE);
	return (FALSE);
}

/* ---- decision / execution split ----
 *
 * autoplay_decide() runs the priority ladder and fills an action descriptor
 * WITHOUT executing it (and with a human-readable English `advice`).
 * autoplay_perform() executes a descriptor. The bot does perform(decide()); the
 * Oracle just shows decide()->advice; and the Lua brain will later plug in at
 * decide(). decide() is side-effect-light (only benign explore bookkeeping), so
 * it is safe to call on demand for advice. */

#define AP_NONE     0
#define AP_QUAFF    1   /* drink heal potion (item)            */
#define AP_FLEE     2   /* step away from threat at (y,x)       */
#define AP_FIGHT    3   /* approach / melee enemy at (y,x)      */
#define AP_TOWN     4   /* town shopping step                  */
#define AP_EAT      5   /* eat food (item)                     */
#define AP_LIGHT    6   /* tend the light source               */
#define AP_IDENTIFY 7   /* identify unknown gear (item)        */
#define AP_EQUIP    8   /* wear a better item (item)           */
#define AP_RECALL   9   /* recall to town for resupply         */
#define AP_REST     10  /* rest to recover                     */
#define AP_EXPLORE  11  /* step toward exploration goal (y,x)  */
#define AP_DESCEND  12  /* take the down stair underfoot       */
#define AP_GOSTAIR  13  /* head to the down stair at (y,x)     */
#define AP_DELVE    14  /* push into unseen real floor (y,x)   */
#define AP_ASCEND   15  /* take an up stair (scum: regenerate) */
#define AP_WAIT     16  /* pass a turn (e.g. waiting on recall) */
#define AP_CURE     17  /* quaff a potion to lift a status (item)   */
#define AP_ESCAPE   18  /* read phase/teleport to break away (item) */
#define AP_SEARCH   19  /* search for secret doors (at y,x)         */
#define AP_PUSHPAST 20  /* step toward (y,x) swapping past a friend */
#define AP_SHOOT    21  /* fire launcher ammo (item) at foe (y,x)   */
#define AP_THROW    22  /* throw an item (item) at foe (y,x)        */
#define AP_DEVICE   23  /* use staff/rod/scroll (item; kind in .y)  */
#define AP_DETECT   24  /* use a detection device (item; kind .y)   */

typedef struct autoplay_action autoplay_action;
struct autoplay_action
{
	int type;
	int item;          /* pack index for quaff/eat/identify/equip */
	int y, x;          /* target cell                             */
	bool pickup;       /* grab loot while stepping?               */
	char advice[80];   /* English, human-readable                 */
};

/* Read-only: is (gy,gx) reachable over terrain accepted by 'hook'? (A* probe.) */
static bool autoplay_can_reach_hook(int gy, int gx, astar_walkable_hook hook)
{
	path_result *r = astar_find_path_cb(cur_hgt, cur_wid, hook,
	                                    NULL, p_ptr->py, p_ptr->px, gy, gx, ASTAR_8DIR_CUT);
	bool ok = (r && (r->length >= 2));
	if (r) path_free(r);
	return (ok);
}

/* Combat reach: ignore monsters (we want to reach the foe's own tile). */
static bool autoplay_can_reach(int gy, int gx)
{
	return (autoplay_can_reach_hook(gy, gx, explore_walkable_hook));
}

/* Pick a staircase of the requested kind to head for, committing to it across
 * turns so we don't oscillate between equidistant stairs (see ap_stair_*). If a
 * still-valid, reachable stair of this kind is already locked we keep it;
 * otherwise we lock the nearest *reachable* one. Reachability is checked with
 * A* (not straight-line), so we never commit to a stair we can't actually walk
 * to. Returns TRUE and fills (*sy,*sx). `down` = down stairs, else up. */
static bool autoplay_pick_stair(bool down, int *sy, int *sx)
{
	int y, x, bestd = -1;

	/* Keep the locked stair if it's still a reachable stair of the right kind. */
	if ((ap_stair_y >= 0) && (ap_stair_x >= 0))
	{
		int f = cave[ap_stair_y][ap_stair_x].feat;
		bool ok = down ? ((f == FEAT_MORE) || (f == FEAT_WAY_MORE))
		               : ((f == FEAT_LESS) || (f == FEAT_WAY_LESS));
		if (ok && explore_is_seen(ap_stair_y, ap_stair_x) &&
		                autoplay_can_reach_hook(ap_stair_y, ap_stair_x, explore_walkable_clear))
		{
			*sy = ap_stair_y; *sx = ap_stair_x;
			return (TRUE);
		}
		autoplay_clear_stair();   /* stale (gone / unseen / unreachable): re-pick */
	}

	/* Lock the nearest reachable stair of this kind. */
	for (y = 0; y < cur_hgt; y++)
	{
		for (x = 0; x < cur_wid; x++)
		{
			int f = cave[y][x].feat, d;
			bool match = down ? ((f == FEAT_MORE) || (f == FEAT_WAY_MORE))
			                  : ((f == FEAT_LESS) || (f == FEAT_WAY_LESS));
			if (!match) continue;
			if (!explore_is_seen(y, x)) continue;
			d = distance(p_ptr->py, p_ptr->px, y, x);
			if ((bestd >= 0) && (d >= bestd)) continue;       /* not closer */
			if (!autoplay_can_reach_hook(y, x, explore_walkable_clear)) continue;
			bestd = d; *sy = y; *sx = x;
		}
	}
	if (bestd < 0) return (FALSE);

	ap_stair_y = *sy; ap_stair_x = *sx;
	return (TRUE);
}

/* A2: nearest reachable explored floor cell that is adjacent to a seen wall
 * (where a secret door could be hiding) and that we have not searched from yet.
 * Used to sweep a dead-end level for secret doors before giving up on it. Walls
 * are feat >= FEAT_SECRET (secret doors display as plain granite, so we can't
 * spot them -- we have to stand next to the wall and search). TRUE + (*sy,*sx). */
static bool autoplay_find_search_spot(int *sy, int *sx)
{
	static const int dy8[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
	static const int dx8[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
	int y, x, bestd = -1;

	if (!ap_searched) return (FALSE);

	for (y = 0; y < cur_hgt; y++)
	{
		for (x = 0; x < cur_wid; x++)
		{
			int cell = y * cur_wid + x, k, d;
			bool by_wall = FALSE;

			if (ap_searched[cell]) continue;
			if (!explore_is_seen(y, x)) continue;
			if (!cave_floor_bold(y, x)) continue;

			for (k = 0; k < 8; k++)
			{
				int ny = y + dy8[k], nx = x + dx8[k];
				if (!in_bounds2(ny, nx)) continue;
				if (explore_is_seen(ny, nx) && (cave[ny][nx].feat >= FEAT_SECRET))
				{ by_wall = TRUE; break; }
			}
			if (!by_wall) continue;

			d = distance(p_ptr->py, p_ptr->px, y, x);
			if ((bestd >= 0) && (d >= bestd)) continue;
			if (!((y == p_ptr->py) && (x == p_ptr->px)) &&
			                !autoplay_can_reach_hook(y, x, explore_walkable_clear)) continue;
			bestd = d; *sy = y; *sx = x;
		}
	}
	return (bestd >= 0);
}

/* Read-only: is a fleeing step (away from (ty,tx), onto empty known floor)
 * available right now? */
static bool autoplay_can_flee(int ty, int tx)
{
	int cur = distance(p_ptr->py, p_ptr->px, ty, tx), d;
	for (d = 1; d <= 9; d++)
	{
		int ny, nx;
		if (d == 5) continue;
		ny = p_ptr->py + ddy[d];
		nx = p_ptr->px + ddx[d];
		if (!in_bounds2(ny, nx)) continue;
		if (!cave_floor_bold(ny, nx)) continue;
		if (cave[ny][nx].m_idx) continue;
		if (distance(ny, nx, ty, tx) > cur) return (TRUE);
	}
	return (FALSE);
}

static void autoplay_decide(autoplay_action *a)
{
	int chp = p_ptr->chp, mhp = p_ptr->mhp, ed = 0;
	monster_type *enemy;
	bool full = autoplay_pack_full();
	int it;

	a->type = AP_NONE;
	a->item = -1;
	a->y = a->x = 0;
	a->pickup = !full;
	strcpy(a->advice, "Nothing to do.");

	/* Hallucination (can't trust the display) and wild_mode (overworld travel)
	 * we genuinely can't auto-play through; confusion is handled below (we can
	 * still quaff a cure, or wait it out, rather than abandoning the run). */
	if (p_ptr->image || p_ptr->wild_mode)
	{
		strcpy(a->advice, "Wait -- you can't act sensibly right now.");
		return;
	}

	/* Make our view of the level current before reading it. On the very first
	 * turn of a freshly entered level the view/lite isn't applied yet, so nothing
	 * is "seen" and exploration had nothing to work from -- the bot stalled until
	 * the player nudged it. Forcing the pending update fixes that. */
	if (p_ptr->update) update_stuff();

	explore_sync_seen();

	/* 1. Heal when hurt and a potion is at hand (cheap cure first; big heal if low). */
	if (chp * 2 <= mhp)
	{
		it = autoplay_find_heal(chp * 4 <= mhp);
		if (it >= 0)
		{
			char nm[80];
			object_desc(nm, &p_ptr->inventory[it], TRUE, 3);
			a->type = AP_QUAFF; a->item = it;
			strnfmt(a->advice, 80, "Quaff %s to heal.", nm);
			return;
		}
	}

	/* 1b. Lift a crippling status (confusion / blindness / meaningful poison). A
	 * potion can be quaffed even while blind or confused, so this self-clears the
	 * two states that would otherwise leave the bot unable to see or move. */
	{
		cptr what = NULL;
		it = autoplay_find_cure(&what);
		if (it >= 0)
		{
			a->type = AP_CURE; a->item = it;
			strnfmt(a->advice, 80, "Quaff a potion to cure %s.", what);
			return;
		}
	}

	/* 1c. Still confused and nothing to cure it with: wait it out. Confused
	 * movement is randomised, so stumbling around would just walk us into walls
	 * or hazards -- passing turns until it lifts is safer (and we already healed
	 * above if we were hurt). */
	if (p_ptr->confused)
	{
		a->type = AP_WAIT;
		strcpy(a->advice, "Wait out the confusion.");
		return;
	}

	/* 1c2. Overwhelmed by a PACK: if the combined danger/turn of the nearby foes
	 * is lethal and there is more than one, break away -- blink/teleport if we
	 * can, else flee on foot -- before they surround and grind us down (the
	 * jackal / spider-pack killer). If we can't get away, fall through and
	 * fight/shoot what we can. */
	if (dun_level > 0)
	{
		int foes = 0;
		monster_type *near = NULL;
		int cd = autoplay_cluster_danger(&foes, &near);

		if ((foes >= 2) && near &&
		                (cd * autoplay_cfg("pack_flee_turns", 4) >= p_ptr->chp))
		{
			if (!p_ptr->blind && !p_ptr->confused)
			{
				int s = autoplay_find_escape();
				if (s >= 0)
				{
					a->type = AP_ESCAPE; a->item = s;
					strnfmt(a->advice, 80, "A pack of %d closing in -- escape!", foes);
					return;
				}
			}
			if (autoplay_can_flee(near->fy, near->fx))
			{
				a->type = AP_FLEE; a->y = near->fy; a->x = near->fx;
				strnfmt(a->advice, 80, "A pack of %d -- flee!", foes);
				return;
			}
			/* Cornered: nothing better -- fall through to ranged / melee. */
		}
	}

	/* 1c3. (B1) Detect once per level: if we carry a detection staff/rod/scroll
	 * and nothing is adjacent, use it so the threat/trap picture is current before
	 * we wander into it. */
	if ((dun_level > 0) && !ap_detected && !autoplay_adjacent_enemy())
	{
		int kind = 0, it2 = autoplay_find_detection(&kind);
		if (it2 >= 0)
		{
			a->type = AP_DETECT; a->item = it2; a->y = kind;
			strcpy(a->advice, "Use a detection device.");
			return;
		}
	}

	/* 1d. Ranged attack ("Random bullshit go!"): in the dungeon, with nothing
	 * adjacent (melee threats are dealt with below) and a clear line of fire,
	 * shoot/throw at the nearest foe -- crucially INCLUDING paralysers and other
	 * foes too dangerous to melee, which we can safely pick off from range. */
	if ((dun_level > 0) && !p_ptr->blind && !autoplay_adjacent_enemy())
	{
		monster_type *rt = autoplay_nearest_ranged();
		if (rt)
		{
			char nm[80];
			monster_desc(nm, rt, 0);

			if (autoplay_have_launcher())
			{
				int am = autoplay_find_ammo();
				if (am != -1)
				{
					a->type = AP_SHOOT; a->item = am; a->y = rt->fy; a->x = rt->fx;
					strnfmt(a->advice, 80, "Shoot %s.", nm);
					return;
				}
			}
			{
				int th = autoplay_find_throwable();
				if (th >= 0)
				{
					a->type = AP_THROW; a->item = th; a->y = rt->fy; a->x = rt->fx;
					strnfmt(a->advice, 80, "Throw something at %s.", nm);
					return;
				}
			}
		}
	}

	/* 2. A visible enemy: flee the dangerous/desperate cases, else close and melee.
	 * Only commit to flee/fight when the move is actually possible, so an
	 * unreachable foe behind a wall doesn't stall us -- we explore instead. */
	enemy = autoplay_nearest_enemy(&ed);

	/* On the surface (town/world) ignore any foe that isn't right next to us: out
	 * there hostiles wander in and out of view and chasing them just stalls the
	 * shopping/travel. We still defend against something adjacent. */
	if (enemy && (dun_level == 0) &&
	                ((ABS(enemy->fy - p_ptr->py) > 1) || (ABS(enemy->fx - p_ptr->px) > 1)))
		enemy = NULL;

	if (enemy)
	{
		/* nearest_enemy already filtered out foes too dangerous to melee, so a
		 * returned enemy is one we're willing to fight. We still bail out (flee)
		 * if we're critically low and out of cures. */
		bool desperate = (chp * 4 <= mhp) && (autoplay_find_heal(TRUE) < 0);
		char nm[80];
		monster_desc(nm, enemy, 0);
		a->y = enemy->fy; a->x = enemy->fx;

		/* (B2) Haste before a genuinely dangerous fight (foe that hits for a big
		 * chunk per turn), if we have a Speed potion and aren't already fast. */
		if (!p_ptr->fast && (autoplay_monster_danger(enemy) * 3 >= chp))
		{
			int sp = autoplay_find_speed();
			if (sp >= 0)
			{
				a->type = AP_QUAFF; a->item = sp;
				strnfmt(a->advice, 80, "Quaff Speed to fight %s.", nm);
				return;
			}
		}

		if (desperate && autoplay_can_flee(enemy->fy, enemy->fx))
		{
			a->type = AP_FLEE;
			strnfmt(a->advice, 80, "Flee from %s.", nm);
			return;
		}

		/* Desperate and cornered (can't step away): blink / teleport out. Reading
		 * needs eyes, so only when not blind/confused (which got a cure shot at
		 * step 1b). Phase Door is preferred over Teleport in autoplay_find_escape. */
		if (desperate && !p_ptr->blind && !p_ptr->confused)
		{
			int s = autoplay_find_escape();
			if (s >= 0)
			{
				a->type = AP_ESCAPE; a->item = s;
				strnfmt(a->advice, 80, "Read a scroll to escape %s.", nm);
				return;
			}
		}

		/* Afraid: fear blocks melee. Drink courage so we can actually fight back;
		 * failing that, keep our distance rather than bumping the foe uselessly. */
		if (p_ptr->afraid)
		{
			int b = autoplay_find_boldness();
			if (b >= 0)
			{
				a->type = AP_CURE; a->item = b;
				strnfmt(a->advice, 80, "Quaff courage to face %s.", nm);
				return;
			}
			if (autoplay_can_flee(enemy->fy, enemy->fx))
			{
				a->type = AP_FLEE;
				strnfmt(a->advice, 80, "Flee from %s (too afraid to fight).", nm);
				return;
			}
			/* Can't cure, can't flee: fall through and try to fight regardless. */
		}

		if (autoplay_can_reach(enemy->fy, enemy->fx))
		{
			int eidx = (int)(enemy - m_list);

			/* Count consecutive turns spent on this target. A foe we can actually
			 * kill goes down in a handful of melee turns; if it is still alive
			 * after many turns of our attention it is something we can't bring
			 * down -- it flees / keeps its distance (bait-and-switch), or it's an
			 * NPC we shouldn't be hitting at all (e.g. Farmer Maggot). Give up on
			 * it (ignore until it dies or leaves) and get on with exploring. */
			if (eidx == ap_chase_idx) ap_chase_turns++;
			else { ap_chase_idx = eidx; ap_chase_turns = 1; }

			if (ap_chase_turns <= autoplay_cfg("chase_turns", 15))
			{
				a->type = AP_FIGHT;
				strnfmt(a->advice, 80, "Fight %s.", nm);
				return;
			}

			/* Can't bring it down: give it up and carry on (explore). */
			ap_ignore_idx = eidx;
			ap_chase_idx = 0;
			ap_chase_turns = 0;
			enemy = NULL;
		}
		/* Unreachable / given up: ignore it and carry on. */
	}

	/* 3. In town: shop, then recall back down. */
	if (autoplay_in_town())
	{
		a->type = AP_TOWN;
		strcpy(a->advice, "Shop in town for supplies, then recall down.");
		return;
	}

	/* 4. Eat when hungry (any quiet moment; sooner if about to faint). */
	if ((p_ptr->food < PY_FOOD_ALERT) && (!enemy || (p_ptr->food < PY_FOOD_FAINT)))
	{
		it = autoplay_find_food();
		if (it >= 0)
		{
			char nm[80];
			object_desc(nm, &p_ptr->inventory[it], TRUE, 3);
			a->type = AP_EAT; a->item = it;
			strnfmt(a->advice, 80, "Eat %s -- you are hungry.", nm);
			return;
		}
	}

	/* The rest only happen with no foe pressing. */
	if (!enemy)
	{
		/* 5. Tend the light source (swap in a fresh torch / refuel the lamp). */
		if (autoplay_light_needs())
		{
			a->type = AP_LIGHT;
			strcpy(a->advice, "Tend your light source.");
			return;
		}

		/* 5b. In the dark with nothing to relight with: bail to town to restock.
		 * Diving blind is suicide, and with nothing lit the frontier search picks
		 * no goal -- so this MUST come before exploration or the bot just stalls
		 * in the dark (which is exactly what it did when a torch burnt out with no
		 * spare). */
		if ((dun_level > 0) && autoplay_light_dark())
		{
			if (p_ptr->word_recall > 0)
			{
				a->type = AP_WAIT;
				strcpy(a->advice, "Out of light -- waiting for recall to town.");
				return;
			}
			a->type = AP_RECALL;
			strcpy(a->advice, "Out of light -- recall to town to restock torches.");
			return;
		}

		/* 6. Identify unknown gear with a scroll. */
		it = autoplay_find_unknown_id();
		if (it >= 0)
		{
			char nm[80];
			object_desc(nm, &p_ptr->inventory[it], TRUE, 3);
			a->type = AP_IDENTIFY; a->item = it;
			strnfmt(a->advice, 80, "Identify %s.", nm);
			return;
		}

		/* 7. Wear a better item. */
		it = autoplay_find_upgrade();
		if (it >= 0)
		{
			char nm[80];
			object_desc(nm, &p_ptr->inventory[it], TRUE, 3);
			a->type = AP_EQUIP; a->item = it;
			strnfmt(a->advice, 80, "Equip %s.", nm);
			return;
		}

		/* 7b. (B3) Restore a drained stat if we carry the matching potion. */
		{
			int st = 0, ri = autoplay_find_restore(&st);
			if (ri >= 0)
			{
				a->type = AP_QUAFF; a->item = ri;
				strcpy(a->advice, "Quaff a potion to restore a drained stat.");
				return;
			}
		}

		/* 7c. (B3) Uncurse a stuck cursed item if we have a Remove Curse scroll. */
		if (autoplay_wearing_cursed())
		{
			int rc = autoplay_find_remove_curse();
			if (rc >= 0)
			{
				a->type = AP_DEVICE; a->item = rc; a->y = APDEV_SCROLL;
				strcpy(a->advice, "Read Remove Curse.");
				return;
			}
		}

		/* 8. Out of supplies: recall back to town (no progress lost). */
		if ((dun_level > 0) && (p_ptr->word_recall == 0) && autoplay_needs_resupply())
		{
			a->type = AP_RECALL;
			strcpy(a->advice, "Low on supplies -- recall to town.");
			return;
		}

		/* 9. Rest to recover when wounded and not starving. */
		if ((chp < mhp) && (p_ptr->food >= PY_FOOD_ALERT))
		{
			a->type = AP_REST;
			strcpy(a->advice, "Rest to recover.");
			return;
		}
	}

	/* 10. Explore the seen frontier; if that is exhausted, delve toward the
	 * nearest unseen real-floor cell (pushing into the dark like click-to-move);
	 * if even that finds nothing, head to / take a down staircase. Whatever's
	 * left over is reported with diagnostics so the Oracle can explain a stall. */
	{
		int gy = 0, gx = 0, sy = 0, sx = 0;
		int here = cave[p_ptr->py][p_ptr->px].feat;
		bool got, reach = FALSE, blind, gotstair, reachstair = FALSE;

		explore_no_items = full;
		got = explore_pick_goal(&gy, &gx);
		explore_no_items = FALSE;
		if (got) reach = autoplay_can_reach_hook(gy, gx, explore_walkable_clear);
		if (got && reach)
		{
			a->type = AP_EXPLORE; a->y = gy; a->x = gx;
			strcpy(a->advice, "Keep exploring.");
			return;
		}

		/* (A3) Blocked from the goal only because a FRIENDLY creature sits in a
		 * 1-wide corridor with no detour? Push past it (swap). We still avoid
		 * hostiles -- explore_walkable_friend only opens friendly-held cells. */
		if (got && !reach && autoplay_can_reach_hook(gy, gx, explore_walkable_friend))
		{
			a->type = AP_PUSHPAST; a->y = gy; a->x = gx;
			strcpy(a->advice, "Push past a friendly creature blocking the way.");
			return;
		}

		blind = autoplay_blind_goal(&gy, &gx);
		if (blind)
		{
			a->type = AP_DELVE; a->y = gy; a->x = gx;
			strcpy(a->advice, "Delve toward the unexplored.");
			return;
		}

		if ((here == FEAT_MORE) || (here == FEAT_WAY_MORE))
		{
			a->type = AP_DESCEND;
			strcpy(a->advice, "Descend the staircase.");
			return;
		}
		gotstair = autoplay_pick_stair(TRUE, &sy, &sx);   /* nearest reachable, locked */
		reachstair = gotstair;                            /* pick_stair already filters */
		if (gotstair)
		{
			a->type = AP_GOSTAIR; a->y = sy; a->x = sx;
			strcpy(a->advice, "Head to the down staircase.");
			return;
		}

		/* 11b. Route says move on? If this level is exhausted and the strategic
		 * objective (Paso C) is a *different* dungeon -- this one is cleared, or
		 * we're off-plan -- recall to town and switch, rather than scumming this
		 * dungeon forever. (If we're still meant to be here, fall through and
		 * scum to regenerate a level with a way down.) */
		if (dun_level > 0)
		{
			int obj_dn = 0, obj_depth = 0;
			char obj_name[80];
			autoplay_objective(&obj_dn, &obj_depth, obj_name);
			if (obj_dn != dungeon_type)
			{
				if (p_ptr->word_recall > 0)
				{
					a->type = AP_WAIT;
					strnfmt(a->advice, 80, "Cleared -- waiting for recall (next: %s).", obj_name);
					return;
				}
				a->type = AP_RECALL;
				strnfmt(a->advice, 80, "Cleared this dungeon -- recall to town (next: %s).", obj_name);
				return;
			}
		}

		/* 11c. (A2) Before scumming, sweep for secret doors: a room sealed by a
		 * secret door looks like a dead end (no frontier, no stairs) but isn't.
		 * Walk to explored floor cells that border a wall and search there; a
		 * revealed door becomes a frontier and normal exploration resumes. Bounded
		 * (ap_searched marks each spot once, AP_SEARCH_MAX caps the total). */
		if (ap_search_done < AP_SEARCH_MAX)
		{
			int spy = 0, spx = 0;
			if (autoplay_find_search_spot(&spy, &spx))
			{
				if ((spy == p_ptr->py) && (spx == p_ptr->px))
				{
					a->type = AP_SEARCH; a->y = spy; a->x = spx;
					strcpy(a->advice, "Search for secret doors.");
				}
				else
				{
					a->type = AP_GOSTAIR; a->y = spy; a->x = spx;
					strcpy(a->advice, "Move to a wall to search for secret doors.");
				}
				return;
			}
		}

		/* 12. Dead end: nothing left to explore and no way down. Scum the level --
		 * take an up staircase and come back down so it regenerates; failing that
		 * (no stairs at all), recall to town and re-dive a fresh level. */
		if ((here == FEAT_LESS) || (here == FEAT_WAY_LESS))
		{
			a->type = AP_ASCEND;
			strcpy(a->advice, "Dead end: take the stairs up to regenerate the level.");
			return;
		}
		if (autoplay_pick_stair(FALSE, &sy, &sx))   /* nearest reachable up stair, locked */
		{
			a->type = AP_GOSTAIR; a->y = sy; a->x = sx;
			strcpy(a->advice, "Dead end: head to a staircase to regenerate the level.");
			return;
		}
		if (dun_level > 0)
		{
			if (p_ptr->word_recall > 0)
			{
				a->type = AP_WAIT;
				strcpy(a->advice, "Dead end: waiting for recall.");
				return;
			}
			a->type = AP_RECALL;
			strcpy(a->advice, "Dead end: no stairs -- recalling to town.");
			return;
		}

		a->type = AP_NONE;
		strnfmt(a->advice, 80,
		        "Nothing to do (goal=%d reach=%d stair=%d/%d dl=%d seen=%ld).",
		        (int)got, (int)reach, (int)gotstair, (int)reachstair,
		        (int)dun_level, (long)explore_seen_count);
	}
}

static void autoplay_perform(autoplay_action *a)
{
	switch (a->type)
	{
	case AP_QUAFF:    autoplay_quaff(a->item); break;
	case AP_EAT:      eat_food(a->item); break;
	case AP_IDENTIFY: autoplay_do_identify(a->item); break;
	case AP_EQUIP:    autoplay_wield(a->item); break;
	case AP_LIGHT:    (void)autoplay_manage_light(); break;
	case AP_CURE:     autoplay_quaff(a->item); break;
	case AP_ESCAPE:   autoplay_escape(a->item); break;
	case AP_PUSHPAST: (void)autoplay_step_towards_hook(a->y, a->x, a->pickup, explore_walkable_friend); break;
	case AP_SHOOT:    { int mi = cave[a->y][a->x].m_idx; if (mi) autoplay_shoot(a->item, &m_list[mi]); break; }
	case AP_THROW:    { int mi = cave[a->y][a->x].m_idx; if (mi) autoplay_throw_at(a->item, &m_list[mi]); break; }
	case AP_DEVICE:   autoplay_use_device(a->item, a->y); break;
	case AP_DETECT:   autoplay_use_device(a->item, a->y); ap_detected = TRUE; break;
	case AP_SEARCH:
		energy_use = 100;
		search();                                  /* reveals adjacent secret doors/traps */
		if (ap_searched) ap_searched[p_ptr->py * cur_wid + p_ptr->px] = 1;
		ap_search_done++;
		break;
	case AP_FLEE:     (void)autoplay_flee_from(a->y, a->x); break;
	case AP_FIGHT:    (void)autoplay_step_towards(a->y, a->x, a->pickup); break;
	case AP_EXPLORE:  (void)autoplay_step_towards_hook(a->y, a->x, a->pickup, explore_walkable_clear); break;
	case AP_DELVE:    (void)autoplay_step_towards_hook(a->y, a->x, a->pickup, real_walkable_clear); break;
	case AP_GOSTAIR:  (void)autoplay_step_towards_hook(a->y, a->x, a->pickup, explore_walkable_clear); break;
	case AP_DESCEND:  autoplay_descend(); break;
	case AP_ASCEND:   autoplay_ascend(); break;
	case AP_TOWN:     (void)autoplay_town_step(); break;
	case AP_RECALL:   (void)autoplay_start_recall(); break;
	case AP_REST:     resting = -1; p_ptr->redraw |= (PR_STATE); break;
	case AP_WAIT:     energy_use = 100; break;   /* pass a turn (recall pending) */
	default:          break;
	}
}

/*
 * One auto-play decision. Called from process_player()'s energy loop: decide the
 * single best action and perform it (or stop when there is nothing left to do).
 */
void autoplay_step(void)
{
	autoplay_action a;

	/* Hallucination (display can't be trusted) and wild_mode (overworld) we can't
	 * auto-play through. Confusion is NOT a stop any more: autoplay_decide cures
	 * it (a potion works while confused) or waits it out. */
	if (p_ptr->image || p_ptr->wild_mode)
	{
		autoplaying = 0;
		p_ptr->redraw |= (PR_STATE);
		msg_print("Autoplay stopped.");
		return;
	}

	autoplay_decide(&a);

	if (a.type == AP_NONE)
	{
		autoplaying = 0;
		p_ptr->redraw |= (PR_STATE);
		msg_print("Autoplay: nothing left to do.");
		return;
	}

	/* Every autoplay action must spend a turn. If one doesn't (a step that found
	 * no route, a tend-light that couldn't act, ...), process_player's
	 * `while (p_ptr->energy >= 100)` loop re-runs the SAME decision forever with
	 * the same state -- the bot looks frozen ("se queda parado"). Detect a
	 * no-energy action and stop cleanly, naming the culprit so the cause is
	 * visible (this is how the "torch out -> stuck" freeze surfaces). */
	energy_use = 0;
	autoplay_perform(&a);

	/* Some actions legitimately don't leave energy spent yet still make progress:
	 *   - AP_REST hands off to the resting subsystem (resting = -1), dispatched
	 *     ahead of autoplay, which burns the turns.
	 *   - Taking stairs (AP_ASCEND/AP_DESCEND) sets p_ptr->leaving and regenerates
	 *     the level; do_cmd_go_up even zeroes energy_use on purpose (so monsters
	 *     don't get to act first). That's a level change, not a stall.
	 * Any other action must consume a turn; if one didn't it's a genuine no-op (a
	 * step that found no route, a missing perform case, ...) -- stop cleanly so
	 * process_player's `while (energy >= 100)` loop can't spin on it forever. */
	if (autoplaying && (energy_use == 0) && !p_ptr->leaving && (a.type != AP_REST))
	{
		autoplaying = 0;
		p_ptr->redraw |= (PR_STATE);
		msg_format("Autoplay stalled -- no turn taken for: %s", a.advice);
	}
}

/*
 * The Oracle: say what auto-play WOULD do next, without doing it. Bound to a key
 * (Ctrl-N) and the GTK2 Action menu; works whether or not auto-play is running.
 */
void do_cmd_oracle(void)
{
	autoplay_action a;
	autoplay_decide(&a);
	msg_format("Oracle: %s", a.advice);
}

/*
 * Start auto-playing. Bound to a command key (Ctrl-V) and the GTK2 Action menu.
 */
void do_cmd_autoplay(void)
{
	if (p_ptr->immovable) return;

	if (p_ptr->wild_mode)
	{
		msg_print("You cannot auto-play the world map.");
		return;
	}

	if (p_ptr->confused)
	{
		msg_print("You are too confused!");
		return;
	}

	/* Cancel running/resting/repeat/travel/explore, then take over. */
	disturb(0, 0);
	exploring = 0;
	autoplaying = 1;
	autoplay_shopping = FALSE;     /* fresh shopping trip bookkeeping */
	autoplay_no_resupply_until = 0;
	ap_ignore_idx = ap_chase_idx = 0;
	ap_chase_turns = 0;
	autoplay_clear_stair();
	autoplay_clear_stuck();
	p_ptr->redraw |= (PR_STATE);
	msg_print("Autoplay started (press any key to stop).");
}


/*
 * Take care of the various things that can happen when you step
 * into a space. (Objects, traps, and stores.)
 */
void step_effects(int y, int x, int do_pickup)
{
	/* Handle "objects" */
	py_pickup_floor(do_pickup);

	/* Handle "store doors" */
	if (cave[y][x].feat == FEAT_SHOP)
	{
		/* Disturb */
		disturb(0, 0);

		/* Hack -- Enter store */
		command_new = KTRL('V');
	}

	/* Discover/set off traps */
	else if (cave[y][x].t_idx != 0)
	{
		/* Disturb */
		disturb(0, 0);

		if (!(cave[y][x].info & CAVE_TRDT))
		{
			/* Message */
			msg_print("You found a trap!");

			/* Pick a trap */
			pick_trap(y, x);
		}

		/* Hit the trap */
		hit_trap();
	}
}

/*
 * Issue a pet command
 */
void do_cmd_pet(void)
{
	int i = 0;

	int num = 0;

	int powers[36];

	char power_desc[36][80];

	bool flag, redraw;

	int ask;

	char choice;

	char out_val[160];

	int pets = 0, pet_ctr = 0;

	bool all_pets = FALSE;

	monster_type *m_ptr;


	for (num = 0; num < 36; num++)
	{
		powers[num] = 0;
		strcpy(power_desc[num], "");
	}

	num = 0;

	if (p_ptr->confused)
	{
		msg_print("You are too confused to command your pets");
		energy_use = 0;
		return;
	}

	/* Calculate pets */
	/* Process the monsters (backwards) */
	for (pet_ctr = m_max - 1; pet_ctr >= 1; pet_ctr--)
	{
		/* Access the monster */
		m_ptr = &m_list[pet_ctr];

		if (m_ptr->status >= MSTATUS_FRIEND) pets++;
	}

	if (pets == 0)
	{
		msg_print("You have no pets/companions.");
		energy_use = 0;
		return;
	}
	else
	{
		strcpy(power_desc[num], "dismiss pets");
		powers[num++] = 1;
		strcpy(power_desc[num], "dismiss companions");
		powers[num++] = 10;
		strcpy(power_desc[num], "call pets");
		powers[num++] = 2;
		strcpy(power_desc[num], "follow me");
		powers[num++] = 6;
		strcpy(power_desc[num], "seek and destroy");
		powers[num++] = 3;
		if (p_ptr->pet_open_doors)
			strcpy(power_desc[num], "disallow open doors");
		else
			strcpy(power_desc[num], "allow open doors");
		powers[num++] = 4;
		if (p_ptr->pet_pickup_items)
			strcpy(power_desc[num], "disallow pickup items");
		else
			strcpy(power_desc[num], "allow pickup items");
		powers[num++] = 5;
		strcpy(power_desc[num], "give target to a friend");
		powers[num++] = 7;
		strcpy(power_desc[num], "give target to all friends");
		powers[num++] = 8;
		strcpy(power_desc[num], "friend forget target");
		powers[num++] = 9;
	}

	/* Nothing chosen yet */
	flag = FALSE;

	/* No redraw yet */
	redraw = FALSE;

	/* Build a prompt (accept all spells) */
	if (num <= 26)
	{
		/* Build a prompt (accept all spells) */
		strnfmt(out_val, 78,
		        "(Command %c-%c, *=List, ESC=exit) Select a command: ", I2A(0),
		        I2A(num - 1));
	}
	else
	{
		strnfmt(out_val, 78,
		        "(Command %c-%c, *=List, ESC=exit) Select a command: ", I2A(0),
		        '0' + num - 27);
	}

	/* Get a command from the user */
	while (!flag && get_com(out_val, &choice))
	{
		/* Request redraw */
		if ((choice == ' ') || (choice == '*') || (choice == '?'))
		{
			/* Show the list */
			if (!redraw)
			{
				byte y = 1, x = 0;
				int ctr = 0;
				char dummy[80];

				strcpy(dummy, "");

				/* Show list */
				redraw = TRUE;

				/* Save the screen */
				character_icky = TRUE;
				Term_save();

				prt("", y++, x);

				while (ctr < num)
				{
					strnfmt(dummy, 80, "%c) %s", I2A(ctr), power_desc[ctr]);
					prt(dummy, y + ctr, x);
					ctr++;
				}

				if (ctr < 17)
				{
					prt("", y + ctr, x);
				}
				else
				{
					prt("", y + 17, x);
				}
			}

			/* Hide the list */
			else
			{
				/* Hide list */
				redraw = FALSE;

				/* Restore the screen */
				Term_load();
				character_icky = FALSE;
			}

			/* Redo asking */
			continue;
		}

		if (choice == '\r' && num == 1)
		{
			choice = 'a';
		}

		if (isalpha(choice))
		{
			/* Note verify */
			ask = (isupper(choice));

			/* Lowercase */
			if (ask) choice = tolower(choice);

			/* Extract request */
			i = (islower(choice) ? A2I(choice) : -1);
		}
		else
		{
			ask = FALSE; 		/* Can't uppercase digits */

			i = choice - '0' + 26;
		}

		/* Totally Illegal */
		if ((i < 0) || (i >= num))
		{
			bell();
			continue;
		}

		/* Verify it */
		if (ask)
		{
			char tmp_val[160];

			/* Prompt */
			strnfmt(tmp_val, 78, "Use %s? ", power_desc[i]);

			/* Belay that order */
			if (!get_check(tmp_val)) continue;
		}

		/* Stop the loop */
		flag = TRUE;
	}

	/* Restore the screen */
	if (redraw)
	{
		Term_load();
		character_icky = FALSE;
	}

	/* Abort if needed */
	if (!flag)
	{
		energy_use = 0;
		return;
	}

	switch (powers[i])
	{
		/* forget target */
	case 9:
		{
			monster_type *m_ptr;
			int ii, jj;

			msg_print("Select the friendly monster:");
			if (!tgt_pt(&ii, &jj)) return;

			if (cave[jj][ii].m_idx)
			{
				m_ptr = &m_list[cave[jj][ii].m_idx];

				if (m_ptr->status < MSTATUS_PET)
				{
					msg_print("You cannot give orders to this monster.");
					return;
				}

				m_ptr->target = -1;
			}
			break;
		}
		/* Give target to all */
	case 8:
		{
			monster_type *m_ptr;
			int ii, jj, i;


			msg_print("Select the target monster:");
			if (!tgt_pt(&ii, &jj)) return;

			if (cave[jj][ii].m_idx)
			{
				msg_print("Target selected");

				for (i = m_max - 1; i >= 1; i--)
				{
					/* Access the monster */
					m_ptr = &m_list[i];

					if (!m_ptr->r_idx) continue;

					if (m_ptr->status < MSTATUS_PET) continue;

					m_ptr->target = cave[jj][ii].m_idx;
				}
			}
			else
			{
				msg_print("This is not a correct target.");
				return;
			}
			break;
		}
	case 1: 				/* Dismiss pets */
		{
			int Dismissed = 0;

			if (get_check("Dismiss all pets? ")) all_pets = TRUE;

			/* Process the monsters (backwards) */
			for (pet_ctr = m_max - 1; pet_ctr >= 1; pet_ctr--)
			{
				monster_race *r_ptr;

				/* Access the monster */
				m_ptr = &m_list[pet_ctr];
				r_ptr = &r_info[m_ptr->r_idx];

				if ((!(r_ptr->flags7 & RF7_NO_DEATH)) && ((m_ptr->status == MSTATUS_PET) || (m_ptr->status == MSTATUS_FRIEND)))	/* Get rid of it! */
				{
					bool delete_this = FALSE;

					if (all_pets)
						delete_this = TRUE;
					else
					{
						char friend_name[80], check_friend[80];
						monster_desc(friend_name, m_ptr, 0x80);
						strnfmt(check_friend, 80, "Dismiss %s? ", friend_name);

						if (get_check(check_friend))
							delete_this = TRUE;
					}

					if (delete_this)
					{
						delete_monster_idx(pet_ctr);
						Dismissed++;
					}
				}
			}

			msg_format("You have dismissed %d pet%s.", Dismissed,
			           (Dismissed == 1 ? "" : "s"));
			break;
		}
	case 10: 				/* Dismiss companions */
		{
			int Dismissed = 0;

			if (get_check("Dismiss all companions? ")) all_pets = TRUE;

			/* Process the monsters (backwards) */
			for (pet_ctr = m_max - 1; pet_ctr >= 1; pet_ctr--)
			{
				monster_race *r_ptr;

				/* Access the monster */
				m_ptr = &m_list[pet_ctr];
				r_ptr = &r_info[m_ptr->r_idx];

				if ((!(r_ptr->flags7 & RF7_NO_DEATH)) && ((m_ptr->status == MSTATUS_COMPANION)))	/* Get rid of it! */
				{
					bool delete_this = FALSE;

					if (all_pets)
						delete_this = TRUE;
					else
					{
						char friend_name[80], check_friend[80];
						monster_desc(friend_name, m_ptr, 0x80);
						strnfmt(check_friend, 80, "Dismiss %s? ", friend_name);

						if (get_check(check_friend))
							delete_this = TRUE;
					}

					if (delete_this)
					{
						delete_monster_idx(pet_ctr);
						Dismissed++;
					}
				}
			}

			msg_format("You have dismissed %d companion%s.", Dismissed,
			           (Dismissed == 1 ? "" : "s"));
			break;
		}
		/* Call pets */
	case 2:
		{
			p_ptr->pet_follow_distance = 1;
			break;
		}
		/* "Seek and destroy" */
	case 3:
		{
			p_ptr->pet_follow_distance = 255;
			break;
		}
		/* flag - allow pets to open doors */
	case 4:
		{
			p_ptr->pet_open_doors = !p_ptr->pet_open_doors;
			break;
		}
		/* flag - allow pets to pickup items */
	case 5:
		{
			p_ptr->pet_pickup_items = !p_ptr->pet_pickup_items;

			/* Drop objects being carried by pets */
			if (!p_ptr->pet_pickup_items)
			{
				for (pet_ctr = m_max - 1; pet_ctr >= 1; pet_ctr--)
				{
					/* Access the monster */
					m_ptr = &m_list[pet_ctr];

					if (m_ptr->status >= MSTATUS_PET)
					{
						monster_drop_carried_objects(m_ptr);
					}
				}
			}

			break;
		}
		/* "Follow Me" */
	case 6:
		{
			p_ptr->pet_follow_distance = 6;
			break;
		}
	}
}

/*
 * Incarnate into a body
 */
bool do_cmd_integrate_body()
{
	cptr q, s;

	int item;

	object_type *o_ptr;


	if (!p_ptr->disembodied)
	{
		msg_print("You are already in a body");
		return FALSE;
	}

	/* Restrict choices to monsters */
	item_tester_tval = TV_CORPSE;

	/* Get an item */
	q = "Incarnate in which body? ";
	s = "You have no corpse to incarnate in.";
	if (!get_item(&item, q, s, (USE_FLOOR))) return FALSE;

	o_ptr = &o_list[0 - item];

	if (o_ptr->sval != SV_CORPSE_CORPSE)
	{
		msg_print("You must select a corpse");
		return FALSE;
	}

	p_ptr->body_monster = o_ptr->pval2;
	p_ptr->chp = o_ptr->pval3;

	floor_item_increase(0 - item, -1);
	floor_item_describe(0 - item);
	floor_item_optimize(0 - item);

	msg_print("Your spirit is incarnated in your new body.");
	p_ptr->wraith_form = FALSE;
	p_ptr->disembodied = FALSE;
	do_cmd_redraw();

	return TRUE;
}

/*
 * Leave a body
 */
bool do_cmd_leave_body(bool drop_body)
{
	object_type *o_ptr, forge;

	monster_race *r_ptr = &r_info[p_ptr->body_monster];

	int i;


	if (p_ptr->disembodied)
	{
		msg_print("You are already disembodied.");
		return FALSE;
	}

	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		if (p_ptr->body_parts[i - INVEN_WIELD] && p_ptr->inventory[i].k_idx &&
		                cursed_p(&p_ptr->inventory[i]))
		{
			msg_print("A cursed object is preventing you from leaving your body.");
			return FALSE;
		}
	}

	if (drop_body)
	{
		if (magik(25 + get_skill_scale(SKILL_POSSESSION, 25) + get_skill(SKILL_PRESERVATION)))
		{
			o_ptr = &forge;
			object_prep(o_ptr, lookup_kind(TV_CORPSE, SV_CORPSE_CORPSE));
			o_ptr->number = 1;
			o_ptr->pval = 0;
			o_ptr->pval2 = p_ptr->body_monster;
			o_ptr->pval3 = p_ptr->chp;
			o_ptr->weight = (r_ptr->weight + rand_int(r_ptr->weight) / 10) + 1;
			object_aware(o_ptr);
			object_known(o_ptr);
			o_ptr->ident |= IDENT_STOREB;

			/* Unique corpses are unique */
			if (r_ptr->flags1 & RF1_UNIQUE)
			{
				o_ptr->name1 = 201;
			}

			drop_near(o_ptr, -1, p_ptr->py, p_ptr->px);
		}
		else
			msg_print
			("You do not manage to keep the corpse from rotting away.");
	}

	msg_print("Your spirit leaves your body.");
	p_ptr->disembodied = TRUE;

	/* Turn into a lost soul(just for the picture) */
	p_ptr->body_monster = test_monster_name("Lost soul");
	do_cmd_redraw();

	return (TRUE);
}


bool execute_inscription(byte i, byte y, byte x)
{
	cave_type *c_ptr = &cave[y][x];


	/* Not enough mana in the current grid */
	if (c_ptr->mana < inscription_info[i].mana) return (TRUE);


	/* Reduce the grid mana -- note: it can't be restored */
	c_ptr->mana -= inscription_info[i].mana;

	/* Analyse inscription type */
	switch (i)
	{
	case INSCRIP_LIGHT:
		{
			msg_print("The inscription shines in a bright light !");
			lite_room(y, x);

			break;
		}

	case INSCRIP_DARK:
		{
			msg_print("The inscription is enveloped in a dark aura!");
			unlite_room(y, x);

			break;
		}

	case INSCRIP_STORM:
		{
			msg_print("The inscription releases a powerful storm !");
			project(0, 3, y, x, damroll(10, 10),
			        GF_ELEC, PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM |
			        PROJECT_KILL | PROJECT_JUMP);

			break;
		}

	case INSCRIP_PROTECTION:
		{
			return (FALSE);

			break;
		}

	case INSCRIP_DWARF_SUMMON:
		{
			int yy = y, xx = x;

			scatter(&yy, &xx, y, x, 3, 0);
			place_monster_one(yy, xx, test_monster_name("Dwarven Warrior"),
			                  0, FALSE, MSTATUS_FRIEND);

			break;
		}

	case INSCRIP_CHASM:
		{
			monster_type *m_ptr;
			monster_race *r_ptr;
			cave_type *c_ptr;
			int ii = x, ij = y;

			cave_set_feat(ij, ii, FEAT_DARK_PIT);
			msg_print("A chasm appears in the floor!");

			if (cave[ij][ii].m_idx)
			{
				m_ptr = &m_list[cave[ij][ii].m_idx];
				r_ptr = race_inf(m_ptr);

				if (r_ptr->flags7 & RF7_CAN_FLY)
				{
					msg_print("The monster simply flies over the chasm.");
				}
				else
				{
					if (!(r_ptr->flags1 & RF1_UNIQUE))
					{
						msg_print("The monster fall in the chasm !");
						delete_monster_idx(cave[ij][ii].m_idx);
					}
				}
			}

			if (cave[ij][ii].o_idx)
			{
				s16b this_o_idx, next_o_idx = 0;

				c_ptr = &cave[ij][ii];

				/* Scan all objects in the grid */
				for (this_o_idx = c_ptr->o_idx; this_o_idx;
				                this_o_idx = next_o_idx)
				{
					object_type * o_ptr;
					bool plural = FALSE;

					char o_name[80];

					/* Acquire object */
					o_ptr = &o_list[this_o_idx];

					if (o_ptr->number > 1) plural = TRUE;

					/* Acquire next object */
					next_o_idx = o_ptr->next_o_idx;

					/* Effect "observed" */
					if (o_ptr->marked)
					{
						object_desc(o_name, o_ptr, FALSE, 0);
					}

					/* Artifacts get to resist */
					if (o_ptr->name1)
					{
						/* Observe the resist */
						if (o_ptr->marked)
						{
							msg_format("The %s %s simply fly over the chasm!",
							           o_name, (plural ? "are" : "is"));
						}
					}

					/* Kill it */
					else
					{
						/* Delete the object */
						delete_object_idx(this_o_idx);

						/* Redraw */
						lite_spot(ij, ii);
					}
				}
			}

			break;
		}

	case INSCRIP_BLACK_FIRE:
		{
			msg_print("The inscription releases a blast of hellfire !");
			project(0, 3, y, x, 200,
			        GF_HELL_FIRE, PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM |
			        PROJECT_KILL | PROJECT_JUMP);

			break;
		}
	}

	return (TRUE);
}


/*
 * Choose an inscription and engrave it
 */
void do_cmd_engrave()
{
	char buf[41] = "";

	byte i;

	strnfmt(buf, 41, "%s", inscription_info[cave[p_ptr->py][p_ptr->px].inscription].text);

	get_string("Engrave what? ", buf, 40);

	/* Silently do nothing when player his escape or enters an empty string */
	if (!buf[0]) return;

	for (i = 0; i < MAX_INSCRIPTIONS; i++)
	{
		if (!strcmp(inscription_info[i].text, buf))
		{
			if (inscription_info[i].know)
			{
				/* Save the inscription */
				cave[p_ptr->py][p_ptr->px].inscription = i;
			}
			else
				msg_print("You can't use this inscription for now.");
		}
	}

	/* Execute the inscription */
	if (inscription_info[cave[p_ptr->py][p_ptr->px].inscription].when & INSCRIP_EXEC_ENGRAVE)
	{
		execute_inscription(cave[p_ptr->py][p_ptr->px].inscription, p_ptr->py, p_ptr->px);
	}

	energy_use += 300;
}


/*
 * Let's do a spinning around attack:                   -- DG --
 *     aDb
 *     y@k
 *     ooT
 * Ah ... all of those will get hit.
 */
void do_spin()
{
	int i, j;


	msg_print("You start spinning around...");

	for (j = p_ptr->py - 1; j <= p_ptr->py + 1; j++)
	{
		for (i = p_ptr->px - 1; i <= p_ptr->px + 1; i++)
		{
			/* Avoid stupid bugs */
			if (in_bounds(j, i) && cave[j][i].m_idx)
				py_attack(j, i, 1);
		}
	}
}
