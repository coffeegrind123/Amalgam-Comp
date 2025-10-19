#include "Groups.h"

#include "../../Players/PlayerUtils.h"
#include "../../Simulation/ProjectileSimulation/ProjectileSimulation.h"

bool CGroups::ShouldTargetTeam(bool bType, Group_t& tGroup, CBaseEntity* pEntity, CTFPlayer* pLocal)
{
	if (!bType)
		return false;

	if (tGroup.m_iConditions & ConditionsEnum::Relative)
	{
		if (pEntity->m_iTeamNum() != pLocal->m_iTeamNum())
			return tGroup.m_iConditions & ConditionsEnum::Enemy;
		else
			return tGroup.m_iConditions & ConditionsEnum::Team;
	}
	else
	{
		if (pEntity->m_iTeamNum() != pLocal->m_iTeamNum()
			? !(tGroup.m_iConditions & ConditionsEnum::Enemy)
			: !(tGroup.m_iConditions & ConditionsEnum::Team))
		{
			return false;
		}

		switch (pEntity->m_iTeamNum())
		{
		case TF_TEAM_BLUE:
			return tGroup.m_iConditions & ConditionsEnum::BLU;
		case TF_TEAM_RED:
			return tGroup.m_iConditions & ConditionsEnum::RED;
		}
	}
	return false;
}

bool CGroups::ShouldTargetOwner(bool bType, Group_t& tGroup, CBaseEntity* pOwner, CBaseEntity* pEntity, CTFPlayer* pLocal)
{
	if (!bType)
		return false;

	if (tGroup.m_iConditions & ConditionsEnum::Local && pOwner == pLocal
		|| tGroup.m_iConditions & ConditionsEnum::Friends && H::Entities.IsFriend(pOwner->entindex())
		|| tGroup.m_iConditions & ConditionsEnum::Party && H::Entities.InParty(pOwner->entindex())
		|| tGroup.m_iConditions & ConditionsEnum::Priority && F::PlayerUtils.IsPrioritized(pOwner->entindex())
		|| tGroup.m_iConditions & ConditionsEnum::Target && pEntity->entindex() == G::AimTarget.m_iEntIndex)
	{
		return true;
	}

	return ShouldTargetTeam(bType, tGroup, pEntity, pLocal);
}

static inline bool ShouldTargetPlayer(Group_t& tGroup, int iBit, CBaseEntity* pEntity, CTFPlayer* pLocal)
{
    if (!(tGroup.m_iTargets & iBit))
        return false;

    auto pPlayer = pEntity->As<CTFPlayer>();
    if (pPlayer == pLocal && !(tGroup.m_iConditions & ConditionsEnum::Local))
        return false;

    if (tGroup.m_iPlayers)
    {
        if (tGroup.m_iPlayers & PlayerEnum::Classes)
        {
            int iFlag = 0;
            switch (pPlayer->m_iClass())
            {
            case TF_CLASS_SCOUT: iFlag = PlayerEnum::Scout; break;
            case TF_CLASS_SOLDIER: iFlag = PlayerEnum::Soldier; break;
            case TF_CLASS_PYRO: iFlag = PlayerEnum::Pyro; break;
            case TF_CLASS_DEMOMAN: iFlag = PlayerEnum::Demoman; break;
            case TF_CLASS_HEAVY: iFlag = PlayerEnum::Heavy; break;
            case TF_CLASS_ENGINEER: iFlag = PlayerEnum::Engineer; break;
            case TF_CLASS_MEDIC: iFlag = PlayerEnum::Medic; break;
            case TF_CLASS_SNIPER: iFlag = PlayerEnum::Sniper; break;
            case TF_CLASS_SPY: iFlag = PlayerEnum::Spy; break;
            }
            if (!(tGroup.m_iPlayers & iFlag))
                return false;
        }
        if (tGroup.m_iPlayers & PlayerEnum::Conds)
        {
            if (tGroup.m_iPlayers & PlayerEnum::Invulnerable && !pPlayer->IsInvulnerable()
                || tGroup.m_iPlayers & PlayerEnum::Crits && !pPlayer->IsCritBoosted()
                || tGroup.m_iPlayers & PlayerEnum::Invisible && !pPlayer->IsInvisible()
                || tGroup.m_iPlayers & PlayerEnum::Disguise && !pPlayer->InCond(TF_COND_DISGUISED)
                || tGroup.m_iPlayers & PlayerEnum::Hurt && pPlayer->m_iHealth() >= pPlayer->GetMaxHealth())
                return false;
        }
    }

    return ShouldTargetOwner(tGroup.m_iTargets & iBit, tGroup, pEntity, pEntity, pLocal);
}

