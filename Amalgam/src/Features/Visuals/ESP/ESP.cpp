#include "ESP.h"

#include "../../Players/PlayerUtils.h"
#include "../../Spectate/Spectate.h"
#include "../../Simulation/MovementSimulation/MovementSimulation.h"
#include "../../Simulation/ProjectileSimulation/ProjectileSimulation.h"
#include "../../../Utils/Math/SIMDMath.h"

MAKE_SIGNATURE(CTFPlayerSharedUtils_GetEconItemViewByLoadoutSlot, "client.dll", "48 89 6C 24 ? 56 41 54 41 55 41 56 41 57 48 83 EC", 0x0);
MAKE_SIGNATURE(CEconItemView_GetItemName, "client.dll", "40 53 48 83 EC ? 48 8B D9 C6 81 ? ? ? ? ? E8 ? ? ? ? 48 8B 8B", 0x0);

void CESP::Store(CTFPlayer* pLocal)
{
	m_mPlayerCache.clear();
	m_mBuildingCache.clear();
	m_mWorldCache.clear();

	if (!Vars::ESP::Draw.Value || !pLocal)
		return;

	StorePlayers(pLocal);
	StoreBuildings(pLocal);
	StoreProjectiles(pLocal);
	StoreObjective(pLocal);
	StoreWorld();
}

