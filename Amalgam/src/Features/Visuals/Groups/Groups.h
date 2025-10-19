#pragma once
#include "../../../SDK/SDK.h"

Enum(Targets, Players = 1 << 0, Buildings = 1 << 1, Projectiles = 1 << 2, Ragdolls = 1 << 3, Objective = 1 << 4, NPCs = 1 << 5, Health = 1 << 6, Ammo = 1 << 7, Money = 1 << 8, Powerups = 1 << 9, Spellbook = 1 << 10, Bombs = 1 << 11, Gargoyle = 1 << 12, FakeAngle = 1 << 13, ViewmodelWeapon = 1 << 14, ViewmodelHands = 1 << 15, ESP = ~(Ragdolls | FakeAngle | ViewmodelWeapon | ViewmodelHands), Occluded = ~(FakeAngle | ViewmodelWeapon | ViewmodelHands))
Enum(Conditions, Enemy = 1 << 0, Team = 1 << 1, BLU = 1 << 2, RED = 1 << 3, Local = 1 << 4, Friends = 1 << 5, Party = 1 << 6, Priority = 1 << 7, Target = 1 << 8, Dormant = 1 << 9, Relative = 1 << 10)
Enum(Player, Scout = 1 << 0, Soldier = 1 << 1, Pyro = 1 << 2, Demoman = 1 << 3, Heavy = 1 << 4, Engineer = 1 << 5, Medic = 1 << 6, Sniper = 1 << 7, Spy = 1 << 8, Invulnerable = 1 << 9, Crits = 1 << 10, Invisible = 1 << 11, Disguise = 1 << 12, Hurt = 1 << 13, Classes = Scout | Soldier | Pyro | Demoman | Heavy | Engineer | Medic | Sniper | Spy, Conds = Invulnerable | Crits | Invisible | Disguise | Hurt)
Enum(Building, Sentry = 1 << 0, Dispenser = 1 << 1, Teleporter = 1 << 2, Hurt = 1 << 3, Classes = Sentry | Dispenser | Teleporter, Conds = Hurt)
Enum(Projectile, Rocket = 1 << 0, Sticky = 1 << 1, Pipe = 1 << 2, Arrow = 1 << 3, Heal = 1 << 4, Flare = 1 << 5, Fire = 1 << 6, Repair = 1 << 7, Cleaver = 1 << 8, Milk = 1 << 9, Jarate = 1 << 10, Gas = 1 << 11, Bauble = 1 << 12, Baseball = 1 << 13, Energy = 1 << 14, ShortCircuit = 1 << 15, MeteorShower = 1 << 16, Lightning = 1 << 17, Fireball = 1 << 18, Bomb = 1 << 19, Bats = 1 << 20, Pumpkin = 1 << 21, Monoculus = 1 << 22, Skeleton = 1 << 23, Misc = 1 << 24, Crit = 1 << 25, Minicrit = 1 << 26, Classes = Rocket | Sticky | Pipe | Arrow | Heal | Flare | Fire | Repair | Cleaver | Milk | Jarate | Gas | Bauble | Baseball | Energy | ShortCircuit | MeteorShower | Lightning | Fireball | Bomb | Bats | Pumpkin | Monoculus | Skeleton | Misc, Conds = Crit | Minicrit)
Enum(ESP, Name = 1 << 0, Box = 1 << 1, Distance = 1 << 2, Bones = 1 << 3, HealthBar = 1 << 4, HealthText = 1 << 5, UberBar = 1 << 6, UberText = 1 << 7, ClassIcon = 1 << 8, ClassText = 1 << 9, WeaponIcon = 1 << 10, WeaponText = 1 << 11, Priority = 1 << 12, Labels = 1 << 13, Buffs = 1 << 14, Debuffs = 1 << 15, Misc = 1 << 16, Lag = 1 << 17, Ping = 1 << 18, KDR = 1 << 19, Owner = 1 << 20, Flags = 1 << 21, Level = 1 << 22, IntelReturnTime = 1 << 23)
Enum(Backtrack, Last = 1 << 0, First = 1 << 1, Always = 1 << 2, IgnoreTeam = 1 << 3)

struct Group_t
{
	std::string m_sName = "";
	int m_iTargets = 0b0; // ragdolls don't draw esp
	int m_iConditions = 0b11001; // red/blu only apply if not relative, classes only apply if at least 1 is selected

	int m_iPlayers = 0b0;
	int m_iBuildings = 0b0;
	int m_iProjectiles = 0b0;

	int m_iESP = 0b0;

	bool m_bOutOfFOVArrows = false;
	int m_iOutOfFOVArrowsOffset = 100;
	float m_flOutOfFOVArrowsMaxDistance = 1000.f;

	int m_iActiveAlpha = 255;
	int m_iDormantAlpha = 100;
	float m_flDormantDuration = 1.f;

	bool m_bChams = false;
	Chams_t m_tChams = {};

	bool m_bGlow = false;
	Glow_t m_tGlow = {};

	// keep backtrack separate?
	bool m_bBacktrack = false;
	int m_iBacktrackDraw = 0b0;
	Chams_t m_tBacktrackChams = {};
	bool m_bBacktrackIgnoreZ = false;
	Glow_t m_tBacktrackGlow = {};
};

class CGroups
{
private:
	bool ShouldTarget(Group_t& tGroup, CBaseEntity* pEntity, CTFPlayer* pLocal);
	bool ShouldTargetOwner(bool bType, Group_t& tGroup, CBaseEntity* pOwner, CBaseEntity* pEntity, CTFPlayer* pLocal);
	bool ShouldTargetTeam(bool bType, Group_t& tGroup, CBaseEntity* pEntity, CTFPlayer* pLocal);

public:
	// GetESP
	// GetChams
	// GetGlow
	// GetBacktrackChams?
	// GetBacktrackGlow?

	std::vector<Group_t> m_vGroups = {}; // loop through this in reverse so back groups have higher priority
};

ADD_FEATURE(CGroups, Groups);