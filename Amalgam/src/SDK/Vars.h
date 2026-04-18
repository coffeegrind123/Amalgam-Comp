#pragma once
#include "../SDK/Definitions/Types.h"
#include "../Utils/Macros/Macros.h"
#include <windows.h>
#include <unordered_map>
#include <map>
#include <typeinfo>

#define DEFAULT_BIND -1

template <class T>
class ConfigVar;

class BaseVar
{
public:
	std::vector<const char*> m_vNames;
	int m_iFlags = 0;
	union {
		int i = 0;
		float f;
	} m_unMin;
	union {
		int i = 0;
		float f;
	} m_unMax;
	union {
		int i = 0;
		float f;
	} m_unStep;
	std::vector<const char*> m_vValues = {};
	const char* m_sExtra = nullptr;

protected:
	std::string m_sName;
	const char* m_sSection;

public:
	constexpr const char* Name() const
	{
		return m_sName.c_str();
	}
	constexpr const char* Section() const
	{
		return m_sSection;
	}

public:
	size_t m_iType;

	template <class T>
	inline ConfigVar<T>* As()
	{
		if (typeid(T).hash_code() != m_iType)
			return nullptr;

		return reinterpret_cast<ConfigVar<T>*>(this);
	}
};

namespace G
{
	inline std::vector<BaseVar*> Vars = {};
};

template <class T>
class ConfigVar : public BaseVar
{
public:
	T Value;
	T Default;
	std::unordered_map<int, T> Map = {};

	ConfigVar(T tValue, std::vector<const char*> vNames, const char* sName, const char* sSection, int iFlags = 0, std::vector<const char*> vValues = {}, const char* sNone = nullptr)
	{
		Value = Default = Map[DEFAULT_BIND] = tValue;
		m_iType = typeid(T).hash_code();

		m_vNames = vNames;
		m_sName = std::string(sName).replace(strlen(sName) - 1, 1, "");
		m_sSection = sSection;

		m_iFlags = iFlags;
		m_vValues = vValues;
		m_sExtra = sNone;

		G::Vars.push_back(this);
	}
	ConfigVar(T tValue, std::vector<const char*> vNames, const char* sName, const char* sSection, int iFlags, int iMin, int iMax, int iStep = 1, const char* sFormat = "%i")
	{
		Value = Default = Map[DEFAULT_BIND] = tValue;
		m_iType = typeid(T).hash_code();

		m_vNames = vNames;
		m_sName = std::string(sName).replace(strlen(sName) - 1, 1, "");
		m_sSection = sSection;

		m_iFlags = iFlags;
		m_unMin.i = iMin;
		m_unMax.i = iMax;
		m_unStep.i = iStep;
		m_sExtra = sFormat;

		G::Vars.push_back(this);
	}
	ConfigVar(T tValue, std::vector<const char*> vNames, const char* sName, const char* sSection, int iFlags, float flMin, float flMax, float flStep = 1.f, const char* sFormat = "%g")
	{
		Value = Default = Map[DEFAULT_BIND] = tValue;
		m_iType = typeid(T).hash_code();

		m_vNames = vNames;
		m_sName = std::string(sName).replace(strlen(sName) - 1, 1, "");
		m_sSection = sSection;

		m_iFlags = iFlags;
		m_unMin.f = flMin;
		m_unMax.f = flMax;
		m_unStep.f = flStep;
		m_sExtra = sFormat;

		G::Vars.push_back(this);
	}

	inline T& operator[](int i)
	{
		return Map[i];
	}
	inline bool contains(int i) const
	{
		return Map.contains(i);
	}
};