void CESP::StorePlayers(CTFPlayer* pLocal)
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Players) || !Vars::ESP::Player.Value)
		return;

	auto pObserverTarget = pLocal->m_hObserverTarget().Get();
	int iObserverMode = pLocal->m_iObserverMode();
	if (F::Spectate.m_iTarget != -1)
	{
		pObserverTarget = F::Spectate.m_pTargetTarget;
		iObserverMode = F::Spectate.m_iTargetMode;
	}

	auto pResource = H::Entities.GetPR();
	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ALL))
	{
		if (!pEntity || pEntity->IsDormant())
			continue;
		auto pPlayer = pEntity->As<CTFPlayer>();
		if (!pPlayer)
			continue;
		int iIndex = pPlayer->entindex();

		bool bLocal = iIndex == I::EngineClient->GetLocalPlayer();
		bool bSpectate = iObserverMode == OBS_MODE_FIRSTPERSON || iObserverMode == OBS_MODE_THIRDPERSON;
		bool bTarget = bSpectate && pObserverTarget == pPlayer;

		if (!pPlayer->IsAlive() || pPlayer->IsAGhost())
			continue;

		if (bLocal || bTarget)
		{
			if (!(Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Local) || (!bSpectate ? !I::Input->CAM_IsThirdPerson() && bLocal : iObserverMode == OBS_MODE_FIRSTPERSON && bTarget))
				continue;
		}
		else
		{
			if (pPlayer->IsDormant())
			{
				if (!H::Entities.GetDormancy(iIndex) || !Vars::ESP::DormantAlpha.Value
					|| Vars::ESP::DormantPriority.Value && !F::PlayerUtils.IsPrioritized(iIndex))
					continue;
			}

			if (!(Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Prioritized && F::PlayerUtils.IsPrioritized(iIndex))
				&& !(Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Friends && H::Entities.IsFriend(iIndex))
				&& !(Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Party && H::Entities.InParty(iIndex))
				&& !(pPlayer->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Enemy : Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Team))
				continue;
		}

		int iClassNum = pPlayer->m_iClass();
		auto pActiveWeapon = pPlayer->m_hActiveWeapon().Get();
		auto pWeapon = pActiveWeapon ? pActiveWeapon->As<CTFWeaponBase>() : nullptr;

		PlayerCache& tCache = m_mPlayerCache[pEntity];
		tCache.m_flAlpha = (pPlayer->IsDormant() ? Vars::ESP::DormantAlpha.Value : Vars::ESP::ActiveAlpha.Value) / 255.f;
		tCache.m_tColor = GetColor(pLocal, pPlayer);
		tCache.m_bBox = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Box;
		tCache.m_bBones = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Bones;

		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Distance && !bLocal)
		{
			Vec3 vDelta = pPlayer->m_vecOrigin() - pLocal->m_vecOrigin();
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, std::format("[{:.0f}M]", CSIMDMath::FastLength2D(vDelta) / 41), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		PlayerInfo_t pi{};
		if (I::EngineClient->GetPlayerInfo(iIndex, &pi))
		{
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Name)
				tCache.m_vText.emplace_back(ESPTextEnum::Top, F::PlayerUtils.GetPlayerName(iIndex, pi.name), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Priority)
			{
				if (auto pTag = F::PlayerUtils.GetSignificantTag(pi.friendsID, 1)) // 50 alpha as white outline tends to be more apparent
					tCache.m_vText.emplace_back(ESPTextEnum::Top, pTag->m_sName, pTag->m_tColor, H::Draw.IsColorDark(pTag->m_tColor) ? Color_t(255, 255, 255, 50) : Color_t(0, 0, 0));
			}

			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Labels)
			{
				std::vector<PriorityLabel_t*> vTags = {};
				for (auto& iID : F::PlayerUtils.m_mPlayerTags[pi.friendsID])
				{
					auto pTag = F::PlayerUtils.GetTag(iID);
					if (pTag && pTag->m_bLabel)
						vTags.push_back(pTag);
				}
				if (H::Entities.IsFriend(pi.friendsID))
				{
					auto pTag = &F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(FRIEND_TAG)];
					if (pTag->m_bLabel)
						vTags.push_back(pTag);
				}
				if (H::Entities.InParty(pi.friendsID))
				{
					auto pTag = &F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(PARTY_TAG)];
					if (pTag->m_bLabel)
						vTags.push_back(pTag);
				}
				if (H::Entities.IsF2P(pi.friendsID))
				{
					auto pTag = &F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(F2P_TAG)];
					if (pTag->m_bLabel)
						vTags.push_back(pTag);
				}

				if (vTags.size())
				{
					std::sort(vTags.begin(), vTags.end(), [&](const auto a, const auto b) -> bool
						{
							// sort by priority if unequal
							if (a->m_iPriority != b->m_iPriority)
								return a->m_iPriority > b->m_iPriority;

							return a->m_sName < b->m_sName;
						});

					for (auto& pTag : vTags) // 50 alpha as white outline tends to be more apparent
						tCache.m_vText.emplace_back(ESPTextEnum::Right, pTag->m_sName, pTag->m_tColor, H::Draw.IsColorDark(pTag->m_tColor) ? Color_t(255, 255, 255, 50) : Color_t(0, 0, 0));
				}
			}
		}

		float flHealth = pPlayer->m_iHealth(), flMaxHealth = pPlayer->GetMaxHealth();
		if (tCache.m_bHealthBar = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::HealthBar)
		{
			if (flHealth > flMaxHealth)
			{
				float flMaxOverheal = floorf(flMaxHealth / 10.f) * 5;
				tCache.m_flHealth = 1.f + std::clamp((flHealth - flMaxHealth) / flMaxOverheal, 0.f, 1.f);
			}
			else
				tCache.m_flHealth = std::clamp(flHealth / flMaxHealth, 0.f, 1.f);
		}
		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::HealthText)
			tCache.m_vText.emplace_back(ESPTextEnum::Health, std::format("{}", flHealth), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (iClassNum == TF_CLASS_MEDIC)
		{
			auto pMediGun = pPlayer->GetWeaponFromSlot(SLOT_SECONDARY);
			if (pMediGun && pMediGun->GetClassID() == ETFClassID::CWeaponMedigun)
			{
				auto pMediGunTyped = pMediGun->As<CWeaponMedigun>();
				if (pMediGunTyped)
				{
					tCache.m_flUber = std::clamp(pMediGunTyped->m_flChargeLevel(), 0.f, 1.f);
					tCache.m_bUberBar = Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::UberBar;
					if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::UberText)
						tCache.m_vText.emplace_back(ESPTextEnum::Uber, std::format("{:.0f}%", tCache.m_flUber * 100.f), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				}
			}
		}

		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::ClassIcon)
			tCache.m_iClassIcon = iClassNum;
		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::ClassText)
			tCache.m_vText.emplace_back(ESPTextEnum::Right, SDK::GetClassByIndex(iClassNum, false), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::WeaponIcon && pWeapon)
			tCache.m_pWeaponIcon = pWeapon->GetWeaponIcon();
		if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::WeaponText && pWeapon)
		{
			auto pAttributeManager = U::Memory.CallVirtual<1, void*>(uintptr_t(pWeapon) + 3096);
			auto pCurItemData = reinterpret_cast<void*>(uintptr_t(pAttributeManager) + 144);
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, SDK::ConvertWideToUTF8(S::CEconItemView_GetItemName.Call<const wchar_t*>(pCurItemData)), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		if (Vars::Debug::Info.Value && !pPlayer->IsDormant() && !bLocal)
		{
			int iAverage = TIME_TO_TICKS(F::MoveSim.GetPredictedDelta(pPlayer));
			int iCurrent = H::Entities.GetChoke(iIndex);
			tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Lag {}, {}", iAverage, iCurrent), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		{
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::LagCompensation && !pPlayer->IsDormant() && !bLocal)
			{
				if (H::Entities.GetLagCompensation(iIndex))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Lagcomp", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
			}

			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Ping && pResource && !bLocal)
			{
				int iPing = pResource->m_iPing(iIndex);
				if (iPing && (iPing >= 200 || iPing <= 5))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("{}MS", iPing), Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
			}

			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::KDR && pResource && !bLocal)
			{
				int iKills = pResource->m_iScore(iIndex), iDeaths = pResource->m_iDeaths(iIndex);
				if (iKills >= 20)
				{
					int iKDR = iKills / std::max(iDeaths, 1);
					if (iKDR >= 10)
						tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("High KD [{} / {}]", iKills, iDeaths), Vars::Colors::IndicatorTextMid.Value, Vars::Menu::Theme::Background.Value);
				}
			}

			// Buffs
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Buffs)
			{
				if (pPlayer->InCond(TF_COND_INVULNERABLE) ||
					pPlayer->InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED) ||
					pPlayer->InCond(TF_COND_INVULNERABLE_USER_BUFF) ||
					pPlayer->InCond(TF_COND_INVULNERABLE_CARD_EFFECT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Uber", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_MEGAHEAL))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Megaheal", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_PHASE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bonk", Vars::Colors::IndicatorTextMid.Value, Vars::Menu::Theme::Background.Value);

				bool bCrits = false, bMiniCrits = false;
				if (pPlayer->IsCritBoosted())
					pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_PARTICLE_CANNON ? bMiniCrits = true : bCrits = true;
				if (pPlayer->IsMiniCritBoosted())
					pWeapon && SDK::AttribHookValue(0, "minicrits_become_crits", pWeapon) == 1 ? bCrits = true : bMiniCrits = true;
				if (pWeapon && SDK::AttribHookValue(0, "crit_while_airborne", pWeapon) && pPlayer->InCond(TF_COND_BLASTJUMPING))
					bCrits = true, bMiniCrits = false;

				if (bCrits)
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crits", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (bMiniCrits)
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Mini-crits", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);

				/* vaccinator effects */
				if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BULLET_RESIST) || pPlayer->InCond(TF_COND_BULLET_IMMUNE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bullet+", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_BULLET_RESIST))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bullet", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BLAST_RESIST) || pPlayer->InCond(TF_COND_BLAST_IMMUNE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Blast+", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_BLAST_RESIST))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Blast", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_FIRE_RESIST) || pPlayer->InCond(TF_COND_FIRE_IMMUNE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Fire+", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_FIRE_RESIST))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Fire", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_OFFENSEBUFF))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Banner", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_DEFENSEBUFF))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Battalions", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_REGENONDAMAGEBUFF))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Conch", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_RUNE_STRENGTH))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Strength", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_HASTE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Haste", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_REGEN))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Regen", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_RESIST))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Resistance", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_VAMPIRE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Vampire", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_REFLECT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflect", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_PRECISION))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Precision", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_AGILITY))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Agility", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_KNOCKOUT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Knockout", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_KING))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "King", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_PLAGUE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Plague", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_RUNE_SUPERNOVA))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Supernova", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				if (pPlayer->InCond(TF_COND_POWERUPMODE_DOMINANT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Dominant", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				for (int i = 0; i < MAX_WEAPONS; i++)
				{
					auto pWeapon = pPlayer->GetWeaponFromSlot(i)->As<CTFSpellBook>();
					if (!pWeapon || pWeapon->GetWeaponID() != TF_WEAPON_SPELLBOOK || !pWeapon->m_iSpellCharges())
						continue;

					switch (pWeapon->m_iSelectedSpellIndex())
					{
					case 0:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Fireball", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 1:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bats", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 2:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Heal", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 3:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Pumpkins", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 4:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Jump", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 5:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Stealth", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 6:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Teleport", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 7:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Lightning", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 8:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Minify", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 9:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Meteors", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 10:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Monoculus", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 11:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Skeletons", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 12:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Glove", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 13:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Parachute", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 14:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Heal", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case 15:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bomb", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					}
				}

				if (pPlayer->InCond(TF_COND_RADIUSHEAL) ||
					pPlayer->InCond(TF_COND_HEALTH_BUFF) ||
					pPlayer->InCond(TF_COND_RADIUSHEAL_ON_DAMAGE) ||
					pPlayer->InCond(TF_COND_HALLOWEEN_QUICK_HEAL) ||
					pPlayer->InCond(TF_COND_HALLOWEEN_HELL_HEAL) ||
					pPlayer->InCond(TF_COND_KING_BUFFED))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "HP+", Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_HEALTH_OVERHEALED))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "HP", Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value);

				//if (pPlayer->InCond(TF_COND_BLASTJUMPING))
				//	tCache.m_vText.emplace_back(ESPTextEnum::Right, "Blastjump", Vars::Colors::IndicatorTextMid.Value, Vars::Menu::Theme::Background.Value);
			}

			// Debuffs
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Debuffs)
			{
				if (pPlayer->InCond(TF_COND_MARKEDFORDEATH) || pPlayer->InCond(TF_COND_MARKEDFORDEATH_SILENT))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Marked", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_URINE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Jarate", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_MAD_MILK))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Milk", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_STUNNED))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Stun", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_BURNING))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Burn", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_BLEEDING))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Bleed", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
			}

			// Misc
			if (Vars::ESP::Player.Value & Vars::ESP::PlayerEnum::Misc)
			{
				if (pPlayer->m_bFeignDeathReady())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "DR", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				else if (pPlayer->InCond(TF_COND_FEIGN_DEATH))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Feign", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				
				if (float flInvis = pPlayer->GetEffectiveInvisibilityLevel())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Invis {:.0f}%", flInvis * 100), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_DISGUISING) || pPlayer->InCond(TF_COND_DISGUISED))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Disguise", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (pPlayer->InCond(TF_COND_AIMING) || pPlayer->InCond(TF_COND_ZOOMED))
				{
					switch (pWeapon ? pWeapon->GetWeaponID() : -1)
					{
					case TF_WEAPON_MINIGUN:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Rev", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					case TF_WEAPON_SNIPERRIFLE:
					case TF_WEAPON_SNIPERRIFLE_CLASSIC:
					case TF_WEAPON_SNIPERRIFLE_DECAP:
					{
						if (bLocal)
						{
							auto pSniperRifle = pWeapon->As<CTFSniperRifle>();
							if (pSniperRifle)
								tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Charging {:.0f}%", Math::RemapVal(pSniperRifle->m_flChargedDamage(), 0.f, 150.f, 0.f, 100.f)), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
							break;
						}
						else
						{
							auto GetSniperDot = [](CBaseEntity* pEntity) -> CSniperDot*
								{
									for (auto pDot : H::Entities.GetGroup(EGroupType::MISC_DOTS))
									{
										if (pDot->m_hOwnerEntity().Get() == pEntity)
											return pDot->As<CSniperDot>();
									}
									return nullptr;
								};
							if (CSniperDot* pPlayerDot = GetSniperDot(pEntity))
							{
								float flChargeTime = std::max(SDK::AttribHookValue(3.f, "mult_sniper_charge_per_sec", pWeapon), 1.5f);
								tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Charging {:.0f}%", Math::RemapVal(TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick) - pPlayerDot->m_flChargeStartTime() - 0.3f, 0.f, flChargeTime, 0.f, 100.f)), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
								break;
							}
						}
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charging", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					}
					case TF_WEAPON_COMPOUND_BOW:
						if (bLocal)
						{
							auto pPipebombLauncher = pWeapon->As<CTFPipebombLauncher>();
							if (pPipebombLauncher)
								tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Charging {:.0f}%", Math::RemapVal(TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick) - pPipebombLauncher->m_flChargeBeginTime(), 0.f, 1.f, 0.f, 100.f)), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
							break;
						}
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charging", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
						break;
					default:
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charging", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
					}
				}

				if (pPlayer->InCond(TF_COND_SHIELD_CHARGE))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charge", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

				if (Vars::Visuals::Removals::Taunts.Value && pPlayer->InCond(TF_COND_TAUNTING))
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Taunt", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
			}
		}
	}
}

