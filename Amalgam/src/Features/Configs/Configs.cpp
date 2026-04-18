#include "Configs.h"

#include "../Binds/Binds.h"
#include "../Visuals/Groups/Groups.h"
#include "../Visuals/Materials/Materials.h"
#include "../Chat/Chat.h"

template <class T> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const T& v)
{
	t.put(s, v);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const IntRange_t& v)
{
	boost::property_tree::ptree tChild;
	SaveJson(tChild, "Min", v.Min);
	SaveJson(tChild, "Max", v.Max);

	t.put_child(s, tChild);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const FloatRange_t& v)
{
	boost::property_tree::ptree tChild;
	SaveJson(tChild, "Min", v.Min);
	SaveJson(tChild, "Max", v.Max);

	t.put_child(s, tChild);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const Color_t& v)
{
	boost::property_tree::ptree tChild;
	SaveJson(tChild, "r", v.r);
	SaveJson(tChild, "g", v.g);
	SaveJson(tChild, "b", v.b);
	SaveJson(tChild, "a", v.a);

	t.put_child(s, tChild);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const std::vector<std::pair<std::string, Color_t>>& v)
{
	boost::property_tree::ptree tChild;
	for (auto& [m, c] : v)
	{
		boost::property_tree::ptree tLayer;
		SaveJson(tLayer, "Material", m);
		SaveJson(tLayer, "Color", c);

		tChild.push_back({ "", tLayer });
	}

	t.put_child(s, tChild);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const Gradient_t& v)
{
	boost::property_tree::ptree tChild;
	SaveJson(tChild, "StartColor", v.StartColor);
	SaveJson(tChild, "EndColor", v.EndColor);

	t.put_child(s, tChild);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const DragBox_t& v)
{
	boost::property_tree::ptree tChild;
	SaveJson(tChild, "x", v.x);
	SaveJson(tChild, "y", v.y);

	t.put_child(s, tChild);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const WindowBox_t& v)
{
	boost::property_tree::ptree tChild;
	SaveJson(tChild, "x", v.x);
	SaveJson(tChild, "y", v.y);
	SaveJson(tChild, "w", v.w);
	SaveJson(tChild, "h", v.h);

	t.put_child(s, tChild);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const Chams_t& v)
{
	boost::property_tree::ptree tChild;
	SaveJson(tChild, "Visible", v.Visible);
	SaveJson(tChild, "Occluded", v.Occluded);

	t.put_child(s, tChild);
}

template <> void CConfigs::SaveJson(boost::property_tree::ptree& t, const std::string& s, const Glow_t& v)
{
	boost::property_tree::ptree tChild;
	SaveJson(tChild, "Stencil", v.Stencil);
	SaveJson(tChild, "Blur", v.Blur);

	t.put_child(s, tChild);
}



template <class T> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, T& v)
{
	if (auto o = t.get_optional<T>(s))
		v = *o;
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, IntRange_t& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		LoadJson(*tChild, "Min", v.Min);
		LoadJson(*tChild, "Max", v.Max);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, FloatRange_t& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		LoadJson(*tChild, "Min", v.Min);
		LoadJson(*tChild, "Max", v.Max);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, Color_t& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		LoadJson(*tChild, "r", v.r);
		LoadJson(*tChild, "g", v.g);
		LoadJson(*tChild, "b", v.b);
		LoadJson(*tChild, "a", v.a);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, std::vector<std::pair<std::string, Color_t>>& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		v.clear();
		for (auto& [_, tLayer] : *tChild)
		{
			if (auto o = tLayer.get_optional<std::string>("Material"))
			{
				std::string& m = *o;
				Color_t c; LoadJson(tLayer, "Color", c);
				v.emplace_back(m, c);
			}
		}
	}

	// remove invalid/duplicate materials
	for (auto it = v.begin(); it != v.end();)
	{
		auto uHash = FNV1A::Hash32(it->first.c_str());
		bool bValid = uHash != FNV1A::Hash32Const("None") && (uHash == FNV1A::Hash32Const("Original") || F::Materials.m_mMaterials.contains(uHash));
		if (bValid)
		{
			int i = 0; for (auto& [s, _] : v)
			{
				auto uHash2 = FNV1A::Hash32(s.c_str());
				if (uHash == uHash2)
					i++;
			}
			bValid = i <= 1;
		}

		if (bValid)
			++it;
		else
			it = v.erase(it);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, Gradient_t& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		LoadJson(*tChild, "StartColor", v.StartColor);
		LoadJson(*tChild, "EndColor", v.EndColor);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, DragBox_t& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		LoadJson(*tChild, "x", v.x);
		LoadJson(*tChild, "y", v.y);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, WindowBox_t& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		LoadJson(*tChild, "x", v.x);
		LoadJson(*tChild, "y", v.y);
		LoadJson(*tChild, "w", v.w);
		LoadJson(*tChild, "h", v.h);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, Chams_t& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		LoadJson(*tChild, "Visible", v.Visible);
		LoadJson(*tChild, "Occluded", v.Occluded);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, Glow_t& v)
{
	if (auto tChild = t.get_child_optional(s))
	{
		LoadJson(*tChild, "Stencil", v.Stencil);
		LoadJson(*tChild, "Blur", v.Blur);
	}
}



template <class T> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, ConfigVar<T>* c, int i)
{
	LoadJson(t, s, c->Map[i]);
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, ConfigVar<int>* c, int i)
{
	auto& v = c->Map[i];
	LoadJson(t, s, v);

	if (!c->m_vValues.empty())
	{
		if (c->m_iFlags & DROPDOWN_NOSANITIZATION)
			return;

		if (!(c->m_iFlags & DROPDOWN_MULTI))
			v = std::clamp(v, 0, int(c->m_vValues.size() - 1));
		else
		{
			for (int i = 0; i < sizeof(int) * 8; i++)
			{
				bool bFound = v & (1 << i) && i < c->m_vValues.size();
				if (!bFound)
					v &= ~(1 << i);
			}
		}
	}
	else if (c->m_sExtra)
	{
		if (!(c->m_iFlags & SLIDER_PRECISION))
			v = float(v) - fnmodf(float(v) - c->m_unStep.i / 2.f, c->m_unStep.i) + c->m_unStep.i / 2.f;
		if (c->m_iFlags & SLIDER_CLAMP)
			v = std::clamp(v, c->m_unMin.i, c->m_unMax.i);
		else if (c->m_iFlags & SLIDER_MIN)
			v = std::max(v, c->m_unMin.i);
		else if (c->m_iFlags & SLIDER_MAX)
			v = std::min(v, c->m_unMax.i);
	}
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, ConfigVar<float>* c, int i)
{
	auto& v = c->Map[i];
	LoadJson(t, s, v);

	if (!(c->m_iFlags & SLIDER_PRECISION))
		v = v - fnmodf(v - c->m_unStep.f / 2, c->m_unStep.f) + c->m_unStep.f / 2;
	if (c->m_iFlags & SLIDER_CLAMP)
		v = std::clamp(v, c->m_unMin.f, c->m_unMax.f);
	else if (c->m_iFlags & SLIDER_MIN)
		v = std::max(v, c->m_unMin.f);
	else if (c->m_iFlags & SLIDER_MAX)
		v = std::min(v, c->m_unMax.f);
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, ConfigVar<IntRange_t>* c, int i)
{
	auto& v = c->Map[i];
	LoadJson(t, s, v);

	if (!(c->m_iFlags & SLIDER_PRECISION))
	{
		v.Min = float(v.Min) - fnmodf(float(v.Min) - c->m_unStep.i / 2.f, c->m_unStep.i) + c->m_unStep.i / 2.f;
		v.Max = float(v.Max) - fnmodf(float(v.Max) - c->m_unStep.i / 2.f, c->m_unStep.i) + c->m_unStep.i / 2.f;
	}
	if (c->m_iFlags & SLIDER_CLAMP)
	{
		v.Min = std::clamp(v.Min, c->m_unMin.i, c->m_unMax.i - c->m_unStep.i);
		v.Max = std::clamp(v.Max, c->m_unMin.i + c->m_unStep.i, c->m_unMax.i);
	}
	else if (c->m_iFlags & SLIDER_MIN)
	{
		v.Min = std::max(v.Min, c->m_unMin.i);
		v.Max = std::max(v.Max, c->m_unMin.i + c->m_unStep.i);
	}
	else if (c->m_iFlags & SLIDER_MAX)
	{
		v.Min = std::min(v.Min, c->m_unMax.i - c->m_unStep.i);
		v.Max = std::min(v.Max, c->m_unMax.i);
	}
	v.Max = std::max(v.Max, v.Min + c->m_unStep.i);
}