static inline bool ShouldTargetBuilding(Group_t& tGroup, int iBit, CBaseEntity* pEntity, CTFPlayer* pLocal)
{
    if (!(tGroup.m_iTargets & iBit))
        return false;

    auto pBuilding = pEntity->As<CBaseObject>();
    if (!pBuilding->m_iHealth() || pBuilding->m_bCarried())
        return false;

    if (tGroup.m_iBuildings)
    {
        if (tGroup.m_iBuildings & BuildingEnum::Classes)
        {
            int iFlag = 0;
            switch (pBuilding->GetClassID())
            {
            case ETFClassID::CObjectSentrygun: iFlag = BuildingEnum::Sentry; break;
            case ETFClassID::CObjectDispenser: iFlag = BuildingEnum::Dispenser; break;
            case ETFClassID::CObjectTeleporter: iFlag = BuildingEnum::Teleporter; break;
            }
            if (!(tGroup.m_iBuildings & iFlag))
                return false;
        }
        if (tGroup.m_iBuildings & BuildingEnum::Conds)
        {
            if (tGroup.m_iBuildings & BuildingEnum::Hurt && pBuilding->m_iHealth() >= pBuilding->m_iMaxHealth())
                return false;
        }
    }

    auto pOwner = pBuilding->m_hBuilder().Get();
    if (!pOwner) pOwner = pBuilding;
    return ShouldTargetOwner(tGroup.m_iTargets & iBit, tGroup, pOwner, pEntity, pLocal);
}