void CESP::StoreBuildings(CTFPlayer* pLocal)
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Buildings) || !Vars::ESP::Building.Value)
		return;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_ALL))
	{
		if (!pEntity || pEntity->IsDormant())
			continue;
		auto pBuilding = pEntity->As<CBaseObject>();
		if (!pBuilding)
			continue;
		auto pOwner = pBuilding->m_hBuilder().Get();
		int iIndex = pOwner ? pOwner->entindex() : -1;

		if (pOwner)
		{
			if (iIndex == I::EngineClient->GetLocalPlayer())
			{
				if (!(Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Local))
					continue;
			}
			else
			{
				if (!(Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Prioritized && F::PlayerUtils.IsPrioritized(iIndex))
					&& !(Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Friends && H::Entities.IsFriend(iIndex))
					&& !(Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Party && H::Entities.InParty(iIndex))
					&& !(pOwner->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Enemy : Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Team))
					continue;
			}
		}
		else if (!(pEntity->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Enemy : Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Team))
			continue;

		bool bIsMini = pBuilding->m_bMiniBuilding();

		BuildingCache& tCache = m_mBuildingCache[pEntity];
		tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
		tCache.m_tColor = GetColor(pLocal, pOwner ? pOwner : pEntity);
		tCache.m_bBox = Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Box;

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Distance)
		{
			Vec3 vDelta = pEntity->m_vecOrigin() - pLocal->m_vecOrigin();
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, std::format("[{:.0f}M]", CSIMDMath::FastLength2D(vDelta) / 41), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Name)
		{
			const char* szName = "Building";
			switch (pEntity->GetClassID())
			{
			case ETFClassID::CObjectSentrygun: szName = bIsMini ? "Mini-Sentry" : "Sentry"; break;
			case ETFClassID::CObjectDispenser: szName = "Dispenser"; break;
			case ETFClassID::CObjectTeleporter: szName = pBuilding->m_iObjectMode() ? "Teleporter Exit" : "Teleporter Entrance";
			}
			tCache.m_vText.emplace_back(ESPTextEnum::Top, szName, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		float flHealth = pBuilding->m_iHealth(), flMaxHealth = pBuilding->m_iMaxHealth();
		if (tCache.m_bHealthBar = Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::HealthBar)
			tCache.m_flHealth = std::clamp(flHealth / flMaxHealth, 0.f, 1.f);
		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::HealthText)
			tCache.m_vText.emplace_back(ESPTextEnum::Health, std::format("{}", flHealth), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Owner && !pBuilding->m_bWasMapPlaced() && pOwner)
		{
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(iIndex, &pi))
				tCache.m_vText.emplace_back(ESPTextEnum::Top, F::PlayerUtils.GetPlayerName(iIndex, pi.name), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Level && !bIsMini)
			tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("{} / {}", pBuilding->m_iUpgradeLevel(), bIsMini ? 1 : 3), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Building.Value & Vars::ESP::BuildingEnum::Flags)
		{
			float flConstructed = pBuilding->m_flPercentageConstructed();
			if (flConstructed < 1.f)
				tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("{:.0f}%", flConstructed * 100.f), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

			if (pBuilding->IsSentrygun())
			{
				auto pSentry = pBuilding->As<CObjectSentrygun>();
				if (pSentry && pSentry->m_bPlayerControlled())
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Wrangled", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
			}

			if (pBuilding->m_bHasSapper())
				tCache.m_vText.emplace_back(ESPTextEnum::Right, "Sapped", Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value);
			else if (pBuilding->m_bDisabled())
				tCache.m_vText.emplace_back(ESPTextEnum::Right, "Disabled", Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value);

			if (pBuilding->IsSentrygun() && !pBuilding->m_bBuilding())
			{
				auto pSentry = pBuilding->As<CObjectSentrygun>();
				if (pSentry)
				{
					int iShells, iMaxShells, iRockets, iMaxRockets;
					pSentry->GetAmmoCount(iShells, iMaxShells, iRockets, iMaxRockets);
					if (!iShells)
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "No ammo", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
					if (!bIsMini && !iRockets)
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "No rockets", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
				}
			}
		}
	}
}