template <> void CConfigs::LoadJson(const boost::property_tree::ptree& t, const std::string& s, ConfigVar<FloatRange_t>* c, int i)
{
	auto& v = c->Map[i];
	LoadJson(t, s, v);

	if (!(c->m_iFlags & SLIDER_PRECISION))
	{
		v.Min = v.Min - fnmodf(v.Min - c->m_unStep.f / 2, c->m_unStep.f) + c->m_unStep.f / 2;
		v.Max = v.Max - fnmodf(v.Max - c->m_unStep.f / 2, c->m_unStep.f) + c->m_unStep.f / 2;
	}
	if (c->m_iFlags & SLIDER_CLAMP)
	{
		v.Min = std::clamp(v.Min, c->m_unMin.f, c->m_unMax.f - c->m_unStep.f);
		v.Max = std::clamp(v.Max, c->m_unMin.f + c->m_unStep.f, c->m_unMax.f);
	}
	else if (c->m_iFlags & SLIDER_MIN)
	{
		v.Min = std::max(v.Min, c->m_unMin.f);
		v.Max = std::max(v.Max, c->m_unMin.f + c->m_unStep.f);
	}
	else if (c->m_iFlags & SLIDER_MAX)
	{
		v.Min = std::min(v.Min, c->m_unMax.f - c->m_unStep.f);
		v.Max = std::min(v.Max, c->m_unMax.f);
	}
	v.Max = std::max(v.Max, v.Min + c->m_unStep.f);
}



CConfigs::CConfigs()
{
	m_sConfigPath = std::filesystem::current_path().string() + "\\Amalgam\\";
	m_sVisualsPath = m_sConfigPath + "Visuals\\";
	m_sCorePath = m_sConfigPath + "Core\\";
	m_sMaterialsPath = m_sConfigPath + "Materials\\";

	if (!std::filesystem::exists(m_sConfigPath))
		std::filesystem::create_directory(m_sConfigPath);

	if (!std::filesystem::exists(m_sVisualsPath))
		std::filesystem::create_directory(m_sVisualsPath);

	if (!std::filesystem::exists(m_sCorePath))
		std::filesystem::create_directory(m_sCorePath);

	if (!std::filesystem::exists(m_sMaterialsPath))
		std::filesystem::create_directory(m_sMaterialsPath);
}

#define IsType(t) pBase->m_iType == typeid(t).hash_code()