static inline bool ShouldTargetProjectile(Group_t& tGroup, int iBit, CBaseEntity* pEntity, CTFPlayer* pLocal)
{
    if (!(tGroup.m_iTargets & iBit))
        return false;

    if (tGroup.m_iProjectiles)
    {
        if (tGroup.m_iProjectiles & ProjectileEnum::Classes)
        {
            int iFlag = ProjectileEnum::Misc;
            switch (pEntity->GetClassID())
            {
            case ETFClassID::CTFProjectile_Rocket:
            case ETFClassID::CTFProjectile_EnergyBall:
            case ETFClassID::CTFProjectile_SentryRocket: iFlag = ProjectileEnum::Rocket; break;
            case ETFClassID::CTFGrenadePipebombProjectile: iFlag = pEntity->As<CTFGrenadePipebombProjectile>()->HasStickyEffects() ? ProjectileEnum::Sticky : ProjectileEnum::Pipe; break;
            case ETFClassID::CTFProjectile_Arrow: iFlag = pEntity->As<CTFProjectile_Arrow>()->m_iProjectileType() == TF_PROJECTILE_BUILDING_REPAIR_BOLT ? ProjectileEnum::Repair : ProjectileEnum::Arrow; break;
            case ETFClassID::CTFProjectile_HealingBolt: iFlag = ProjectileEnum::Heal; break;
            case ETFClassID::CTFProjectile_Flare: iFlag = ProjectileEnum::Flare; break;
            case ETFClassID::CTFProjectile_BallOfFire: iFlag = ProjectileEnum::Fire; break;
            case ETFClassID::CTFProjectile_Cleaver: iFlag = ProjectileEnum::Cleaver; break;
            case ETFClassID::CTFProjectile_JarMilk:
            case ETFClassID::CTFProjectile_ThrowableBreadMonster: iFlag = ProjectileEnum::Milk; break;
            case ETFClassID::CTFProjectile_Jar: iFlag = ProjectileEnum::Jarate; break;
            case ETFClassID::CTFProjectile_JarGas: iFlag = ProjectileEnum::Gas; break;
            case ETFClassID::CTFBall_Ornament: iFlag = ProjectileEnum::Bauble; break;
            case ETFClassID::CTFStunBall: iFlag = ProjectileEnum::Baseball; break;
            case ETFClassID::CTFProjectile_EnergyRing: iFlag = ProjectileEnum::Energy; break;
            case ETFClassID::CTFProjectile_MechanicalArmOrb: iFlag = ProjectileEnum::ShortCircuit; break;
            case ETFClassID::CTFProjectile_SpellMeteorShower: iFlag = ProjectileEnum::MeteorShower; break;
            case ETFClassID::CTFProjectile_SpellLightningOrb: iFlag = ProjectileEnum::Lightning; break;
            case ETFClassID::CTFProjectile_SpellFireball: iFlag = ProjectileEnum::Fireball; break;
            case ETFClassID::CTFWeaponBaseMerasmusGrenade: iFlag = ProjectileEnum::Bomb; break;
            case ETFClassID::CTFProjectile_SpellBats:
            case ETFClassID::CTFProjectile_SpellKartBats: iFlag = ProjectileEnum::Bats; break;
            case ETFClassID::CTFProjectile_SpellMirv:
            case ETFClassID::CTFProjectile_SpellPumpkin: iFlag = ProjectileEnum::Pumpkin; break;
            case ETFClassID::CTFProjectile_SpellSpawnBoss: iFlag = ProjectileEnum::Monoculus; break;
            case ETFClassID::CTFProjectile_SpellSpawnHorde:
            case ETFClassID::CTFProjectile_SpellSpawnZombie: iFlag = ProjectileEnum::Skeleton; break;
            }
            if (!(tGroup.m_iProjectiles & iFlag))
                return false;
        }
        if (tGroup.m_iProjectiles & ProjectileEnum::Conds)
        {
            bool bCrit = false, bMinicrit = false;
            switch (pEntity->GetClassID())
            {
            case ETFClassID::CTFWeaponBaseMerasmusGrenade:
            case ETFClassID::CTFGrenadePipebombProjectile:
            case ETFClassID::CTFStunBall:
            case ETFClassID::CTFBall_Ornament:
            case ETFClassID::CTFProjectile_Jar:
            case ETFClassID::CTFProjectile_Cleaver:
            case ETFClassID::CTFProjectile_JarGas:
            case ETFClassID::CTFProjectile_JarMilk:
            case ETFClassID::CTFProjectile_SpellBats:
            case ETFClassID::CTFProjectile_SpellKartBats:
            case ETFClassID::CTFProjectile_SpellMeteorShower:
            case ETFClassID::CTFProjectile_SpellMirv:
            case ETFClassID::CTFProjectile_SpellPumpkin:
            case ETFClassID::CTFProjectile_SpellSpawnBoss:
            case ETFClassID::CTFProjectile_SpellSpawnHorde:
            case ETFClassID::CTFProjectile_SpellSpawnZombie:
                bCrit = pEntity->As<CTFWeaponBaseGrenadeProj>()->m_bCritical();
                bMinicrit = pEntity->As<CTFWeaponBaseGrenadeProj>()->m_iDeflected() && (pEntity->GetClassID() != ETFClassID::CTFGrenadePipebombProjectile || !pEntity->GetAbsVelocity().IsZero());
                break;
            case ETFClassID::CTFProjectile_Arrow:
            case ETFClassID::CTFProjectile_HealingBolt:
                bCrit = pEntity->As<CTFProjectile_Arrow>()->m_bCritical();
                bMinicrit = pEntity->As<CTFBaseRocket>()->m_iDeflected();
                break;
            case ETFClassID::CTFProjectile_Rocket:
            case ETFClassID::CTFProjectile_BallOfFire:
            case ETFClassID::CTFProjectile_MechanicalArmOrb:
            case ETFClassID::CTFProjectile_SentryRocket:
            case ETFClassID::CTFProjectile_SpellFireball:
            case ETFClassID::CTFProjectile_SpellLightningOrb:
                bCrit = pEntity->As<CTFProjectile_Rocket>()->m_bCritical();
                bMinicrit = pEntity->As<CTFBaseRocket>()->m_iDeflected();
                break;
            case ETFClassID::CTFProjectile_EnergyBall:
                bMinicrit = pEntity->As<CTFProjectile_EnergyBall>()->m_bChargedShot() || pEntity->As<CTFBaseRocket>()->m_iDeflected();
                break;
            case ETFClassID::CTFProjectile_Flare:
                bCrit = pEntity->As<CTFProjectile_Flare>()->m_bCritical();
                bMinicrit = pEntity->As<CTFBaseRocket>()->m_iDeflected();
                break;
            }

            if (/*tGroup.m_iProjectiles & (ProjectileEnum::Crit | ProjectileEnum::Minicrit)
                &&*/ !(tGroup.m_iProjectiles & ProjectileEnum::Crit && bCrit)
                && !(tGroup.m_iProjectiles & ProjectileEnum::Minicrit && bMinicrit))
                return false;
        }
    }

    auto pOwner = F::ProjSim.GetEntities(pEntity).second->As<CBaseEntity>();
    if (!pOwner) pOwner = pEntity;
    return ShouldTargetOwner(tGroup.m_iTargets & iBit, tGroup, pOwner, pEntity, pLocal);
}