static inline const char* GetProjectileName(CBaseEntity* pProjectile)
{
	const char* sReturn = "Projectile";
	switch (pProjectile->GetClassID())
	{
	case ETFClassID::CTFWeaponBaseMerasmusGrenade: sReturn = "Bomb"; break;
	case ETFClassID::CTFGrenadePipebombProjectile:
	{
		auto pSticky = pProjectile->As<CTFGrenadePipebombProjectile>();
		sReturn = (pSticky && pSticky->HasStickyEffects()) ? "Sticky" : "Pipe";
		break;
	}
	case ETFClassID::CTFStunBall: sReturn = "Baseball"; break;
	case ETFClassID::CTFBall_Ornament: sReturn = "Bauble"; break;
	case ETFClassID::CTFProjectile_Jar: sReturn = "Jarate"; break;
	case ETFClassID::CTFProjectile_Cleaver: sReturn = "Cleaver"; break;
	case ETFClassID::CTFProjectile_JarGas: sReturn = "Gas"; break;
	case ETFClassID::CTFProjectile_JarMilk:
	case ETFClassID::CTFProjectile_ThrowableBreadMonster: sReturn = "Milk"; break;
	case ETFClassID::CTFProjectile_SpellBats:
	case ETFClassID::CTFProjectile_SpellKartBats: sReturn = "Bats"; break;
	case ETFClassID::CTFProjectile_SpellMeteorShower: sReturn = "Meteor shower"; break;
	case ETFClassID::CTFProjectile_SpellMirv:
	case ETFClassID::CTFProjectile_SpellPumpkin: sReturn = "Pumpkin"; break;
	case ETFClassID::CTFProjectile_SpellSpawnBoss: sReturn = "Monoculus"; break;
	case ETFClassID::CTFProjectile_SpellSpawnHorde:
	case ETFClassID::CTFProjectile_SpellSpawnZombie: sReturn = "Skeleton"; break;
	case ETFClassID::CTFProjectile_SpellTransposeTeleport: sReturn = "Teleport"; break;
	case ETFClassID::CTFProjectile_Arrow:
	{
		auto pArrow = pProjectile->As<CTFProjectile_Arrow>();
		sReturn = (pArrow && pArrow->m_iProjectileType() == TF_PROJECTILE_BUILDING_REPAIR_BOLT) ? "Repair" : "Arrow";
		break;
	}
	case ETFClassID::CTFProjectile_GrapplingHook: sReturn = "Grapple"; break;
	case ETFClassID::CTFProjectile_HealingBolt: sReturn = "Heal"; break;
	case ETFClassID::CTFProjectile_Rocket:
	case ETFClassID::CTFProjectile_EnergyBall:
	case ETFClassID::CTFProjectile_SentryRocket: sReturn = "Rocket"; break;
	case ETFClassID::CTFProjectile_BallOfFire: sReturn = "Fire"; break;
	case ETFClassID::CTFProjectile_MechanicalArmOrb: sReturn = "Short circuit"; break;
	case ETFClassID::CTFProjectile_SpellFireball: sReturn = "Fireball"; break;
	case ETFClassID::CTFProjectile_SpellLightningOrb: sReturn = "Lightning"; break;
	case ETFClassID::CTFProjectile_SpellKartOrb: sReturn = "Fist"; break;
	case ETFClassID::CTFProjectile_Flare: sReturn = "Flare"; break;
	case ETFClassID::CTFProjectile_EnergyRing: sReturn = "Energy"; break;
	}
	return sReturn;
}
void CESP::StoreProjectiles(CTFPlayer* pLocal)
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Projectiles) || !Vars::ESP::Projectile.Value)
		return;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
	{
		if (!pEntity || pEntity->IsDormant())
			continue;
		auto pOwner = F::ProjSim.GetEntities(pEntity).second;
		int iIndex = pOwner ? pOwner->entindex() : -1;

		if (pOwner)
		{
			if (iIndex == I::EngineClient->GetLocalPlayer())
			{
				if (!(Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Local))
					continue;
			}
			else
			{
				if (!(Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Prioritized && F::PlayerUtils.IsPrioritized(iIndex))
					&& !(Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Friends && H::Entities.IsFriend(iIndex))
					&& !(Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Party && H::Entities.InParty(iIndex))
					&& !(pOwner->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Enemy : Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Team))
					continue;
			}
		}
		else if (!(pEntity->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Enemy : Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Team))
			continue;

		WorldCache& tCache = m_mWorldCache[pEntity];
		tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
		tCache.m_tColor = GetColor(pLocal, pOwner ? pOwner : pEntity);
		tCache.m_bBox = Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Box;

		if (Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Distance)
		{
			Vec3 vDelta = pEntity->m_vecOrigin() - pLocal->m_vecOrigin();
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, std::format("[{:.0f}M]", CSIMDMath::FastLength2D(vDelta) / 41), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		if (Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Name)
			tCache.m_vText.emplace_back(ESPTextEnum::Top, GetProjectileName(pEntity), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

		if (Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Owner && pOwner)
		{
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(iIndex, &pi))
				tCache.m_vText.emplace_back(ESPTextEnum::Top, F::PlayerUtils.GetPlayerName(iIndex, pi.name), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		if (Vars::ESP::Projectile.Value & Vars::ESP::ProjectileEnum::Flags)
		{
			switch (pEntity->GetClassID())
			{
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
				{
					auto pGrenade = pEntity->As<CTFWeaponBaseGrenadeProj>();
					if (pGrenade)
					{
						if (pGrenade->m_bCritical())
							tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crit", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
						if (pGrenade->m_iDeflected() && (pEntity->GetClassID() != ETFClassID::CTFGrenadePipebombProjectile || !pEntity->GetAbsVelocity().IsZero()))
							tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					}
					break;
				}
			case ETFClassID::CTFProjectile_Arrow:
			case ETFClassID::CTFProjectile_GrapplingHook:
			case ETFClassID::CTFProjectile_HealingBolt:
				{
					auto pArrow = pEntity->As<CTFProjectile_Arrow>();
					auto pRocket = pEntity->As<CTFBaseRocket>();
					if (pArrow && pArrow->m_bCritical())
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crit", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					if (pRocket && pRocket->m_iDeflected())
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					if (pArrow && pArrow->m_bArrowAlight())
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Alight", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					break;
				}
			case ETFClassID::CTFProjectile_Rocket:
			case ETFClassID::CTFProjectile_BallOfFire:
			case ETFClassID::CTFProjectile_MechanicalArmOrb:
			case ETFClassID::CTFProjectile_SentryRocket:
			case ETFClassID::CTFProjectile_SpellFireball:
			case ETFClassID::CTFProjectile_SpellLightningOrb:
			case ETFClassID::CTFProjectile_SpellKartOrb:
				{
					auto pRocket = pEntity->As<CTFBaseRocket>();
					auto pRocketTyped = pEntity->As<CTFProjectile_Rocket>();
					if (pRocket)
					{
						if (pRocketTyped && pRocketTyped->m_bCritical())
							tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crit", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
						if (pRocket->m_iDeflected())
							tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					}
					break;
				}
			case ETFClassID::CTFProjectile_EnergyBall:
				{
					auto pEnergyBall = pEntity->As<CTFProjectile_EnergyBall>();
					auto pRocket = pEntity->As<CTFBaseRocket>();
					if (pEnergyBall && pEnergyBall->m_bChargedShot())
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Charge", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					if (pRocket && pRocket->m_iDeflected())
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					break;
				}
			case ETFClassID::CTFProjectile_Flare:
				{
					auto pFlare = pEntity->As<CTFProjectile_Flare>();
					auto pRocket = pEntity->As<CTFBaseRocket>();
					if (pFlare && pFlare->m_bCritical())
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Crit", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					if (pRocket && pRocket->m_iDeflected())
						tCache.m_vText.emplace_back(ESPTextEnum::Right, "Reflected", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
					break;
				}
			}
		}
	}
}

void CESP::StoreObjective(CTFPlayer* pLocal)
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Objective) || !Vars::ESP::Objective.Value)
		return;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_OBJECTIVE))
	{
		if (!pEntity || pEntity->IsDormant())
			continue;
		if (!(pEntity->m_iTeamNum() != pLocal->m_iTeamNum() ? Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Enemy : Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Team))
			continue;

		WorldCache& tCache = m_mWorldCache[pEntity];
		tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
		tCache.m_tColor = GetColor(pLocal, pEntity);
		tCache.m_bBox = Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Box;

		if (Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Distance)
		{
			Vec3 vDelta = pEntity->m_vecOrigin() - pLocal->m_vecOrigin();
			tCache.m_vText.emplace_back(ESPTextEnum::Bottom, std::format("[{:.0f}M]", CSIMDMath::FastLength2D(vDelta) / 41), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
		}

		switch (pEntity->GetClassID())
		{
		case ETFClassID::CCaptureFlag:
		{
			auto pIntel = pEntity->As<CCaptureFlag>();
			if (!pIntel)
				break;

			if (Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Name)
				tCache.m_vText.emplace_back(ESPTextEnum::Top, "Intel", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);

			if (Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::Flags)
			{
				switch (pIntel->m_nFlagStatus())
				{
				case TF_FLAGINFO_HOME:
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Home", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
					break;
				case TF_FLAGINFO_DROPPED:
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Dropped", Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
					break;
				default:
					tCache.m_vText.emplace_back(ESPTextEnum::Right, "Stolen", Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value);
				}
			}

			if (Vars::ESP::Objective.Value & Vars::ESP::ObjectiveEnum::IntelReturnTime && pIntel->m_nFlagStatus() == TF_FLAGINFO_DROPPED)
			{
				float flReturnTime = std::max(pIntel->m_flResetTime() - TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick), 0.f);
				tCache.m_vText.emplace_back(ESPTextEnum::Right, std::format("Return {:.1f}s", pIntel->m_flResetTime() - TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick)).c_str(), Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value);
			}

			break;
		}
		}
	}
}