template <class T>
static inline void SaveMain(BaseVar*& pBase, boost::property_tree::ptree& tTree)
{
	auto pVar = pBase->As<T>();

	boost::property_tree::ptree tMap;
	for (auto& [iBind, tValue] : pVar->Map)
		F::Configs.SaveJson(tMap, std::to_string(iBind), tValue);
	tTree.put_child(pVar->Name(), tMap);
}
<<<<<<< HEAD
#define SaveMain(type, tree) if (IsType(type)) SaveCond(type, tree)
#define LoadCond(type, tree)\
{\
	auto currentValue = pVar->As<type>()->Map.find(DEFAULT_BIND) != pVar->As<type>()->Map.end() ? pVar->As<type>()->Map[DEFAULT_BIND] : pVar->As<type>()->Default;\
	pVar->As<type>()->Map = { { DEFAULT_BIND, currentValue } };\
	if (const auto mapTree = tree.get_child_optional(pVar->m_sName))\
	{\
		for (auto& it : *mapTree)\
		{\
			if (!bLegacy)\
			{\
				int iBind = std::stoi(it.first);\
				if (iBind == DEFAULT_BIND || F::Binds.m_vBinds.size() > iBind && !(pVar->As<type>()->m_iFlags & NOBIND))\
				{\
					LoadJson(*mapTree, it.first, pVar->As<type>()->Map[iBind]);\
					if (iBind != DEFAULT_BIND)\
						std::next(F::Binds.m_vBinds.begin(), iBind)->m_vVars.push_back(pVar);\
				}\
			}\
			else\
			{\
				int iBind = -2; /*invalid bind*/ \
				auto uHash = FNV1A::Hash32(it.first.c_str());\
				if (uHash == FNV1A::Hash32Const("default"))\
					iBind = DEFAULT_BIND;\
				else\
				{\
					for (auto it2 = F::Binds.m_vBinds.begin(); it2 != F::Binds.m_vBinds.end(); it2++)\
					{\
						if (uHash == FNV1A::Hash32(it2->m_sName.c_str()))\
						{\
							iBind = std::distance(F::Binds.m_vBinds.begin(), it2);\
							break;\
						}\
					}\
				}\
				if (iBind != -2 && (iBind == DEFAULT_BIND || F::Binds.m_vBinds.size() > iBind && !(pVar->As<type>()->m_iFlags & NOBIND)))\
				{\
					LoadJson(*mapTree, it.first, pVar->As<type>()->Map[iBind]);\
					if (iBind != DEFAULT_BIND)\
						std::next(F::Binds.m_vBinds.begin(), iBind)->m_vVars.push_back(pVar);\
				}\
			}\
		}\
	}\
	else if (!(pVar->m_iFlags & NOSAVE))\
	{\
		/* Suppress warnings for new Competitive variables to reduce console spam */\
		bool bSuppressWarning = pVar->m_sName.find("Competitive::") == 0;\
		if (!bSuppressWarning)\
			SDK::Output("Amalgam", std::format("{} not found", pVar->m_sName).c_str(), { 175, 150, 255, 127 }, true, true);\
	}\
=======
#define Save(t, j) if (IsType(t)) SaveMain<t>(pBase, j);

template <class T>
static inline void LoadMain(BaseVar*& pBase, boost::property_tree::ptree& tTree)
{
	auto pVar = pBase->As<T>();

	pVar->Map = { { DEFAULT_BIND, pVar->Default } };
	if (auto tMap = tTree.get_child_optional(pVar->Name()))
	{
		for (auto& [sKey, _] : *tMap)
		{
			int iBind = std::stoi(sKey);
			if (iBind == DEFAULT_BIND || F::Binds.m_vBinds.size() > iBind && !(pVar->m_iFlags & NOBIND))
			{
				F::Configs.LoadJson(*tMap, sKey, pVar, iBind);
				if (iBind != DEFAULT_BIND)
					std::next(F::Binds.m_vBinds.begin(), iBind)->m_vVars.push_back(pVar);
			}
		}
	}
	else if (!(pVar->m_iFlags & NOSAVE))
		SDK::Output("Amalgam", std::format("{} not found", pVar->Name()).c_str(), ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
>>>>>>> upstream/master
}
#define Load(t, j) if (IsType(t)) LoadMain<t>(pBase, j);

bool CConfigs::SaveConfig(const std::string& sConfigName, bool bNotify)
{
	try
	{
		boost::property_tree::ptree tWrite;

		{
			boost::property_tree::ptree tSub;
			for (int iID = 0; iID < F::Binds.m_vBinds.size(); iID++)
			{
				auto& tBind = F::Binds.m_vBinds[iID];

				boost::property_tree::ptree tChild;
				SaveJson(tChild, "Name", tBind.m_sName);
				SaveJson(tChild, "Type", tBind.m_iType);
				SaveJson(tChild, "Info", tBind.m_iInfo);
				SaveJson(tChild, "Key", tBind.m_iKey);
				SaveJson(tChild, "Enabled", tBind.m_bEnabled);
				SaveJson(tChild, "Visibility", tBind.m_iVisibility);
				SaveJson(tChild, "Not", tBind.m_bNot);
				SaveJson(tChild, "Active", tBind.m_bActive);
				SaveJson(tChild, "Parent", tBind.m_iParent);

				tSub.put_child(std::to_string(iID), tChild);
			}
			tWrite.put_child("Binds", tSub);
		}

		{
			boost::property_tree::ptree tSub;
			bool bNoSave = GetAsyncKeyState(VK_SHIFT) & 0x8000;
			for (auto& pBase : G::Vars)
			{
				if (!bNoSave && pBase->m_iFlags & NOSAVE)
					continue;

<<<<<<< HEAD
			// Special handling for Chat credentials - encrypt password and clear others when SaveCredentials is disabled
			if ((pVar->m_sName == "Vars::Chat::Password" || pVar->m_sName == "Vars::Chat::Username" || 
				 pVar->m_sName == "Vars::Chat::Email" || pVar->m_sName == "Vars::Chat::Server" ||
				 pVar->m_sName == "Vars::Chat::Space" || pVar->m_sName == "Vars::Chat::Room") && pVar->m_iType == typeid(std::string).hash_code())
			{
				auto stringVar = pVar->As<std::string>();
				std::string originalValue;
				
				// Get the current value
				if (stringVar->Map.find(DEFAULT_BIND) != stringVar->Map.end())
					originalValue = stringVar->Map[DEFAULT_BIND];
				
				// Check if SaveCredentials is enabled
				bool shouldSaveCredentials = false;
				for (auto& pCheckVar : G::Vars)
				{
					if (pCheckVar->m_sName == "Vars::Chat::SaveCredentials" && pCheckVar->m_iType == typeid(bool).hash_code())
					{
						auto saveCredsVar = pCheckVar->As<bool>();
						if (saveCredsVar->Map.find(DEFAULT_BIND) != saveCredsVar->Map.end())
							shouldSaveCredentials = saveCredsVar->Map[DEFAULT_BIND];
						break;
					}
				}
				
				if (pVar->m_sName == "Vars::Chat::Password")
				{
					// Special password handling - encrypt if SaveCredentials is enabled
					if (shouldSaveCredentials && !originalValue.empty())
					{
						// Encrypt the password
						std::string encryptedPassword = F::Chat.EncryptPassword(originalValue);
						if (!encryptedPassword.empty())
						{
							// Temporarily replace with encrypted version
							stringVar->Map[DEFAULT_BIND] = encryptedPassword;
							SaveMain(std::string, varTree);
							// Restore original password
							stringVar->Map[DEFAULT_BIND] = originalValue;
						}
						else
						{
							// Encryption failed, save empty string
							stringVar->Map[DEFAULT_BIND] = "";
							SaveMain(std::string, varTree);
							stringVar->Map[DEFAULT_BIND] = originalValue;
						}
					}
					else
					{
						// SaveCredentials disabled or password empty, save empty string
						stringVar->Map[DEFAULT_BIND] = "";
						SaveMain(std::string, varTree);
						stringVar->Map[DEFAULT_BIND] = originalValue;
					}
				}
				else
				{
					// Other chat credentials - save if SaveCredentials is enabled, otherwise save empty
					if (shouldSaveCredentials)
					{
						SaveMain(std::string, varTree);
					}
					else
					{
						// SaveCredentials disabled, save empty string
						std::string tempValue = stringVar->Map[DEFAULT_BIND];
						stringVar->Map[DEFAULT_BIND] = "";
						SaveMain(std::string, varTree);
						stringVar->Map[DEFAULT_BIND] = tempValue;
					}
				}
			}
			else
			{
				// Standard variable saving
				SaveMain(bool, varTree)
				else SaveMain(int, varTree)
				else SaveMain(float, varTree)
				else SaveMain(IntRange_t, varTree)
				else SaveMain(FloatRange_t, varTree)
				else SaveMain(std::string, varTree)
				else SaveMain(VA_LIST(std::vector<std::pair<std::string, Color_t>>), varTree)
				else SaveMain(Color_t, varTree)
				else SaveMain(Gradient_t, varTree)
				else SaveMain(DragBox_t, varTree)
				else SaveMain(WindowBox_t, varTree)
			}
=======
				Save(bool, tSub)
				else Save(int, tSub)
				else Save(float, tSub)
				else Save(IntRange_t, tSub)
				else Save(FloatRange_t, tSub)
				else Save(std::string, tSub)
				else Save(VA_LIST(std::vector<std::pair<std::string, Color_t>>), tSub)
				else Save(Color_t, tSub)
				else Save(Gradient_t, tSub)
				else Save(DragBox_t, tSub)
				else Save(WindowBox_t, tSub)
			}
			tWrite.put_child("Vars", tSub);
>>>>>>> upstream/master
		}

		{
			boost::property_tree::ptree tSub;
			for (int iID = 0; iID < F::Groups.m_vGroups.size(); iID++)
			{
				auto& tGroup = F::Groups.m_vGroups[iID];

				boost::property_tree::ptree tChild;
				SaveJson(tChild, "Name", tGroup.m_sName);
				SaveJson(tChild, "Color", tGroup.m_tColor);
				SaveJson(tChild, "TagsOverrideColor", tGroup.m_bTagsOverrideColor);
				SaveJson(tChild, "Targets", tGroup.m_iTargets);
				SaveJson(tChild, "Conditions", tGroup.m_iConditions);
				SaveJson(tChild, "Players", tGroup.m_iPlayers);
				SaveJson(tChild, "Buildings", tGroup.m_iBuildings);
				SaveJson(tChild, "Projectiles", tGroup.m_iProjectiles);
				SaveJson(tChild, "ESP", tGroup.m_iESP);
				SaveJson(tChild, "Chams", tGroup.m_tChams);
				SaveJson(tChild, "Glow", tGroup.m_tGlow);
				SaveJson(tChild, "OffscreenArrows", tGroup.m_bOffscreenArrows);
				SaveJson(tChild, "OffscreenArrowsOffset", tGroup.m_iOffscreenArrowsOffset);
				SaveJson(tChild, "OffscreenArrowsMaxDistance", tGroup.m_flOffscreenArrowsMaxDistance);
				SaveJson(tChild, "PickupTimer", tGroup.m_bPickupTimer);
				SaveJson(tChild, "Backtrack", tGroup.m_iBacktrack);
				SaveJson(tChild, "BacktrackChams", tGroup.m_vBacktrackChams);
				SaveJson(tChild, "BacktrackGlow", tGroup.m_tBacktrackGlow);
				SaveJson(tChild, "Trajectory", tGroup.m_iTrajectory);
				SaveJson(tChild, "Sightlines", tGroup.m_iSightlines);

				tSub.put_child(std::to_string(iID), tChild);
				if (F::Groups.m_vGroups.size() >= sizeof(int) * 8)
					break;
			}
			tWrite.put_child("Groups", tSub);
		}

		write_json(m_sConfigPath + sConfigName + m_sConfigExtension, tWrite);

		m_sCurrentConfig = sConfigName; m_sCurrentVisuals = "";
		if (bNotify)
			SDK::Output("Amalgam", std::format("Config {} saved", sConfigName).c_str(), DEFAULT_COLOR, OUTPUT_CONSOLE | OUTPUT_TOAST | OUTPUT_MENU | OUTPUT_DEBUG);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Save config failed", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
		return false;
	}

	return true;
}

bool CConfigs::LoadConfig(const std::string& sConfigName, bool bNotify)
{
	try
	{
		if (!std::filesystem::exists(m_sConfigPath + sConfigName + m_sConfigExtension))
		{
			if (sConfigName == std::string("default"))
			{
				SaveConfig("default", false);

				H::Fonts.Reload(Vars::Menu::Scale[DEFAULT_BIND]);
			}
			return false;
		}

		boost::property_tree::ptree tRead;
		read_json(m_sConfigPath + sConfigName + m_sConfigExtension, tRead);

		F::Binds.m_vBinds.clear();
		F::Groups.m_vGroups.clear();

		if (auto tSub = tRead.get_child_optional("Binds"))
		{
			for (const auto& [_, tChild] : *tSub)
			{
				Bind_t tBind = {};
				LoadJson(tChild, "Name", tBind.m_sName);
				LoadJson(tChild, "Type", tBind.m_iType);
				LoadJson(tChild, "Info", tBind.m_iInfo);
				LoadJson(tChild, "Key", tBind.m_iKey);
				LoadJson(tChild, "Enabled", tBind.m_bEnabled);
				LoadJson(tChild, "Visibility", tBind.m_iVisibility);
				LoadJson(tChild, "Not", tBind.m_bNot);
				LoadJson(tChild, "Active", tBind.m_bActive);
				LoadJson(tChild, "Parent", tBind.m_iParent);
				if (F::Binds.m_vBinds.size() == tBind.m_iParent)
					tBind.m_iParent = DEFAULT_BIND - 1; // prevent infinite loop

				F::Binds.m_vBinds.push_back(tBind);
			}
		}
		else
			SDK::Output("Amalgam", "Config binds not found", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);

		if (auto tSub = tRead.get_child_optional("Vars");
			tSub || (tSub = tRead.get_child_optional("ConVars")))
		{
<<<<<<< HEAD
			auto& varTree = *conVars;
			
			// First pass: Load all variables normally
			for (auto& pVar : G::Vars)
=======
			bool bNoSave = GetAsyncKeyState(VK_SHIFT) & 0x8000;
			for (auto& pBase : G::Vars)
>>>>>>> upstream/master
			{
				if (!bNoSave && pBase->m_iFlags & NOSAVE)
					continue;

<<<<<<< HEAD
				// Standard variable loading for all variables (including chat credentials)
				LoadMain(bool, varTree)
				else LoadMain(int, varTree)
				else LoadMain(float, varTree)
				else LoadMain(IntRange_t, varTree)
				else LoadMain(FloatRange_t, varTree)
				else LoadMain(std::string, varTree)
				else LoadMain(VA_LIST(std::vector<std::pair<std::string, Color_t>>), varTree)
				else LoadMain(Color_t, varTree)
				else LoadMain(Gradient_t, varTree)
				else LoadMain(DragBox_t, varTree)
				else LoadMain(WindowBox_t, varTree)
=======
				Load(bool, *tSub)
				else Load(int, *tSub)
				else Load(float, *tSub)
				else Load(IntRange_t, *tSub)
				else Load(FloatRange_t, *tSub)
				else Load(std::string, *tSub)
				else Load(VA_LIST(std::vector<std::pair<std::string, Color_t>>), *tSub)
				else Load(Color_t, *tSub)
				else Load(Gradient_t, *tSub)
				else Load(DragBox_t, *tSub)
				else Load(WindowBox_t, *tSub)
>>>>>>> upstream/master
			}
			
			// Second pass: Handle chat credential decryption and clearing
			bool shouldLoadCredentials = false;
			
			// Get SaveCredentials setting (now that all variables are loaded)
			for (auto& pCheckVar : G::Vars)
			{
				if (pCheckVar->m_sName == "Vars::Chat::SaveCredentials" && pCheckVar->m_iType == typeid(bool).hash_code())
				{
					auto saveCredsVar = pCheckVar->As<bool>();
					if (saveCredsVar && saveCredsVar->Map.find(DEFAULT_BIND) != saveCredsVar->Map.end())
						shouldLoadCredentials = saveCredsVar->Map[DEFAULT_BIND];
					break;
				}
			}
			
			// Apply chat credential handling
			for (auto& pVar : G::Vars)
			{
				if ((pVar->m_sName == "Vars::Chat::Password" || pVar->m_sName == "Vars::Chat::Username" || 
					 pVar->m_sName == "Vars::Chat::Email" || pVar->m_sName == "Vars::Chat::Server" ||
					 pVar->m_sName == "Vars::Chat::Space" || pVar->m_sName == "Vars::Chat::Room") && 
					 pVar->m_iType == typeid(std::string).hash_code())
				{
					auto stringVar = pVar->As<std::string>();
					if (!stringVar) continue;
					
					std::string loadedValue;
					if (stringVar->Map.find(DEFAULT_BIND) != stringVar->Map.end())
						loadedValue = stringVar->Map[DEFAULT_BIND];
					
					if (pVar->m_sName == "Vars::Chat::Password")
					{
						// Special password handling - decrypt if SaveCredentials is enabled
						if (shouldLoadCredentials && !loadedValue.empty())
						{
							// Try to decrypt the password
							std::string decryptedPassword = F::Chat.DecryptPassword(loadedValue);
							if (!decryptedPassword.empty())
							{
								// Successfully decrypted, replace the encrypted value
								stringVar->Map[DEFAULT_BIND] = decryptedPassword;
								stringVar->Value = decryptedPassword;
							}
							// If decryption fails, keep the loaded value (might be legacy plain text)
						}
						else
						{
							// SaveCredentials disabled, clear the password
							stringVar->Map[DEFAULT_BIND] = "";
							stringVar->Value = "";
						}
					}
					else
					{
						// Other chat credentials - clear if SaveCredentials is disabled
						if (!shouldLoadCredentials)
						{
							stringVar->Map[DEFAULT_BIND] = "";
							stringVar->Value = "";
						}
						else
						{
							// If SaveCredentials is enabled, ensure Value field is updated
							stringVar->Value = stringVar->Map[DEFAULT_BIND];
						}
					}
				}
			}
		}

		// Populate m_pVar pointers for binds optimization
		for (int i = 0; i < F::Binds.m_vBinds.size(); i++)
		{
			auto& Bind = F::Binds.m_vBinds.at(i);
			for (auto pVar : G::Vars)
			{
				if (pVar->m_iType == typeid(bool).hash_code())
				{
					if (pVar->As<bool>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(int).hash_code())
				{
					if (pVar->As<int>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(float).hash_code())
				{
					if (pVar->As<float>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(IntRange_t).hash_code())
				{
					if (pVar->As<IntRange_t>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(FloatRange_t).hash_code())
				{
					if (pVar->As<FloatRange_t>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(std::string).hash_code())
				{
					if (pVar->As<std::string>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(VA_LIST(std::vector<std::pair<std::string, Color_t>>)).hash_code())
				{
					if (pVar->As<VA_LIST(std::vector<std::pair<std::string, Color_t>>)>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(Color_t).hash_code())
				{
					if (pVar->As<Color_t>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(Gradient_t).hash_code())
				{
					if (pVar->As<Gradient_t>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(Vec3).hash_code())
				{
					if (pVar->As<Vec3>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(DragBox_t).hash_code())
				{
					if (pVar->As<DragBox_t>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
				else if (pVar->m_iType == typeid(WindowBox_t).hash_code())
				{
					if (pVar->As<WindowBox_t>()->Map.contains(i))
					{
						Bind.m_pVar = pVar;
						break;
					}
				}
			}
		}
		else
			SDK::Output("Amalgam", "Config vars not found", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);

		if (auto tSub = tRead.get_child_optional("Groups"))
		{
			for (auto& [_, tChild] : *tSub)
			{
				Group_t tGroup = {};
				LoadJson(tChild, "Name", tGroup.m_sName);
				LoadJson(tChild, "Color", tGroup.m_tColor);
				LoadJson(tChild, "TagsOverrideColor", tGroup.m_bTagsOverrideColor);
				LoadJson(tChild, "Targets", tGroup.m_iTargets);
				LoadJson(tChild, "Conditions", tGroup.m_iConditions);
				LoadJson(tChild, "Players", tGroup.m_iPlayers);
				LoadJson(tChild, "Buildings", tGroup.m_iBuildings);
				LoadJson(tChild, "Projectiles", tGroup.m_iProjectiles);
				LoadJson(tChild, "ESP", tGroup.m_iESP);
				LoadJson(tChild, "Chams", tGroup.m_tChams);
				LoadJson(tChild, "Glow", tGroup.m_tGlow);
				LoadJson(tChild, "OffscreenArrows", tGroup.m_bOffscreenArrows);
				LoadJson(tChild, "OffscreenArrowsOffset", tGroup.m_iOffscreenArrowsOffset);
				LoadJson(tChild, "OffscreenArrowsMaxDistance", tGroup.m_flOffscreenArrowsMaxDistance);
				LoadJson(tChild, "PickupTimer", tGroup.m_bPickupTimer);
				LoadJson(tChild, "Backtrack", tGroup.m_iBacktrack);
				LoadJson(tChild, "BacktrackChams", tGroup.m_vBacktrackChams);
				LoadJson(tChild, "BacktrackGlow", tGroup.m_tBacktrackGlow);
				LoadJson(tChild, "Trajectory", tGroup.m_iTrajectory);
				LoadJson(tChild, "Sightlines", tGroup.m_iSightlines);

				F::Groups.m_vGroups.push_back(tGroup);
			}
		}
		else
			SDK::Output("Amalgam", "Config groups not found", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);

		F::Binds.SetVars(nullptr, nullptr, false);
		H::Fonts.Reload(Vars::Menu::Scale[DEFAULT_BIND]);

		m_sCurrentConfig = sConfigName; m_sCurrentVisuals = "";
		if (bNotify)
			SDK::Output("Amalgam", std::format("Config {} loaded", sConfigName).c_str(), DEFAULT_COLOR, OUTPUT_CONSOLE | OUTPUT_TOAST | OUTPUT_MENU | OUTPUT_DEBUG);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Load config failed", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
		return false;
	}

	return true;
}

<<<<<<< HEAD
bool CConfigs::SaveChatCredentials(const std::string& sConfigName)
{
	try
	{
		
		// Ensure default values are set in runtime variables if they're empty
		for (auto& pVar : G::Vars)
		{
			if (pVar->m_iType == typeid(std::string).hash_code())
			{
				auto stringVar = pVar->As<std::string>();
				if (stringVar->Map.find(DEFAULT_BIND) != stringVar->Map.end())
				{
					std::string& currentValue = stringVar->Map[DEFAULT_BIND];
					
					// Set default values if empty
					if (currentValue.empty())
					{
						if (pVar->m_sName == "Vars::Chat::Server")
							currentValue = "matrix.org";
						else if (pVar->m_sName == "Vars::Chat::Space")
							currentValue = "amalgam-comp";
						else if (pVar->m_sName == "Vars::Chat::Room")
							currentValue = "talk";
					}
				}
			}
		}
		
		// Read existing config if it exists
		boost::property_tree::ptree readTree;
		if (std::filesystem::exists(m_sConfigPath + sConfigName + m_sConfigExtension))
		{
			read_json(m_sConfigPath + sConfigName + m_sConfigExtension, readTree);
		}
		
		// Get or create the ConVars section
		boost::property_tree::ptree conVars;
		if (auto existingConVars = readTree.get_child_optional("ConVars"))
		{
			conVars = *existingConVars;
		}
		
		// Update only chat credential variables
		std::vector<std::string> chatVars = {
			"Vars::Chat::Server", "Vars::Chat::Username", "Vars::Chat::Password", 
			"Vars::Chat::Email", "Vars::Chat::Space", "Vars::Chat::Room", "Vars::Chat::SaveCredentials"
		};
		
		for (auto& pVar : G::Vars)
		{
			// Only process chat credential variables
			if (std::find(chatVars.begin(), chatVars.end(), pVar->m_sName) == chatVars.end())
				continue;
			
			// Debug: Show we found a chat variable  
			SDK::Output("Amalgam", std::format("Found chat var: {}", pVar->m_sName).c_str(), { 255, 255, 0 }, true, true);
			
				
			if (pVar->m_iType != typeid(std::string).hash_code() && pVar->m_iType != typeid(bool).hash_code())
				continue;
			
			// Debug: Show current value in this variable
			if (pVar->m_iType == typeid(std::string).hash_code())
			{
				auto stringVar = pVar->As<std::string>();
				if (stringVar->Map.find(DEFAULT_BIND) != stringVar->Map.end())
				{
					SDK::Output("Amalgam", std::format("  {} current value: '{}'", pVar->m_sName, stringVar->Map[DEFAULT_BIND]).c_str(), { 0, 255, 255 }, true, true);
				}
			}
			else if (pVar->m_iType == typeid(bool).hash_code())
			{
				auto boolVar = pVar->As<bool>();
				if (boolVar->Map.find(DEFAULT_BIND) != boolVar->Map.end())
				{
					SDK::Output("Amalgam", std::format("  {} current value: {}", pVar->m_sName, boolVar->Map[DEFAULT_BIND] ? "true" : "false").c_str(), { 0, 255, 255 }, true, true);
				}
			}
				
			// Special handling for Chat credentials - encrypt password and clear others when SaveCredentials is disabled
			if ((pVar->m_sName == "Vars::Chat::Password" || pVar->m_sName == "Vars::Chat::Username" || 
				 pVar->m_sName == "Vars::Chat::Email" || pVar->m_sName == "Vars::Chat::Server" ||
				 pVar->m_sName == "Vars::Chat::Space" || pVar->m_sName == "Vars::Chat::Room") && pVar->m_iType == typeid(std::string).hash_code())
			{
				auto stringVar = pVar->As<std::string>();
				std::string originalValue;
				
				// Get the current value
				if (stringVar->Map.find(DEFAULT_BIND) != stringVar->Map.end())
					originalValue = stringVar->Map[DEFAULT_BIND];
				
				// Check if SaveCredentials is enabled
				bool shouldSaveCredentials = false;
				for (auto& pCheckVar : G::Vars)
				{
					if (pCheckVar->m_sName == "Vars::Chat::SaveCredentials" && pCheckVar->m_iType == typeid(bool).hash_code())
					{
						auto saveCredsVar = pCheckVar->As<bool>();
						if (saveCredsVar->Map.find(DEFAULT_BIND) != saveCredsVar->Map.end())
							shouldSaveCredentials = saveCredsVar->Map[DEFAULT_BIND];
						break;
					}
				}
				
				boost::property_tree::ptree mapTree;
				
				if (pVar->m_sName == "Vars::Chat::Password")
				{
					// Special password handling - save if SaveCredentials is enabled
					if (shouldSaveCredentials && !originalValue.empty())
					{
						// "Encrypt" the password (currently plaintext for simplicity)
						std::string savedPassword = F::Chat.EncryptPassword(originalValue);
						mapTree.put(std::to_string(DEFAULT_BIND), savedPassword);
					}
					else
					{
						// SaveCredentials disabled or password empty, save empty string
						mapTree.put(std::to_string(DEFAULT_BIND), "");
					}
				}
				else
				{
					// Other chat credentials handling
					if (pVar->m_sName == "Vars::Chat::Username" || pVar->m_sName == "Vars::Chat::Email")
					{
						// Username and Email: save if SaveCredentials is enabled, otherwise save empty
						if (shouldSaveCredentials)
						{
							mapTree.put(std::to_string(DEFAULT_BIND), originalValue);
						}
						else
						{
							// SaveCredentials disabled, save empty string
							mapTree.put(std::to_string(DEFAULT_BIND), "");
						}
					}
					else
					{
						// Server, Space, Room: always save current value, but restore defaults if empty
						std::string valueToSave = originalValue;
						
						// Restore default values if empty
						if (valueToSave.empty())
						{
							if (pVar->m_sName == "Vars::Chat::Server")
								valueToSave = "matrix.org";
							else if (pVar->m_sName == "Vars::Chat::Space")
								valueToSave = "amalgam-comp";
							else if (pVar->m_sName == "Vars::Chat::Room")
								valueToSave = "talk";
						}
						
						mapTree.put(std::to_string(DEFAULT_BIND), valueToSave);
					}
				}
				
				conVars.put_child(pVar->m_sName, mapTree);
			}
			else if (pVar->m_sName == "Vars::Chat::SaveCredentials" && pVar->m_iType == typeid(bool).hash_code())
			{
				// Always save the SaveCredentials setting
				auto boolVar = pVar->As<bool>();
				boost::property_tree::ptree mapTree;
				
				bool value = false;
				if (boolVar->Map.find(DEFAULT_BIND) != boolVar->Map.end())
					value = boolVar->Map[DEFAULT_BIND];
				
				mapTree.put(std::to_string(DEFAULT_BIND), value);
				conVars.put_child(pVar->m_sName, mapTree);
			}
		}
		
		// Update the ConVars section in the tree
		readTree.put_child("ConVars", conVars);
		
		// Write the updated config back
		write_json(m_sConfigPath + sConfigName + m_sConfigExtension, readTree);
		
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool CConfigs::SaveChatCredentials(const std::string& sConfigName, const std::string& username, const std::string& password, const std::string& email)
{
	try
	{
		// Temporarily set the Vars values to the provided parameters
		std::string oldUsername = Vars::Chat::Username.Value;
		std::string oldPassword = Vars::Chat::Password.Value;
		std::string oldEmail = Vars::Chat::Email.Value;
		
		// Set both .Value and the actual Map storage that config system uses
		Vars::Chat::Username.Value = username;
		Vars::Chat::Password.Value = password;
		Vars::Chat::Email.Value = email;
		
		// Also set the Map storage directly (this is what the config system actually reads)
		for (auto& pVar : G::Vars)
		{
			if (pVar->m_sName == "Vars::Chat::Username" && pVar->m_iType == typeid(std::string).hash_code())
			{
				auto stringVar = pVar->As<std::string>();
				stringVar->Map[DEFAULT_BIND] = username;
			}
			else if (pVar->m_sName == "Vars::Chat::Password" && pVar->m_iType == typeid(std::string).hash_code())
			{
				auto stringVar = pVar->As<std::string>();
				stringVar->Map[DEFAULT_BIND] = password;
			}
			else if (pVar->m_sName == "Vars::Chat::Email" && pVar->m_iType == typeid(std::string).hash_code())
			{
				auto stringVar = pVar->As<std::string>();
				stringVar->Map[DEFAULT_BIND] = email;
			}
		}
		
		// Call the regular SaveChatCredentials function
		bool result = SaveChatCredentials(sConfigName);
		
		// Restore the original values (though they should be the same now)
		Vars::Chat::Username.Value = oldUsername;
		Vars::Chat::Password.Value = oldPassword;
		Vars::Chat::Email.Value = oldEmail;
		
		return result;
	}
	catch (...)
	{
		return false;
	}
}

#define SaveRegular(type, tree) SaveJson(tree, pVar->m_sName, pVar->As<type>()->Map[DEFAULT_BIND])
#define SaveMisc(type, tree) if (IsType(type)) SaveRegular(type, tree);
#define LoadRegular(type, tree) LoadJson(tree, pVar->m_sName, pVar->As<type>()->Map[DEFAULT_BIND])
#define LoadMisc(type, tree) if (IsType(type)) LoadRegular(type, tree);
=======
template <class T>
static inline void SaveMiscMain(BaseVar*& pBase, boost::property_tree::ptree& tTree)
{
	F::Configs.SaveJson(tTree, pBase->Name(), pBase->As<T>()->Map[DEFAULT_BIND]);
}
#define SaveMisc(t, j) if (IsType(t)) SaveMiscMain<t>(pBase, j);

template <class T>
static inline void LoadMiscMain(BaseVar*& pBase, boost::property_tree::ptree& tTree)
{
	F::Configs.LoadJson(tTree, pBase->Name(), pBase->As<T>(), DEFAULT_BIND);
}
#define LoadMisc(t, j) if (IsType(t)) LoadMiscMain<t>(pBase, j);
>>>>>>> upstream/master

bool CConfigs::SaveVisual(const std::string& sConfigName, bool bNotify)
{
	try
	{
		boost::property_tree::ptree tWrite;

		{
			boost::property_tree::ptree tSub;
			bool bNoSave = GetAsyncKeyState(VK_SHIFT) & 0x8000;
			for (auto& pBase : G::Vars)
			{
				if (!(pBase->m_iFlags & VISUAL) || !bNoSave && pBase->m_iFlags & NOSAVE)
					continue;

				SaveMisc(bool, tSub)
				else SaveMisc(int, tSub)
				else SaveMisc(float, tSub)
				else SaveMisc(IntRange_t, tSub)
				else SaveMisc(FloatRange_t, tSub)
				else SaveMisc(std::string, tSub)
				else SaveMisc(VA_LIST(std::vector<std::pair<std::string, Color_t>>), tSub)
				else SaveMisc(Color_t, tSub)
				else SaveMisc(Gradient_t, tSub)
				else SaveMisc(DragBox_t, tSub)
				else SaveMisc(WindowBox_t, tSub)
			}
			tWrite.put_child("Vars", tSub);
		}

		{
			boost::property_tree::ptree tSub;
			for (int iID = 0; iID < F::Groups.m_vGroups.size(); iID++)
			{
				auto& tGroup = F::Groups.m_vGroups[iID];

				boost::property_tree::ptree tChild;
				SaveJson(tChild, "Name", tGroup.m_sName);
				SaveJson(tChild, "Color", tGroup.m_tColor);
				SaveJson(tChild, "TagsOverrideColor", tGroup.m_bTagsOverrideColor);
				SaveJson(tChild, "Targets", tGroup.m_iTargets);
				SaveJson(tChild, "Conditions", tGroup.m_iConditions);
				SaveJson(tChild, "Players", tGroup.m_iPlayers);
				SaveJson(tChild, "Buildings", tGroup.m_iBuildings);
				SaveJson(tChild, "Projectiles", tGroup.m_iProjectiles);
				SaveJson(tChild, "ESP", tGroup.m_iESP);
				SaveJson(tChild, "Chams", tGroup.m_tChams);
				SaveJson(tChild, "Glow", tGroup.m_tGlow);
				SaveJson(tChild, "OffscreenArrows", tGroup.m_bOffscreenArrows);
				SaveJson(tChild, "OffscreenArrowsOffset", tGroup.m_iOffscreenArrowsOffset);
				SaveJson(tChild, "OffscreenArrowsMaxDistance", tGroup.m_flOffscreenArrowsMaxDistance);
				SaveJson(tChild, "PickupTimer", tGroup.m_bPickupTimer);
				SaveJson(tChild, "Backtrack", tGroup.m_iBacktrack);
				SaveJson(tChild, "BacktrackChams", tGroup.m_vBacktrackChams);
				SaveJson(tChild, "BacktrackGlow", tGroup.m_tBacktrackGlow);
				SaveJson(tChild, "Trajectory", tGroup.m_iTrajectory);
				SaveJson(tChild, "Sightlines", tGroup.m_iSightlines);

				tSub.put_child(std::to_string(iID), tChild);
				if (F::Groups.m_vGroups.size() >= sizeof(int) * 8)
					break;
			}
			tWrite.put_child("Groups", tSub);
		}

		write_json(m_sVisualsPath + sConfigName + m_sConfigExtension, tWrite);

		if (bNotify)
			SDK::Output("Amalgam", std::format("Visual config {} saved", sConfigName).c_str(), DEFAULT_COLOR, OUTPUT_CONSOLE | OUTPUT_TOAST | OUTPUT_MENU | OUTPUT_DEBUG);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Save visuals failed", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
		return false;
	}
	return true;
}

bool CConfigs::LoadVisual(const std::string& sConfigName, bool bNotify)
{
	try
	{
		if (!std::filesystem::exists(m_sVisualsPath + sConfigName + m_sConfigExtension))
			return false;

		boost::property_tree::ptree tRead;
		read_json(m_sVisualsPath + sConfigName + m_sConfigExtension, tRead);

		F::Groups.m_vGroups.clear();

		if (auto tSub = tRead.get_child_optional("Vars");
			tSub || (tSub = tRead))
		{
			bool bNoSave = GetAsyncKeyState(VK_SHIFT) & 0x8000;
			for (auto& pBase : G::Vars)
			{
				if (!(pBase->m_iFlags & VISUAL) || !bNoSave && pBase->m_iFlags & NOSAVE)
					continue;

				LoadMisc(bool, *tSub)
				else LoadMisc(int, *tSub)
				else LoadMisc(float, *tSub)
				else LoadMisc(IntRange_t, *tSub)
				else LoadMisc(FloatRange_t, *tSub)
				else LoadMisc(std::string, *tSub)
				else LoadMisc(VA_LIST(std::vector<std::pair<std::string, Color_t>>), *tSub)
				else LoadMisc(Color_t, *tSub)
				else LoadMisc(Gradient_t, *tSub)
				else LoadMisc(DragBox_t, *tSub)
				else LoadMisc(WindowBox_t, *tSub)
			}
		}
		else
			SDK::Output("Amalgam", "Config vars not found", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);

		if (auto tSub = tRead.get_child_optional("Groups"))
		{
			for (auto& [_, tChild] : *tSub)
			{
				Group_t tGroup = {};
				LoadJson(tChild, "Name", tGroup.m_sName);
				LoadJson(tChild, "Color", tGroup.m_tColor);
				LoadJson(tChild, "TagsOverrideColor", tGroup.m_bTagsOverrideColor);
				LoadJson(tChild, "Targets", tGroup.m_iTargets);
				LoadJson(tChild, "Conditions", tGroup.m_iConditions);
				LoadJson(tChild, "Players", tGroup.m_iPlayers);
				LoadJson(tChild, "Buildings", tGroup.m_iBuildings);
				LoadJson(tChild, "Projectiles", tGroup.m_iProjectiles);
				LoadJson(tChild, "ESP", tGroup.m_iESP);
				LoadJson(tChild, "Chams", tGroup.m_tChams);
				LoadJson(tChild, "Glow", tGroup.m_tGlow);
				LoadJson(tChild, "OffscreenArrows", tGroup.m_bOffscreenArrows);
				LoadJson(tChild, "OffscreenArrowsOffset", tGroup.m_iOffscreenArrowsOffset);
				LoadJson(tChild, "OffscreenArrowsMaxDistance", tGroup.m_flOffscreenArrowsMaxDistance);
				LoadJson(tChild, "PickupTimer", tGroup.m_bPickupTimer);
				LoadJson(tChild, "Backtrack", tGroup.m_iBacktrack);
				LoadJson(tChild, "BacktrackChams", tGroup.m_vBacktrackChams);
				LoadJson(tChild, "BacktrackGlow", tGroup.m_tBacktrackGlow);
				LoadJson(tChild, "Trajectory", tGroup.m_iTrajectory);
				LoadJson(tChild, "Sightlines", tGroup.m_iSightlines);

				F::Groups.m_vGroups.push_back(tGroup);
			}
		}
		else
			SDK::Output("Amalgam", "Config groups not found", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);

		F::Binds.SetVars(nullptr, nullptr, false);

		m_sCurrentVisuals = sConfigName;
		if (bNotify)
			SDK::Output("Amalgam", std::format("Visual config {} loaded", sConfigName).c_str(), DEFAULT_COLOR, OUTPUT_CONSOLE | OUTPUT_TOAST | OUTPUT_MENU | OUTPUT_DEBUG);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Load visuals failed", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
		return false;
	}
	return true;
}

template <class T>
static inline void ResetMain(BaseVar*& pBase)
{
	auto pVar = pBase->As<T>();

	pVar->Map = { { DEFAULT_BIND, pVar->Default } };
}
#define Reset(t) if (IsType(t)) ResetMain<t>(pBase);

void CConfigs::DeleteConfig(const std::string& sConfigName, bool bNotify)
{
	try
	{
		if (FNV1A::Hash32(sConfigName.c_str()) == FNV1A::Hash32Const("default"))
		{
			ResetConfig(sConfigName);
			return;
		}

		std::filesystem::remove(m_sConfigPath + sConfigName + m_sConfigExtension);
		if (FNV1A::Hash32(m_sCurrentConfig.c_str()) == FNV1A::Hash32(sConfigName.c_str()))
			LoadConfig("default", false);

		if (bNotify)
			SDK::Output("Amalgam", std::format("Config {} deleted", sConfigName).c_str(), DEFAULT_COLOR, OUTPUT_CONSOLE | OUTPUT_TOAST | OUTPUT_MENU | OUTPUT_DEBUG);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Remove config failed", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
	}
}

void CConfigs::ResetConfig(const std::string& sConfigName, bool bNotify)
{
	try
	{
		F::Binds.m_vBinds.clear();
		F::Groups.m_vGroups.clear();

		bool bNoSave = GetAsyncKeyState(VK_SHIFT) & 0x8000;
		for (auto& pBase : G::Vars)
		{
			if (!bNoSave && pBase->m_iFlags & NOSAVE)
				continue;

			Reset(bool)
			else Reset(int)
			else Reset(float)
			else Reset(IntRange_t)
			else Reset(FloatRange_t)
			else Reset(std::string)
			else Reset(std::vector<std::string>)
			else Reset(Color_t)
			else Reset(Gradient_t)
			else Reset(DragBox_t)
			else Reset(WindowBox_t)
		}

		SaveConfig(sConfigName, false);
		F::Binds.SetVars(nullptr, nullptr, false);
		H::Fonts.Reload(Vars::Menu::Scale[DEFAULT_BIND]);

		if (bNotify)
			SDK::Output("Amalgam", std::format("Config {} reset", sConfigName).c_str(), DEFAULT_COLOR, OUTPUT_CONSOLE | OUTPUT_TOAST | OUTPUT_MENU | OUTPUT_DEBUG);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Reset config failed", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
	}
}

void CConfigs::DeleteVisual(const std::string& sConfigName, bool bNotify)
{
	try
	{
		std::filesystem::remove(m_sVisualsPath + sConfigName + m_sConfigExtension);

		if (bNotify)
			SDK::Output("Amalgam", std::format("Visual config {} deleted", sConfigName).c_str(), DEFAULT_COLOR, OUTPUT_CONSOLE | OUTPUT_TOAST | OUTPUT_MENU | OUTPUT_DEBUG);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Remove visuals failed", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
	}
}

void CConfigs::ResetVisual(const std::string& sConfigName, bool bNotify)
{
	try
	{
		F::Groups.m_vGroups.clear();

		bool bNoSave = GetAsyncKeyState(VK_SHIFT) & 0x8000;
		for (auto& pBase : G::Vars)
		{
			if (!(pBase->m_iFlags & VISUAL) || !bNoSave && pBase->m_iFlags & NOSAVE)
				continue;

			Reset(bool)
			else Reset(int)
			else Reset(float)
			else Reset(IntRange_t)
			else Reset(FloatRange_t)
			else Reset(std::string)
			else Reset(std::vector<std::string>)
			else Reset(Color_t)
			else Reset(Gradient_t)
			else Reset(DragBox_t)
			else Reset(WindowBox_t)
		}

		SaveVisual(sConfigName, false);
		F::Binds.SetVars(nullptr, nullptr, false);

		if (bNotify)
			SDK::Output("Amalgam", std::format("Visual config {} reset", sConfigName).c_str(), DEFAULT_COLOR, OUTPUT_CONSOLE | OUTPUT_TOAST | OUTPUT_MENU | OUTPUT_DEBUG);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Reset visuals failed", ALTERNATE_COLOR, OUTPUT_CONSOLE | OUTPUT_MENU | OUTPUT_DEBUG);
	}
}