bool CGroups::ShouldTarget(Group_t& tGroup, CBaseEntity* pEntity, CTFPlayer* pLocal)
{
	switch (pEntity->GetClassID())
	{
	// players
	case ETFClassID::CTFPlayer:
		return ShouldTargetPlayer(tGroup, TargetsEnum::Players, pEntity, pLocal);
	// buildings
	case ETFClassID::CObjectSentrygun:
	case ETFClassID::CObjectDispenser:
	case ETFClassID::CObjectTeleporter:
	{
		return ShouldTargetBuilding(tGroup, TargetsEnum::Buildings, pEntity, pLocal);
	}
	// projectiles
	case ETFClassID::CBaseProjectile:
	case ETFClassID::CBaseGrenade:
	case ETFClassID::CTFWeaponBaseGrenadeProj:
	case ETFClassID::CTFWeaponBaseMerasmusGrenade:
	case ETFClassID::CTFGrenadePipebombProjectile:
	case ETFClassID::CTFStunBall:
	case ETFClassID::CTFBall_Ornament:
	case ETFClassID::CTFProjectile_Jar:
	case ETFClassID::CTFProjectile_Cleaver:
	case ETFClassID::CTFProjectile_JarGas:
	case ETFClassID::CTFProjectile_JarMilk:
	case ETFClassID::CTFProjectile_SpellBats:
	case ETFClassID::CTFProjectile_SpellKartBats:
	case ETFClassID::CTFProjectile_SpellMeteorShower:
	case ETFClassID::CTFProjectile_SpellMirv:
	case ETFClassID::CTFProjectile_SpellPumpkin:
	case ETFClassID::CTFProjectile_SpellSpawnBoss:
	case ETFClassID::CTFProjectile_SpellSpawnHorde:
	case ETFClassID::CTFProjectile_SpellSpawnZombie:
	case ETFClassID::CTFProjectile_SpellTransposeTeleport:
	case ETFClassID::CTFProjectile_Throwable:
	case ETFClassID::CTFProjectile_ThrowableBreadMonster:
	case ETFClassID::CTFProjectile_ThrowableBrick:
	case ETFClassID::CTFProjectile_ThrowableRepel:
	case ETFClassID::CTFBaseRocket:
	case ETFClassID::CTFFlameRocket:
	case ETFClassID::CTFProjectile_Arrow:
	case ETFClassID::CTFProjectile_GrapplingHook:
	case ETFClassID::CTFProjectile_HealingBolt:
	case ETFClassID::CTFProjectile_Rocket:
	case ETFClassID::CTFProjectile_BallOfFire:
	case ETFClassID::CTFProjectile_MechanicalArmOrb:
	case ETFClassID::CTFProjectile_SentryRocket:
	case ETFClassID::CTFProjectile_SpellFireball:
	case ETFClassID::CTFProjectile_SpellLightningOrb:
	case ETFClassID::CTFProjectile_SpellKartOrb:
	case ETFClassID::CTFProjectile_EnergyBall:
	case ETFClassID::CTFProjectile_Flare:
	case ETFClassID::CTFBaseProjectile:
	case ETFClassID::CTFProjectile_EnergyRing:
	//case ETFClassID::CTFProjectile_Syringe:
	{
		return ShouldTargetProjectile(tGroup, TargetsEnum::Projectiles, pEntity, pLocal);
	}
	// ragdolls
	case ETFClassID::CTFRagdoll:
	case ETFClassID::CRagdollProp:
	case ETFClassID::CRagdollPropAttached:
	{
		auto pOwner = pEntity->As<CTFRagdoll>()->m_hPlayer().Get();
		if (!pOwner) pOwner = pEntity;
		return ShouldTargetOwner(tGroup.m_iTargets & TargetsEnum::Ragdolls, tGroup, pOwner, pEntity, pLocal);
	}
	// objectives
	case ETFClassID::CCaptureFlag:
		return ShouldTargetTeam(tGroup.m_iTargets & TargetsEnum::Objective, tGroup, pEntity, pLocal);
	// npcs
	case ETFClassID::CEyeballBoss:
	case ETFClassID::CHeadlessHatman:
	case ETFClassID::CMerasmus:
	case ETFClassID::CTFBaseBoss:
	case ETFClassID::CTFTankBoss:
	case ETFClassID::CZombie:
		if (pEntity->IsInValidTeam())
		{
			if (auto pOwner = pEntity->m_hOwnerEntity().Get())
				return ShouldTargetOwner(tGroup.m_iTargets & TargetsEnum::NPCs, tGroup, pOwner, pEntity, pLocal);
			return ShouldTargetTeam(tGroup.m_iTargets & TargetsEnum::NPCs, tGroup, pEntity, pLocal);
		}
		return tGroup.m_iTargets & TargetsEnum::NPCs;
	// pickups
	case ETFClassID::CBaseAnimating:
	{
		if (H::Entities.IsHealth(H::Entities.GetModel(pEntity->entindex())))
			return tGroup.m_iTargets & TargetsEnum::Health;
		else if (H::Entities.IsAmmo(H::Entities.GetModel(pEntity->entindex())))
			return tGroup.m_iTargets & TargetsEnum::Ammo;
		else if (H::Entities.IsPowerup(H::Entities.GetModel(pEntity->entindex())))
			return tGroup.m_iTargets & TargetsEnum::Powerups;
		else if (H::Entities.IsSpellbook(H::Entities.GetModel(pEntity->entindex())))
			return tGroup.m_iTargets & TargetsEnum::Spellbook;
		break;
	}
	case ETFClassID::CTFAmmoPack:
		return tGroup.m_iTargets & TargetsEnum::Ammo;
	case ETFClassID::CCurrencyPack:
		return tGroup.m_iTargets & TargetsEnum::Money;
	case ETFClassID::CHalloweenGiftPickup:
		return tGroup.m_iTargets & TargetsEnum::Gargoyle;
	// bombs
	case ETFClassID::CTFPumpkinBomb:
	case ETFClassID::CTFGenericBomb:
		return tGroup.m_iTargets & TargetsEnum::Bombs;
	case ETFClassID::CTFMedigunShield:
		return false;
	}

	// players
	auto pOwner = pEntity->m_hOwnerEntity().Get();
	if (pOwner && pOwner->IsPlayer())
		return ShouldTargetOwner(tGroup.m_iTargets & TargetsEnum::Players, tGroup, pOwner, pOwner, pLocal);

	return false;
}