void CESP::StoreWorld()
{
	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::NPCs)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_NPC))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			WorldCache& tCache = m_mWorldCache[pEntity];
			tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
			tCache.m_tColor = Vars::Colors::NPC.Value;

			const char* szName = "NPC";
			switch (pEntity->GetClassID())
			{
			case ETFClassID::CEyeballBoss: szName = "Monoculus"; break;
			case ETFClassID::CHeadlessHatman: szName = "Horseless Headless Horsemann"; break;
			case ETFClassID::CMerasmus: szName = "Merasmus"; break;
			case ETFClassID::CTFBaseBoss: szName = "Boss"; break;
			case ETFClassID::CTFTankBoss: szName = "Tank"; break;
			case ETFClassID::CZombie: szName = "Skeleton"; break;
			}

			tCache.m_vText.emplace_back(ESPTextEnum::Top, szName, Vars::Colors::NPC.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Health)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_HEALTH))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			WorldCache& tCache = m_mWorldCache[pEntity];
			tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
			tCache.m_tColor = Vars::Colors::Health.Value;
			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Health", Vars::Colors::Health.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Ammo)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_AMMO))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			WorldCache& tCache = m_mWorldCache[pEntity];
			tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
			tCache.m_tColor = Vars::Colors::Ammo.Value;
			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Ammo", Vars::Colors::Ammo.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Money)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_MONEY))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			WorldCache& tCache = m_mWorldCache[pEntity];
			tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
			tCache.m_tColor = Vars::Colors::Money.Value;
			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Money", Vars::Colors::Money.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Powerups)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_POWERUP))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			WorldCache& tCache = m_mWorldCache[pEntity];
			tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
			tCache.m_tColor = Vars::Colors::Powerup.Value;

			const char* szName = "Powerup";
			switch (FNV1A::Hash32(I::ModelInfoClient->GetModelName(pEntity->GetModel())))
			{
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_agility.mdl"): szName = "Agility"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_crit.mdl"): szName = "Revenge"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_defense.mdl"): szName = "Resistance"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_haste.mdl"): szName = "Haste"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_king.mdl"): szName = "King"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_knockout.mdl"): szName = "Knockout"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_plague.mdl"): szName = "Plague"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_precision.mdl"): szName = "Precision"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_reflect.mdl"): szName = "Reflect"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_regen.mdl"): szName = "Regeneration"; break;
			//case FNV1A::Hash32Const("models/pickups/pickup_powerup_resistance.mdl"): szName = "11"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_strength.mdl"): szName = "Strength"; break;
			//case FNV1A::Hash32Const("models/pickups/pickup_powerup_strength_arm.mdl"): szName = "13"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_supernova.mdl"): szName = "Supernova"; break;
			//case FNV1A::Hash32Const("models/pickups/pickup_powerup_thorns.mdl"): szName = "15"; break;
			//case FNV1A::Hash32Const("models/pickups/pickup_powerup_uber.mdl"): szName = "16"; break;
			case FNV1A::Hash32Const("models/pickups/pickup_powerup_vampire.mdl"): szName = "Vampire";
			}
			tCache.m_vText.emplace_back(ESPTextEnum::Top, szName, Vars::Colors::Powerup.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Bombs)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_BOMBS))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			WorldCache& tCache = m_mWorldCache[pEntity];
			tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
			tCache.m_tColor = Vars::Colors::Halloween.Value;
			tCache.m_vText.emplace_back(ESPTextEnum::Top, pEntity->GetClassID() == ETFClassID::CTFPumpkinBomb ? "Pumpkin Bomb" : "Bomb", Vars::Colors::Halloween.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Spellbook)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::PICKUPS_SPELLBOOK))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			WorldCache& tCache = m_mWorldCache[pEntity];
			tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
			tCache.m_tColor = Vars::Colors::Halloween.Value;
			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Spellbook", Vars::Colors::Halloween.Value, Vars::Menu::Theme::Background.Value);
		}
	}

	if (Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Gargoyle)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_GARGOYLE))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			WorldCache& tCache = m_mWorldCache[pEntity];
			tCache.m_flAlpha = Vars::ESP::ActiveAlpha.Value / 255.f;
			tCache.m_tColor = Vars::Colors::Halloween.Value;
			tCache.m_vText.emplace_back(ESPTextEnum::Top, "Gargoyle", Vars::Colors::Halloween.Value, Vars::Menu::Theme::Background.Value);
		}
	}
}