#define NAMESPACE_BEGIN(name, ...) \
	namespace name { \
		constexpr inline const char* Section() { return !std::string(#__VA_ARGS__).empty() ? ""#__VA_ARGS__ : #name; }
#define NAMESPACE_END(name) \
	}

#define CVar(name, title, value, ...) \
	constexpr inline const char* name##_() { return __FUNCTION__; } \
	inline ConfigVar<decltype(value)> name = { value, { title }, name##_(), Section(), __VA_ARGS__ }
#define CVarValues(name, title, value, flags, none, ...) \
	constexpr inline const char* name##_() { return __FUNCTION__; } \
	inline ConfigVar<decltype(value)> name = { value, { title }, name##_(), Section(), flags, { __VA_ARGS__ }, none }
#define Enum(name, ...) \
	namespace name##Enum { enum name##Enum { __VA_ARGS__ }; }
#define CVarEnum(name, title, value, flags, none, values, ...) \
	CVarValues(name, title, value, flags, none, values); \
	Enum(name, __VA_ARGS__);

#define NONE 0
#define VISUAL (1 << 31)
#define NOSAVE (1 << 30)
#define NOBIND (1 << 29)
#define DEBUGVAR (1 << 28)

// flags to be automatically used in widgets. keep these as the same values as the flags in components, do not include visual flags
#define SLIDER_CLAMP (1 << 2)
#define SLIDER_MIN (1 << 3)
#define SLIDER_MAX (1 << 4)
#define SLIDER_PRECISION (1 << 5)
#define SLIDER_NOAUTOUPDATE (1 << 6)
#define DROPDOWN_MULTI (1 << 2)
#define DROPDOWN_MODIFIABLE (1 << 3)
#define DROPDOWN_NOSANITIZATION (1 << 4)
#define DROPDOWN_CUSTOM (1 << 2)
#define DROPDOWN_AUTOUPDATE (1 << 3)

NAMESPACE_BEGIN(Vars)
	NAMESPACE_BEGIN(Menu)
		CVar(CheatTitle, "Cheat title", std::string("Amalgam"), VISUAL | DROPDOWN_AUTOUPDATE);
		CVar(CheatTag, "Cheat tag", std::string("[Amalgam]"), VISUAL);
		CVar(PrimaryKey, "Primary key", VK_INSERT, NOBIND);
		CVar(SecondaryKey, "Secondary key", VK_F3, NOBIND);

		CVar(BindWindow, "Bind window", true);
		CVar(BindWindowTitle, "Bind window title", true);
		CVar(MenuShowsBinds, "Menu shows binds", false, NOBIND);

		CVarEnum(Indicators, "Indicators", 0b00000, VISUAL | DROPDOWN_MULTI, nullptr,
			VA_LIST("Ticks", "Crit hack", "Spectators", "Ping", "Conditions", "Seed prediction"),
			Ticks = 1 << 0, CritHack = 1 << 1, Spectators = 1 << 2, Ping = 1 << 3, Conditions = 1 << 4, SeedPrediction = 1 << 5);

		CVar(BindsDisplay, "Binds display", DragBox_t(100, 100), VISUAL | NOBIND);
		CVar(TicksDisplay, "Ticks display", DragBox_t(), VISUAL | NOBIND);
		CVar(CritsDisplay, "Crits display", DragBox_t(), VISUAL | NOBIND);
		CVar(SpectatorsDisplay, "Spectators display", DragBox_t(), VISUAL | NOBIND);
		CVar(PingDisplay, "Ping display", DragBox_t(), VISUAL | NOBIND);
		CVar(ConditionsDisplay, "Conditions display", DragBox_t(), VISUAL | NOBIND);
		CVar(SeedPredictionDisplay, "Seed prediction display", DragBox_t(), VISUAL | NOBIND);

		CVar(Scale, "Scale", 1.f, NOBIND | SLIDER_MIN | SLIDER_PRECISION | SLIDER_NOAUTOUPDATE, 0.75f, 2.f, 0.25f);
		CVar(CheapText, "Cheap text", false);

		NAMESPACE_BEGIN(Theme)
			CVar(Accent, "Accent color", Color_t(175, 150, 255, 255), VISUAL);
			CVar(Background, "Background color", Color_t(0, 0, 0, 250), VISUAL);
			CVar(Active, "Active color", Color_t(255, 255, 255, 255), VISUAL);
			CVar(Inactive, "Inactive color", Color_t(150, 150, 150, 255), VISUAL);
		NAMESPACE_END(Theme)
	NAMESPACE_END(Menu)

	NAMESPACE_BEGIN(Colors)
		CVar(FOVCircle, "FOV circle color", Color_t(255, 255, 255, 100), VISUAL);
		CVar(Local, "Local color", Color_t(255, 255, 255, 0), VISUAL);

		CVar(IndicatorGood, "Indicator good", Color_t(0, 255, 100, 255), NOSAVE | DEBUGVAR);
		CVar(IndicatorMid, "Indicator mid", Color_t(255, 200, 0, 255), NOSAVE | DEBUGVAR);
		CVar(IndicatorBad, "Indicator bad", Color_t(255, 0, 0, 255), NOSAVE | DEBUGVAR);
		CVar(IndicatorMisc, "Indicator misc", Color_t(75, 175, 255, 255), NOSAVE | DEBUGVAR);
		CVar(IndicatorTextGood, "Indicator text good", Color_t(150, 255, 150, 255), NOSAVE | DEBUGVAR);
		CVar(IndicatorTextMid, "Indicator text mid", Color_t(255, 200, 0, 255), NOSAVE | DEBUGVAR);
		CVar(IndicatorTextBad, "Indicator text bad", Color_t(255, 150, 150, 255), NOSAVE | DEBUGVAR);
		CVar(IndicatorTextMisc, "Indicator text misc", Color_t(100, 255, 255, 255), NOSAVE | DEBUGVAR);

		CVar(WorldModulation, VA_LIST("World modulation", "World modulation color"), Color_t(255, 255, 255, 255), VISUAL);
		CVar(SkyModulation, VA_LIST("Sky modulation", "Sky modulation color"), Color_t(255, 255, 255, 255), VISUAL);
		CVar(PropModulation, VA_LIST("Prop modulation", "Prop modulation color"), Color_t(255, 255, 255, 255), VISUAL);
		CVar(ParticleModulation, VA_LIST("Particle modulation", "Particle modulation color"), Color_t(255, 255, 255, 255), VISUAL);
		CVar(FogModulation, VA_LIST("Fog modulation", "Fog modulation color"), Color_t(255, 255, 255, 255), VISUAL);

		CVar(Line, "Line color", Color_t(255, 255, 255, 255), VISUAL);
		CVar(LineIgnoreZ, "Line ignore Z color", Color_t(255, 255, 255, 0), VISUAL);

		CVar(PlayerPath, "Player path color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(PlayerPathIgnoreZ, "Player path ignore Z color", Color_t(255, 255, 255, 255), VISUAL);
		CVar(ProjectilePath, "Projectile path color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(ProjectilePathIgnoreZ, "Projectile path ignore Z color", Color_t(255, 255, 255, 255), VISUAL);
		CVar(TrajectoryPath, "Trajectory path color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(TrajectoryPathIgnoreZ, "Trajectory path ignore Z color", Color_t(255, 255, 255, 255), VISUAL);
		CVar(ShotPath, "Shot path color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(ShotPathIgnoreZ, "Shot path ignore Z color", Color_t(255, 255, 255, 255), VISUAL);
		CVar(SplashRadius, "Splash radius color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(SplashRadiusIgnoreZ, "Splash radius ignore Z color", Color_t(255, 255, 255, 255), VISUAL);
		CVar(RealPath, "Real path color", Color_t(255, 255, 255, 0), NOSAVE | DEBUGVAR);
		CVar(RealPathIgnoreZ, "Real path ignore Z color", Color_t(255, 255, 255, 255), NOSAVE | DEBUGVAR);

		CVar(BoneHitboxEdge, "Bone hitbox edge color", Color_t(255, 255, 255, 255), VISUAL);
		CVar(BoneHitboxEdgeIgnoreZ, "Bone hitbox edge ignore Z color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(BoneHitboxFace, "Bone hitbox face color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(BoneHitboxFaceIgnoreZ, "Bone hitbox face ignore Z color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(TargetHitboxEdge, "Target hitbox edge color", Color_t(255, 150, 150, 255), VISUAL);
		CVar(TargetHitboxEdgeIgnoreZ, "Target hitbox edge ignore Z color", Color_t(255, 150, 150, 0), VISUAL);
		CVar(TargetHitboxFace, "Target hitbox face color", Color_t(255, 150, 150, 0), VISUAL);
		CVar(TargetHitboxFaceIgnoreZ, "Target hitbox face ignore Z color", Color_t(255, 150, 150, 0), VISUAL);
		CVar(BoundHitboxEdge, "Bound hitbox edge color", Color_t(255, 255, 255, 255), VISUAL);
		CVar(BoundHitboxEdgeIgnoreZ, "Bound hitbox edge ignore Z color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(BoundHitboxFace, "Bound hitbox face color", Color_t(255, 255, 255, 0), VISUAL);
		CVar(BoundHitboxFaceIgnoreZ, "Bound hitbox face ignore Z color", Color_t(255, 255, 255, 0), VISUAL);

		CVar(SpellFootstep, "Spell footstep color", Color_t(255, 255, 255, 255), VISUAL);
	NAMESPACE_END(Colors)

	NAMESPACE_BEGIN(Aimbot)
		NAMESPACE_BEGIN(General, Aimbot)
			CVarEnum(AimType, "Aim type", 0, NONE, nullptr,
				VA_LIST("Off", "Plain", "Smooth", "Silent"),
				Off, Plain, Smooth, Silent);
			CVarEnum(TargetSelection, "Target selection", 0, NONE, nullptr,
				VA_LIST("FOV", "Distance", "Hybrid"),
				FOV, Distance, Hybrid);
			CVarEnum(Target, "Target", 0b0000001, DROPDOWN_MULTI, nullptr,
				VA_LIST("Players", "Sentries", "Dispensers", "Teleporters", "Stickies", "NPCs", "Bombs"),
<<<<<<< HEAD
				Players = 1 << 0, Sentry = 1 << 1, Dispenser = 1 << 2, Teleporter = 1 << 3, Stickies = 1 << 4, NPCs = 1 << 5, Bombs = 1 << 6);
			CVarEnum(Ignore, "Ignore", 0b000000000, DROPDOWN_MULTI, nullptr,
				VA_LIST("Friends", "Party", "Invulnerable", "Cloaked", "Invisible", "Unsimulated players", "Dead Ringer", "Vaccinator", "Disguised", "Taunting"),
				Friends = 1 << 0, Party = 1 << 1, Invulnerable = 1 << 2, Cloaked = 1 << 3, Invisible = 1 << 4, Unsimulated = 1 << 5, DeadRinger = 1 << 6, Vaccinator = 1 << 7, Disguised = 1 << 8, Taunting = 1 << 9);
			CVar(AimFOV, "Aim FOV", 30.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 180.f);
			CVar(MaxTargets, "Max targets", 2, SLIDER_MIN, 1, 6);
			CVar(IgnoreCloak, "Ignore cloak", 100.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 10.f, "%g%%");
			CVar(IgnoreInvisible, "Ignore invisible", 100.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 10.f, "%g%%");
			CVar(SmoothStrength, "Smooth strength", 30.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.1f, 999.f, 1.f, "%g%%");
			CVar(AssistStrength, "Assist strength", 50.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 1.f, "%g%%");
			CVarEnum(CurveType, "Curve type", 3, NONE, nullptr,
				VA_LIST("Curve 0 (Medium)", "Curve 1 (Aggressive)", "Curve 2 (Slow)", "Natural Human"),
				Curve0, Curve1, Curve2, NaturalHuman);
			CVar(HumanizationJitter, "Humanization jitter", 5.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 20.f, 0.5f, "%g%%");
			CVar(MicroOvershootAmount, "Micro overshoot", 2.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 10.f, 0.5f, "%g%%");
			CVar(TickTolerance, "Tick tolerance", 7, SLIDER_CLAMP, 0, 21);
			CVar(AutoShoot, "Auto shoot", true);
			CVar(FOVCircle, "FOV Circle", false);
=======
				Players = 1 << 0, Sentry = 1 << 1, Dispenser = 1 << 2, Teleporter = 1 << 3, Stickies = 1 << 4, NPCs = 1 << 5, Bombs = 1 << 6,
				Building = Sentry | Dispenser | Teleporter);
			CVarEnum(Ignore, "Ignore", 0b00000001000, DROPDOWN_MULTI, nullptr,
				VA_LIST("Friends", "Party", "Unprioritized", "Invulnerable", "Invisible", "Unsimulated", "Dead ringer", "Vaccinator", "Disguised", "Taunting", "Team"),
				Friends = 1 << 0, Party = 1 << 1, Unprioritized = 1 << 2, Invulnerable = 1 << 3, Invisible = 1 << 4, Unsimulated = 1 << 5, DeadRinger = 1 << 6, Vaccinator = 1 << 7, Disguised = 1 << 8, Taunting = 1 << 9, Team = 1 << 10);
			CVar(AimFOV, "Aim FOV", 30.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 180.f);
			CVar(MaxTargets, "Max targets", 2, SLIDER_MIN, 1, 6);
			CVar(IgnoreInvisible, "Ignore invisible", 50.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 10.f, "%g%%");
			CVar(AssistStrength, "Assist strength", 25.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 1.f, "%g%%");
			CVar(TickTolerance, "Tick tolerance", 4, SLIDER_CLAMP, 0, 21);
			CVar(AutoShoot, "Auto shoot", true);
			CVar(FOVCircle, "FOV Circle", true, VISUAL);
>>>>>>> upstream/master
			CVar(NoSpread, "No spread", false);
			CVar(SwitchDelay, "Target switch delay", 100.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 2000.f, 0.1f, "%.1f ms");

			CVarEnum(AimHoldsFire, "Aim holds fire", 2, NOSAVE | DEBUGVAR, nullptr,
				VA_LIST("False", "Minigun only", "Always"),
				False, MinigunOnly, Always);
			CVar(NoSpreadOffset, "No spread offset", 0.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, -1.f, 1.f, 0.1f);
			CVar(NoSpreadAverage, "No spread average", 5, NOSAVE | DEBUGVAR | SLIDER_MIN, 1, 25);
			CVar(NoSpreadInterval, "No spread interval", 0.1f, NOSAVE | DEBUGVAR | SLIDER_MIN, 0.05f, 5.f, 0.1f, "%gs");
			CVar(NoSpreadBackupInterval, "No spread backup interval", 2.f, NOSAVE | DEBUGVAR | SLIDER_MIN, 2.f, 10.f, 0.1f, "%gs");
		NAMESPACE_END(Global)

		NAMESPACE_BEGIN(Hitscan)
			CVarEnum(Hitboxes, VA_LIST("Hitboxes", "Hitscan hitboxes"), 0b000111, DROPDOWN_MULTI, nullptr,
				VA_LIST("Head", "Body", "Pelvis", "Arms", "Legs", "##Divider", "Bodyaim if lethal", "Headshot only"),
				Head = 1 << 0, Body = 1 << 1, Pelvis = 1 << 2, Arms = 1 << 3, Legs = 1 << 4, BodyaimIfLethal = 1 << 5, HeadshotOnly = 1 << 6);
			CVarValues(MultipointHitboxes, "Multipoint hitboxes", 0b00000, DROPDOWN_MULTI, "All",
				VA_LIST("Head", "Body", "Pelvis", "Arms", "Legs"));
			CVarEnum(Modifiers, VA_LIST("Modifiers", "Hitscan modifiers"), 0b0100000, DROPDOWN_MULTI, nullptr,
				VA_LIST("Tapfire", "Wait for headshot", "Wait for charge", "Scoped only", "Auto scope", "Auto rev minigun", "Extinguish team"),
				Tapfire = 1 << 0, WaitForHeadshot = 1 << 1, WaitForCharge = 1 << 2, ScopedOnly = 1 << 3, AutoScope = 1 << 4, AutoRev = 1 << 5, ExtinguishTeam = 1 << 6);
<<<<<<< HEAD
			CVar(PointScale, "Point scale", 0.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 5.f, "%g%%");
			CVar(TapFireDist, "Tapfire distance", 1000.f, SLIDER_MIN | SLIDER_PRECISION, 250.f, 1000.f, 50.f);
=======
>>>>>>> upstream/master
			CVar(MultipointScale, "Multipoint scale", 0.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 5.f, "%g%%");
			CVar(TapfireDistance, "Tapfire distance", 1000.f, SLIDER_MIN | SLIDER_PRECISION, 250.f, 1000.f, 50.f);

			CVarEnum(PeekCheck, "Peek check", 1, NOSAVE | DEBUGVAR, nullptr,
				VA_LIST("Off", "Doubletap only", "Always"),
				Off, DoubletapOnly, Always);
			CVar(PeekAmount, "Peek amount", 1, NOSAVE | DEBUGVAR, 0, 5);
			CVar(BoneSizeSubtract, "Bone size subtract", 1.f, NOSAVE | DEBUGVAR | SLIDER_MIN, 0.f, 4.f, 0.25f);
			CVar(BoneSizeMinimumScale, "Bone size minimum scale", 1.f, NOSAVE | DEBUGVAR | SLIDER_CLAMP, 0.f, 1.f, 0.1f);
		NAMESPACE_END(HITSCAN)

		NAMESPACE_BEGIN(Projectile)
			CVarEnum(StrafePrediction, VA_LIST("Predict", "Strafe prediction"), 0b11, DROPDOWN_MULTI, "Off",
				VA_LIST("Air strafing", "Ground strafing"),
				Air = 1 << 0, Ground = 1 << 1);
			CVarEnum(SplashPrediction, VA_LIST("Splash", "Splash prediction"), 0, NONE, nullptr,
				VA_LIST("Off", "Include", "Prefer", "Only"),
				Off, Include, Prefer, Only);
			CVarEnum(AutoDetonate, "Auto detonate", 0b00, DROPDOWN_MULTI, "Off",
<<<<<<< HEAD
				VA_LIST("Stickies", "Flares", "##Divider", "Prevent self damage", "Ignore cloak", "Ignore invisible"),
				Stickies = 1 << 0, Flares = 1 << 1, PreventSelfDamage = 1 << 2, IgnoreCloak = 1 << 3, IgnoreInvisible = 1 << 4);
=======
				VA_LIST("Stickies", "Flares", "##Divider", "Prevent self damage", "Ignore invisible"),
				Stickies = 1 << 0, Flares = 1 << 1, PreventSelfDamage = 1 << 2, IgnoreInvisible = 1 << 3);
>>>>>>> upstream/master
			CVarEnum(AutoAirblast, "Auto airblast", 0b000, DROPDOWN_MULTI, "Off", // todo: implement advanced redirect!!
				VA_LIST("Enabled", "##Divider", "Redirect", "Ignore FOV"),
				Enabled = 1 << 0, Redirect = 1 << 1, IgnoreFOV = 1 << 2);
			CVarEnum(Hitboxes, VA_LIST("Hitboxes", "Projectile hitboxes"), 0b001111, DROPDOWN_MULTI, nullptr,
				VA_LIST("Auto", "##Divider", "Head", "Body", "Feet", "##Divider", "Bodyaim if lethal", "Prioritize feet"),
				Auto = 1 << 0, Head = 1 << 1, Body = 1 << 2, Feet = 1 << 3, BodyaimIfLethal = 1 << 4, PrioritizeFeet = 1 << 5);
			CVarEnum(Modifiers, VA_LIST("Modifiers", "Projectile modifiers"), 0b1010, DROPDOWN_MULTI, nullptr,
				VA_LIST("Charge weapon", "Cancel charge", "Use arm time"),
				ChargeWeapon = 1 << 0, CancelCharge = 1 << 1, UseArmTime = 1 << 2);
			CVar(MaxSimulationTime, "Max simulation time", 2.f, SLIDER_MIN | SLIDER_PRECISION, 0.1f, 2.5f, 0.25f, "%gs");
			CVar(HitChance, "Hit chance", 0.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 10.f, "%g%%");
			CVar(AutodetRadius, "Autodet radius", 90.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 10.f, "%g%%");
			CVar(SplashRadius, "Splash radius", 90.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 10.f, "%g%%");
			CVar(AutoRelease, "Auto release", 0.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 5.f, "%g%%");

			CVar(GroundSamples, "Samples", 33, NOSAVE | DEBUGVAR, 3, 66);
			CVar(GroundStraightFuzzyValue, "Straight fuzzy value", 100.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 500.f, 25.f);
			CVar(GroundLowMinimumSamples, "Low min samples", 16, NOSAVE | DEBUGVAR, 3, 66);
			CVar(GroundHighMinimumSamples, "High min samples", 33, NOSAVE | DEBUGVAR, 3, 66);
			CVar(GroundLowMinimumDistance, "Low min distance", 0.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 2500.f, 100.f);
			CVar(GroundHighMinimumDistance, "High min distance", 1000.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 2500.f, 100.f);
			CVar(GroundMaxChanges, "Max changes", 0, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0, 5);
			CVar(GroundMaxChangeTime, "Max change time", 0, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0, 66);

			CVar(AirSamples, "Samples", 33, NOSAVE | DEBUGVAR, 3, 66);
			CVar(AirStraightFuzzyValue, "Straight fuzzy value", 0.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 500.f, 25.f);
			CVar(AirLowMinimumSamples, "Low min samples", 16, NOSAVE | DEBUGVAR, 3, 66);
			CVar(AirHighMinimumSamples, "High min samples", 16, NOSAVE | DEBUGVAR, 3, 66);
			CVar(AirLowMinimumDistance, "Low min distance", 100000.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 2500.f, 100.f);
			CVar(AirHighMinimumDistance, "High min distance", 100000.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 2500.f, 100.f);
			CVar(AirMaxChanges, "Max changes", 2, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0, 5);
			CVar(AirMaxChangeTime, "Max change time", 16, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0, 66);

			CVar(VelocityAverageCount, "Velocity average count", 5, NOSAVE | DEBUGVAR, 1, 10);
			CVar(VerticalShift, "Vertical shift", 5.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f, 0.5f);
			CVar(DragOverride, "Drag override", 0.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 1.f, 0.01f);
			CVar(TimeOverride, "Time override", 0.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 2.f, 0.01f);
			CVar(HuntsmanLerp, "Huntsman lerp", 50.f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 1.f, "%g%%");
			CVar(HuntsmanLerpLow, "Huntsman lerp low", 100.f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 100.f, 1.f, "%g%%");
			CVar(HuntsmanAdd, "Huntsman add", 0.f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 20.f);
			CVar(HuntsmanAddLow, "Huntsman add low", 0.f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 20.f);
			CVar(HuntsmanClamp, "Huntsman clamp", 5.f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 10.f, 0.5f);
			CVar(HuntsmanPullPoint, "Huntsman pull point", false, NOSAVE | DEBUGVAR);
<<<<<<< HEAD
			CVar(SplashPointsDirect, "Direct splash points", 100, NOSAVE | DEBUGVAR | SLIDER_MIN, 1, 400, 5);
			CVar(SplashPointsArc, "Arc splash points", 100, NOSAVE | DEBUGVAR | SLIDER_MIN, 1, 400, 5);
			CVar(SplashGrates, "Splash grates", true, NOSAVE | DEBUGVAR);
			CVar(SplashRotateX, "Splash Rx", 0.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, -1.f, 360.f);
=======
			CVar(HuntsmanPullNoZ, "Pull no Z", false, NOSAVE | DEBUGVAR);

			CVar(SplashPointsDirect, "Splash points direct", 100, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0, 400, 5);
			CVar(SplashPointsArc, "Splash points arc", 100, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0, 400, 5);
			CVar(SplashCountDirect, "Splash count direct", 100, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 1, 400, 5);
			CVar(SplashCountArc, "Splash count arc", 5, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 1, 400, 5);
			CVar(SplashRotateX, "Splash Rx", -1.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, -1.f, 360.f);
>>>>>>> upstream/master
			CVar(SplashRotateY, "Splash Ry", -1.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, -1.f, 360.f);
			CVar(SplashTraceInterval, "Splash trace interval", 10, NOSAVE | DEBUGVAR, 1, 10);
			CVar(SplashNormalSkip, "Splash normal skip", 1, NOSAVE | DEBUGVAR | SLIDER_MIN, 1, 10);
			CVarEnum(SplashMode, "Splash mode", 0, NOSAVE | DEBUGVAR, nullptr,
				VA_LIST("Multi", "Single"),
				Multi, Single);
			CVarEnum(RocketSplashMode, "Rocket splash mode", 0, NOSAVE | DEBUGVAR, nullptr,
				VA_LIST("Regular", "Special light", "Special heavy"),
				Regular, SpecialLight, SpecialHeavy);
			CVar(SplashGrates, "Splash grates", true, NOSAVE | DEBUGVAR);
			CVar(Out2NormalMin, "Out2 normal min", -0.7f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, -1.f, 1.f, 0.1f);
			CVar(Out2NormalMax, "Out2 normal max", 0.7f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, -1.f, 1.f, 0.1f);

			CVar(DeltaCount, "Delta count", 5, NOSAVE | DEBUGVAR, 1, 5);
			CVarEnum(DeltaMode, "Delta mode", 0, NOSAVE | DEBUGVAR, nullptr,
				VA_LIST("Average", "Max"),
				Average, Max);
			CVarEnum(MovesimFrictionFlags, "Movesim friction flags", 0b01, NOSAVE | DEBUGVAR | DROPDOWN_MULTI, nullptr,
				VA_LIST("Run reduce", "Calculate increase"),
				RunReduce = 1 << 0, CalculateIncrease = 1 << 1);
		NAMESPACE_END(Projectile)

		NAMESPACE_BEGIN(Melee)
			CVar(AutoBackstab, "Auto backstab", true);
			CVar(IgnoreRazorback, "Ignore razorback", true);
			CVar(SwingPrediction, "Swing prediction", false);
			CVar(WhipTeam, "Whip team", false);

			CVar(SwingOffset, "Swing offset", -1, NOSAVE | DEBUGVAR, -1, 1);
			CVar(SwingPredictLag, "Swing predict lag", true, NOSAVE | DEBUGVAR);
			CVar(BackstabAccountPing, "Backstab account ping", true, NOSAVE | DEBUGVAR);
			CVar(BackstabDoubleTest, "Backstab double test", true, NOSAVE | DEBUGVAR);
		NAMESPACE_END(Melee)

		NAMESPACE_BEGIN(Healing)
			CVarEnum(HealPriority, "Heal Priority", 0, NONE, nullptr,
				VA_LIST("None", "Prioritize team", "Prioritize friends", "Friends only"),
				None, PrioritizeTeam, PrioritizeFriends, FriendsOnly);
			CVar(AutoHeal, "Auto heal", false);
			CVar(AutoArrow, "Auto arrow", false);
			CVar(AutoRepair, "Auto repair", false);
			CVar(AutoSandvich, "Auto sandvich", false);
			CVar(AutoVaccinator, "Auto vaccinator", false);
			CVar(ActivateOnVoice, "Activate on voice", false);

			CVar(AutoVaccinatorBulletScale, "Auto vaccinator bullet scale", 100.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 200.f, 10.f, "%g%%");
			CVar(AutoVaccinatorBlastScale, "Auto vaccinator blast scale", 100.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 200.f, 10.f, "%g%%");
			CVar(AutoVaccinatorFireScale, "Auto vaccinator fire scale", 100.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 200.f, 10.f, "%g%%");
			CVar(AutoVaccinatorFlamethrowerDamageOnly, "Auto vaccinator flamethrower damage only", false, NOSAVE | DEBUGVAR);
<<<<<<< HEAD
			CVar(AutoVaccinatorSwitchCooldown, "Auto vaccinator switch cooldown", 0.5f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.0f, 5.0f, 0.1f, "%g s");
			CVar(AutoVaccinatorHysteresis, "Auto vaccinator hysteresis", 20.0f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, 0.0f, 100.0f, 5.0f, "%g%%");
			CVar(AutoVaccinatorFOVRestriction, "Auto vaccinator FOV restriction", true, NOSAVE | DEBUGVAR);
			CVar(AutoVaccinatorFOV, "Auto vaccinator FOV", 90.0f, NOSAVE | DEBUGVAR | SLIDER_CLAMP | SLIDER_PRECISION, 0.0f, 180.0f, 5.0f, "%g°");
		SUBNAMESPACE_END(Healing);
	NAMESPACE_END(AIMBOT);
=======
		NAMESPACE_END(Healing)
	NAMESPACE_END(Aimbot)
>>>>>>> upstream/master
	
	NAMESPACE_BEGIN(CritHack, Crit Hack)
		CVar(ForceCrits, "Force crits", false);
		CVar(AvoidRandomCrits, "Avoid random crits", false);
		CVar(AlwaysMeleeCrit, "Always melee crit", false);
	NAMESPACE_END(CritHack)

	NAMESPACE_BEGIN(Backtrack)
		CVar(Latency, "Fake latency", 0, SLIDER_CLAMP, 0, 1000, 5);
		CVar(Interp, "Fake interp", 0, SLIDER_CLAMP | SLIDER_PRECISION, 0, 1000, 5);
		CVar(Window, VA_LIST("Window", "Backtrack window"), 185, SLIDER_CLAMP | SLIDER_PRECISION, 0, 200, 5);
		CVar(PreferOnShot, "Prefer on shot", false);
		CVar(CrosshairBacktrack, "Crosshair backtrack", false);

		CVar(Offset, "Offset", 0, NOSAVE | DEBUGVAR, -1, 1);
	NAMESPACE_END(Backtrack)

	NAMESPACE_BEGIN(Doubletap)
		CVar(Doubletap, "Doubletap", false);
		CVar(Warp, "Warp", false);
		CVar(RechargeTicks, "Recharge ticks", false);
		CVar(AntiWarp, "Anti-warp", true);
		CVar(TickLimit, "Tick limit", 22, SLIDER_CLAMP, 2, 22);
		CVar(WarpRate, "Warp rate", 22, SLIDER_CLAMP, 2, 22);
		CVar(RechargeLimit, "Recharge limit", 24, SLIDER_MIN, 1, 24);
		CVar(PassiveRecharge, "Passive recharge", 0, SLIDER_CLAMP, 0, 67);
	NAMESPACE_END(DoubleTap)

	NAMESPACE_BEGIN(Fakelag)
		CVarEnum(Fakelag, "Fakelag", 0, NONE, nullptr,
			VA_LIST("Off", "Plain", "Random", "Adaptive"),
			Off, Plain, Random, Adaptive);
		CVarEnum(Options, VA_LIST("Options", "Fakelag options"), 0b000, DROPDOWN_MULTI, nullptr,
			VA_LIST("Only moving", "On unduck", "Not airborne"),
			OnlyMoving = 1 << 0, OnUnduck = 1 << 1, NotAirborne = 1 << 2);
		CVar(PlainTicks, "Plain ticks", 12, SLIDER_CLAMP, 1, 22);
		CVar(RandomTicks, "Random ticks", IntRange_t(14, 18), SLIDER_CLAMP, 1, 22, 1, "%i - %i");
		CVar(UnchokeOnAttack, "Unchoke on attack", true);
		CVar(RetainBlastJump, "Retain blastjump", false);

		CVar(RetainSoldierOnly, "Retain blastjump soldier only", true, NOSAVE | DEBUGVAR);
	NAMESPACE_END(FakeLag)

	NAMESPACE_BEGIN(AutoPeek, Auto Peek)
		CVar(Enabled, VA_LIST("Enabled", "Auto peek"), false);
	NAMESPACE_END(AutoPeek)

	NAMESPACE_BEGIN(Speedhack)
		CVar(Enabled, VA_LIST("Enabled", "Speedhack enabled"), false);
		CVar(Amount, VA_LIST("Amount", "SpeedHack amount"), 1, NONE, 1, 50);
	NAMESPACE_END(Speedhack)

	NAMESPACE_BEGIN(AntiAim, Antiaim)
		CVar(Enabled, VA_LIST("Enabled", "Antiaim enabled"), false);
		CVarEnum(PitchReal, "Real pitch", 0, NONE, nullptr,
			VA_LIST("None", "Up", "Down", "Zero", "Jitter", "Reverse jitter"),
			None, Up, Down, Zero, Jitter, ReverseJitter);
		CVarEnum(PitchFake, "Fake pitch", 0, NONE, nullptr,
			VA_LIST("None", "Up", "Down", "Jitter", "Reverse jitter"),
			None, Up, Down, Jitter, ReverseJitter);
		Enum(Yaw, Forward, Left, Right, Backwards, Edge, Jitter, Spin);
		CVarValues(YawReal, "Real yaw", 0, NONE, nullptr,
			"Forward", "Left", "Right", "Backwards", "Edge", "Jitter", "Spin");
		CVarValues(YawFake, "Fake yaw", 0, NONE, nullptr,
			"Forward", "Left", "Right", "Backwards", "Edge", "Jitter", "Spin");
		Enum(YawMode, View, Target);
		CVarValues(RealYawBase, "Real base", 0, NONE, nullptr,
			"View", "Target");
		CVarValues(FakeYawBase, "Fake base", 0, NONE, nullptr,
			"View", "Target");
		CVar(RealYawOffset, "Real offset", 0.f, SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 5.f);
		CVar(FakeYawOffset, "Fake offset", 0.f, SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 5.f);
		CVar(RealYawValue, "Real value", 90.f, SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 5.f);
		CVar(FakeYawValue, "Fake value", -90.f, SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 5.f);
		CVar(SpinSpeed, "Spin speed", 15.f, SLIDER_PRECISION, -30.f, 30.f);
		CVar(MinWalk, "Minwalk", true);
		CVar(AntiOverlap, "Anti-overlap", false);
		CVar(InvalidShootPitch, "Hide pitch on shot", false);
	NAMESPACE_END(AntiAim)

	NAMESPACE_BEGIN(Resolver)
		CVar(Enabled, VA_LIST("Enabled", "Resolver enabled"), false);
		CVar(AutoResolve, "Auto resolve", false);
		CVar(AutoResolveCheatersOnly, "Auto resolve cheaters only", false);
		CVar(AutoResolveHeadshotOnly, "Auto resolve headshot only", false);
		CVar(AutoResolveYawAmount, "Auto resolve yaw", 90.f, SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 45.f);
		CVar(AutoResolvePitchAmount, "Auto resolve pitch", 90.f, SLIDER_CLAMP, -180.f, 180.f, 90.f);
		CVar(CycleYaw, "Cycle yaw", 0.f, SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 45.f);
		CVar(CyclePitch, "Cycle pitch", 0.f, SLIDER_CLAMP, -180.f, 180.f, 90.f);
		CVar(CycleView, "Cycle view", false);
		CVar(CycleMinwalk, "Cycle minwalk", false);
	NAMESPACE_END(Resolver)

	NAMESPACE_BEGIN(CheaterDetection, Cheater Detection)
		CVarEnum(Methods, "Detection methods", 0b0000, DROPDOWN_MULTI, nullptr,
			VA_LIST("Invalid pitch", "Packet choking", "Aim flicking", "Duck Speed"),
			InvalidPitch = 1 << 0, PacketChoking = 1 << 1, AimFlicking = 1 << 2, DuckSpeed = 1 << 3);
		CVar(DetectionsRequired, "Detections required", 10, SLIDER_MIN, 0, 50);
		CVar(MinimumChoking, "Minimum choking", 20, SLIDER_MIN, 4, 22);
		CVar(MinimumFlick, "Minimum flick angle", 20.f, SLIDER_PRECISION, 10.f, 30.f); // min flick size to suspect
		CVar(MaximumNoise, "Maximum flick noise", 1.f, SLIDER_PRECISION, 1.f, 10.f); // max difference between angles before and after flick
	NAMESPACE_END(CheaterDetection)

	NAMESPACE_BEGIN(ESP)
		CVarValues(ActiveGroups, "Active groups", int(0b11111111111111111111111111111111), VISUAL | DROPDOWN_MULTI | DROPDOWN_NOSANITIZATION, nullptr);
	NAMESPACE_END(ESP)

	NAMESPACE_BEGIN(Visuals)
		NAMESPACE_BEGIN(UI)
			CVarEnum(StreamerMode, "Streamer mode", 0, VISUAL, nullptr,
				VA_LIST("Off", "Local", "Friends", "Party", "All"),
				Off, Local, Friends, Party, All);
			CVarEnum(ChatTags, "Chat tags", 0b000, VISUAL | DROPDOWN_MULTI, nullptr,
				VA_LIST("Local", "Friends", "Party", "Assigned"),
				Local = 1 << 0, Friends = 1 << 1, Party = 1 << 2, Assigned = 1 << 3);
			CVar(FieldOfView, "Field of view## FOV", 0.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 160.f, 5.f);
			CVar(ZoomFieldOfView, "Zoomed field of view## Zoomed FOV", 0.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 160.f, 5.f);
			CVar(AspectRatio, "Aspect ratio", 0.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 5.f, 0.05f);
			CVar(RevealScoreboard, "Reveal scoreboard", false, VISUAL);
			CVar(ScoreboardUtility, "Scoreboard utility", false);
			CVar(ScoreboardColors, "Scoreboard colors", false, VISUAL);
			CVar(CleanScreenshots, "Clean screenshots", true);
		NAMESPACE_END(UI)

		NAMESPACE_BEGIN(Thirdperson)
			CVar(Enabled, "Thirdperson", false, VISUAL);
			CVar(Crosshair, VA_LIST("Crosshair", "Thirdperson crosshair"), false, VISUAL);
			CVar(Distance, "Thirdperson distance", 150.f, VISUAL | SLIDER_PRECISION, 0.f, 400.f, 10.f);
			CVar(Right, "Thirdperson right", 0.f, VISUAL | SLIDER_PRECISION, -100.f, 100.f, 5.f);
			CVar(Up, "Thirdperson up", 0.f, VISUAL | SLIDER_PRECISION, -100.f, 100.f, 5.f);

			CVar(Scale, "Thirdperson scales", true, NOSAVE | DEBUGVAR);
			CVar(Collide, "Thirdperson collides", true, NOSAVE | DEBUGVAR);
		NAMESPACE_END(ThirdPerson)

		NAMESPACE_BEGIN(Removals)
			CVar(Interpolation, VA_LIST("Interpolation", "Remove interpolation"), false);
			CVar(Lerp, VA_LIST("Lerp", "Remove lerp"), true);
			CVar(Disguises, VA_LIST("Disguises", "Remove disguises"), false, VISUAL);
			CVar(Taunts, VA_LIST("Taunts", "Remove taunts"), false, VISUAL);
			CVar(Scope, VA_LIST("Scope", "Remove scope"), false, VISUAL);
			CVar(PostProcessing, VA_LIST("Post processing", "Remove post processing"), false, VISUAL);
			CVar(ScreenOverlays, VA_LIST("Screen overlays", "Remove screen overlays"), false, VISUAL);
			CVar(ScreenEffects, VA_LIST("Screen effects", "Remove screen effects"), false, VISUAL);
			CVar(ViewPunch, VA_LIST("View punch", "Remove view punch"), false, VISUAL);
			CVar(AngleForcing, VA_LIST("Angle forcing", "Remove angle forcing"), false, VISUAL);
			CVar(Ragdolls, VA_LIST("Ragdolls", "Remove ragdoll"), false, VISUAL);
			CVar(Gibs, VA_LIST("Gibs", "Remove gibs"), false, VISUAL);
			CVar(MOTD, VA_LIST("MOTD", "Remove MOTD"), false, VISUAL);
		NAMESPACE_END(Removals)

		NAMESPACE_BEGIN(Effects)
			CVarValues(BulletTracer, "Bullet tracer", std::string("Default"), VISUAL | DROPDOWN_CUSTOM, nullptr,
				"Default", "None", "Big nasty", "Distortion trail", "Machina", "Sniper rail", "Short circuit", "C.A.P.P.E.R", "Merasmus ZAP", "Merasmus ZAP 2", "Black ink", "Line", "Line ignore Z", "Beam");
			CVarValues(CritTracer, "Crit tracer", std::string("Default"), VISUAL | DROPDOWN_CUSTOM, nullptr,
				"Default", "None", "Big nasty", "Distortion trail", "Machina", "Sniper rail", "Short circuit", "C.A.P.P.E.R", "Merasmus ZAP", "Merasmus ZAP 2", "Black ink", "Line", "Line ignore Z", "Beam");
			CVarValues(MedigunBeam, "Medigun beam", std::string("Default"), VISUAL | DROPDOWN_CUSTOM, nullptr,
				"Default", "None", "Uber", "Dispenser", "Passtime", "Bombonomicon", "White", "Orange");
			CVarValues(MedigunCharge, "Medigun charge", std::string("Default"), VISUAL | DROPDOWN_CUSTOM, nullptr,
				"Default", "None", "Electrocuted", "Halloween", "Fireball", "Teleport", "Burning", "Scorching", "Purple energy", "Green energy", "Nebula", "Purple stars", "Green stars", "Sunbeams", "Spellbound", "Purple sparks", "Yellow sparks", "Green zap", "Yellow zap", "Plasma", "Frostbite", "Time warp", "Purple souls", "Green souls", "Bubbles", "Hearts");
			CVarValues(ProjectileTrail, "Projectile trail", std::string("Default"), VISUAL | DROPDOWN_CUSTOM, nullptr,
				"Default", "None", "Rocket", "Critical", "Energy", "Charged", "Ray", "Fireball", "Teleport", "Fire", "Flame", "Sparks", "Flare", "Trail", "Health", "Smoke", "Bubbles", "Halloween", "Monoculus", "Sparkles", "Rainbow");
			CVarEnum(SpellFootsteps, "Spell footsteps", 0, VISUAL, nullptr,
				VA_LIST("Off", "Color", "Team", "Halloween"),
				Off, Color, Team, Halloween);
			CVarEnum(RagdollEffects, "Ragdoll effects", 0b000000, VISUAL | DROPDOWN_MULTI, nullptr,
				VA_LIST("Burning", "Electrocuted", "Ash", "Dissolve", "##Divider", "Gold", "Ice"),
				Burning = 1 << 0, Electrocuted = 1 << 1, Ash = 1 << 2, Dissolve = 1 << 3, Gold = 1 << 4, Ice = 1 << 5);
			CVar(DrawIconsThroughWalls, "Draw icons through walls", false, VISUAL);
			CVar(DrawDamageNumbersThroughWalls, "Draw damage numbers through walls", false, VISUAL);
		NAMESPACE_END(Tracers)

		NAMESPACE_BEGIN(Viewmodel)
			CVar(CrosshairAim, "Crosshair aim position", false, VISUAL);
			CVar(ViewmodelAim, "Viewmodel aim position", false, VISUAL);
			CVar(OffsetX, VA_LIST("Offset X", "Viewmodel offset X"), 0.f, VISUAL | SLIDER_PRECISION, -45.f, 45.f, 5.f);
			CVar(OffsetY, VA_LIST("Offset Y", "Viewmodel offset Y"), 0.f, VISUAL | SLIDER_PRECISION, -45.f, 45.f, 5.f);
			CVar(OffsetZ, VA_LIST("Offset Z", "Viewmodel offset Z"), 0.f, VISUAL | SLIDER_PRECISION, -45.f, 45.f, 5.f);
			CVar(Pitch, VA_LIST("Pitch", "Viewmodel pitch"), 0.f, VISUAL | SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 5.f);
			CVar(Yaw, VA_LIST("Yaw", "Viewmodel yaw"), 0.f, VISUAL | SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 5.f);
			CVar(Roll, VA_LIST("Roll", "Viewmodel roll"), 0.f, VISUAL | SLIDER_CLAMP | SLIDER_PRECISION, -180.f, 180.f, 5.f);
			CVar(SwayScale, VA_LIST("Sway scale", "Viewmodel sway scale"), 0.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 5.f, 0.5f);
			CVar(SwayInterp, VA_LIST("Sway interp", "Viewmodel sway interp"), 0.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 1.f, 0.1f);
		NAMESPACE_END(Viewmodel)

		NAMESPACE_BEGIN(World)
			CVarEnum(Modulations, "Modulations", 0b00000, VISUAL | DROPDOWN_MULTI, nullptr,
				VA_LIST("World", "Sky", "Prop", "Particle", "Fog"),
				World = 1 << 0, Sky = 1 << 1, Prop = 1 << 2, Particle = 1 << 3, Fog = 1 << 4);
			CVarValues(SkyboxChanger, "Skybox changer", std::string("Off"), VISUAL | DROPDOWN_CUSTOM, nullptr,
				VA_LIST("Off"));
			CVarValues(WorldTexture, "World texture", std::string("Default"), VISUAL | DROPDOWN_CUSTOM, nullptr,
				"Default", "Dev", "Camo", "Black", "White", "Gray", "Flat");
			CVar(NearPropFade, "Near prop fade", false, VISUAL);
			CVar(NoPropFade, "No prop fade", false, VISUAL);
		NAMESPACE_END(World)

		NAMESPACE_BEGIN(Beams) // as of now, these will stay out of the menu
			CVar(Model, "Model", std::string("sprites/physbeam.vmt"), VISUAL);
			CVar(Life, "Life", 2.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f);
			CVar(Width, "Width", 2.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f);
			CVar(EndWidth, "End width", 2.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f);
			CVar(FadeLength, "Fade length", 10.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 30.f);
			CVar(Amplitude, "Amplitude", 2.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f);
			CVar(Brightness, "Brightness", 255.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 255.f);
			CVar(Speed, "Speed", 0.2f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 5.f);
			CVar(Segments, "Segments", 2, VISUAL | SLIDER_MIN, 1, 10);
			CVar(Color, "Color", Color_t(255, 255, 255, 255), VISUAL);
			CVarEnum(Flags, "Flags", 0b10000000100000000, VISUAL | DROPDOWN_MULTI, nullptr,
				VA_LIST("Start entity", "End entity", "Fade in", "Fade out", "Sine noise", "Solid", "Shade in", "Shade out", "Only noise once", "No tile", "Use hitboxes", "Start visible", "End visible", "Is active", "Forever", "Halobeam", "Reverse"),
				StartEntity = 1 << 0, EndEntity = 1 << 1, FadeIn = 1 << 2, FadeOut = 1 << 3, SineNoise = 1 << 4, Solid = 1 << 5, ShadeIn = 1 << 6, ShadeOut = 1 << 7, OnlyNoiseOnce = 1 << 8, NoTile = 1 << 9, UseHitboxes = 1 << 10, StartVisible = 1 << 11, EndVisible = 1 << 12, IsActive = 1 << 13, Forever = 1 << 14, Halobeam = 1 << 15, Reverse = 1 << 16);
		NAMESPACE_END(Beams)

		NAMESPACE_BEGIN(Line)
			CVar(Enabled, "Line tracers", false, VISUAL);
			CVar(DrawDuration, VA_LIST("Draw duration", "Line draw duration"), 5.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f);
		NAMESPACE_END(Line)

		NAMESPACE_BEGIN(Hitbox)
			CVarEnum(BonesEnabled, VA_LIST("Bones enabled", "Hitbox bones enabled"), 0b00, VISUAL | DROPDOWN_MULTI, "Off",
				VA_LIST("On shot", "On hit"),
				OnShot = 1 << 0, OnHit = 1 << 1);
			CVarEnum(BoundsEnabled, VA_LIST("Bounds enabled", "Hitbox bounds enabled"), 0b000, VISUAL | DROPDOWN_MULTI, "Off",
				VA_LIST("On shot", "On hit", "Aim point"),
				OnShot = 1 << 0, OnHit = 1 << 1, AimPoint = 1 << 2);
			CVar(DrawDuration, VA_LIST("Draw duration", "Hitbox draw duration"), 5.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f);
		NAMESPACE_END(Hitbox)

		NAMESPACE_BEGIN(Simulation)
			Enum(Style, Off, Line, Separators, Spaced, Arrows, Boxes);
			CVarValues(PlayerPath, "Player path", 0, VISUAL, nullptr,
				"Off", "Line", "Separators", "Spaced", "Arrows", "Boxes");
			CVarValues(ProjectilePath, "Projectile path", 0, VISUAL, nullptr,
				"Off", "Line", "Separators", "Spaced", "Arrows", "Boxes");
			CVarValues(TrajectoryPath, "Trajectory path", 0, VISUAL, nullptr,
				"Off", "Line", "Separators", "Spaced", "Arrows", "Boxes");
			CVarValues(ShotPath, "Shot path", 0, VISUAL, nullptr,
				"Off", "Line", "Separators", "Spaced", "Arrows", "Boxes");
			CVarEnum(SplashRadius, "Splash radius", 0b0, VISUAL | DROPDOWN_MULTI, "Off",
				VA_LIST("Rockets", "Stickies", "Pipes", "Scorch shot", "##Divider", "Trace", "Sphere"),
				Rockets = 1 << 0, Stickies = 1 << 1, Pipes = 1 << 2, ScorchShot = 1 << 3, Trace = 1 << 4, Sphere = 1 << 5,
				Enabled = Rockets | Stickies | Pipes | ScorchShot);
			CVar(Timed, VA_LIST("Timed", "Timed path"), false, VISUAL);
			CVar(Box, VA_LIST("Box", "Path box"), true, VISUAL);
			CVar(ProjectileCamera, "Projectile camera", false, VISUAL);
			CVar(ProjectileWindow, "Projectile window", WindowBox_t(), VISUAL | NOBIND);
			CVar(SwingLines, "Swing lines", false, VISUAL);
			CVar(DrawDuration, VA_LIST("Draw duration", "Simulation draw duration"), 5.f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f);

			CVarValues(RealPath, "Real path", 0, NOSAVE | DEBUGVAR, nullptr,
				"Off", "Line", "Separators", "Spaced", "Arrows", "Boxes");
			CVar(SeparatorSpacing, "Separator spacing", 4, NOSAVE | DEBUGVAR, 1, 16);
			CVar(SeparatorLength, "Separator length", 12.f, NOSAVE | DEBUGVAR, 2.f, 16.f);
		NAMESPACE_END(Simulation)

		NAMESPACE_BEGIN(Trajectory)
			CVar(Override, "Simulation override", false, NOSAVE | DEBUGVAR);
			CVar(OffsetX, "Offset X", 16.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, -25.f, 25.f, 0.5f);
			CVar(OffsetY, "Offset Y", 8.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, -25.f, 25.f, 0.5f);
			CVar(OffsetZ, "Offset Z", -6.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, -25.f, 25.f, 0.5f);
			CVar(ForwardRedirect, "Forward redirect", 2000.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 2000.f, 100.f);
			CVar(ForwardCutoff, "Forward cutoff", 0.1f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 1.f, 0.1f);
			CVar(Hull, "Hull", 5.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f, 0.5f);
			CVar(Speed, "Speed", 1200.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 5000.f, 50.f);
			CVar(Gravity, "Gravity", 1.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 1.f, 0.1f);
			CVar(LifeTime, "Life time", 2.2f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 10.f, 0.1f);
			CVar(UpVelocity, "Up velocity", 200.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 1000.f, 50.f);
			CVar(AngularVelocityX, "Angular velocity X", 600.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, -1000.f, 1000.f, 50.f);
			CVar(AngularVelocityY, "Angular velocity Y", -1200.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, -1000.f, 1000.f, 50.f);
			CVar(AngularVelocityZ, "Angular velocity Z", 0.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, -1000.f, 1000.f, 50.f);
			CVar(Drag, "Drag", 1.f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 2.f, 0.1f);
			CVar(DragX, "Drag X", 0.003902f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 0.1f, 0.01f, "%.15g");
			CVar(DragY, "Drag Y", 0.009962f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 0.1f, 0.01f, "%.15g");
			CVar(DragZ, "Drag Z", 0.009962f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 0.1f, 0.01f, "%.15g");
			CVar(AngularDragX, "Angular drag X", 0.003618f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 0.1f, 0.01f, "%.15g");
			CVar(AngularDragY, "Angular drag Y", 0.001514f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 0.1f, 0.01f, "%.15g");
			CVar(AngularDragZ, "Angular drag Z", 0.001514f, NOSAVE | DEBUGVAR | SLIDER_PRECISION, 0.f, 0.1f, 0.01f, "%.15g");
			CVar(MaxVelocity, "Max velocity", 2000.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 4000.f, 50.f);
			CVar(MaxAngularVelocity, "Max angular velocity", 3600.f, NOSAVE | DEBUGVAR | SLIDER_MIN | SLIDER_PRECISION, 0.f, 7200.f, 50.f);
<<<<<<< HEAD
		SUBNAMESPACE_END(ProjectileTrajectory);
	NAMESPACE_END(Visuals);

	NAMESPACE_BEGIN(Competitive)
		SUBNAMESPACE_BEGIN(Features, Competitive Features)
			CVar(EnemyCam, "Enemy Camera", true, VISUAL);
			CVar(StickyCam, "Sticky Camera", true, VISUAL);
			CVar(PylonESP, "Medic Pylon ESP", true, VISUAL);
			CVar(FocusFire, "Focus Fire Detection", true, VISUAL);
			CVar(SentryESP, "Sentry Gun ESP", true, VISUAL);
			CVar(StickyESP, "Sticky Bomb ESP", true, VISUAL);
			CVar(SplashRadius, "Splash Radius Circles", true, VISUAL);
			CVar(CraterCheck, "Crater Check Indicator", false, VISUAL);
			CVar(PlayerTrails, "Player Movement Trails", true, VISUAL);
			CVar(CritHeals, "Critical Heal Indicators", true, VISUAL);
			CVar(ChatBubbles, "Chat Bubbles", true, VISUAL);
			CVar(ChatBubblesSteamIDColor, "Chat Bubbles SteamID Color", false, VISUAL);
			CVar(ChatBubblesNonFloat, "Chat Bubbles Non-Float", true, VISUAL);
			CVar(ChatBubblesEnemyOnly, "Chat Bubbles Enemy Only", true, VISUAL);
			CVar(ChatBubblesTextChat, "Chat Bubbles Text Chat", false, VISUAL);
			CVar(ChatBubblesVoiceSounds, "Chat Bubbles Voice Sounds", true, VISUAL);
			CVar(ChatBubblesReplaceMessage, "Replace Last Message", false, VISUAL);
			CVar(HealthBarESP, "Health Bar ESP", true, VISUAL);
			CVar(UberTracker, "Uber Advantage Tracker", true, VISUAL);
			CVar(MedicUberBar, "Medic Uber Bar ESP", true, VISUAL);
			CVar(AmmoTracker, "Ammo Pack Respawn ESP", true, VISUAL);
			CVar(MarkSpot, "Mark Spot Feature", true, VISUAL);
			CVar(OffScreenIndicators, "Off-Screen Player Indicators", false, VISUAL);
			CVar(ScoreboardRevealer, "Reveal Class Icons in Scoreboard", false, VISUAL);
			CVar(FlatTextures, "Flat Textures", false, VISUAL);
			CVar(SpectateAll, "Spectate All Players", true, VISUAL);
			CVar(OffScreenIndicatorsName, "Show player names", true, VISUAL);
			CVar(OffScreenIndicatorsClass, "Show player classes", true, VISUAL);
			CVar(OffScreenIndicatorsHealth, "Show player health", true, VISUAL);
			CVar(OffScreenIndicatorsDistance, "Show distance", false, VISUAL);
			CVar(OffScreenIndicatorsRange, "Detection range", 200.0f, VISUAL | SLIDER_MIN, 50.0f, 500.0f, 10.0f);
			CVar(OffScreenIndicatorsSize, "Arrow size", 15, VISUAL | SLIDER_CLAMP, 5, 30, 1);
			CVar(OffScreenIndicatorsAlpha, "Text transparency", 255, VISUAL | SLIDER_CLAMP, 100, 255, 5);
			CVar(OffScreenIndicatorsAvatars, "Use Steam avatars", true, VISUAL);
			CVar(MatchHUD, "Match HUD Enhancement", true, VISUAL);
			CVar(SafeBhop, "Safe Bunnyhop", false, VISUAL);
			CVar(DisableFreezeCam, "Disable Freezecam", true, VISUAL);
			CVar(NoHats, "Remove Hats/Cosmetics", true, VISUAL);
			CVar(HiderESP, "Hider ESP", true, VISUAL);
		SUBNAMESPACE_END(Features);

		SUBNAMESPACE_BEGIN(HiderESP, Hider ESP)
			CVar(ShowBoxes, "Show Boxes", true, VISUAL);
			CVar(ShowTracers, "Show Tracers", false, VISUAL);
			CVar(BoxColor, "Box Color", Color_t(255, 165, 0, 255), VISUAL);
			CVar(TracerColor, "Tracer Color", Color_t(0, 0, 255, 255), VISUAL);
			CVar(TimeToMark, "Time to Mark (seconds)", 5.5f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 20.0f, 0.5f);
			CVar(MovementThreshold, "Movement Threshold", 1.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.1f, 10.0f, 0.1f);
			CVar(MaxDistance, "Max Distance", 1650.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 100.0f, 5000.0f, 50.0f);
		SUBNAMESPACE_END(HiderESP);

		SUBNAMESPACE_BEGIN(EnemyCam, Enemy Camera)
			CVarEnum(Mode, "Camera mode", 0, VISUAL, nullptr,
				VA_LIST("Closest", "Healed", "Medic", "Top Score", "Random"),
				Closest, Healed, Medic, TopScore, Random);
			CVarEnum(ViewMode, "View mode", 1, VISUAL, nullptr,
				VA_LIST("Raw", "Offset"),
				Raw, Offset);
			CVar(TrackTime, "Track time", 3.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.0f, 10.0f, 0.5f);
			CVar(OffsetX, "Offset X (forward/back)", 50.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, -200.0f, 200.0f, 5.0f);
			CVar(OffsetY, "Offset Y (up/down)", 37.5f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, -200.0f, 200.0f, 5.0f);
			CVar(WindowX, "Window X position", -1, VISUAL | SLIDER_CLAMP, -1, 1920, 5);
			CVar(WindowY, "Window Y position", -1, VISUAL | SLIDER_CLAMP, -1, 1080, 5);
			CVar(WindowWidth, "Window width", 320, VISUAL | SLIDER_CLAMP, 160, 640, 10);
			CVar(WindowHeight, "Window height", 240, VISUAL | SLIDER_CLAMP, 120, 480, 10);
		SUBNAMESPACE_END(EnemyCam);

		SUBNAMESPACE_BEGIN(StickyCam, Sticky Camera)
			CVarEnum(Mode, "Camera mode", 0, VISUAL, nullptr,
				VA_LIST("Manual", "Follow Latest"),
				Manual, FollowLatest);
			CVarEnum(ViewMode, "View mode", 1, VISUAL, nullptr,
				VA_LIST("Raw", "Offset"),
				Raw, Offset);
			CVar(TrackTime, "Target lock time", 5.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.0f, 10.0f, 0.5f);
			CVar(OffsetX, "Offset X (forward/back)", 0.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, -200.0f, 200.0f, 5.0f);
			CVar(OffsetY, "Offset Y (up/down)", 35.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, -200.0f, 200.0f, 5.0f);
			CVar(WindowX, "Window X position", 5, VISUAL | SLIDER_CLAMP, -1, 1920, 5);
			CVar(WindowY, "Window Y position", 300, VISUAL | SLIDER_CLAMP, -1, 1080, 5);
			CVar(WindowWidth, "Window width", 500, VISUAL | SLIDER_CLAMP, 160, 640, 10);
			CVar(WindowHeight, "Window height", 300, VISUAL | SLIDER_CLAMP, 120, 480, 10);
			CVar(SearchRadius, "Target search radius", 584.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 100.0f, 1000.0f, 10.0f);
			CVar(CameraOffset, "Camera offset distance", 35.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 10.0f, 100.0f, 5.0f);
			CVar(AngleSpeed, "Angle interpolation speed", 0.1f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.01f, 1.0f, 0.01f);
			CVar(ShowWarnings, "Show range warnings", true, VISUAL);
			CVar(ShowOverlay, "Show camera overlay", true, VISUAL);
			CVar(AlwaysShow, "Always show camera (even without targets)", false, VISUAL);
			CVar(ShowChams, "Show chams on players in sticky radius", true, VISUAL);
		SUBNAMESPACE_END(StickyCam);

		SUBNAMESPACE_BEGIN(UberTracker, Uber Tracker)
			CVar(ShowTopBox, "Show info box", true, VISUAL);
			CVar(ShowAdvantage, "Show advantage text", true, VISUAL);
			CVar(ShowKritz, "Show kritzkrieg", true, VISUAL);
			CVar(IgnoreKritz, "Treat kritz as uber", false, VISUAL);
			CVar(MedDeathDuration, "Medic death timer duration", 3.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 10.0f, 0.5f);
			CVar(EvenThresholdRange, "Even uber threshold range", 5, VISUAL | SLIDER_CLAMP, 1, 20, 1);
			CVar(MidUberThreshold, "Mid uber threshold", 40, VISUAL | SLIDER_CLAMP, 20, 80, 5);
			CVar(HighUberThreshold, "High uber threshold", 70, VISUAL | SLIDER_CLAMP, 50, 99, 5);
			CVar(UberRate, "Uber build rate", 2.5f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 5.0f, 0.1f);
			CVar(KritzRate, "Kritz build rate", 3.125f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 5.0f, 0.1f);
			CVar(KritzDropThreshold, "Kritz drop threshold", 10.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 5.0f, 50.0f, 1.0f);
			CVar(BoxX, "Info box X position", 5, VISUAL | SLIDER_CLAMP, 0, 1920, 5);
			CVar(BoxY, "Info box Y position", 115, VISUAL | SLIDER_CLAMP, 0, 1080, 5);
			CVar(BoxWidth, "Info box width", 360, VISUAL | SLIDER_CLAMP, 200, 800, 10);
			CVar(BoxFontSize, "Info box font size", 14, VISUAL | SLIDER_CLAMP, 10, 24, 1);
			CVar(AdvantageX, "Advantage text X offset", 0, VISUAL | SLIDER_CLAMP, -500, 500, 5);
			CVar(AdvantageY, "Advantage text Y offset", 0, VISUAL | SLIDER_CLAMP, -500, 500, 5);
			CVar(AdvantageFontSize, "Advantage font size", 16, VISUAL | SLIDER_CLAMP, 10, 24, 1);
		SUBNAMESPACE_END(UberTracker);

		SUBNAMESPACE_BEGIN(HealthBarESP, Health Bar ESP)
			CVar(Alpha, "Transparency", 70, VISUAL | SLIDER_CLAMP, 0, 255, 5);
			CVar(BarHeight, "Bar height", 10, VISUAL | SLIDER_CLAMP, 5, 25, 1);
			CVar(BarWidth, "Bar width", 90, VISUAL | SLIDER_CLAMP, 50, 150, 5);
			CVar(MaxDistance, "Max distance", 3500.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 500.0f, 8000.0f, 100.0f);
			CVar(MedicMode, "Medic mode (teammates)", true, VISUAL);
			CVar(OverhealColor, "Overheal color", Color_t(71, 166, 255, 255), VISUAL);
			CVar(ShowHealthBars, "Show health bars", true, VISUAL);
			CVar(ShowThroughWalls, "Show bars through walls", false, VISUAL);
			CVar(ShowPolygons, "Show health polygons", false, VISUAL);
			CVar(PolygonThroughWalls, "Show polygons through walls", false, VISUAL);
			CVar(PolygonRadius, "Polygon radius", 50.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 20.0f, 200.0f, 5.0f);
			CVar(PolygonSegments, "Polygon segments", 16, VISUAL | SLIDER_CLAMP, 8, 64, 1);
			CVar(ShowPolygonFill, "Show polygon fill", true, VISUAL);
			CVar(ShowPolygonEdge, "Show polygon edge", true, VISUAL);
		SUBNAMESPACE_END(HealthBarESP);

		SUBNAMESPACE_BEGIN(MedicUberBar, Medic Uber Bar ESP)
			CVar(ShowFriendly, "Show friendly medics", true, VISUAL);
			CVar(ShowEnemy, "Show enemy medics", true, VISUAL);
			CVar(BarWidth, "Bar width", 90, VISUAL | SLIDER_CLAMP, 50, 150, 5);
			CVar(BarHeight, "Bar height", 10, VISUAL | SLIDER_CLAMP, 5, 25, 1);
			CVar(MaxDistance, "Max distance", 2000.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 500.0f, 5000.0f, 100.0f);
			CVar(Alpha, "Transparency", 200, VISUAL | SLIDER_CLAMP, 50, 255, 5);
			CVar(ShowThroughWalls, "Show through walls", false, VISUAL);
			CVar(ShowPercentage, "Show percentage text", true, VISUAL);
			CVar(LowUberColor, "Low uber color (0-33%)", Color_t(255, 0, 0, 255), VISUAL);
			CVar(MidUberColor, "Mid uber color (34-66%)", Color_t(255, 255, 0, 255), VISUAL);
			CVar(HighUberColor, "High uber color (67-99%)", Color_t(0, 255, 0, 255), VISUAL);
			CVar(FullUberColor, "Full uber color (100%)", Color_t(0, 255, 255, 255), VISUAL);
			CVar(DeadColor, "Dead medic color", Color_t(128, 128, 128, 255), VISUAL);
		SUBNAMESPACE_END(MedicUberBar);

		SUBNAMESPACE_BEGIN(PylonESP, Pylon ESP)
			CVar(PylonWidth, "Pylon width", 6, VISUAL | SLIDER_CLAMP, 2, 15, 1);
			CVar(PylonHeight, "Pylon height", 350.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 100.0f, 800.0f, 25.0f);
			CVar(MinDistance, "Min distance", 800.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 200.0f, 2000.0f, 50.0f);
			CVar(PylonColor, "Pylon color", Color_t(255, 0, 0, 255), VISUAL);
			CVar(PylonOffset, "Pylon offset", 35.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.0f, 100.0f, 5.0f);
			CVar(StartAlpha, "Start alpha", 200, VISUAL | SLIDER_CLAMP, 50, 255, 5);
			CVar(EndAlpha, "End alpha", 25, VISUAL | SLIDER_CLAMP, 0, 100, 5);
			CVar(Segments, "Segments", 10, VISUAL | SLIDER_CLAMP, 5, 20, 1);
		SUBNAMESPACE_END(PylonESP);

		SUBNAMESPACE_BEGIN(PlayerTrails, Player Trails)
			CVar(MaxTrailLength, "Max trail length", 19, VISUAL | SLIDER_CLAMP, 10, 50, 1);
			CVar(TrailLifetime, "Trail lifetime", 7.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 20.0f, 0.5f);
			CVar(MinDistance, "Min distance", 100.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 50.0f, 500.0f, 25.0f);
			CVar(MaxDistance, "Max distance", 1850.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1000.0f, 8000.0f, 100.0f);
			CVar(UpdateInterval, "Update interval", 6, VISUAL | SLIDER_CLAMP, 1, 20, 1);
			CVar(VisibilityTimeout, "Visibility timeout", 4.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 10.0f, 0.5f);
			CVar(MaxVisibleDuration, "Max visible duration", 2.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.5f, 10.0f, 0.5f);
			CVar(TrailHeight, "Trail height", 1.5f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.0f, 10.0f, 0.5f);
			CVar(MinMovementDistance, "Min movement distance", 10.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 5.0f, 50.0f, 1.0f);
		SUBNAMESPACE_END(PlayerTrails);

		SUBNAMESPACE_BEGIN(StickyESP, Sticky ESP)
			CVar(MaxDistance, "Max distance", 2800.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 500.0f, 5000.0f, 100.0f);
			CVar(BoxSize, "Box size", 20, VISUAL | SLIDER_CLAMP, 10, 50, 2);
			CVar(ShowTimer, "Show detonation timer", true, VISUAL);
			CVar(ShowDamage, "Show damage radius", true, VISUAL);
			CVar(EnemyOnly, "Enemy stickies only", true, VISUAL);
			CVar(Box3D, "3D boxes", false, VISUAL);
			CVar(Box2D, "2D boxes", true, VISUAL);
			CVar(BoxOnlyWhenVisible, "Only show when visible", true, VISUAL);
			CVar(EnemyVisibleColor, "Enemy visible color", Color_t(255, 100, 100, 255), VISUAL);
			CVar(EnemyInvisibleColor, "Enemy invisible color", Color_t(255, 0, 0, 255), VISUAL);
			CVar(TeamVisibleColor, "Team visible color", Color_t(100, 100, 255, 255), VISUAL);
			CVar(TeamInvisibleColor, "Team invisible color", Color_t(0, 0, 255, 255), VISUAL);
			CVar(EnableChams, "Enable chams", false, VISUAL);
		SUBNAMESPACE_END(StickyESP);

		SUBNAMESPACE_BEGIN(SentryESP, Sentry ESP)
			CVar(ShowLine, "Show aim line", true, VISUAL);
			CVar(ShowBoxes, "Show ESP boxes", true, VISUAL);
			CVar(ShowChams, "Show chams on targeting sentries", true, VISUAL);
			CVar(ShowThroughWalls, "Show ESP through walls", true, VISUAL);
			CVar(ShowFriendly, "Show friendly sentries", false, VISUAL);
			CVar(UseCorners, "Use corner boxes", true, VISUAL);
			CVar(CornerLength, "Corner length", 10, VISUAL | SLIDER_CLAMP, 5, 25, 1);
			CVar(BoxThickness, "Box thickness", 2, VISUAL | SLIDER_CLAMP, 1, 5, 1);
			CVar(LineLength, "Aim line length", 1100.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 200.0f, 2000.0f, 50.0f);
			CVar(SafeColor, "Safe color (visible)", Color_t(0, 255, 0, 125), VISUAL);
			CVar(DangerColor, "Danger color (targeting)", Color_t(255, 0, 0, 255), VISUAL);
			CVar(HiddenColor, "Hidden color (not visible)", Color_t(150, 150, 150, 125), VISUAL);
			CVar(LineColor, "Aim line color", Color_t(255, 0, 0, 255), VISUAL);
			CVar(TextColor, "Level text color", Color_t(255, 255, 255, 255), VISUAL);
		SUBNAMESPACE_END(SentryESP);

		SUBNAMESPACE_BEGIN(SplashRadius, Splash Radius)
			CVar(FillColor, "Fill color", Color_t(255, 255, 255, 25), VISUAL);
			CVar(EdgeColor, "Edge color", Color_t(255, 255, 255, 45), VISUAL);
			CVar(Segments, "Circle segments", 32, VISUAL | SLIDER_CLAMP, 8, 64, 4);
			CVar(ShowFill, "Show filled polygon", true, VISUAL);
			CVar(ShowEdge, "Show edge lines", true, VISUAL);
			CVar(EdgeWidth, "Edge width", 2, VISUAL | SLIDER_CLAMP, 1, 10, 1);
			CVar(EnemyOnly, "Enemy stickies only", true, VISUAL);
			CVar(ShowRockets, "Show rocket radius", true, VISUAL);
			CVar(ShowPipebombs, "Show pipebomb radius", true, VISUAL);
			CVar(ShowStickybombs, "Show stickybomb radius", true, VISUAL);
			CVar(MergeOverlapping, "Merge overlapping circles", true, VISUAL);
			CVar(UseLOD, "Use distance-based LOD", true, VISUAL);
			CVar(TeamColors, "Use team colors", true, VISUAL);
			CVar(ShowOnlyVisible, "Show only visible projectiles", true, VISUAL);
		SUBNAMESPACE_END(SplashRadius);

		SUBNAMESPACE_BEGIN(CraterCheck, Crater Check)
			CVar(Radius, "Crater radius", 100.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 50.0f, 500.0f, 25.0f);
			CVar(Segments, "Circle segments", 24, VISUAL | SLIDER_CLAMP, 8, 64, 4);
			CVar(ShowFill, "Show filled polygon", true, VISUAL);
			CVar(ShowEdge, "Show edge lines", true, VISUAL);
			CVar(EdgeWidth, "Edge width", 2, VISUAL | SLIDER_CLAMP, 1, 10, 1);
			CVar(OnlyShowLethal, "Only show lethal landings", true, VISUAL);
			CVar(LethalFillColor, "Lethal fill color", Color_t(255, 0, 0, 60), VISUAL);
			CVar(LethalEdgeColor, "Lethal edge color", Color_t(255, 0, 0, 150), VISUAL);
			CVar(SafeFillColor, "Safe fill color", Color_t(0, 255, 0, 60), VISUAL);
			CVar(SafeEdgeColor, "Safe edge color", Color_t(0, 255, 0, 150), VISUAL);
		SUBNAMESPACE_END(CraterCheck);

		SUBNAMESPACE_BEGIN(CritHeals, Crit Heals)
			CVar(Range, "Detection range", 800.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 200.0f, 1500.0f, 50.0f);
			CVar(FontSize, "Font size", 16, VISUAL | SLIDER_CLAMP, 10, 30, 1);
			CVar(CritColor, "Critical heal color", Color_t(0, 255, 0, 255), VISUAL);
			CVar(TextColor, "Text color", Color_t(255, 255, 255, 255), VISUAL);
			CVar(ShowPercentage, "Show heal percentage", true, VISUAL);
			CVar(CritHealTime, "Crit heal time", 10.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 5.0f, 30.0f, 1.0f);
			CVar(MaxOverhealMultiplier, "Max overheal multiplier", 1.5f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 2.0f, 0.1f);
			CVar(UberPenaltyThreshold, "Uber penalty threshold", 1.425f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 2.0f, 0.025f);
			CVar(BuffThreshold, "Buff threshold", 0.9f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.5f, 1.0f, 0.05f);
			CVar(BuffFadeThreshold, "Buff fade threshold", 0.33f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 0.1f, 1.0f, 0.05f);
			CVar(TriangleSize, "Triangle size", 12, VISUAL | SLIDER_CLAMP, 5, 30, 1);
			CVar(TriangleVerticalOffset, "Triangle vertical offset", 100, VISUAL | SLIDER_CLAMP, 50, 200, 5);
			CVar(OutlineThickness, "Outline thickness", 2, VISUAL | SLIDER_CLAMP, 1, 5, 1);
			CVar(FriendColor, "Friend color", Color_t(50, 255, 50, 255), VISUAL);
			CVar(EnemyColor, "Enemy color", Color_t(255, 50, 50, 255), VISUAL);
			CVar(OutlineColor, "Outline color", Color_t(0, 0, 0, 255), VISUAL);
			CVar(WarningColor, "Warning color", Color_t(255, 200, 0, 255), VISUAL);
			CVar(UberBuildWarning, "Uber build warning", true, VISUAL);
			CVar(ShowOnEnemies, "Show on enemies", false, VISUAL);
		SUBNAMESPACE_END(CritHeals);

		SUBNAMESPACE_BEGIN(FocusFire, Focus Fire)
			CVar(MaxTargets, "Max targets shown", 5, VISUAL | SLIDER_CLAMP, 1, 15, 1);
			CVar(ShowHealthDrop, "Show health drop", true, VISUAL);
			CVar(MinAttackers, "Min attackers required", 2, VISUAL | SLIDER_CLAMP, 1, 10, 1);
			CVar(TrackerTimeWindow, "Tracker time window", 4.5f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 1.0f, 10.0f, 0.5f);
			CVar(EnableChams, "Enable chams", true, VISUAL);
			CVar(EnableBox, "Enable box", true, VISUAL);
			CVar(VisibleOnly, "Visible only", false, VISUAL);
			CVar(Color, "Focus color", Color_t(255, 0, 0, 190), VISUAL);
			CVar(BoxThickness, "Box thickness", 2, VISUAL | SLIDER_CLAMP, 1, 5, 1);
			CVar(BoxPadding, "Box padding", 3, VISUAL | SLIDER_CLAMP, 0, 10, 1);
			CVar(UseCorners, "Use corners", true, VISUAL);
			CVar(CornerLength, "Corner length", 10, VISUAL | SLIDER_CLAMP, 5, 20, 1);
		SUBNAMESPACE_END(FocusFire);

		SUBNAMESPACE_BEGIN(SafeBhop, Safe Bunnyhop)
			CVar(SuccessRate, "Success rate", 95, VISUAL | SLIDER_CLAMP, 0, 100, 5);
			CVar(SafetyEnabled, "Safety enabled", true, VISUAL);
		SUBNAMESPACE_END(SafeBhop);

		SUBNAMESPACE_BEGIN(AmmoTracker, Ammo Tracker)
			CVar(ShowGroundPieChart, "Show ground pie charts", false, VISUAL);
			CVar(ShowScreenPieChart, "Show screen pie charts", true, VISUAL);
			CVar(ShowSeconds, "Show seconds timer", false, VISUAL);
			CVar(ShowMilliseconds, "Show milliseconds", false, VISUAL);
			CVar(ShowThroughWalls, "Show through walls", false, VISUAL);
			CVar(ShowHudOverlay, "Show HUD overlay when standing on supplies", true, VISUAL);
			CVar(ScaleWithDistance, "Scale with distance", true, VISUAL);
			CVar(ScaleTextWithDistance, "Scale text with distance", true, VISUAL);
			CVar(MaxDistanceGroundPieChart, "Max distance for ground charts", 1000.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 100.0f, 3000.0f, 50.0f);
			CVar(MaxDistanceScreenPieChart, "Max distance for screen charts", 1000.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 100.0f, 3000.0f, 50.0f);
			CVar(MaxDistanceText, "Max distance for text", 1000.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 100.0f, 3000.0f, 50.0f);
			CVar(PieChartSize, "Pie chart size", 20, VISUAL | SLIDER_CLAMP, 10, 50, 2);
			CVar(PieChartSegments, "Pie chart segments", 32, VISUAL | SLIDER_CLAMP, 8, 64, 4);
			CVar(MinScreenChartSize, "Min screen chart size", 5, VISUAL | SLIDER_CLAMP, 3, 15, 1);
			CVar(MaxScreenChartSize, "Max screen chart size", 20, VISUAL | SLIDER_CLAMP, 10, 50, 2);
			CVar(MaxScaleDistance, "Max scale distance", 1000.0f, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 500.0f, 2000.0f, 50.0f);
			CVar(TextSizeMin, "Min text size", 12, VISUAL | SLIDER_CLAMP, 8, 20, 1);
			CVar(TextSizeMax, "Max text size", 20, VISUAL | SLIDER_CLAMP, 14, 30, 1);
			CVar(TextSizeDefault, "Default text size", 16, VISUAL | SLIDER_CLAMP, 10, 24, 1);
			CVar(HealthColor, "Health pack color", Color_t(155, 200, 155, 200), VISUAL);
			CVar(AmmoColor, "Ammo pack color", Color_t(255, 255, 155, 200), VISUAL);
			CVar(SecondsColor, "Timer text color", Color_t(255, 255, 255, 255), VISUAL);
		SUBNAMESPACE_END(AmmoTracker);

		SUBNAMESPACE_BEGIN(MarkSpot, Mark Spot)
			CVar(ShowThroughWalls, "Show through walls", true, VISUAL);
			CVar(MarkDuration, "Mark duration (seconds)", 15.0f, VISUAL | SLIDER_CLAMP, 5.0f, 60.0f, 5.0f);
			CVar(RateLimit, "Rate limit (seconds)", 2.0f, VISUAL | SLIDER_CLAMP, 1.0f, 10.0f, 0.5f);
			CVar(CircleRadius, "Circle radius", 50, VISUAL | SLIDER_CLAMP, 10, 200, 5);
			CVar(CircleSegments, "Circle segments", 32, VISUAL | SLIDER_CLAMP, 8, 64, 4);
			CVar(CircleAlpha, "Circle alpha", 180, VISUAL | SLIDER_CLAMP, 50, 255, 5);
			CVar(PylonHeight, "Pylon height", 275.0f, VISUAL | SLIDER_CLAMP, 50.0f, 500.0f, 25.0f);
			CVar(PylonSegments, "Pylon segments", 20, VISUAL | SLIDER_CLAMP, 3, 20, 1);
			CVar(PylonWidth, "Pylon width", 2, VISUAL | SLIDER_CLAMP, 1, 10, 1);
			CVar(PylonStartAlpha, "Pylon start alpha", 120, VISUAL | SLIDER_CLAMP, 50, 255, 5);
			CVar(PylonEndAlpha, "Pylon end alpha", 20, VISUAL | SLIDER_CLAMP, 0, 200, 5);
			CVar(ShowOffScreenIndicators, "Off-screen indicators", true, VISUAL);
			CVar(ShowPulseRings, "Show pulse rings", false, VISUAL);
			CVar(PulseRingSpeed, "Pulse ring speed", 1.0f, VISUAL | SLIDER_CLAMP, 0.5f, 3.0f, 0.1f);
			CVar(PulseRingMaxRadius, "Pulse ring max radius", 150, VISUAL | SLIDER_CLAMP, 75, 300, 10);
			CVar(PulseRingAlpha, "Pulse ring alpha", 120, VISUAL | SLIDER_CLAMP, 50, 255, 5);
		SUBNAMESPACE_END(MarkSpot);
		
		SUBNAMESPACE_BEGIN(SpectateAll, Spectate All)
			CVar(EnemySpectate, "Enemy spectate", true, VISUAL);
			CVar(FreeCamera, "Free camera", false, VISUAL);
			CVar(HideSpectatedPlayer, "Hide spectated player model", true, VISUAL);
			CVar(HideSpectatedWeapons, "Hide spectated player weapons", false, VISUAL);
			CVar(ExcludeMapCameras, "Exclude map cameras", true, VISUAL);
			CVar(ExcludeObjectiveCameras, "Exclude objective cameras", true, VISUAL);
			CVar(AutoSwitchOnDeath, "Auto switch when spectated player dies", true, VISUAL);
			CVar(PreferKillerSpectate, "Prefer spectating killer after death", true, VISUAL);
			CVar(MouseLockAfterRespawn, "Lock mouse after respawn", true, VISUAL);
			CVar(MouseLockDuration, "Mouse lock duration (ms)", 500.0f, VISUAL | SLIDER_CLAMP, 100.0f, 2000.0f, 50.0f);
			CVar(SpyVision, "Show enemy health/names", true, VISUAL);
			CVar(CameraSpeed, "Camera speed", 5.0f, VISUAL | SLIDER_CLAMP, 1.0f, 20.0f, 0.5f);
			CVar(CameraDistance, "Camera distance", 150.0f, VISUAL | SLIDER_CLAMP, 50.0f, 500.0f, 10.0f);
		SUBNAMESPACE_END(SpectateAll);
	NAMESPACE_END(Competitive);

	NAMESPACE_BEGIN(Radar)
		SUBNAMESPACE_BEGIN(Main, Radar)
			CVar(Enabled, VA_LIST("Enabled", "Radar enabled"), false, VISUAL);
			CVar(DrawOutOfRange, "Draw out of range", true, VISUAL);
			CVarEnum(Style, VA_LIST("Style", "Radar style"), 0, VISUAL, nullptr,
				VA_LIST("Circle", "Rectangle"),
				Circle, Rectangle);
			CVar(Window, "Radar window", WindowBox_t(), NOBIND);
			CVar(Range, VA_LIST("Range", "Radar range"), 1500, VISUAL | SLIDER_MIN | SLIDER_PRECISION, 50, 3000, 50);
			CVar(BackgroundAlpha, VA_LIST("Background alpha", "Radar background alpha"), 128, VISUAL | SLIDER_CLAMP, 0, 255, 5);
			CVar(LineAlpha, VA_LIST("Line alpha", "Radar line alpha"), 255, VISUAL | SLIDER_CLAMP, 0, 255, 5);
		SUBNAMESPACE_END(Main);

		SUBNAMESPACE_BEGIN(Player, Player Radar)
			CVar(Enabled, VA_LIST("Enabled", "Radar player enabled"), false, VISUAL);
			CVar(Background, VA_LIST("Background", "Radar player background"), true, VISUAL);
			CVarEnum(Draw, VA_LIST("Draw", "Radar player draw"), 0b1001010, VISUAL | DROPDOWN_MULTI, nullptr,
				VA_LIST("Local", "Enemy", "Team", "Friends", "Party", "Prioritized", "Cloaked"),
				Local = 1 << 0, Enemy = 1 << 1, Team = 1 << 2, Prioritized = 1 << 3, Friends = 1 << 4, Party = 1 << 5, Cloaked = 1 << 6);
			CVarEnum(Icon, VA_LIST("Icon", "Radar player icon"), 1, VISUAL, nullptr,
				VA_LIST("Icons", "Portraits", "Avatar"),
				Icons, Portraits, Avatars);
			CVar(Size, VA_LIST("Size", "Radar player size"), 24, VISUAL, 12, 30, 2);
			CVar(Health, VA_LIST("Health bar", "Radar player health bar"), false, VISUAL);
			CVar(Height, VA_LIST("Height indicator", "Radar player height indicator"), false, VISUAL);
		SUBNAMESPACE_END(Players);

		SUBNAMESPACE_BEGIN(Building, Building Radar)
			CVar(Enabled, VA_LIST("Enabled", "Radar building enabled"), false, VISUAL);
			CVar(Background, VA_LIST("Background", "Radar building background"), true, VISUAL);
			CVarEnum(Draw, VA_LIST("Draw", "Radar building draw"), 0b001011, VISUAL | DROPDOWN_MULTI, nullptr,
				VA_LIST("Local", "Enemy", "Team", "Friends", "Party", "Prioritized"),
				Local = 1 << 0, Enemy = 1 << 1, Team = 1 << 2, Prioritized = 1 << 3, Friends = 1 << 4, Party = 1 << 5);
			CVar(Size, VA_LIST("Size", "Radar building size"), 18, VISUAL, 12, 30, 2);
			CVar(Health, VA_LIST("Health bar", "Radar building health bar"), false, VISUAL);
		SUBNAMESPACE_END(Buildings);

		SUBNAMESPACE_BEGIN(World, World Radar)
			CVar(Enabled, VA_LIST("Enabled", "Radar world enabled"), false, VISUAL);
			CVar(Background, VA_LIST("Background", "Radar world background"), true, VISUAL);
			CVarEnum(Draw, VA_LIST("Draw", "Radar world draw"), 0b0000011, VISUAL | DROPDOWN_MULTI, nullptr,
				VA_LIST("Health", "Ammo", "Money", "Bombs", "Powerup", "Spellbook", "Gargoyle"),
				Health = 1 << 0, Ammo = 1 << 1, Money = 1 << 2, Bombs = 1 << 3, Powerup = 1 << 4, Spellbook = 1 << 5, Gargoyle = 1 << 6);
			CVar(Size, VA_LIST("Size", "Radar world size"), 14, VISUAL, 12, 30, 2);
		SUBNAMESPACE_END(World);
	NAMESPACE_END(Radar);
=======
		NAMESPACE_END(ProjectileTrajectory)
	NAMESPACE_END(Visuals)
>>>>>>> upstream/master

	NAMESPACE_BEGIN(Misc)
		NAMESPACE_BEGIN(Movement)
			CVarEnum(AutoStrafe, "Auto strafe", 0, NONE, nullptr,
				VA_LIST("Off", "Legit", "Directional"),
				Off, Legit, Directional);
			CVar(AutoStrafeTurnScale, "Auto strafe turn scale", 0.5f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 1.f, 0.1f);
			CVar(AutoStrafeMaxDelta, "Auto strafe max delta", 180.f, SLIDER_CLAMP | SLIDER_PRECISION, 0.f, 180.f, 5.f);
			CVar(Bunnyhop, "Bunnyhop", false);
			CVar(EdgeJump, "Edge jump", false);
			CVar(AutoJumpbug, "Auto jumpbug", false);
			CVar(NoPush, "No push", false);
			CVar(AutoRocketJump, "Auto rocket jump", false);
			CVar(AutoCTap, "Auto ctap", false);
			CVar(FastStop, "Fast stop", false);
			CVar(FastAccelerate, "Fast accelerate", false);
			CVar(DuckSpeed, "Duck speed", false);
			CVar(MovementLock, "Movement lock", false);
			CVar(BreakJump, "Break jump", false);
			CVar(ShieldTurnRate, "Shield turn rate", false);

			CVar(AutoRocketJumpChokeGrounded, "Choke grounded", 1, NOSAVE | DEBUGVAR, 0, 3);
			CVar(AutoRocketJumpChokeAir, "Choke air", 1, NOSAVE | DEBUGVAR, 0, 3);
			CVar(AutoRocketJumpSkipGround, "Skip grounded", 0, NOSAVE | DEBUGVAR, 0, 3);
			CVar(AutoRocketJumpSkipAir, "Skip air", 1, NOSAVE | DEBUGVAR, 0, 3);
			CVar(AutoRocketJumpTimingOffset, "Timing offset", 0, NOSAVE | DEBUGVAR, 0, 3);
			CVar(AutoRocketJumpApplyAbove, "Apply offset above", 0, NOSAVE | DEBUGVAR, 0, 10);
		NAMESPACE_END(Movement)

		NAMESPACE_BEGIN(Automation)
			CVarEnum(AntiBackstab, "Anti-backstab", 0, NONE, nullptr,
				VA_LIST("Off", "Yaw", "Pitch", "Fake"),
				Off, Yaw, Pitch, Fake);
			CVar(AntiAFK, "Anti-AFK", false);
			CVar(AntiAutobalance, "Anti-autobalance", false);
			CVar(TauntControl, "Taunt control", false);
			CVar(KartControl, "Kart control", false);
			CVar(AutoF2Ignored, "Auto F2 ignored", false);
			CVar(AutoF1Priority, "Auto F1 priority", false);
			CVar(AcceptItemDrops, "Auto accept item drops", false);
		NAMESPACE_END(Automation)

<<<<<<< HEAD
		SUBNAMESPACE_BEGIN(AutoDisguise)
			CVar(Enabled, "Auto disguise", false);
		SUBNAMESPACE_END(AutoDisguise);

		SUBNAMESPACE_BEGIN(Sound)
			CVarEnum(Block, VA_LIST("Block", "Sound block"), 0b0000, DROPDOWN_MULTI, nullptr,
				VA_LIST("Footsteps", "Noisemaker", "Frying pan", "Water"),
				Footsteps = 1 << 0, Noisemaker = 1 << 1, FryingPan = 1 << 2, Water = 1 << 3);
			CVar(HitsoundAlways, "Hitsound always", false);
			CVar(GiantWeaponSounds, "Giant weapon sounds", false);
		SUBNAMESPACE_END(Sound);


		SUBNAMESPACE_BEGIN(Game)
			CVar(AntiCheatCompatibility, "Anti-cheat compatibility", true);
=======
		NAMESPACE_BEGIN(Exploits)
			CVar(PureBypass, "Pure bypass", false);
			CVar(CheatsBypass, "Cheats bypass", false);
			CVar(UnlockCVars, "Unlock CVars", false);
			CVar(EquipRegionUnlock, "Equip region unlock", false);
			CVar(BackpackExpander, "Backpack expander", false);
			CVar(NoisemakerSpam, "Noisemaker spam", false);
			CVar(PingReducer, "Ping reducer", false);
			CVar(PingTarget, "cl_cmdrate", 1, SLIDER_CLAMP, 1, 66);
		NAMESPACE_END(Exploits)

		NAMESPACE_BEGIN(Game)
			CVar(AntiCheatCompatibility, "Anti-cheat compatibility", false);
>>>>>>> upstream/master
			CVar(F2PChatBypass, "F2P chat bypass", false);
			CVar(NetworkFix, "Network fix", false);
			CVar(SetupBonesOptimization, "Bones optimization", false);

			CVar(AntiCheatCritHack, "Anti-cheat crit hack", false, NOSAVE | DEBUGVAR);
		NAMESPACE_END(Game)

		NAMESPACE_BEGIN(Queueing)
			CVarEnum(ForceRegions, "Force regions", 0b0, DROPDOWN_MULTI, nullptr, // i'm not sure all of these are actually used for tf2 servers
				VA_LIST("Atlanta", "Chicago", "Dallas", "Los Angeles", "Seattle", "Virginia", "##Divider", "Amsterdam", "Falkenstein", "Frankfurt", "Helsinki", "London", "Madrid", "Paris", "Stockholm", "Vienna", "Warsaw", "##Divider", "Buenos Aires", "Lima", "Santiago", "Sao Paulo", "##Divider", "Chennai", "Dubai", "Hong Kong", "Mumbai", "Seoul", "Singapore", "Tokyo", "##Divider", "Sydney", "##Divider", "Johannesburg"),
				// North America
				ATL = 1 << 0, // Atlanta
				ORD = 1 << 1, // Chicago
				DFW = 1 << 2, // Dallas
				LAX = 1 << 3, // Los Angeles
				SEA = 1 << 4, // Seattle (+DC_EAT?)
				IAD = 1 << 5, // Virginia
				// Europe
				AMS = 1 << 6, // Amsterdam
				FSN = 1 << 7, // Falkenstein
				FRA = 1 << 8, // Frankfurt
				HEL = 1 << 9, // Helsinki
				LHR = 1 << 10, // London
				MAD = 1 << 11, // Madrid
				PAR = 1 << 12, // Paris
				STO = 1 << 13, // Stockholm
				VIE = 1 << 14, // Vienna
				WAW = 1 << 15, // Warsaw
				// South America
				EZE = 1 << 16, // Buenos Aires
				LIM = 1 << 17, // Lima
				SCL = 1 << 18, // Santiago
				GRU = 1 << 19, // Sao Paulo
				// Asia
				MAA = 1 << 20, // Chennai
				DXB = 1 << 21, // Dubai
				HKG = 1 << 22, // Hong Kong
				BOM = 1 << 23, // Mumbai
				SEO = 1 << 24, // Seoul
				SGP = 1 << 25, // Singapore
				TYO = 1 << 26, // Tokyo
				// Australia
				SYD = 1 << 27, // Sydney
				// Africa
				JNB = 1 << 28, // Johannesburg
			);
			CVar(ExtendQueue, "Extend queue", false);
			CVar(AutoCasualQueue, "Auto casual queue", false);
		NAMESPACE_END(Queueing)

		NAMESPACE_BEGIN(MannVsMachine, Mann vs. Machine)
			CVar(InstantRespawn, "Instant respawn", false);
			CVar(InstantRevive, "Instant revive", false);
			CVar(AllowInspect, "Allow inspect", false);
		NAMESPACE_END(Sound)

		NAMESPACE_BEGIN(Sound)
			CVarEnum(Block, VA_LIST("Block", "Sound block"), 0b0000, DROPDOWN_MULTI, nullptr,
				VA_LIST("Footsteps", "Noisemaker", "Frying pan", "Water"),
				Footsteps = 1 << 0, Noisemaker = 1 << 1, FryingPan = 1 << 2, Water = 1 << 3);
			CVar(HitsoundAlways, "Hitsound always", false);
			CVar(RemoveDSP, "Remove DSP", false);
			CVar(GiantWeaponSounds, "Giant weapon sounds", false);
		NAMESPACE_END(Sound)
	NAMESPACE_END(Misc)

	NAMESPACE_BEGIN(Logging)
		CVarEnum(Logs, "Logs", 0b0000011, DROPDOWN_MULTI, "Off",
			VA_LIST("Vote start", "Vote cast", "Class changes", "Damage", "Cheat detection", "Tags", "Aliases", "Resolver"),
			VoteStart = 1 << 0, VoteCast = 1 << 1, ClassChanges = 1 << 2, Damage = 1 << 3, CheatDetection = 1 << 4, Tags = 1 << 5, Aliases = 1 << 6, Resolver = 1 << 7);
		Enum(LogTo, Toasts = 1 << 0, Chat = 1 << 1, Party = 1 << 2, Console = 1 << 3, Menu = 1 << 4, Debug = 1 << 5);
		CVarEnum(NotificationPosition, "Notification position", 0, VISUAL, nullptr,
			VA_LIST("Top left", "Top right", "Bottom left", "Bottom right"),
			TopLeft, TopRight, BottomLeft, BottomRight);
		CVar(Lifetime, "Notification time", 5.f, VISUAL, 0.5f, 5.f, 0.5f);

		NAMESPACE_BEGIN(VoteStart, Logging)
			CVarValues(LogTo, VA_LIST("Log to", "Vote start log to"), 0b000001, DROPDOWN_MULTI, nullptr,
				"Toasts", "Chat", "Party", "Console", "Menu", "Debug");
		NAMESPACE_END(VoteStart)

		NAMESPACE_BEGIN(VoteCast, Logging)
			CVarValues(LogTo, VA_LIST("Log to", "Vote cast log to"), 0b000001, DROPDOWN_MULTI, nullptr,
				"Toasts", "Chat", "Party", "Console", "Menu", "Debug");
		NAMESPACE_END(VoteCast)

		NAMESPACE_BEGIN(ClassChange, Logging)
			CVarValues(LogTo, VA_LIST("Log to", "Class change log to"), 0b000001, DROPDOWN_MULTI, nullptr,
				"Toasts", "Chat", "Party", "Console", "Menu", "Debug");
		NAMESPACE_END(ClassChange)

		NAMESPACE_BEGIN(Damage, Logging)
			CVarValues(LogTo, VA_LIST("Log to", "Damage log to"), 0b000001, DROPDOWN_MULTI, nullptr,
				"Toasts", "Chat", "Party", "Console", "Menu", "Debug");
		NAMESPACE_END(Damage)

		NAMESPACE_BEGIN(CheatDetection, Logging)
			CVarValues(LogTo, VA_LIST("Log to", "Cheat detection log to"), 0b000001, DROPDOWN_MULTI, nullptr,
				"Toasts", "Chat", "Party", "Console", "Menu", "Debug");
		NAMESPACE_END(CheatDetection)

		NAMESPACE_BEGIN(Tags, Logging)
			CVarValues(LogTo, VA_LIST("Log to", "Tags log to"), 0b000001, DROPDOWN_MULTI, nullptr,
				"Toasts", "Chat", "Party", "Console", "Menu", "Debug");
		NAMESPACE_END(Tags)

		NAMESPACE_BEGIN(Aliases, Logging)
			CVarValues(LogTo, VA_LIST("Log to", "Aliases log to"), 0b000001, DROPDOWN_MULTI, nullptr,
				"Toasts", "Chat", "Party", "Console", "Menu", "Debug");
		NAMESPACE_END(Aliases)

		NAMESPACE_BEGIN(Resolver, Logging)
			CVarValues(LogTo, VA_LIST("Log to", "Resolver log to"), 0b000001, DROPDOWN_MULTI, nullptr,
				"Toasts", "Chat", "Party", "Console", "Menu", "Debug");
		NAMESPACE_END(Resolver)
	NAMESPACE_END(Logging)

	NAMESPACE_BEGIN(Debug)
		CVar(Info, "Debug info", false, NOSAVE);
		CVar(Logging, "Debug logging", false, NOSAVE);
		CVar(Options, "Debug options", false, NOSAVE);
		CVar(DrawHitboxes, "Show hitboxes", false, NOSAVE);
		CVar(AntiAimLines, "Antiaim lines", false);
		CVar(CrashLogging, "Crash logging", true);
		// Aimbot debugging
		CVar(AimbotDebug, "Aimbot debug visualization", false);
		CVar(AimbotDrawFOV, "Draw aimbot FOV cone", false);
		CVar(AimbotDrawTargets, "Draw all aimbot targets", false);
		// Mouse movement logger - records to CSV when holding mouse2
		CVar(MouseMovementLogger, "Mouse movement logger (hold Mouse2)", false);
#ifdef DEBUG_TRACES
		CVar(VisualizeTraces, "Visualize traces", false, NOSAVE);
		CVar(VisualizeTraceHits, "Visualize trace hits", false, NOSAVE);
#endif
<<<<<<< HEAD
	NAMESPACE_END(Debug);

#ifdef DEBUG_HOOKS
	NAMESPACE_BEGIN(Hooks)
		CVar(bf_read_ReadString, "bf_read_ReadString", true, NOSAVE | DEBUGVAR);
		CVar(CAchievementMgr_CheckAchievementsEnabled, "CAchievementMgr_CheckAchievementsEnabled", true, NOSAVE | DEBUGVAR);
		CVar(CAttributeManager_AttribHookValue, "CAttributeManager_AttribHookValue", true, NOSAVE | DEBUGVAR);
		CVar(CBaseAnimating_Interpolate, "CBaseAnimating_Interpolate", true, NOSAVE | DEBUGVAR);
		CVar(CBaseAnimating_MaintainSequenceTransitions, "CBaseAnimating_MaintainSequenceTransitions", true, NOSAVE | DEBUGVAR);
		CVar(CBaseAnimating_SetSequence, "CBaseAnimating_SetSequence", true, NOSAVE | DEBUGVAR);
		CVar(CBaseAnimating_SetupBones, "CBaseAnimating_SetupBones", true, NOSAVE | DEBUGVAR);
		CVar(CBaseAnimating_UpdateClientSideAnimation, "CBaseAnimating_UpdateClientSideAnimation", true, NOSAVE | DEBUGVAR);
		CVar(CBaseEntity_BaseInterpolatePart1, "CBaseEntity_BaseInterpolatePart1", true, NOSAVE | DEBUGVAR);
		CVar(CBaseEntity_EstimateAbsVelocity, "CBaseEntity_EstimateAbsVelocity", true, NOSAVE | DEBUGVAR);
		CVar(CBaseEntity_ResetLatched, "CBaseEntity_ResetLatched", true, NOSAVE | DEBUGVAR);
		CVar(CBaseEntity_SetAbsVelocity, "CBaseEntity_SetAbsVelocity", true, NOSAVE | DEBUGVAR);
		CVar(CBaseEntity_WorldSpaceCenter, "CBaseEntity_WorldSpaceCenter", true, NOSAVE | DEBUGVAR);
		CVar(CBaseHudChatLine_InsertAndColorizeText, "CBaseHudChatLine_InsertAndColorizeText", true, NOSAVE | DEBUGVAR);
		CVar(CBasePlayer_CalcPlayerView, "CBasePlayer_CalcPlayerView", true, NOSAVE | DEBUGVAR);
		CVar(CBasePlayer_CalcViewModelView, "CBasePlayer_CalcViewModelView", true, NOSAVE | DEBUGVAR);
		CVar(CBasePlayer_ItemPostFrame, "CBasePlayer_ItemPostFrame", true, NOSAVE | DEBUGVAR);
		CVar(CBaseViewModel_ShouldFlipViewModel, "CBaseViewModel_ShouldFlipViewModel", true, NOSAVE | DEBUGVAR);
		CVar(Cbuf_ExecuteCommand, "Cbuf_ExecuteCommand", true, NOSAVE | DEBUGVAR);
		CVar(CClientModeShared_CreateMove, "CClientModeShared_CreateMove", true, NOSAVE | DEBUGVAR);
		CVar(CClientModeShared_DoPostScreenSpaceEffects, "CClientModeShared_DoPostScreenSpaceEffects", true, NOSAVE | DEBUGVAR);
		CVar(CClientModeShared_OverrideView, "CClientModeShared_OverrideView", true, NOSAVE | DEBUGVAR);
		CVar(CClientModeShared_ShouldDrawViewModel, "CClientModeShared_ShouldDrawViewModel", true, NOSAVE | DEBUGVAR);
		CVar(CClientState_GetClientInterpAmount, "CClientState_GetClientInterpAmount", true, NOSAVE | DEBUGVAR);
		CVar(CClientState_ProcessFixAngle, "CClientState_ProcessFixAngle", true, NOSAVE | DEBUGVAR);
		CVar(CHudCrosshair_GetDrawPosition, "CHudCrosshair_GetDrawPosition", true, NOSAVE | DEBUGVAR);
		CVar(CInventoryManager_ShowItemsPickedUp, "CInventoryManager_ShowItemsPickedUp", true, NOSAVE | DEBUGVAR);
		CVar(CL_CheckForPureServerWhitelist, "CL_CheckForPureServerWhitelist", true, NOSAVE | DEBUGVAR);
		CVar(CL_Move, "CL_Move", true, NOSAVE | DEBUGVAR);
		CVar(CL_ProcessPacketEntities, "CL_ProcessPacketEntities", true, NOSAVE | DEBUGVAR);
		CVar(CL_ReadPackets, "CL_ReadPackets", true, NOSAVE | DEBUGVAR);
		CVar(ClientModeTFNormal_BIsFriendOrPartyMember, "ClientModeTFNormal_BIsFriendOrPartyMember", true, NOSAVE | DEBUGVAR);
		CVar(ClientModeTFNormal_UpdateSteamRichPresence, "ClientModeTFNormal_UpdateSteamRichPresence", true, NOSAVE | DEBUGVAR);
		CVar(CMatchInviteNotification_OnTick, "CMatchInviteNotification_OnTick", true, NOSAVE | DEBUGVAR);
		CVar(CMaterial_Uncache, "CMaterial_Uncache", true, NOSAVE | DEBUGVAR);
		CVar(CNetChannel_SendDatagram, "CNetChannel_SendDatagram", true, NOSAVE | DEBUGVAR);
		CVar(CNetChannel_SendNetMsg, "CNetChannel_SendNetMsg", true, NOSAVE | DEBUGVAR);
		CVar(COPRenderSprites_Render, "COPRenderSprites_Render", true, NOSAVE | DEBUGVAR);
		CVar(CParticleProperty_Create, "CParticleProperty_Create", true, NOSAVE | DEBUGVAR);
		CVar(CPlayerResource_GetPlayerName, "CPlayerResource_GetPlayerName", true, NOSAVE | DEBUGVAR);
		CVar(CPrediction_RunSimulation, "CPrediction_RunSimulation", true, NOSAVE | DEBUGVAR);
		CVar(CRendering3dView_EnableWorldFog, "CRendering3dView_EnableWorldFog", true, NOSAVE | DEBUGVAR);
		CVar(CSkyboxView_Enable3dSkyboxFog, "CSkyboxView_Enable3dSkyboxFog", true, NOSAVE | DEBUGVAR);
		CVar(CSniperDot_GetRenderingPositions, "CSniperDot_GetRenderingPositions", true, NOSAVE | DEBUGVAR);
		CVar(CSoundEmitterSystem_EmitSound, "CSoundEmitterSystem_EmitSound", true, NOSAVE | DEBUGVAR);
		CVar(CStaticPropMgr_ComputePropOpacity, "CStaticPropMgr_ComputePropOpacity", true, NOSAVE | DEBUGVAR);
		CVar(CStaticPropMgr_DrawStaticProps, "CStaticPropMgr_DrawStaticProps", true, NOSAVE | DEBUGVAR);
		CVar(CStudioRender_DrawModelStaticProp, "CStudioRender_DrawModelStaticProp", true, NOSAVE | DEBUGVAR);
		CVar(CStudioRender_SetAlphaModulation, "CStudioRender_SetAlphaModulation", true, NOSAVE | DEBUGVAR);
		CVar(CStudioRender_SetColorModulation, "CStudioRender_SetColorModulation", true, NOSAVE | DEBUGVAR);
		CVar(CTFBadgePanel_SetupBadge, "CTFBadgePanel_SetupBadge", true, NOSAVE | DEBUGVAR);
		CVar(CTFClientScoreBoardDialog_UpdatePlayerAvatar, "CTFClientScoreBoardDialog_UpdatePlayerAvatar", true, NOSAVE | DEBUGVAR);
		CVar(CTFGCClientSystem_UpdateAssignedLobby, "CTFGCClientSystem_UpdateAssignedLobby", true, NOSAVE | DEBUGVAR);
		CVar(CTFHudDeathNotice_AddAdditionalMsg, "CTFHudDeathNotice_AddAdditionalMsg", true, NOSAVE | DEBUGVAR);
		CVar(CTFInput_ApplyMouse, "CTFInput_ApplyMouse", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayer_AvoidPlayers, "CTFPlayer_AvoidPlayers", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayer_BRenderAsZombie, "CTFPlayer_BRenderAsZombie", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayer_BuildTransformations, "CTFPlayer_BuildTransformations", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayer_DoAnimationEvent, "CTFPlayer_DoAnimationEvent", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayer_FireBullet, "CTFPlayer_FireBullet", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayer_IsPlayerClass, "CTFPlayer_IsPlayerClass", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayer_ShouldDraw, "CTFPlayer_ShouldDraw", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayer_UpdateStepSound, "CTFPlayer_UpdateStepSound", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayerInventory_GetMaxItemCount, "CTFPlayerInventory_GetMaxItemCount", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayerPanel_GetTeam, "CTFPlayerPanel_GetTeam", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayerShared_InCond, "CTFPlayerShared_InCond", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayerShared_IsPlayerDominated, "CTFPlayerShared_IsPlayerDominated", true, NOSAVE | DEBUGVAR);
		CVar(CTFPlayerShared_ShouldSuppressPrediction, "CTFPlayerShared_ShouldSuppressPrediction", true, NOSAVE | DEBUGVAR);
		CVar(CTFRagdoll_CreateTFRagdoll, "CTFRagdoll_CreateTFRagdoll", true, NOSAVE | DEBUGVAR);
		CVar(CTFWeaponBase_CalcIsAttackCritical, "CTFWeaponBase_CalcIsAttackCritical", true, NOSAVE | DEBUGVAR);
		CVar(CTFWeaponBase_GetShootSound, "CTFWeaponBase_GetShootSound", true, NOSAVE | DEBUGVAR);
		CVar(CThirdPersonManager_Update, "CThirdPersonManager_Update", true, NOSAVE | DEBUGVAR);
		CVar(CViewRender_DrawUnderwaterOverlay, "CViewRender_DrawUnderwaterOverlay", true, NOSAVE | DEBUGVAR);
		CVar(CViewRender_LevelInit, "CViewRender_LevelInit", true, NOSAVE | DEBUGVAR);
		CVar(CViewRender_PerformScreenOverlay, "CViewRender_PerformScreenOverlay", true, NOSAVE | DEBUGVAR);
		CVar(CViewRender_RenderView, "CViewRender_RenderView", true, NOSAVE | DEBUGVAR);
		CVar(DoEnginePostProcessing, "DoEnginePostProcessing", true, NOSAVE | DEBUGVAR);
		CVar(DSP_Process, "DSP_Process", true, NOSAVE | DEBUGVAR);
		CVar(FX_FireBullets, "FX_FireBullets", true, NOSAVE | DEBUGVAR);
		CVar(GenerateEquipRegionConflictMask, "GenerateEquipRegionConflictMask", true, NOSAVE | DEBUGVAR);
		CVar(GetClientInterpAmount, "GetClientInterpAmount", true, NOSAVE | DEBUGVAR);
		CVar(HostState_Shutdown, "HostState_Shutdown", true, NOSAVE | DEBUGVAR);
		CVar(IBaseClientDLL_DispatchUserMessage, "IBaseClientDLL_DispatchUserMessage", true, NOSAVE | DEBUGVAR);
		CVar(IBaseClientDLL_FrameStageNotify, "IBaseClientDLL_FrameStageNotify", true, NOSAVE | DEBUGVAR);
		CVar(IBaseClientDLL_LevelShutdown, "IBaseClientDLL_LevelShutdown", true, NOSAVE | DEBUGVAR);
		CVar(IEngineTrace_SetTraceEntity, "IEngineTrace_SetTraceEntity", true, NOSAVE | DEBUGVAR);
		CVar(IEngineTrace_TraceRay, "IEngineTrace_TraceRay", true, NOSAVE | DEBUGVAR);
		CVar(IEngineVGui_Paint, "IEngineVGui_Paint", true, NOSAVE | DEBUGVAR);
		CVar(IInput_GetUserCmd, "IInput_GetUserCmd", true, NOSAVE | DEBUGVAR);
		CVar(IMatSystemSurface_OnScreenSizeChanged, "IMatSystemSurface_OnScreenSizeChanged", true, NOSAVE | DEBUGVAR);
		CVar(IPanel_PaintTraverse, "IPanel_PaintTraverse", true, NOSAVE | DEBUGVAR);
		CVar(ISteamFriends_GetFriendPersonaName, "ISteamFriends_GetFriendPersonaName", true, NOSAVE | DEBUGVAR);
		CVar(ISteamNetworkingUtils_GetPingToDataCenter, "ISteamNetworkingUtils_GetPingToDataCenter", true, NOSAVE | DEBUGVAR);
		CVar(IVEngineClient_ClientCmd_Unrestricted, "IVEngineClient_ClientCmd_Unrestricted", true, NOSAVE | DEBUGVAR);
		CVar(IVEngineClient_IsHLTV, "IVEngineClient_IsHLTV", true, NOSAVE | DEBUGVAR);
		CVar(IVModelRender_DrawModelExecute, "IVModelRender_DrawModelExecute", true, NOSAVE | DEBUGVAR);
		CVar(IVModelRender_ForcedMaterialOverride, "IVModelRender_ForcedMaterialOverride", true, NOSAVE | DEBUGVAR);
		CVar(KeyValues_SetInt, "KeyValues_SetInt", true, NOSAVE | DEBUGVAR);
		CVar(NotificationQueue_Add, "NotificationQueue_Add", true, NOSAVE | DEBUGVAR);
		CVar(R_DrawSkyBox, "R_DrawSkyBox", true, NOSAVE | DEBUGVAR);
		CVar(RecvProxy_SimulationTime, "RecvProxy_SimulationTime", true, NOSAVE | DEBUGVAR);
		CVar(TF_IsHolidayActive, "TF_IsHolidayActive", true, NOSAVE | DEBUGVAR);
		CVar(VGuiMenuBuilder_AddMenuItem, "VGuiMenuBuilder_AddMenuItem", true, NOSAVE | DEBUGVAR);
	NAMESPACE_END(Hooks);
#endif

	NAMESPACE_BEGIN(Chat)
		CVar(Server, "Matrix server", std::string("matrix.org"));
		CVar(Username, "Username", std::string(""));
		CVar(Password, "Password", std::string(""));
		CVar(Email, "Email for registration", std::string(""));
		CVar(RecaptchaResponse, "reCAPTCHA response token", std::string(""), NOSAVE);
		CVar(Space, "Space/Community", std::string("amalgam-comp"));
		CVar(Room, "Room", std::string("talk"));
		CVar(AutoConnect, "Auto connect on startup", false);
		CVar(ShowTimestamps, "Show timestamps", true);
		CVar(SaveCredentials, "Save login details", false);
	NAMESPACE_END(Chat);
}
=======
	NAMESPACE_END(Debug)
NAMESPACE_END(Vars)
>>>>>>> upstream/master