void CESP::Draw()
{
	if (!Vars::ESP::Draw.Value)
		return;

	DrawWorld();
	DrawBuildings();
	DrawPlayers();
}

void CESP::DrawPlayers()
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Players) || !Vars::ESP::Player.Value)
		return;

	const auto& fFont = H::Fonts.GetFont(FONT_ESP);
	const int nTall = fFont.m_nTall + H::Draw.Scale(2);

	// Use index-based iteration to avoid dangling pointer issues
	int maxEntities = I::ClientEntityList->GetMaxEntities();
	for (int i = 0; i < maxEntities; i++)
	{
		auto pEntity = I::ClientEntityList->GetClientEntity(i);
		if (!pEntity)
			continue;

		// Convert to CBaseEntity for cache lookup
		auto pBaseEntity = pEntity->As<CBaseEntity>();
		if (!pBaseEntity)
			continue;

		// Skip if this entity isn't in our cache
		auto it = m_mPlayerCache.find(pBaseEntity);
		if (it == m_mPlayerCache.end())
			continue;

		auto& tCache = it->second;

		// Double-check entity is still valid after cache lookup
		if (pEntity->IsDormant())
			continue;

		auto pPlayer = pEntity->As<CTFPlayer>();
		if (!pPlayer || !pPlayer->IsAlive())
			continue;

		// Ensure entity index is still valid
		int iIndex = pPlayer->entindex();
		if (iIndex <= 0 || i != iIndex)
			continue;

		float x, y, w, h;
		if (!GetDrawBounds(pBaseEntity, x, y, w, h))
			continue;

		int l = x - H::Draw.Scale(6), r = x + w + H::Draw.Scale(6), m = x + w / 2;
		int t = y - H::Draw.Scale(5), b = y + h + H::Draw.Scale(5);
		int lOffset = 0, rOffset = 0, bOffset = 0, tOffset = 0;

		I::MatSystemSurface->DrawSetAlphaMultiplier(tCache.m_flAlpha);
		
		if (tCache.m_bBox)
			H::Draw.LineRectOutline(x, y, w, h, tCache.m_tColor, { 0, 0, 0, 255 });

		if (tCache.m_bBones)
		{
			// Skip bones if model is not ready or player is invalid
			if (!pPlayer->GetModel() || pPlayer->IsDormant())
				goto skip_bones;

			int nEntIndex = pPlayer->entindex();

			// Draw skeleton bones with safety checks
			matrix3x4 aBones[MAXSTUDIOBONES];
			if (pPlayer->SetupBones(aBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime))
			{
				int iHead = pPlayer->GetBaseToHitbox(HITBOX_HEAD);
				int iSpine2 = pPlayer->GetBaseToHitbox(HITBOX_SPINE2);
				int iPelvis = pPlayer->GetBaseToHitbox(HITBOX_PELVIS);
				int iLeftUpperarm = pPlayer->GetBaseToHitbox(HITBOX_LEFT_UPPERARM);
				int iLeftForearm = pPlayer->GetBaseToHitbox(HITBOX_LEFT_FOREARM);
				int iLeftHand = pPlayer->GetBaseToHitbox(HITBOX_LEFT_HAND);
				int iRightUpperarm = pPlayer->GetBaseToHitbox(HITBOX_RIGHT_UPPERARM);
				int iRightForearm = pPlayer->GetBaseToHitbox(HITBOX_RIGHT_FOREARM);
				int iRightHand = pPlayer->GetBaseToHitbox(HITBOX_RIGHT_HAND);
				int iLeftThigh = pPlayer->GetBaseToHitbox(HITBOX_LEFT_THIGH);
				int iLeftCalf = pPlayer->GetBaseToHitbox(HITBOX_LEFT_CALF);
				int iLeftFoot = pPlayer->GetBaseToHitbox(HITBOX_LEFT_FOOT);
				int iRightThigh = pPlayer->GetBaseToHitbox(HITBOX_RIGHT_THIGH);
				int iRightCalf = pPlayer->GetBaseToHitbox(HITBOX_RIGHT_CALF);
				int iRightFoot = pPlayer->GetBaseToHitbox(HITBOX_RIGHT_FOOT);

				// Validate hitbox indices before drawing
				if (iHead >= 0 && iSpine2 >= 0 && iPelvis >= 0)
				{
					DrawBones(pPlayer, aBones, std::vector<int>{ iHead, iSpine2, iPelvis }, tCache.m_tColor);
				}
				if (iSpine2 >= 0 && iLeftUpperarm >= 0 && iLeftForearm >= 0 && iLeftHand >= 0)
				{
					DrawBones(pPlayer, aBones, std::vector<int>{ iSpine2, iLeftUpperarm, iLeftForearm, iLeftHand }, tCache.m_tColor);
				}
				if (iSpine2 >= 0 && iRightUpperarm >= 0 && iRightForearm >= 0 && iRightHand >= 0)
				{
					DrawBones(pPlayer, aBones, std::vector<int>{ iSpine2, iRightUpperarm, iRightForearm, iRightHand }, tCache.m_tColor);
				}
				if (iPelvis >= 0 && iLeftThigh >= 0 && iLeftCalf >= 0 && iLeftFoot >= 0)
				{
					DrawBones(pPlayer, aBones, std::vector<int>{ iPelvis, iLeftThigh, iLeftCalf, iLeftFoot }, tCache.m_tColor);
				}
				if (iPelvis >= 0 && iRightThigh >= 0 && iRightCalf >= 0 && iRightFoot >= 0)
				{
					DrawBones(pPlayer, aBones, std::vector<int>{ iPelvis, iRightThigh, iRightCalf, iRightFoot }, tCache.m_tColor);
				}
			}
		}
		skip_bones:

		if (tCache.m_bHealthBar)
		{
			if (tCache.m_flHealth > 1.f)
			{
				Color_t cColor = Vars::Colors::IndicatorGood.Value;
				H::Draw.FillRectPercent(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, 1.f, cColor, { 0, 0, 0, 255 }, ALIGN_BOTTOM, true);

				cColor = Vars::Colors::IndicatorMisc.Value;
				H::Draw.FillRectPercent(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, tCache.m_flHealth - 1.f, cColor, { 0, 0, 0, 0 }, ALIGN_BOTTOM, true);
			}
			else
			{
				Color_t cColor = Vars::Colors::IndicatorBad.Value.Lerp(Vars::Colors::IndicatorGood.Value, tCache.m_flHealth);
				H::Draw.FillRectPercent(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, tCache.m_flHealth, cColor, { 0, 0, 0, 255 }, ALIGN_BOTTOM, true);
			}
			lOffset += H::Draw.Scale(6);
		}

		if (tCache.m_bUberBar)
		{
			H::Draw.FillRectPercent(x, y + h + H::Draw.Scale(4), w, H::Draw.Scale(2, Scale_Round), tCache.m_flUber, Vars::Colors::IndicatorMisc.Value);
			bOffset += H::Draw.Scale(6);
		}

		int iVerticalOffset = H::Draw.Scale(3, Scale_Floor) - 1;
		for (auto& [iMode, sText, tColor, tOutline] : tCache.m_vText)
		{
			switch (iMode)
			{
			case ESPTextEnum::Top:
				H::Draw.String(fFont, m, t - tOffset, tColor, ALIGN_BOTTOM, sText.c_str());
				tOffset += nTall;
				break;
			case ESPTextEnum::Bottom:
				H::Draw.String(fFont, m, b + bOffset, tColor, ALIGN_TOP, sText.c_str());
				bOffset += nTall;
				break;
			case ESPTextEnum::Right:
				H::Draw.String(fFont, r, t + iVerticalOffset + rOffset, tColor, ALIGN_TOPLEFT, sText.c_str());
				rOffset += nTall;
				break;
			case ESPTextEnum::Health:
				H::Draw.String(fFont, l - lOffset, t + iVerticalOffset + h - h * std::min(tCache.m_flHealth, 1.f), tOutline, ALIGN_TOPRIGHT, sText.c_str());
				break;
			case ESPTextEnum::Uber:
				H::Draw.String(fFont, r, y + h, tColor, ALIGN_TOPLEFT, sText.c_str());
			}
		}

		if (tCache.m_iClassIcon)
		{
			int size = H::Draw.Scale(18, Scale_Round);
			H::Draw.Texture(m, t - tOffset, size, size, tCache.m_iClassIcon - 1, ALIGN_BOTTOM);
		}

		if (tCache.m_pWeaponIcon)
		{
			float flW = tCache.m_pWeaponIcon->Width(), flH = tCache.m_pWeaponIcon->Height();
			float flScale = H::Draw.Scale(std::min((w + 40) / 2.f, 80.f) / std::max(flW, flH * 2));
			H::Draw.DrawHudTexture(m - flW / 2.f * flScale, b + bOffset, flScale, tCache.m_pWeaponIcon, Vars::Menu::Theme::Active.Value);
		}
	}

	I::MatSystemSurface->DrawSetAlphaMultiplier(1.f);
}

void CESP::DrawBuildings()
{
	if (!(Vars::ESP::Draw.Value & Vars::ESP::DrawEnum::Buildings) || !Vars::ESP::Building.Value)
		return;

	const auto& fFont = H::Fonts.GetFont(FONT_ESP);
	const int nTall = fFont.m_nTall + H::Draw.Scale(2);

	// Use index-based iteration to avoid dangling pointer issues
	int maxEntities = I::ClientEntityList->GetMaxEntities();
	for (int i = 0; i < maxEntities; i++)
	{
		auto pEntity = I::ClientEntityList->GetClientEntity(i);
		if (!pEntity)
			continue;

		// Convert to CBaseEntity for cache lookup
		auto pBaseEntity = pEntity->As<CBaseEntity>();
		if (!pBaseEntity)
			continue;

		// Skip if this entity isn't in our cache
		auto it = m_mBuildingCache.find(pBaseEntity);
		if (it == m_mBuildingCache.end())
			continue;

		auto& tCache = it->second;

		// Double-check entity is still valid after cache lookup
		if (pEntity->IsDormant())
			continue;

		auto pBuilding = pEntity->As<CBaseObject>();
		if (!pBuilding)
			continue;

		// Ensure entity index is still valid
		int iIndex = pBuilding->entindex();
		if (iIndex <= 0 || i != iIndex)
			continue;

		float x, y, w, h;
		if (!GetDrawBounds(pBaseEntity, x, y, w, h))
			continue;

		int l = x - H::Draw.Scale(6), r = x + w + H::Draw.Scale(6), m = x + w / 2;
		int t = y - H::Draw.Scale(5), b = y + h + H::Draw.Scale(5);
		int lOffset = 0, rOffset = 0, bOffset = 0, tOffset = 0;

		I::MatSystemSurface->DrawSetAlphaMultiplier(tCache.m_flAlpha);

		if (tCache.m_bBox)
			H::Draw.LineRectOutline(x, y, w, h, tCache.m_tColor, { 0, 0, 0, 255 });

		if (tCache.m_bHealthBar)
		{
			Color_t cColor = Vars::Colors::IndicatorBad.Value.Lerp(Vars::Colors::IndicatorGood.Value, tCache.m_flHealth);
			H::Draw.FillRectPercent(x - H::Draw.Scale(6), y, H::Draw.Scale(2, Scale_Round), h, tCache.m_flHealth, cColor, { 0, 0, 0, 255 }, ALIGN_BOTTOM, true);
			lOffset += H::Draw.Scale(6);
		}

		int iVerticalOffset = H::Draw.Scale(3, Scale_Floor) - 1;
		for (auto& [iMode, sText, tColor, tOutline] : tCache.m_vText)
		{
			switch (iMode)
			{
			case ESPTextEnum::Top:
				H::Draw.String(fFont, m, t - tOffset, tColor, ALIGN_BOTTOM, sText.c_str());
				tOffset += nTall;
				break;
			case ESPTextEnum::Bottom:
				H::Draw.String(fFont, m, b + bOffset, tColor, ALIGN_TOP, sText.c_str());
				bOffset += nTall;
				break;
			case ESPTextEnum::Right:
				H::Draw.String(fFont, r, t + iVerticalOffset + rOffset, tColor, ALIGN_TOPLEFT, sText.c_str());
				rOffset += nTall;
				break;
			case ESPTextEnum::Health:
				H::Draw.String(fFont, l - lOffset, t + iVerticalOffset + h - h * std::min(tCache.m_flHealth, 1.f), tOutline, ALIGN_TOPRIGHT, sText.c_str());
				break;
			}
		}
	}

	I::MatSystemSurface->DrawSetAlphaMultiplier(1.f);
}

void CESP::DrawWorld()
{
	// Early return if no world ESP options are enabled
	if (!(Vars::ESP::Draw.Value & (Vars::ESP::DrawEnum::NPCs | Vars::ESP::DrawEnum::Health | Vars::ESP::DrawEnum::Ammo | Vars::ESP::DrawEnum::Money | Vars::ESP::DrawEnum::Powerups | Vars::ESP::DrawEnum::Bombs | Vars::ESP::DrawEnum::Spellbook | Vars::ESP::DrawEnum::Gargoyle)))
		return;

	const auto& fFont = H::Fonts.GetFont(FONT_ESP);
	const int nTall = fFont.m_nTall + H::Draw.Scale(2);

	// Use index-based iteration to avoid dangling pointer issues
	int maxEntities = I::ClientEntityList->GetMaxEntities();
	for (int i = 0; i < maxEntities; i++)
	{
		auto pEntity = I::ClientEntityList->GetClientEntity(i);
		if (!pEntity)
			continue;

		// Convert to CBaseEntity for cache lookup
		auto pBaseEntity = pEntity->As<CBaseEntity>();
		if (!pBaseEntity)
			continue;

		// Skip if this entity isn't in our cache
		auto it = m_mWorldCache.find(pBaseEntity);
		if (it == m_mWorldCache.end())
			continue;

		auto& tCache = it->second;

		// Double-check entity is still valid after cache lookup
		if (pEntity->IsDormant())
			continue;

		// Ensure entity index is still valid
		int iIndex = pBaseEntity->entindex();
		if (iIndex <= 0 || i != iIndex)
			continue;

		float x, y, w, h;
		if (!GetDrawBounds(pBaseEntity, x, y, w, h))
			continue;

		int l = x - H::Draw.Scale(6), r = x + w + H::Draw.Scale(6), m = x + w / 2;
		int t = y - H::Draw.Scale(5), b = y + h + H::Draw.Scale(5);
		int lOffset = 0, rOffset = 0, bOffset = 0, tOffset = 0;

		I::MatSystemSurface->DrawSetAlphaMultiplier(tCache.m_flAlpha);

		if (tCache.m_bBox)
			H::Draw.LineRectOutline(x, y, w, h, tCache.m_tColor, { 0, 0, 0, 255 });

		int iVerticalOffset = H::Draw.Scale(3, Scale_Floor) - 1;
		for (auto& [iMode, sText, tColor, tOutline] : tCache.m_vText)
		{
			switch (iMode)
			{
			case ESPTextEnum::Top:
				H::Draw.String(fFont, m, t - tOffset, tColor, ALIGN_BOTTOM, sText.c_str());
				tOffset += nTall;
				break;
			case ESPTextEnum::Bottom:
				H::Draw.String(fFont, m, b + bOffset, tColor, ALIGN_TOP, sText.c_str());
				bOffset += nTall;
				break;
			case ESPTextEnum::Right:
				H::Draw.String(fFont, r, t + iVerticalOffset + rOffset, tColor, ALIGN_TOPLEFT, sText.c_str());
				rOffset += nTall;
			}
		}
	}

	I::MatSystemSurface->DrawSetAlphaMultiplier(1.f);
}

Color_t CESP::GetColor(CTFPlayer* pLocal, CBaseEntity* pEntity)
{
	if (!pEntity || !pLocal)
		return Vars::Colors::Enemy.Value;

	// Validate entity indices before using
	int iEntityIndex = pEntity->entindex();
	if (iEntityIndex <= 0)
		return Vars::Colors::Enemy.Value;

	// Only do player-specific checks if this is actually a player
	if (pEntity->As<CTFPlayer>())
	{
		if (iEntityIndex == I::EngineClient->GetLocalPlayer())
			return Vars::Colors::Local.Value;
		if (iEntityIndex == G::AimTarget.m_iEntIndex)
			return Vars::Colors::Target.Value;
	}
	return H::Color.GetTeamColor(pLocal->m_iTeamNum(), pEntity->m_iTeamNum(), Vars::Colors::Relative.Value);
}

bool CESP::GetDrawBounds(CBaseEntity* pEntity, float& x, float& y, float& w, float& h)
{
	if (!pEntity)
		return false;

	// Ensure entity has a valid model and is not being destroyed
	if (!pEntity->GetModel() || pEntity->IsDormant())
		return false;

	Vec3 vOrigin = pEntity->GetAbsOrigin();

	// Check for valid origin
	if (vOrigin.IsZero())
		return false;

	matrix3x4 mTransform = { { 1, 0, 0, vOrigin.x }, { 0, 1, 0, vOrigin.y }, { 0, 0, 1, vOrigin.z } };
	//if (pEntity->entindex() == I::EngineClient->GetLocalPlayer())
		Math::AngleMatrix({ 0.f, I::EngineClient->GetViewAngles().y, 0.f }, mTransform, false);

	float flLeft, flRight, flTop, flBottom;
	if (!SDK::IsOnScreen(pEntity, mTransform, &flLeft, &flRight, &flTop, &flBottom, true))
		return false;

	x = flLeft;
	y = flBottom;
	w = flRight - flLeft;
	h = flTop - flBottom;

	// Validate computed bounds
	if (w <= 0 || h <= 0 || w > 4096.f || h > 4096.f)
		return false;

	switch (pEntity->GetClassID())
	{
	case ETFClassID::CTFPlayer:
	case ETFClassID::CObjectSentrygun:
	case ETFClassID::CObjectDispenser:
	case ETFClassID::CObjectTeleporter:
		x += w * 0.125f;
		w *= 0.75f;
	}

	return !(x > H::Draw.m_nScreenW || x + w < 0 || y > H::Draw.m_nScreenH || y + h < 0);
}

void CESP::DrawBones(CTFPlayer* pPlayer, matrix3x4* aBones, std::vector<int> vecBones, Color_t clr)
{
	if (!pPlayer || !aBones || vecBones.empty())
		return;

	for (size_t n = 1; n < vecBones.size(); n++)
	{
		// Validate hitbox indices before using them
		if (vecBones[n] < 0 || vecBones[n - 1] < 0)
			continue;

		auto vBone1 = pPlayer->GetHitboxCenter(aBones, vecBones[n]);
		auto vBone2 = pPlayer->GetHitboxCenter(aBones, vecBones[n - 1]);

		// Check for valid hitbox positions
		if (vBone1.IsZero() || vBone2.IsZero())
			continue;

		Vec3 vScreen1, vScreen2;
		if (SDK::W2S(vBone1, vScreen1) && SDK::W2S(vBone2, vScreen2))
			H::Draw.Line(vScreen1.x, vScreen1.y, vScreen2.x, vScreen2.y, clr);
	}
}