-- Bundled by luabundle {"version":"1.7.0"}
local __bundle_require, __bundle_loaded, __bundle_register, __bundle_modules = (function(superRequire)
	local loadingPlaceholder = {[{}] = true}

	local register
	local modules = {}

	local require
	local loaded = {}

	register = function(name, body)
		if not modules[name] then
			modules[name] = body
		end
	end

	require = function(name)
		local loadedModule = loaded[name]

		if loadedModule then
			if loadedModule == loadingPlaceholder then
				return nil
			end
		else
			if not modules[name] then
				if not superRequire then
					local identifier = type(name) == 'string' and '\"' .. name .. '\"' or tostring(name)
					error('Tried to require ' .. identifier .. ', but no such module has been registered')
				else
					return superRequire(name)
				end
			end

			loaded[name] = loadingPlaceholder
			loadedModule = modules[name](require, loaded, register, modules)
			loaded[name] = loadedModule
		end

		return loadedModule
	end

	return require, loaded, register, modules
end)(require)
__bundle_register("__root", function(require, _LOADED, __bundle_register, __bundle_modules)
local version = "10"

local settings = require("src.settings")
assert(settings, "[PROJ AIMBOT] Settings module failed to load!")

local wep_utils = require("src.utils.weapon_utils")
assert(wep_utils, "[PROJ AIMBOT] Weapon utils module failed to load!")
printc(150, 255, 150, 255, "[PROJ AIMBOT] Weapon utils loaded")

local math_utils = require("src.utils.math")
assert(math_utils, "[PROJ AIMBOT] Math utils module failed to load!")
printc(150, 255, 150, 255, "[PROJ AIMBOT] Math utils loaded")

local ent_utils = require("src.utils.entity")
assert(ent_utils, "[PROJ AIMBOT] Entity utils module failed to load!")
printc(150, 255, 150, 255, "[PROJ AIMBOT] Entity utils loaded")

local player_sim = require("src.simulation.player")
assert(player_sim, "[PROJ AIMBOT] Player prediction module failed to load")
printc(150, 255, 150, 255, "[PROJ AIMBOT] Player prediction module loaded")

---@type ProjectileSimulation
local proj_sim = require("src.simulation.proj")
assert(proj_sim, "[PROJ AIMBOT] Projectile prediction module failed to load")
printc(150, 255, 150, 255, "[PROJ AIMBOT] Projectile prediction module loaded")

local GetProjectileInformation = require("src.projectile_info")
assert(GetProjectileInformation, "[PROJ AIMBOT] GetProjectileInformation module failed to load")
printc(150, 255, 150, 255, "[PROJ AIMBOT] GetProjectileInformation module loaded")

local Visuals = require("src.visuals")
assert(Visuals, "[PROJ AIMBOT] Visuals module failed to load")
printc(150, 255, 150, 255, "[PROJ AIMBOT] Visuals module loaded")

local multipoint = require("src.multipoint")
assert(multipoint, "[PROJ AIMBOT] Multipoint module failed to load")
printc(150, 255, 150, 255, "[PROJ AIMBOT] Multipoint module loaded")

local visuals = Visuals.new()

local menu = require("src.gui")
menu.init(version)

---@type Entity?, Entity?, WeaponInfo?
local plocal, weapon, weaponInfo = nil, nil, nil

local vAngles = nil

---@param outputList table<integer, EntityInfo>
local function ProcessClass(className, includeTeam, outputList)
    if plocal == nil then
        return
    end

    local list = entities.FindByClass(className)

    for _, entity in pairs(list) do
        if entity:IsDormant() or (entity:IsPlayer() and not entity:IsAlive() or entity:GetHealth() <= 0) then
            goto continue
        end

        if not includeTeam and entity:GetTeamNumber() == plocal:GetTeamNumber() then
            goto continue
        end

        outputList[#outputList + 1] = {
            index = entity:GetIndex(),
            health = entity:GetHealth(),
            maxs = entity:GetMaxs(),
            mins = entity:GetMins(),
            velocity = entity:EstimateAbsVelocity() or Vector3(),
            maxspeed = entity:GetPropFloat("m_flMaxspeed") or 0,
            angvelocity = player_sim.GetSmoothedAngularVelocity(entity) or 0,
            stepsize = entity:GetPropFloat("m_flStepSize") or 18,
            origin = entity:GetAbsOrigin(),
            name = entity:GetName() or "unnamed",
            fov = math.huge,
            dist = math.huge,
            friction = entity:GetPropFloat("localdata", "m_flFriction") or 1.0,
            team = entity:GetTeamNumber(),
            score = 0,
            class = entity:GetPropInt("m_iClass") or nil,
            isUbered = entity:InCond(E_TFCOND.TFCond_Ubercharged),
            maxhealth = entity:GetMaxBuffedHealth(),
            timesecs = math.huge,
        }

        ::continue::
    end
end

---@param data EntityInfo
---@return number
local function CalculateScore(data, eyePos, viewAngles, includeTeam)
    if plocal == nil then
        return 0
    end

    local score = 0
    local w = settings.weights

    --- distance (closer = higher score)
    if w.distance_weight > 0 then
        local dist_score = 1 - math.min(data.dist / settings.max_distance, 1)
        score = score + dist_score * w.distance_weight
    end

    --- health (lower health = higher score)
    if w.health_weight > 0 then
        local health_score = 1 - math.min(data.health / data.maxhealth, 1)
        score = score + health_score * w.health_weight
    end

    --- lower fov = better
    if w.fov_weight > 0 and settings.onfov_only == false then
        local angle = math_utils.PositionAngles(eyePos, data.finalPos or data.origin)
        if angle then
            local fov = math_utils.AngleFov(viewAngles, angle)
            local fov_score = 1 - math.min(fov / settings.fov, 1)
            score = score + fov_score * w.fov_weight
        end
    end

    --- visibility (if visible = full weight)
    if w.visibility_weight > 0 then
        score = score + w.visibility_weight
    end

    --- speed (slower = easier to hit)
    if w.speed_weight and w.speed_weight > 0 then
        local speed = data.velocity:Length()
        local speed_score = 1 - math.min(speed / data.maxspeed, 1) -- normalize
        score = score + speed_score * w.speed_weight
    end

    --- class priority
    if data.class and data.class == E_Character.TF2_Medic then
        score = score + w.medic_priority
    elseif data.class and data.class == E_Character.TF2_Sniper then
        score = score + w.sniper_priority
    end

    --- uber penalty (skip ubercharged targets)
    if data.isUbered and w.uber_penalty then
        score = score + w.uber_penalty
    end

    --- favor a lot our team
    if includeTeam and data.team == plocal:GetTeamNumber() then
        score = score + settings.weights.teammate_weight
    end

    return score
end

--- Returns a sorted table (:))
---@return table<integer, EntityInfo>?
local function GetTargetsSmart(includeTeam)
    if plocal == nil or weapon == nil or weaponInfo == nil then
        return nil
    end

    local startList = {}

    -- collect entities
    if settings.ents["aim players"] then
        ProcessClass("CTFPlayer", includeTeam, startList)
    end

    if settings.ents["aim sentries"] then
        ProcessClass("CObjectSentrygun", includeTeam, startList)
    end

    if settings.ents["aim dispensers"] then
        ProcessClass("CObjectDispenser", includeTeam, startList)
    end

    if settings.ents["aim teleporters"] then
        ProcessClass("CObjectTeleporter", includeTeam, startList)
    end

    --- make a early return here
    --- if there are no valid entities
    --- then dont even bother
    if #startList == 0 then
        return startList
    end

    local lpPos = plocal:GetAbsOrigin()
    local eyePos = lpPos + plocal:GetPropVector("localdata", "m_vecViewOffset[0]")
    local viewAngles = engine.GetViewAngles()
    local projectileSpeed = weaponInfo:GetVelocity(0):Length2D()

    local candidates = {}

    --- basic filtering
    for _, data in ipairs(startList) do
        local ent = entities.GetByIndex(data.index)
        if not ent then goto continue end

        local dist = (data.origin - lpPos):Length()
        if dist > settings.max_distance then goto continue end
        data.dist = dist

        if settings.onfov_only then
            local angle = math_utils.PositionAngles(eyePos, data.origin)
            if angle then
                local fov = math_utils.AngleFov(viewAngles, angle)
                if fov > settings.fov then goto continue end
            end
        end

        candidates[#candidates + 1] = data

        ::continue::
    end

    --- another early return
    --- dont bother if we have no candidates
    if #candidates == 0 then
        return candidates
    end

    local det_mult = weapon:AttributeHookFloat("sticky_arm_time") or 1.0
    local detonate_time = (settings.sim.use_detonate_time and weapon:GetWeaponID() == E_WeaponBaseID.TF_WEAPON_PIPEBOMBLAUNCHER) and
        0.7 * det_mult or 0
    local choked_time = clientstate:GetChokedCommands()

    local final_targets = {}

    for _, data in ipairs(candidates) do
        local ent = entities.GetByIndex(data.index)
        if not ent then goto continue end

        local travel_time_est = data.dist / projectileSpeed
        local total_time = travel_time_est + detonate_time
        local finalPos = Vector3(data.origin:Unpack())

        -- simulate player path if moving
        if data.velocity:Length() > 0 then
            data.origin.z = data.origin.z + 1 --- smol offset to fix a issue
            local time_ticks = math.ceil((total_time * 66.67) + 0.5) + choked_time + 1
            data.sim_path = player_sim.Run(data, ent, data.origin, time_ticks)
            if data.sim_path and #data.sim_path > 0 then
                finalPos = data.sim_path[#data.sim_path]
                travel_time_est = (finalPos - eyePos):Length() / projectileSpeed
                total_time = travel_time_est + detonate_time
            end
        else
            data.sim_path = { data.origin }
        end

        if total_time > settings.max_sim_time then goto continue end

        local visible, mpFinalPos = multipoint.Run(ent, weapon, weaponInfo, eyePos, finalPos)
        if not visible then goto continue end
        if mpFinalPos then finalPos = mpFinalPos end

        data.dist = (finalPos - lpPos):Length()
        data.finalPos = finalPos

        data.score = CalculateScore(data, eyePos, viewAngles, includeTeam)
        data.timesecs = total_time

        if data.score < (settings.min_score or 0) then
            goto continue
        end

        final_targets[#final_targets + 1] = data

        ::continue::
    end

    if #final_targets == 0 then
        return final_targets
    end

    -- sort by weighted score (highest first)
    table.sort(final_targets, function(a, b)
        return (a.score or 0) > (b.score or 0)
    end)

    -- limit number of targets
    local max_targets = settings.max_targets or 2
    if #final_targets > max_targets then
        for i = max_targets + 1, #final_targets do
            final_targets[i] = nil
        end
    end

    return final_targets
end

--- Normal closest to crosshair mode
--- with no weights or anything like that
---@return table<integer, EntityInfo>
local function GetTargetsNormal(includeTeam)
    if plocal == nil or weapon == nil or weaponInfo == nil then
        return {}
    end

    ---@type table<integer, EntityInfo>
    local startList = {}

    -- collect entities
    if settings.ents["aim players"] then
        ProcessClass("CTFPlayer", includeTeam, startList)
    end
    if settings.ents["aim sentries"] then
        ProcessClass("CObjectSentrygun", includeTeam, startList)
    end
    if settings.ents["aim dispensers"] then
        ProcessClass("CObjectDispenser", includeTeam, startList)
    end
    if settings.ents["aim teleporters"] then
        ProcessClass("CObjectTeleporter", includeTeam, startList)
    end

    if #startList == 0 then
        return {}
    end

    local lpPos = plocal:GetAbsOrigin()
    local eyePos = lpPos + plocal:GetPropVector("localdata", "m_vecViewOffset[0]")
    local viewAngles = engine.GetViewAngles()
    local projectileSpeed = weaponInfo:GetVelocity(0):Length2D()

    local det_mult = weapon:AttributeHookFloat("sticky_arm_time") or 1.0
    local detonate_time = (settings.sim.use_detonate_time and weapon:GetWeaponID() == E_WeaponBaseID.TF_WEAPON_PIPEBOMBLAUNCHER) and
        0.7 * det_mult or 0
    local choked_time = clientstate:GetChokedCommands()

    local candidates = {}

    for _, data in ipairs(startList) do
        local dist = (data.origin - lpPos):Length()
        if dist > settings.max_distance then
            goto continue
        end

        local angle = math_utils.PositionAngles(eyePos, data.origin)
        if angle then
            local fov = math_utils.AngleFov(viewAngles, angle)
            if fov > settings.fov then
                goto continue
            end

            data.fov = fov
            data.dist = dist
            data.finalPos = data.origin

            local ent = entities.GetByIndex(data.index)
            if ent then
                local travel_time_est = dist / projectileSpeed
                local total_time = travel_time_est + detonate_time
                local finalPos = Vector3(data.origin:Unpack())

                if data.velocity:Length() > 0 then
                    data.origin.z = data.origin.z + 1
                    local time_ticks = math.ceil((total_time * 66.67) + 0.5) + choked_time + 1
                    data.sim_path = player_sim.Run(data, ent, data.origin, time_ticks)
                    if data.sim_path and #data.sim_path > 0 then
                        finalPos = data.sim_path[#data.sim_path]
                        travel_time_est = (finalPos - eyePos):Length() / projectileSpeed
                        total_time = travel_time_est + detonate_time
                    end
                else
                    data.sim_path = { data.origin }
                end

                if total_time > settings.max_sim_time then
                    goto continue
                end

                -- multipoint
                local visible, mpFinalPos = multipoint.Run(ent, weapon, weaponInfo, eyePos, finalPos)
                if not visible then
                    goto continue
                end
                if mpFinalPos then
                    finalPos = mpFinalPos
                end

                data.dist = (finalPos - lpPos):Length()
                data.finalPos = finalPos
                data.score = 1.0 -- dummy, since sorting is by fov
            end

            candidates[#candidates + 1] = data
        end
        ::continue::
    end

    if #candidates == 0 then
        return {}
    end

    table.sort(candidates, function(a, b)
        return (a.fov or math.huge) < (b.fov or math.huge)
    end)

    local max_targets = settings.max_targets or 2
    if #candidates > max_targets then
        for i = max_targets + 1, #candidates do
            candidates[i] = nil
        end
    end

    return candidates
end

---@param cmd UserCmd
local function GetWeaponElapsedCharge(cmd)
    if weapon == nil or weaponInfo == nil then
        return 0.0
    end

    if weaponInfo.m_bCharges == false then
        return 0.0
    end

    local begintime = weapon:GetChargeBeginTime()
    local maxtime   = weapon:GetChargeMaxTime()
    local elapsed   = globals.CurTime() - begintime

    if elapsed > maxtime and (cmd.buttons & IN_ATTACK) == 0 then
        return 0.0
    end

    if weapon:GetPropInt("m_iItemDefinitionIndex") == 996 then
        elapsed = math.max(0, 1 - elapsed)
    end

    return elapsed
end

---@param cmd UserCmd
local function CreateMove(cmd)
    if clientstate.GetNetChannel() == nil then
        return
    end

    vAngles = nil

    if settings.enabled == false then
        return
    end

    if plocal == nil or weapon == nil or weaponInfo == nil then
        return
    end

    if (engine.IsChatOpen() or engine.Con_IsVisible() or engine.IsGameUIVisible()) == true then
        return
    end

    local isBeggarsBazooka = weapon:GetPropInt("m_iItemDefinitionIndex") == 730

    if not isBeggarsBazooka and not wep_utils.CanShoot() then
        return
    end

    if gui.GetValue("aim key") ~= 0 and input.IsButtonDown(gui.GetValue("aim key")) == false then
        return
    end

    if plocal:InCond(E_TFCOND.TFCond_Taunting) then
        return
    end

    if plocal:InCond(E_TFCOND.TFCond_HalloweenKart) then
        return
    end

    local weaponID = weapon:GetWeaponID()

    local includeTeam = weaponID == E_WeaponBaseID.TF_WEAPON_CROSSBOW
        or weaponID == E_WeaponBaseID.TF_WEAPON_LUNCHBOX

    ---@type table<integer, EntityInfo>?
    local targets = settings.smart_targeting and GetTargetsSmart(includeTeam) or GetTargetsNormal(includeTeam)
    if targets == nil or #targets == 0 then
        return
    end

    ---@type EulerAngles?
    local angle = nil

    local weaponNoPSilent = weaponID == E_WeaponBaseID.TF_WEAPON_LUNCHBOX
        or weaponID == E_WeaponBaseID.TF_WEAPON_FLAME_BALL
        or weaponID == E_WeaponBaseID.TF_WEAPON_BAT_WOOD
        or weaponID == E_WeaponBaseID.TF_WEAPON_JAR_MILK
        or weaponID == E_WeaponBaseID.TF_WEAPON_JAR
        or weaponID == E_WeaponBaseID.TF_WEAPON_BAT_GIFTWRAP

    local in_attack2 = weaponID == E_WeaponBaseID.TF_WEAPON_LUNCHBOX
        or weaponID == E_WeaponBaseID.TF_WEAPON_BAT_WOOD
        or weaponID == E_WeaponBaseID.TF_WEAPON_KNIFE
        or weaponID == E_WeaponBaseID.TF_WEAPON_BAT_GIFTWRAP

    local charge = weaponInfo.m_bCharges and weapon:GetChargeBeginTime() or globals.CurTime()
    local eyePos = plocal:GetAbsOrigin() + plocal:GetPropVector("localdata", "m_vecViewOffset[0]")
    local projectileSpeed = weaponInfo:GetVelocity(charge):Length2D()
    local gravity = client.GetConVar("sv_gravity") * weaponInfo:GetGravity(charge) * 0.5

    local isRocketLauncher = isBeggarsBazooka or weaponID == E_WeaponBaseID.TF_WEAPON_ROCKETLAUNCHER

    visuals:set_eye_position(eyePos)

    local elapsedCharge = GetWeaponElapsedCharge(cmd)
    local ammo = weapon:GetPropInt("m_iClip1")
    for _, target in ipairs(targets) do
        local finalPos = target.finalPos or target.origin

        if settings.draw_only == false then
            angle = math_utils.SolveBallisticArc(eyePos, finalPos, projectileSpeed, gravity)
            if angle then
                if settings.autoshoot then
                    if weaponInfo.m_bCharges then
                        if elapsedCharge < 0.01 then
                            -- just started charging
                            cmd.buttons = cmd.buttons | IN_ATTACK
                            return
                        end

                        cmd.buttons = cmd.buttons & ~IN_ATTACK
                    else
                        if in_attack2 then
                            cmd.buttons = cmd.buttons | IN_ATTACK2
                        else
                            if isBeggarsBazooka then
                                --- gotta check CanShoot() as we skip it
                                --- because it returns false with 0 ammo
                                if ammo == 0 and wep_utils.CanShoot() == false then
                                    cmd.buttons = cmd.buttons | IN_ATTACK
                                    return
                                end
                            else
                                cmd.buttons = cmd.buttons | IN_ATTACK
                            end
                        end
                    end
                end

                if settings.psilent and weaponNoPSilent == false then
                    cmd.sendpacket = false
                end

                cmd.viewangles = Vector3(angle:Unpack())
                vAngles = angle
            end
        end

        if target then
            local ent = entities.GetByIndex(target.index)
            local proj_path = nil
            if ent and angle and settings.draw_proj_path then
                local weaponFirePos = weaponInfo:GetFirePosition(plocal, eyePos, angle, weapon:IsViewModelFlipped())
                if isRocketLauncher then
                  proj_path = {{pos = weaponFirePos}, {pos = target.sim_path[#target.sim_path]}}
                else
                  proj_path = proj_sim.Run(ent, plocal, weapon, weaponFirePos, angle:Forward(),
                      target.sim_path[#target.sim_path], target.timesecs, weaponInfo, charge)
                end
            end

            visuals:update_paths(target.sim_path, proj_path)
            visuals:set_target_hull(target.mins, target.maxs)
            visuals:set_displayed_time(globals.CurTime() + settings.draw_time)
            return
        end
    end
end

local function FrameStage(stage)
    if stage == E_ClientFrameStage.FRAME_NET_UPDATE_END then
        plocal = entities.GetLocalPlayer()
        if plocal == nil then
            weapon = nil
            weaponInfo = nil
            return
        end

        weapon = plocal:GetPropEntity("m_hActiveWeapon")
        weaponInfo = GetProjectileInformation(weapon:GetPropInt("m_iItemDefinitionIndex"))

        player_sim.RunBackground(entities.FindByClass("CTFPlayer"))
    elseif stage == E_ClientFrameStage.FRAME_RENDER_START and vAngles and settings.show_angles then
        if plocal == nil then return end
        if plocal:GetPropBool("m_nForceTauntCam") == false then return end
        plocal:SetVAngles(Vector3(vAngles:Unpack()))
    end
end

local function Draw()
    if not settings.enabled then
        return
    end

    if clientstate.GetNetChannel() == nil then
        return
    end

    visuals:draw()
end

local function Unload()
    menu.unload()
    visuals:destroy()
end

callbacks.Register("Draw", Draw)
callbacks.Register("CreateMove", CreateMove)
callbacks.Register("FrameStageNotify", FrameStage)
callbacks.Register("Unload", Unload)

end)
__bundle_register("src.gui", function(require, _LOADED, __bundle_register, __bundle_modules)
local gui = {}

local ui = require("src.ui")
local settings = require("src.settings")

---@param version string
function gui.init(version)
	local menu = ui.New({ title = string.format("NAVET'S PROJECTILE AIMBOT (v%s)", tostring(version)) })
	menu.y = 50
	menu.x = 50
	-- Create tabs
	local aim_tab = menu:CreateTab("aimbot")
	local visuals_tab = menu:CreateTab("visuals")
	local misc_tab = menu:CreateTab("misc")
	local conds_tab = menu:CreateTab("conditions")
	local colors_tab = menu:CreateTab("colors")
	local thick_tab = menu:CreateTab("thickness")
	local target_weights = menu:CreateTab("weights")
	local sim_tab = menu:CreateTab("simulation")

	local component_width = 260
	local component_height = 25

	-- AIMBOT TAB
	-- Left column toggles
	menu:CreateToggle(aim_tab, component_width, component_height, "enabled", settings.enabled, function(checked)
		settings.enabled = checked
	end)

	menu:CreateToggle(aim_tab, component_width, component_height, "autoshoot", settings.autoshoot, function(checked)
		settings.autoshoot = checked
	end)

	menu:CreateToggle(
		visuals_tab,
		component_width,
		component_height,
		"draw projectile path",
		settings.draw_proj_path,
		function(checked)
			settings.draw_proj_path = checked
		end
	)

	menu:CreateToggle(
		visuals_tab,
		component_width,
		component_height,
		"draw player path",
		settings.draw_player_path,
		function(checked)
			settings.draw_player_path = checked
		end
	)

	menu:CreateToggle(
		visuals_tab,
		component_width,
		component_height,
		"draw bounding box",
		settings.draw_bounding_box,
		function(checked)
			settings.draw_bounding_box = checked
		end
	)

	menu:CreateToggle(visuals_tab, component_width, component_height, "draw only", settings.draw_only, function(checked)
		settings.draw_only = checked
	end)

	--[[menu:CreateToggle(visuals_tab, component_width, component_height, "draw multpoint target",
        settings.draw_multipoint_target, function(checked)
            settings.draw_multipoint_target = checked
        end)]]

	--[[menu:CreateToggle(aim_tab, component_width, component_height, "cancel shot", settings.cancel_shot, function(checked)
		settings.cancel_shot = checked
	end)]]

	menu:CreateToggle(
		visuals_tab,
		component_width,
		component_height,
		"draw filled bounding box",
		settings.draw_quads,
		function(checked)
			settings.draw_quads = checked
		end
	)

	-- Right column toggles
	menu:CreateToggle(
		aim_tab,
		component_width,
		component_height,
		"allow aim at teammates",
		settings.allow_aim_at_teammates,
		function(checked)
			settings.allow_aim_at_teammates = checked
		end
	)

	menu:CreateToggle(aim_tab, component_width, component_height, "silent+", settings.psilent, function(checked)
		settings.psilent = checked
	end)

	menu:CreateToggle(
		aim_tab,
		component_width,
		component_height,
		"ping compensation",
		settings.ping_compensation,
		function(checked)
			settings.ping_compensation = checked
		end
	)

	-- Entity toggles
	for name, enabled in pairs(settings.ents) do
		menu:CreateToggle(aim_tab, component_width, component_height, name, enabled, function(checked)
			settings.ents[name] = checked
		end)
	end

	--[[menu:CreateToggle(aim_tab, component_width, component_height, "wait for charge (laggy)", settings.wait_for_charge,
        function(checked)
            settings.wait_for_charge = checked
        end)]]

	menu:CreateToggle(
		visuals_tab,
		component_width,
		component_height,
		"show angles",
		settings.show_angles,
		function(checked)
			settings.show_angles = checked
		end
	)

	-- MISC TAB
	menu:CreateSlider(
		misc_tab,
		component_width,
		component_height,
		"max sim time",
		0.5,
		10,
		settings.max_sim_time,
		function(value)
			settings.max_sim_time = value
		end
	)

	menu:CreateSlider(
		misc_tab,
		component_width,
		component_height,
		"max distance",
		0,
		4096,
		settings.max_distance,
		function(value)
			settings.max_distance = value
		end
	)

	menu:CreateSlider(
		misc_tab,
		component_width,
		component_height,
		"min priority",
		0,
		10,
		settings.min_priority,
		function(value)
			settings.min_priority = math.floor(value)
		end
	)

	menu:CreateSlider(
		misc_tab,
		component_width,
		component_height,
		"draw time",
		0,
		10,
		settings.draw_time,
		function(value)
			settings.draw_time = value
		end
	)

	menu:CreateSlider(
		misc_tab,
		component_width,
		component_height,
		"max charge (%)",
		0,
		100,
		settings.max_percent,
		function(value)
			settings.max_percent = value
		end
	)

	menu:CreateSlider(
		misc_tab,
		component_width,
		component_height,
		"close distance (%)",
		0,
		100,
		settings.close_distance,
		function(value)
			settings.close_distance = value
		end
	)

	menu:CreateSlider(
		misc_tab,
		component_width,
		component_height,
		"max targets",
		1,
		3,
		settings.max_targets,
		function(value)
			settings.max_targets = value // 1
		end
	)

	-- CONDITIONS TAB
	for name, enabled in pairs(settings.ignore_conds) do
		menu:CreateToggle(
			conds_tab,
			component_width,
			component_height,
			string.format("ignore %s", name),
			enabled,
			function(checked)
				settings.ignore_conds[name] = checked
			end
		)
	end

	-- COLORS TAB
	for name, visual in pairs(settings.colors) do
		local label = string.gsub(name, "_", " ")
		menu:CreateHueSlider(colors_tab, component_width, component_height, label, visual, function(value)
			settings.colors[name] = math.floor(value)
		end)
	end

	-- THICKNESS TAB
	for name, visual in pairs(settings.thickness) do
		local label = string.gsub(name, "_", " ")
		menu:CreateSlider(thick_tab, component_width, component_height, label, 0.1, 5, visual, function(value)
			settings.thickness[name] = math.floor(value)
		end)
	end

	menu:CreateLabel(target_weights, component_width, component_height, "Bigger = more priority")

	-- TARGET MODE
	for name, mode in pairs(settings.weights) do
		local label = string.gsub(name, "_", " ")
		menu:CreateAccurateSlider(
			target_weights,
			component_width,
			component_height,
			label,
			-5.0,
			5.0,
			mode,
			function(value)
				settings.weights[name] = value
			end
		)
	end

	menu:CreateToggle(
		target_weights,
		component_width,
		component_height,
		"draw scores",
		settings.draw_scores,
		function(checked)
			settings.draw_scores = checked
		end
	)

	menu:CreateAccurateSlider(
		target_weights,
		component_width,
		component_height,
		"minimum score",
		0,
		10,
		settings.min_score,
		function(value)
			settings.min_score = value
		end
	)

	menu:CreateToggle(
		target_weights,
		component_width,
		component_height,
		"inside fov only",
		settings.onfov_only,
		function(checked)
			settings.onfov_only = checked
		end
	)

	menu:CreateSlider(target_weights, component_width, component_height, "fov", 0, 180, settings.fov, function(value)
		settings.fov = value
	end)

	menu:CreateToggle(
		target_weights,
		component_width,
		component_height,
		"smart mode",
		settings.smart_targeting,
		function(checked)
			settings.smart_targeting = checked
		end
	)

	menu:CreateToggle(sim_tab, component_width, component_height, "fast mode", settings.sim.fast_mode, function(checked)
		settings.sim.fast_mode = checked
	end)

	callbacks.Register("Draw", function(...)
		menu:Draw()
	end)
	printc(150, 255, 150, 255, "[PROJ AIMBOT] Menu loaded")
end

function gui.unload()
	ui.Unload()
end

return gui

end)
__bundle_register("src.settings", function(require, _LOADED, __bundle_register, __bundle_modules)
return {
	enabled = true,
	autoshoot = true,
	fov = gui.GetValue("aim fov"),
	max_sim_time = 2.0,
	draw_time = 1.0,
	draw_proj_path = true,
	draw_player_path = true,
	draw_bounding_box = true,
	draw_only = false,
	draw_multipoint_target = false,
	max_distance = 1024,
	allow_aim_at_teammates = true,
	ping_compensation = true,
	min_priority = 0,
	explosive = true,
	close_distance = 10, --- %
	draw_quads = true,
	show_angles = true,
	max_targets = 2,
	draw_scores = true,
	smart_targeting = true,

	sim = {
		use_detonate_time = true,
		can_rotate = true,
		stay_on_ground = false,
		fast_mode = true,
	},

	max_percent = 90,
	wait_for_charge = false,
	cancel_shot = false,

	ents = {
		["aim players"] = true,
		["aim sentries"] = true,
		["aim dispensers"] = true,
		["aim teleporters"] = true,
	},

	psilent = true,

	ignore_conds = {
		cloaked = true,
		disguised = false,
		ubercharged = true,
		bonked = true,
		taunting = true,
		friends = true,
		bumper_karts = false,
		kritzkrieged = false,
		jarated = false,
		milked = false,
		vaccinator = false,
		ghost = true,
	},

	colors = {
		bounding_box = 360, --{136, 192, 208, 255},
		player_path = 360, --{136, 192, 208, 255},
		projectile_path = 360, --{235, 203, 139, 255}
		multipoint_target = 20,
		target_glow = 360,
		quads = 360,
	},

	thickness = {
		bounding_box = 1,
		player_path = 1,
		projectile_path = 1,
		multipoint_target = 1,
	},

	weights = {
		health_weight = 1.0, -- prefer lower player health
		distance_weight = 1.1, -- prefer closer players
		fov_weight = 2,
		visibility_weight = 1.2,
		speed_weight = 0.6, -- prefer slower targets
		medic_priority = 0.0, -- bonus if Medic
		sniper_priority = 0.0, -- bonus if Sniper
		uber_penalty = -2.0, -- skip/penalize Ubercharged targets
		teammate_weight = 5.0, -- on weapons that can shoot teammates, they have priority
	},

	min_score = 2,
	onfov_only = true,
}

end)
__bundle_register("src.ui", function(require, _LOADED, __bundle_register, __bundle_modules)
---@class Context
---@field mouseX integer
---@field mouseY integer
---@field mouseDown boolean
---@field mouseReleased boolean
---@field mousePressed boolean
---@field tick integer
---@field lastPressedTick integer
---@field windowX integer
---@field windowY integer

local theme = {
    bg_light = { 45, 45, 45 },
    bg = { 35, 35, 35 },
    bg_dark = { 30, 30, 30 },
    primary = { 143, 188, 187 },
    success = { 69, 255, 166 },
    fail = { 255, 69, 69 },
}

local thickness = 1    --- outline thickness
local header_size = 25 --- title height
local tab_section_height = 25

local max_objects_per_column = 9
local column_spacing = 10
local row_spacing = 5
local element_margin = 5

---@class GuiWindow
local window = {
    dragging = false,
    mx = 0,
    my = 0,
    x = 0,
    y = 0,
    w = 0,
    h = 0,
    title = "",
    tabs = {},
    current_tab = 1,
}

local lastPressedTick = 0
local font = draw.CreateFont("TF2 BUILD", 12, 400, FONTFLAG_ANTIALIAS | FONTFLAG_CUSTOM)
local white_texture = draw.CreateTextureRGBA(string.rep(string.char(255, 255, 255, 255), 4), 2, 2)

---@param texture TextureID
---@param centerX integer
---@param centerY integer
---@param radius integer
---@param segments integer
local function DrawFilledCircle(texture, centerX, centerY, radius, segments)
    local vertices = {}

    for i = 0, segments do
        local angle = (i / segments) * math.pi * 2
        local x = centerX + math.cos(angle) * radius
        local y = centerY + math.sin(angle) * radius
        vertices[i + 1] = { x, y, 0, 0 }
    end

    draw.TexturedPolygon(texture, vertices, false)
end

local function draw_tab_button(parent, x, y, width, height, label, i)
    local mousePos = input.GetMousePos()
    local mx, my = mousePos[1], mousePos[2]
    local mouseInside = mx >= x and mx <= x + width
        and my >= y and my <= y + height

    if (mouseInside and input.IsButtonDown(E_ButtonCode.MOUSE_LEFT)) then
        draw.Color(theme.bg_light[1], theme.bg_light[2], theme.bg_light[3], 255)
    elseif (mouseInside) then
        draw.Color(theme.bg[1], theme.bg[2], theme.bg[3], 255)
    else
        draw.Color(theme.bg_dark[1], theme.bg_dark[2], theme.bg_dark[3], 255)
    end
    draw.FilledRect(x, y, x + width, y + height)

    if (parent.current_tab == i) then
        draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        draw.FilledRect(x + 2, y + height - 4, x + width - 2, y + height - 2)
    end

    local tw, th = draw.GetTextSize(label)
    local tx, ty
    tx = (x + (width * 0.5) - (tw * 0.5)) // 1
    ty = (y + (height * 0.5) - (th * 0.5)) // 1

    draw.Color(242, 242, 242, 255)
    draw.Text(tx, ty, label)

    local pressed, tick = input.IsButtonPressed(E_ButtonCode.MOUSE_FIRST)

    if (mouseInside and pressed and tick > lastPressedTick) then
        parent.current_tab = i
    end
end

local function hsv_to_rgb(h, s, v)
    local r, g, b
    local i = math.floor(h * 6)
    local f = h * 6 - i
    local p = v * (1 - s)
    local q = v * (1 - f * s)
    local t = v * (1 - (1 - f) * s)

    i = i % 6

    if i == 0 then
        r, g, b = v, t, p
    elseif i == 1 then
        r, g, b = q, v, p
    elseif i == 2 then
        r, g, b = p, v, t
    elseif i == 3 then
        r, g, b = p, q, v
    elseif i == 4 then
        r, g, b = t, p, v
    elseif i == 5 then
        r, g, b = v, p, q
    end

    return math.floor(r * 255), math.floor(g * 255), math.floor(b * 255)
end

function window.Draw(self)
    if (not gui.IsMenuOpen()) then
        return
    end

    local x, y = self.x, self.y
    local tab = self.tabs[self.current_tab]
    local w = (tab and tab.w or 200)
    local h = (tab and tab.h or 200)
    local title = self.title

    local mousePressed, tick = input.IsButtonPressed(E_ButtonCode.MOUSE_LEFT)
    local mousePos = input.GetMousePos()

    local numTabs = #self.tabs
    local extra_height = (numTabs > 1) and tab_section_height or 0

    if (title and #title > 0) then
        local header_x1 = x - thickness
        local header_y1 = y - header_size
        local header_x2 = x + w + thickness
        local header_y2 = y - thickness

        local mx, my = mousePos[1], mousePos[2]
        local mouseInHeader = mx >= header_x1 and mx <= header_x2
            and my >= header_y1 and my <= header_y2

        if (mouseInHeader and mousePressed) then
            self.dragging = true
        end
    end

    if (not input.IsButtonDown(E_ButtonCode.MOUSE_LEFT)) then
        self.dragging = false
    end

    local dx, dy = mousePos[1] - self.mx, mousePos[2] - self.my
    if (self.dragging) then
        self.x = self.x + dx
        self.y = self.y + dy
    end

    draw.SetFont(font)

    local total_h = h + extra_height

    draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
    draw.OutlinedRect(x - thickness, y - thickness, x + w + thickness, y + total_h + thickness)

    draw.Color(theme.bg[1], theme.bg[2], theme.bg[3], 255)
    draw.FilledRect(x, y, x + w, y + total_h)

    if (numTabs > 1) then
        draw.Color(theme.bg_light[1], theme.bg_light[2], theme.bg_light[3], 255)
        draw.FilledRect(x, y, x + w, y + tab_section_height)

        local btnx = x
        local btny = y
        for i, t in ipairs(self.tabs) do
            local tab_width = math.max(80, draw.GetTextSize(t.name) + 20)
            draw_tab_button(self, btnx, btny, tab_width, tab_section_height, t.name, i)
            btnx = btnx + tab_width
        end
    end

    -- header
    if (title and #title > 0) then
        draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        draw.FilledRect(x - thickness, y - header_size, x + w + thickness, y - thickness)

        local tw, th = draw.GetTextSize(title)
        local tx = (x - thickness + w * 0.5 - tw * 0.5) // 1
        local ty = (y - thickness - header_size * 0.5 - th * 0.5) // 1

        draw.Color(242, 242, 242, 255)
        draw.Text(tx, ty, title)
    end

    local content_x = x
    local content_y = y + extra_height

    local context = {
        mouseX = mousePos[1],
        mouseY = mousePos[2],
        mouseDown = input.IsButtonDown(E_ButtonCode.MOUSE_LEFT),
        mouseReleased = input.IsButtonReleased(E_ButtonCode.MOUSE_LEFT),
        mousePressed = mousePressed,
        tick = tick,
        lastPressedTick = lastPressedTick,
        windowX = content_x,
        windowY = content_y,
    }

    if (tab) then
        for i = #tab.objs, 1, -1 do
            local obj = tab.objs[i]
            if obj then
                obj:Draw(context)
            end
        end
    end

    lastPressedTick = tick
    self.mx, self.my = mousePos[1], mousePos[2]
end

function window:SetCurrentTab(tab_index)
    if (tab_index > #self.tabs or tab_index < 0) then
        error(string.format("Invalid tab index! Received %s", tab_index))
        return false
    end

    self.current_tab = tab_index
    return true
end

function window:CreateTab(tab_name)
    if (#self.tabs == 1 and self.tabs[1].name == "") then
        --- replace the default tab
        --- just in case we have more than 1 tabs
        self.tabs[1].name = tab_name
        return 1
    else
        self.tabs[#self.tabs + 1] = {
            name = tab_name,
            objs = {}
        }
        return #self.tabs
    end
end

--- recalculates positions of all objs in all tabs
--- and adjusts window size to fit contents
function window:RecalculateLayout(tab_index)
    if not tab_index or not self.tabs[tab_index] then return end
    local tab = self.tabs[tab_index]

    local columns = {}
    local col, row = 1, 0

    for i, obj in ipairs(tab.objs) do
        if not columns[col] then
            columns[col] = {}
        end

        table.insert(columns[col], obj)
        row = row + 1

        if row >= max_objects_per_column then
            row = 0
            col = col + 1
        end
    end

    local num_columns = #columns
    local total_spacing = (num_columns - 1) * column_spacing + element_margin * 2

    local min_window_width = 200

    if #self.tabs > 1 then
        local total_tabs_width = 0
        for i, t in ipairs(self.tabs) do
            local tab_button_width = math.max(80, draw.GetTextSize(t.name) + 20)
            total_tabs_width = (total_tabs_width + tab_button_width) // 1
        end

        if total_tabs_width > min_window_width then
            min_window_width = total_tabs_width // 1
        end
    end

    local available_width = min_window_width - total_spacing
    local column_width = available_width / num_columns

    local max_height = 0

    for col_idx, column in ipairs(columns) do
        local x_offset = element_margin + (col_idx - 1) * (column_width + column_spacing)

        for row_idx, obj in ipairs(column) do
            obj.x = x_offset // 1
            obj.y = (element_margin + (row_idx - 1) * (obj.h + row_spacing)) // 1
            obj.w = column_width // 1

            local obj_bottom = obj.y + obj.h
            if obj_bottom > max_height then
                max_height = obj_bottom
            end
        end
    end

    tab.w = min_window_width // 1
    tab.h = (max_height + element_margin) // 1
end

function window:InsertElement(object, tab_index)
    tab_index = tab_index or self.current_tab or 1
    if (tab_index > #self.tabs or tab_index < 0) then
        error(string.format("Invalid tab index! Received %s", tab_index))
        return false
    end

    local tab = self.tabs[tab_index]
    tab.objs[#tab.objs + 1] = object
    self:RecalculateLayout(tab_index)
    return true
end

---@param func fun(checked: boolean)?
function window:CreateToggle(tab_index, width, height, label, checked, func)
    local btn = {
        x = 0,
        y = 0,
        w = width,
        h = height,
        label = label,
        func = func,
        checked = checked,
    }

    ---@param context Context
    function btn:Draw(context)
        local bx, by, bw, bh
        bx = self.x + context.windowX
        by = self.y + context.windowY
        bw = self.w
        bh = self.h

        local mx, my = context.mouseX, context.mouseY
        local mouseInside = mx >= bx and mx <= bx + bw
            and my >= by and my <= by + bh

        draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        draw.OutlinedRect(bx - thickness, by - thickness, bx + bw + thickness, by + bh + thickness)

        if (mouseInside and context.mouseDown) then
            draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        elseif (mouseInside) then
            draw.Color(theme.bg_light[1], theme.bg_light[2], theme.bg_light[3], 255)
        else
            draw.Color(theme.bg[1], theme.bg[2], theme.bg[3], 255)
        end
        draw.FilledRect(bx, by, bx + bw, by + bh)

        local tw, th = draw.GetTextSize(self.label)
        local tx, ty
        tx = bx + 2
        ty = (by + bh * 0.5 - th * 0.5) // 1

        draw.Color(242, 242, 242, 255)
        draw.Text(tx, ty, label)

        local circle_x = bx + bw - 10
        local circle_y = (by + bh * 0.5) // 1
        local radius = 8

        if (btn.checked) then
            draw.Color(theme.success[1], theme.success[2], theme.success[3], 255)
        else
            draw.Color(theme.fail[1], theme.fail[2], theme.fail[3], 255)
        end

        DrawFilledCircle(white_texture, circle_x, circle_y, radius, 4)

        if (mouseInside and context.mousePressed and context.tick > context.lastPressedTick) then
            btn.checked = not btn.checked

            if (func) then
                func(btn.checked)
            end
        end
    end

    self:InsertElement(btn, tab_index or self.current_tab)
    return btn
end

---@param func fun(value: number)?
function window:CreateSlider(tab_index, width, height, label, min, max, currentvalue, func)
    local slider = {
        x = 0,
        y = 0,
        w = width,
        h = height,
        label = label,
        func = func,
        min = min,
        max = max,
        value = currentvalue
    }

    ---@param context Context
    function slider:Draw(context)
        local bx, by, bw, bh
        bx = self.x + context.windowX
        by = self.y + context.windowY
        bw = self.w
        bh = self.h

        local mx, my = context.mouseX, context.mouseY
        local mouseInside = mx >= bx and mx <= bx + bw
            and my >= by and my <= by + bh

        --- draw outline
        draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        draw.OutlinedRect(bx - thickness, by - thickness, bx + bw + thickness, by + bh + thickness)

        --- draw background based on mouse state
        if (mouseInside and context.mouseDown) then
            draw.Color(theme.bg_light[1], theme.bg_light[2], theme.bg_light[3], 255)
        elseif (mouseInside) then
            draw.Color(theme.bg[1], theme.bg[2], theme.bg[3], 255)
        else
            draw.Color(theme.bg_dark[1], theme.bg_dark[2], theme.bg_dark[3], 255)
        end
        draw.FilledRect(bx, by, bx + bw, by + bh)

        -- calculate percentage for the slider fill
        local percent = (self.value - self.min) / (self.max - self.min)
        percent = math.max(0, math.min(1, percent)) --- clamp it ;)

        --- draw slider fill
        draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        draw.FilledRect(bx, by, (bx + (bw * percent)) // 1, by + bh)

        --- draw label text
        local tw, th = draw.GetTextSize(self.label)
        local tx, ty
        tx = bx + 2
        ty = (by + bh * 0.5 - th * 0.5) // 1
        draw.Color(242, 242, 242, 255)
        draw.TextShadow(tx + 2, ty, self.label)

        tw = draw.GetTextSize(string.format("%.0f", self.value))
        tx = bx + bw - tw - 2
        draw.TextShadow(tx, ty, string.format("%.0f", self.value))

        --- handle mouse interaction
        if (mouseInside and context.mousePressed and context.tick > context.lastPressedTick) then
            self.isDragging = true
        end

        --- continue dragging even if mouse is outside the slider
        if (self.isDragging and context.mouseDown) then
            --- update slider value based on mouse position
            local mousePercent = (mx - bx) / bw
            mousePercent = math.max(0, math.min(1, mousePercent))
            self.value = self.min + (self.max - self.min) * mousePercent

            if (self.func) then
                self.func(self.value)
            end
        elseif (not context.mouseDown) then
            --- stop dragging when mouse is released
            self.isDragging = false
        end
    end

    self:InsertElement(slider, tab_index or self.current_tab)
    return slider
end

---@param func fun(value: number)?
function window:CreateHueSlider(tab_index, width, height, label, currentvalue, func)
    local slider = {
        x = 0,
        y = 0,
        w = width,
        h = height,
        label = label,
        func = func,
        min = 0,
        max = 360,
        value = currentvalue
    }

    ---@param context Context
    function slider:Draw(context)
        local bx, by, bw, bh
        bx = self.x + context.windowX
        by = self.y + context.windowY
        bw = self.w
        bh = self.h

        local mx, my = context.mouseX, context.mouseY
        local mouseInside = mx >= bx and mx <= bx + bw
            and my >= by and my <= by + bh

        --- draw outline
        draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        draw.OutlinedRect(bx - thickness, by - thickness, bx + bw + thickness, by + bh + thickness)

        --- draw background
        draw.Color(theme.bg_dark[1], theme.bg_dark[2], theme.bg_dark[3], 255)
        draw.FilledRect(bx, by, bx + bw, by + bh)

        -- calculate percentage for the slider indicator
        local percent = (self.value - self.min) / (self.max - self.min)
        percent = math.max(0, math.min(1, percent))

        --- draw slider indicator line
        local indicator_x = (bx + (bw * percent)) // 1
        if (self.value == 360) then
            draw.Color(255, 255, 255, 255)
        else
            local r, g, b = hsv_to_rgb(self.value / 360, 1.0, 1.0)
            draw.Color(r, g, b, 255)
        end
        draw.FilledRect(bx, (by + bh * 0.6) // 1, indicator_x, by + bh)

        --- draw label text with shadow for better visibility
        local tw, th = draw.GetTextSize(self.label)
        local tx, ty
        tx = bx + 2
        ty = by + 2

        -- Draw main text
        draw.Color(242, 242, 242, 255)
        draw.TextShadow(tx, ty, self.label)

        --- handle mouse interaction
        if (mouseInside and context.mousePressed and context.tick > context.lastPressedTick) then
            self.isDragging = true
        end

        --- continue dragging even if mouse is outside the slider
        if (self.isDragging and context.mouseDown) then
            --- update slider value based on mouse position
            local mousePercent = (mx - bx) / bw
            mousePercent = math.max(0, math.min(1, mousePercent))
            self.value = self.min + (self.max - self.min) * mousePercent

            if (self.func) then
                self.func(self.value)
            end
        elseif (not context.mouseDown) then
            --- stop dragging when mouse is released
            self.isDragging = false
        end
    end

    self:InsertElement(slider, tab_index or self.current_tab)
    return slider
end

---@param func fun(value: number)?
function window:CreateAccurateSlider(tab_index, width, height, label, min, max, currentvalue, func)
    local slider = {
        x = 0,
        y = 0,
        w = width,
        h = height,
        label = label,
        func = func,
        min = min,
        max = max,
        value = currentvalue
    }

    ---@param context Context
    function slider:Draw(context)
        local bx, by, bw, bh
        bx = self.x + context.windowX
        by = self.y + context.windowY
        bw = self.w
        bh = self.h

        local mx, my = context.mouseX, context.mouseY
        local mouseInside = mx >= bx and mx <= bx + bw
            and my >= by and my <= by + bh

        --- draw outline
        draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        draw.OutlinedRect(bx - thickness, by - thickness, bx + bw + thickness, by + bh + thickness)

        --- draw background based on mouse state
        if (mouseInside and context.mouseDown) then
            draw.Color(theme.bg_light[1], theme.bg_light[2], theme.bg_light[3], 255)
        elseif (mouseInside) then
            draw.Color(theme.bg[1], theme.bg[2], theme.bg[3], 255)
        else
            draw.Color(theme.bg_dark[1], theme.bg_dark[2], theme.bg_dark[3], 255)
        end
        draw.FilledRect(bx, by, bx + bw, by + bh)

        -- calculate percentage for the slider fill
        local percent = (self.value - self.min) / (self.max - self.min)
        percent = math.max(0, math.min(1, percent)) --- clamp it ;)

        --- draw slider fill
        draw.Color(theme.primary[1], theme.primary[2], theme.primary[3], 255)
        draw.FilledRect(bx, by, (bx + (bw * percent)) // 1, by + bh)

        --- draw label text
        local tw, th = draw.GetTextSize(self.label)
        local tx, ty
        tx = bx + 2
        ty = (by + bh * 0.5 - th * 0.5) // 1
        draw.Color(242, 242, 242, 255)
        draw.TextShadow(tx + 2, ty, self.label)

        tw = draw.GetTextSize(string.format("%f", self.value))
        tx = bx + bw - tw - 2
        draw.TextShadow(tx, ty, string.format("%f", self.value))

        --- handle mouse interaction
        if (mouseInside and context.mousePressed and context.tick > context.lastPressedTick) then
            self.isDragging = true
        end

        --- continue dragging even if mouse is outside the slider
        if (self.isDragging and context.mouseDown) then
            --- update slider value based on mouse position
            local mousePercent = (mx - bx) / bw
            mousePercent = math.max(0, math.min(1, mousePercent))
            self.value = self.min + (self.max - self.min) * mousePercent

            if (self.func) then
                self.func(self.value)
            end
        elseif (not context.mouseDown) then
            --- stop dragging when mouse is released
            self.isDragging = false
        end
    end

    self:InsertElement(slider, tab_index or self.current_tab)
    return slider
end

function window:CreateLabel(tab_index, width, height, text, func)
    local label = {
        x = 0,
        y = 0,
        w = width,
        h = height,
        text = text,
    }

    ---@param context Context
    function label:Draw(context)
        local x, y, tw, th
        tw, th = draw.GetTextSize(self.text)
        x = (context.windowX + self.x + (self.w * 0.5) - (tw * 0.5)) // 1
        y = (context.windowY + self.y + (self.h * 0.5) - (th * 0.5)) // 1
        draw.Color(255, 255, 255, 255)
        draw.TextShadow(x, y, tostring(text))
    end

    self:InsertElement(label, tab_index or self.current_tab)
    return label
end

---@return GuiWindow
function window.New(tbl)
    local newWindow = tbl or {}
    setmetatable(newWindow, { __index = window })
    newWindow.tabs[1] = { name = "", objs = {} }
    return newWindow
end

function window.Unload()
    draw.DeleteTexture(white_texture)
end

return window

end)
__bundle_register("src.multipoint", function(require, _LOADED, __bundle_register, __bundle_modules)
local multipoint = {}

--- relative to Maxs().z
local z_offsets = { 0.5, 0.7, 0.9, 0.4, 0.2 }

--- inverse of z_offsets
local huntsman_z_offsets = { 0.9, 0.7, 0.5, 0.4, 0.2 }

local splash_offsets = { 0.2, 0.4, 0.5, 0.7, 0.9 }

---@param vHeadPos Vector3
---@param pTarget Entity
---@param vecPredictedPos Vector3
---@param pWeapon Entity
---@param weaponInfo WeaponInfo
---@return boolean, Vector3?  -- visible, final predicted hit position (or nil)
function multipoint.Run(pTarget, pWeapon, weaponInfo, vHeadPos, vecPredictedPos)
    local proj_type = pWeapon:GetWeaponProjectileType()
    local bExplosive = weaponInfo.m_flDamageRadius > 0 and
        proj_type == E_ProjectileType.TF_PROJECTILE_ROCKET or
        proj_type == E_ProjectileType.TF_PROJECTILE_PIPEBOMB or
        proj_type == E_ProjectileType.TF_PROJECTILE_PIPEBOMB_REMOTE or
        proj_type == E_ProjectileType.TF_PROJECTILE_STICKY_BALL or
        proj_type == E_ProjectileType.TF_PROJECTILE_CANNONBALL or
        proj_type == E_ProjectileType.TF_PROJECTILE_PIPEBOMB_PRACTICE

    local bSplashWeapon = proj_type == E_ProjectileType.TF_PROJECTILE_ROCKET
        or proj_type == E_ProjectileType.TF_PROJECTILE_PIPEBOMB_REMOTE
        or proj_type == E_ProjectileType.TF_PROJECTILE_PIPEBOMB_PRACTICE
        or proj_type == E_ProjectileType.TF_PROJECTILE_CANNONBALL
        or proj_type == E_ProjectileType.TF_PROJECTILE_PIPEBOMB
        or proj_type == E_ProjectileType.TF_PROJECTILE_STICKY_BALL
        or proj_type == E_ProjectileType.TF_PROJECTILE_FLAME_ROCKET

    local bHuntsman = pWeapon:GetWeaponID() == E_WeaponBaseID.TF_WEAPON_COMPOUND_BOW
    local chosen_offsets = bHuntsman and huntsman_z_offsets or (bSplashWeapon or bExplosive) and splash_offsets or
    z_offsets

    local trace

    for i = 1, #chosen_offsets do
        local offset = chosen_offsets[i]
        local zOffset = (pTarget:GetMaxs().z * offset)
        local origin = vecPredictedPos + Vector3(0, 0, zOffset)

        trace = engine.TraceHull(vHeadPos, origin, weaponInfo.m_vecMins, weaponInfo.m_vecMaxs, weaponInfo.m_iTraceMask,
            function(ent, contentsMask)
                return false
            end)

        if trace and trace.fraction >= 1 then
            -- build a new Vector3 for the visible hit point
            local finalPos = Vector3(vecPredictedPos:Unpack())
            finalPos.z = origin.z
            return true, finalPos
        end
    end

    -- nothing visible among multipoints
    return false, nil
end

return multipoint

end)
__bundle_register("src.visuals", function(require, _LOADED, __bundle_register, __bundle_modules)
local Visuals = {}
Visuals.__index = Visuals

local settings = require("src.settings")

local function getBoxVertices(pos, mins, maxs)
    if not (pos and mins and maxs) then
        return nil
    end

    local worldMins = pos + mins
    local worldMaxs = pos + maxs

    return {
        Vector3(worldMins.x, worldMins.y, worldMins.z),
        Vector3(worldMins.x, worldMaxs.y, worldMins.z),
        Vector3(worldMaxs.x, worldMaxs.y, worldMins.z),
        Vector3(worldMaxs.x, worldMins.y, worldMins.z),
        Vector3(worldMins.x, worldMins.y, worldMaxs.z),
        Vector3(worldMins.x, worldMaxs.y, worldMaxs.z),
        Vector3(worldMaxs.x, worldMaxs.y, worldMaxs.z),
        Vector3(worldMaxs.x, worldMins.y, worldMaxs.z),
    }
end

local function xyuv(point, u, v)
    return { point[1], point[2], u, v }
end

local function hsvToRgb(hue, saturation, value)
    if saturation == 0 then
        return value, value, value
    end

    local hueSector = math.floor(hue / 60)
    local hueSectorOffset = (hue / 60) - hueSector

    local p = value * (1 - saturation)
    local q = value * (1 - saturation * hueSectorOffset)
    local t = value * (1 - saturation * (1 - hueSectorOffset))

    if hueSector == 0 then
        return value, t, p
    elseif hueSector == 1 then
        return q, value, p
    elseif hueSector == 2 then
        return p, value, t
    elseif hueSector == 3 then
        return p, q, value
    elseif hueSector == 4 then
        return t, p, value
    else
        return value, p, q
    end
end

local function drawQuadFace(texture, projected, indices, flipU, flipV)
    if not (texture and projected and indices) then
        return
    end

    local uvs = {
        { 0, 0 },
        { 1, 0 },
        { 1, 1 },
        { 0, 1 },
    }

    if flipU then
        for i = 1, 4 do
            uvs[i][1] = 1 - uvs[i][1]
        end
    end

    if flipV then
        for i = 1, 4 do
            uvs[i][2] = 1 - uvs[i][2]
        end
    end

    local poly = {}
    for i = 1, 4 do
        local vertex = projected[indices[i]]
        if not vertex then
            return
        end

        poly[i] = xyuv(vertex, uvs[i][1], uvs[i][2])
    end

    draw.TexturedPolygon(texture, poly, true)
end

local function drawLine(texture, p1, p2, thickness)
    if not (texture and p1 and p2) then
        return
    end

    local dx = p2[1] - p1[1]
    local dy = p2[2] - p1[2]
    local len = math.sqrt(dx * dx + dy * dy)
    if len <= 0 then
        return
    end

    dx = dx / len
    dy = dy / len
    local px = -dy * thickness
    local py = dx * thickness

    local verts = {
        { p1[1] + px, p1[2] + py, 0, 0 },
        { p1[1] - px, p1[2] - py, 0, 1 },
        { p2[1] - px, p2[2] - py, 1, 1 },
        { p2[1] + px, p2[2] + py, 1, 0 },
    }

    draw.TexturedPolygon(texture, verts, false)
end

local function buildBoxFaces(worldMins, worldMaxs)
    local midX = (worldMins.x + worldMaxs.x) * 0.5
    local midY = (worldMins.y + worldMaxs.y) * 0.5
    local midZ = (worldMins.z + worldMaxs.z) * 0.5

    return {
        {
            id = "bottom",
            indices = { 1, 4, 3, 2 },
            normal = Vector3(0, 0, -1),
            center = Vector3(midX, midY, worldMins.z),
            flip_v = true,
        },
        {
            id = "top",
            indices = { 5, 6, 7, 8 },
            normal = Vector3(0, 0, 1),
            center = Vector3(midX, midY, worldMaxs.z),
        },
        {
            id = "front",
            indices = { 2, 3, 7, 6 },
            normal = Vector3(0, 1, 0),
            center = Vector3(midX, worldMaxs.y, midZ),
        },
        {
            id = "back",
            indices = { 1, 5, 8, 4 },
            normal = Vector3(0, -1, 0),
            center = Vector3(midX, worldMins.y, midZ),
            flip_u = true,
        },
        {
            id = "left",
            indices = { 1, 2, 6, 5 },
            normal = Vector3(-1, 0, 0),
            center = Vector3(worldMins.x, midY, midZ),
        },
        {
            id = "right",
            indices = { 4, 8, 7, 3 },
            normal = Vector3(1, 0, 0),
            center = Vector3(worldMaxs.x, midY, midZ),
        },
    }
end

local function drawPlayerPath(self)
    local playerPath = self.paths.player_path
    if not playerPath or #playerPath < 2 then
        return
    end

    local last = client.WorldToScreen(playerPath[1])
    if not last then
        return
    end

    for i = 2, #playerPath do
        local current = client.WorldToScreen(playerPath[i])
        if current and last then
            drawLine(self.texture, last, current, settings.thickness.player_path)
        end
        last = current
    end
end

local function drawProjPath(self)
    local projPath = self.paths.proj_path
    if not projPath or #projPath < 2 then
        return
    end

    local first = projPath[1]
    local last = first and first.pos and client.WorldToScreen(first.pos)
    if not last then
        return
    end

    for i = 2, #projPath do
        local entry = projPath[i]
        local current = entry and entry.pos and client.WorldToScreen(entry.pos)
        if current and last then
            drawLine(self.texture, last, current, settings.thickness.projectile_path)
        end
        last = current
    end
end

local function drawMultipointTarget(self)
    local pos = self.multipoint_target_pos
    if not pos then
        return
    end

    local screen = client.WorldToScreen(pos)
    if not screen then
        return
    end

    local s = settings.thickness.multipoint_target
    local verts = {
        { screen[1] - s, screen[2] - s, 0, 0 },
        { screen[1] + s, screen[2] - s, 1, 0 },
        { screen[1] + s, screen[2] + s, 1, 1 },
        { screen[1] - s, screen[2] + s, 0, 1 },
    }

    draw.TexturedPolygon(self.texture, verts, false)
end

local function isFaceVisible(normal, faceCenter, eyePos)
    if not (normal and faceCenter and eyePos) then
        return true
    end

    local toEye = Vector3(eyePos.x - faceCenter.x, eyePos.y - faceCenter.y, eyePos.z - faceCenter.z)
    local dot = (toEye.x * normal.x) + (toEye.y * normal.y) + (toEye.z * normal.z)
    return dot > 0
end

local function drawPlayerHitbox(self, playerPos)
    if not playerPos then
        return
    end

    local mins = self.target_min_hull
    local maxs = self.target_max_hull
    if not (mins and maxs) then
        return
    end

    local worldMins = playerPos + mins
    local worldMaxs = playerPos + maxs

    local corners = {
        Vector3(worldMins.x, worldMins.y, worldMins.z),
        Vector3(worldMins.x, worldMaxs.y, worldMins.z),
        Vector3(worldMaxs.x, worldMaxs.y, worldMins.z),
        Vector3(worldMaxs.x, worldMins.y, worldMins.z),
        Vector3(worldMins.x, worldMins.y, worldMaxs.z),
        Vector3(worldMins.x, worldMaxs.y, worldMaxs.z),
        Vector3(worldMaxs.x, worldMaxs.y, worldMaxs.z),
        Vector3(worldMaxs.x, worldMins.y, worldMaxs.z),
    }

    local projected = {}
    for i = 1, 8 do
        projected[i] = client.WorldToScreen(corners[i])
    end

    for i = 1, 8 do
        if not projected[i] then
            return
        end
    end

    local edges = {
        { 1, 2, "bottom", "left" },
        { 2, 3, "bottom", "front" },
        { 3, 4, "bottom", "right" },
        { 4, 1, "bottom", "back" },
        { 5, 6, "top",    "left" },
        { 6, 7, "top",    "front" },
        { 7, 8, "top",    "right" },
        { 8, 5, "top",    "back" },
        { 1, 5, "left",   "back" },
        { 2, 6, "left",   "front" },
        { 3, 7, "right",  "front" },
        { 4, 8, "right",  "back" },
    }

    local faces = buildBoxFaces(worldMins, worldMaxs)
    local facesVisible = {}
    for _, face in ipairs(faces) do
        facesVisible[face.id] = isFaceVisible(face.normal, face.center, self.eye_pos)
    end
    self.visible_faces = facesVisible

    local thickness = settings.thickness.bounding_box
    for _, edge in ipairs(edges) do
        local a = projected[edge[1]]
        local b = projected[edge[2]]
        local faceA = edge[3]
        local faceB = edge[4]

        local visibleA = facesVisible[faceA]
        local visibleB = facesVisible[faceB]

        if a and b and (visibleA or visibleB) then
            drawLine(self.texture, a, b, thickness)
        end
    end
end

local function drawQuads(self, pos, baseColor)
    if not (pos and self.target_min_hull and self.target_max_hull and self.eye_pos) then
        return
    end

    local worldMins = pos + self.target_min_hull
    local worldMaxs = pos + self.target_max_hull
    local vertices = getBoxVertices(pos, self.target_min_hull, self.target_max_hull)
    if not vertices then
        return
    end

    local projected = {}
    for index, vertex in ipairs(vertices) do
        projected[index] = client.WorldToScreen(vertex)
    end

    local faces = buildBoxFaces(worldMins, worldMaxs)

    local facesVisible = {}
    baseColor = baseColor or { r = 255, g = 255, b = 255, a = 25 }
    local baseR = baseColor.r or 255
    local baseG = baseColor.g or 255
    local baseB = baseColor.b or 255
    local baseA = baseColor.a or 255

    for _, face in ipairs(faces) do
        local visible = isFaceVisible(face.normal, face.center, self.eye_pos)
        facesVisible[face.id] = visible
        if visible then
            local toEyeX = self.eye_pos.x - face.center.x
            local toEyeY = self.eye_pos.y - face.center.y
            local toEyeZ = self.eye_pos.z - face.center.z
            local length = math.sqrt((toEyeX * toEyeX) + (toEyeY * toEyeY) + (toEyeZ * toEyeZ))
            local intensity = 1
            if length > 0 then
                local dirX = toEyeX / length
                local dirY = toEyeY / length
                local dirZ = toEyeZ / length
                local cosTheta = (dirX * face.normal.x) + (dirY * face.normal.y) + (dirZ * face.normal.z)
                if cosTheta < 0 then
                    cosTheta = 0
                elseif cosTheta > 1 then
                    cosTheta = 1
                end
                intensity = 0.42 + (cosTheta * 0.58)
            end

            local r = (baseR * intensity) // 1
            local g = (baseG * intensity) // 1
            local b = (baseB * intensity) // 1
            draw.Color(r, g, b, baseA)
            drawQuadFace(self.texture, projected, face.indices, face.flip_u, face.flip_v)
        end
    end

    self.visible_faces = facesVisible
end

function Visuals.new()
    local instance = setmetatable({}, Visuals)

    if draw and draw.CreateTextureRGBA then
        instance.texture = draw.CreateTextureRGBA(string.char(255, 255, 255, 255), 1, 1)
    else
        error("[PROJ AIMBOT] draw library unavailable - textures cannot be created")
    end

    instance.paths = {
        player_path = {},
        proj_path = {},
    }
    instance.displayed_time = 0
    instance.target_min_hull = Vector3()
    instance.target_max_hull = Vector3()
    instance.eye_pos = nil

    return instance
end

function Visuals:update_paths(playerPath, projPath)
    self.paths.player_path = playerPath or {}
    self.paths.proj_path = projPath or {}
end

function Visuals:set_multipoint_target(pos)
    self.multipoint_target_pos = pos
end

function Visuals:set_target_hull(mins, maxs)
    self.target_min_hull = mins or Vector3()
    self.target_max_hull = maxs or Vector3()
end

function Visuals:set_eye_position(pos)
    self.eye_pos = pos
end

function Visuals:set_displayed_time(time)
    self.displayed_time = time or 0
end

function Visuals:clear()
    self.paths.player_path = {}
    self.paths.proj_path = {}
    self.multipoint_target_pos = nil
    self.eye_pos = nil
end

function Visuals:draw()
    if not settings.enabled then
        return
    end

    local localPlayer = entities and entities.GetLocalPlayer and entities.GetLocalPlayer()
    if localPlayer then
        local origin = localPlayer:GetAbsOrigin()
        local viewOffset = localPlayer:GetPropVector("localdata", "m_vecViewOffset[0]")
        if origin and viewOffset then
            self.eye_pos = origin + viewOffset
        elseif origin then
            self.eye_pos = origin
        else
            self.eye_pos = nil
        end
    else
        self.eye_pos = nil
    end

    if not self.displayed_time or self.displayed_time < globals.CurTime() then
        self:clear()
        return
    end

    local playerPath = self.paths.player_path

    if settings.draw_player_path and playerPath and #playerPath > 0 then
        if settings.colors.player_path >= 360 then
            draw.Color(255, 255, 255, 255)
        else
            local r, g, b = hsvToRgb(settings.colors.player_path, 0.5, 1)
            draw.Color((r * 255) // 1, (g * 255) // 1, (b * 255) // 1, 255)
        end

        drawPlayerPath(self)
    end

    if settings.draw_bounding_box and playerPath and #playerPath > 0 then
        local pos = playerPath[#playerPath]
        if pos then
            if settings.colors.bounding_box >= 360 then
                draw.Color(255, 255, 255, 255)
            else
                local r, g, b = hsvToRgb(settings.colors.bounding_box, 0.5, 1)
                draw.Color((r * 255) // 1, (g * 255) // 1, (b * 255) // 1, 255)
            end

            drawPlayerHitbox(self, pos)
        end
    end

    if settings.draw_proj_path and self.paths.proj_path and #self.paths.proj_path > 0 then
        if settings.colors.projectile_path >= 360 then
            draw.Color(255, 255, 255, 255)
        else
            local r, g, b = hsvToRgb(settings.colors.projectile_path, 0.5, 1)
            draw.Color((r * 255) // 1, (g * 255) // 1, (b * 255) // 1, 255)
        end

        drawProjPath(self)
    end

    if settings.draw_multipoint_target then
        if settings.colors.multipoint_target >= 360 then
            draw.Color(255, 255, 255, 255)
        else
            local r, g, b = hsvToRgb(settings.colors.multipoint_target, 0.5, 1)
            draw.Color((r * 255) // 1, (g * 255) // 1, (b * 255) // 1, 255)
        end

        drawMultipointTarget(self)
    end

    if settings.draw_quads and playerPath and #playerPath > 0 then
        local pos = playerPath[#playerPath]
        if pos then
            local baseColor
            if settings.colors.quads >= 360 then
                baseColor = { r = 255, g = 255, b = 255, a = 25 }
            else
                local r, g, b = hsvToRgb(settings.colors.quads, 0.5, 1)
                baseColor = {
                    r = (r * 255) // 1,
                    g = (g * 255) // 1,
                    b = (b * 255) // 1,
                    a = 25,
                }
            end

            drawQuads(self, pos, baseColor)
        end
    end
end

function Visuals:destroy()
    if self.texture then
        draw.DeleteTexture(self.texture)
        self.texture = nil
    end
end

return Visuals

end)
__bundle_register("src.projectile_info", function(require, _LOADED, __bundle_register, __bundle_modules)
--[[
    This is a port of the GetProjectileInformation function
    from GoodEvening's Visualize Arc Trajectories

    His Github: https://github.com/GoodEveningFellOff
    Source: https://github.com/GoodEveningFellOff/Lmaobox-Scripts/blob/main/Visualize%20Arc%20Trajectories/dev.lua
--]]

local TRACE_HULL = engine.TraceHull
local CLAMP = function(a, b, c)
	return (a < b) and b or (a > c) and c or a
end
local VEC_ROT = function(a, b)
	return (b:Forward() * a.x) + (b:Right() * a.y) + (b:Up() * a.z)
end

local aProjectileInfo = {}
local aItemDefinitions = {}

local PROJECTILE_TYPE_BASIC = 0
local PROJECTILE_TYPE_PSEUDO = 1
local PROJECTILE_TYPE_SIMUL = 2

local COLLISION_NORMAL = 0
local COLLISION_HEAL_TEAMMATES = 1
local COLLISION_HEAL_BUILDINGS = 2
local COLLISION_HEAL_HURT = 3
local COLLISION_NONE = 4

local function AppendItemDefinitions(iType, ...)
	for _, i in pairs({ ... }) do
		aItemDefinitions[i] = iType
	end
end

---@return WeaponInfo
function GetProjectileInformation(itemDefinitionIndex)
	return aProjectileInfo[aItemDefinitions[itemDefinitionIndex or 0]]
end

---@return WeaponInfo?
local function DefineProjectileDefinition(tbl)
	return {
		m_iType = PROJECTILE_TYPE_BASIC,
		m_vecOffset = tbl.vecOffset or Vector3(0, 0, 0),
		m_vecAbsoluteOffset = tbl.vecAbsoluteOffset or Vector3(0, 0, 0),
		m_vecAngleOffset = tbl.vecAngleOffset or Vector3(0, 0, 0),
		m_vecVelocity = tbl.vecVelocity or Vector3(0, 0, 0),
		m_vecAngularVelocity = tbl.vecAngularVelocity or Vector3(0, 0, 0),
		m_vecMins = tbl.vecMins or (not tbl.vecMaxs) and Vector3(0, 0, 0) or -tbl.vecMaxs,
		m_vecMaxs = tbl.vecMaxs or (not tbl.vecMins) and Vector3(0, 0, 0) or -tbl.vecMins,
		m_flGravity = tbl.flGravity or 0.001,
		m_flDrag = tbl.flDrag or 0,
		m_flElasticity = tbl.flElasticity or 0,
		m_iAlignDistance = tbl.iAlignDistance or 0,
		m_iTraceMask = tbl.iTraceMask or 33570827, -- MASK_SOLID
		m_iCollisionType = tbl.iCollisionType or COLLISION_NORMAL,
		m_flCollideWithTeammatesDelay = tbl.flCollideWithTeammatesDelay or 0.25,
		m_flLifetime = tbl.flLifetime or 99999,
		m_flDamageRadius = tbl.flDamageRadius or 0,
		m_bStopOnHittingEnemy = tbl.bStopOnHittingEnemy ~= false,
		m_bCharges = tbl.bCharges or false,
		m_sModelName = tbl.sModelName or "",
		m_bHasGravity = tbl.bGravity == nil and true or tbl.bGravity,

		GetOffset = not tbl.GetOffset
				and function(self, bDucking, bIsFlipped)
					return bIsFlipped and Vector3(self.m_vecOffset.x, -self.m_vecOffset.y, self.m_vecOffset.z)
						or self.m_vecOffset
				end
			or tbl.GetOffset, -- self, bDucking, bIsFlipped

		GetAngleOffset = (not tbl.GetAngleOffset) and function(self, flChargeBeginTime)
			return self.m_vecAngleOffset
		end or tbl.GetAngleOffset, -- self, flChargeBeginTime

		GetFirePosition = tbl.GetFirePosition or function(self, pLocalPlayer, vecLocalView, vecViewAngles, bIsFlipped)
			local resultTrace = TRACE_HULL(
				vecLocalView,
				vecLocalView
					+ VEC_ROT(
						self:GetOffset((pLocalPlayer:GetPropInt("m_fFlags") & FL_DUCKING) ~= 0, bIsFlipped),
						vecViewAngles
					),
				-Vector3(8, 8, 8),
				Vector3(8, 8, 8),
				MASK_SHOT_HULL
			) -- MASK_SHOT_HULL

			return (not resultTrace.startsolid) and resultTrace.endpos or nil
		end,

		GetVelocity = (not tbl.GetVelocity) and function(self, ...)
			return self.m_vecVelocity
		end or tbl.GetVelocity, -- self, flChargeBeginTime

		GetAngularVelocity = (not tbl.GetAngularVelocity) and function(self, ...)
			return self.m_vecAngularVelocity
		end or tbl.GetAngularVelocity, -- self, flChargeBeginTime

		GetGravity = (not tbl.GetGravity) and function(self, ...)
			return self.m_flGravity
		end or tbl.GetGravity, -- self, flChargeBeginTime

		GetLifetime = (not tbl.GetLifetime) and function(self, ...)
			return self.m_flLifetime
		end or tbl.GetLifetime, -- self, flChargeBeginTime

		HasGravity = (not tbl.HasGravity) and function(self, ...)
			return self.m_bHasGravity
		end or tbl.HasGravity,
	}
end

local function DefineBasicProjectileDefinition(tbl)
	local stReturned = DefineProjectileDefinition(tbl)
	stReturned.m_iType = PROJECTILE_TYPE_BASIC

	return stReturned
end

local function DefinePseudoProjectileDefinition(tbl)
	local stReturned = DefineProjectileDefinition(tbl)
	stReturned.m_iType = PROJECTILE_TYPE_PSEUDO

	return stReturned
end

local function DefineSimulProjectileDefinition(tbl)
	local stReturned = DefineProjectileDefinition(tbl)
	stReturned.m_iType = PROJECTILE_TYPE_SIMUL

	return stReturned
end

local function DefineDerivedProjectileDefinition(def, tbl)
	local stReturned = {}
	for k, v in pairs(def) do
		stReturned[k] = v
	end
	for k, v in pairs(tbl) do
		stReturned[((type(v) ~= "function") and "m_" or "") .. k] = v
	end

	if not tbl.GetOffset and tbl.vecOffset then
		stReturned.GetOffset = function(self, bDucking, bIsFlipped)
			return bIsFlipped and Vector3(self.m_vecOffset.x, -self.m_vecOffset.y, self.m_vecOffset.z)
				or self.m_vecOffset
		end
	end

	if not tbl.GetAngleOffset and tbl.vecAngleOffset then
		stReturned.GetAngleOffset = function(self, flChargeBeginTime)
			return self.m_vecAngleOffset
		end
	end

	if not tbl.GetVelocity and tbl.vecVelocity then
		stReturned.GetVelocity = function(self, ...)
			return self.m_vecVelocity
		end
	end

	if not tbl.GetAngularVelocity and tbl.vecAngularVelocity then
		stReturned.GetAngularVelocity = function(self, ...)
			return self.m_vecAngularVelocity
		end
	end

	if not tbl.GetGravity and tbl.flGravity then
		stReturned.GetGravity = function(self, ...)
			return self.m_flGravity
		end
	end

	if not tbl.GetLifetime and tbl.flLifetime then
		stReturned.GetLifetime = function(self, ...)
			return self.m_flLifetime
		end
	end

	return stReturned
end

AppendItemDefinitions(
	1,
	18, -- Rocket Launcher
	205, -- Rocket Launcher (Renamed/Strange)
	228, -- The Black Box
	658, -- Festive Rocket Launcher
	800, -- Silver Botkiller Rocket Launcher Mk.I
	809, -- Gold Botkiller Rocket Launcher Mk.I
	889, -- Rust Botkiller Rocket Launcher Mk.I
	898, -- Blood Botkiller Rocket Launcher Mk.I
	907, -- Carbonado Botkiller Rocket Launcher Mk.I
	916, -- Diamond Botkiller Rocket Launcher Mk.I
	965, -- Silver Botkiller Rocket Launcher Mk.II
	974, -- Gold Botkiller Rocket Launcher Mk.II
	1085, -- Festive Black Box
	15006, -- Woodland Warrior
	15014, -- Sand Cannon
	15028, -- American Pastoral
	15043, -- Smalltown Bringdown
	15052, -- Shell Shocker
	15057, -- Aqua Marine
	15081, -- Autumn
	15104, -- Blue Mew
	15105, -- Brain Candy
	15129, -- Coffin Nail
	15130, -- High Roller's
	15150 -- Warhawk
)
aProjectileInfo[1] = DefineBasicProjectileDefinition({
	vecVelocity = Vector3(1100, 0, 0),
	vecMaxs = Vector3(0, 0, 0),
	iAlignDistance = 2000,
	flDamageRadius = 146,
	bGravity = false,

	GetOffset = function(self, bDucking, bIsFlipped)
		return Vector3(23.5, 12 * (bIsFlipped and -1 or 1), bDucking and 8 or -3)
	end,
})

AppendItemDefinitions(
	2,
	237 -- Rocket Jumper
)
aProjectileInfo[2] = DefineDerivedProjectileDefinition(aProjectileInfo[1], {
	iCollisionType = COLLISION_NONE,
	bGravity = false,
})

AppendItemDefinitions(
	3,
	730 -- The Beggar's Bazooka
)
aProjectileInfo[3] = DefineDerivedProjectileDefinition(aProjectileInfo[1], {
	flDamageRadius = 116.8,
	bGravity = false,
})

AppendItemDefinitions(
	4,
	1104 -- The Air Strike
)
aProjectileInfo[4] = DefineDerivedProjectileDefinition(aProjectileInfo[1], {
	flDamageRadius = 131.4,
})

AppendItemDefinitions(
	5,
	127 -- The Direct Hit
)
aProjectileInfo[5] = DefineDerivedProjectileDefinition(aProjectileInfo[1], {
	vecVelocity = Vector3(2000, 0, 0),
	flDamageRadius = 44,
	bGravity = false,
})

AppendItemDefinitions(
	6,
	414 -- The Liberty Launcher
)
aProjectileInfo[6] = DefineDerivedProjectileDefinition(aProjectileInfo[1], {
	vecVelocity = Vector3(1550, 0, 0),
	bGravity = false,
})

AppendItemDefinitions(
	7,
	513 -- The Original
)
aProjectileInfo[7] = DefineDerivedProjectileDefinition(aProjectileInfo[1], {
	bGravity = false,
	GetOffset = function(self, bDucking)
		return Vector3(23.5, 0, bDucking and 8 or -3)
	end,
})

-- https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_dragons_fury.cpp
AppendItemDefinitions(
	8,
	1178 -- Dragon's Fury
)
aProjectileInfo[8] = DefineBasicProjectileDefinition({
	vecVelocity = Vector3(1600, 0, 0), --Vector3(600, 0, 0),
	vecMaxs = Vector3(1, 1, 1),
	bGravity = false,

	GetOffset = function(self, bDucking, bIsFlipped)
		return Vector3(3, 7, -9)
	end,
})

AppendItemDefinitions(
	9,
	442 -- The Righteous Bison
)
aProjectileInfo[9] = DefineBasicProjectileDefinition({
	vecVelocity = Vector3(1200, 0, 0),
	vecMaxs = Vector3(1, 1, 1),
	iAlignDistance = 2000,
	bGravity = false,

	GetOffset = function(self, bDucking, bIsFlipped)
		return Vector3(23.5, -8 * (bIsFlipped and -1 or 1), bDucking and 8 or -3)
	end,
})

AppendItemDefinitions(
	10,
	20, -- Stickybomb Launcher
	207, -- Stickybomb Launcher (Renamed/Strange)
	661, -- Festive Stickybomb Launcher
	797, -- Silver Botkiller Stickybomb Launcher Mk.I
	806, -- Gold Botkiller Stickybomb Launcher Mk.I
	886, -- Rust Botkiller Stickybomb Launcher Mk.I
	895, -- Blood Botkiller Stickybomb Launcher Mk.I
	904, -- Carbonado Botkiller Stickybomb Launcher Mk.I
	913, -- Diamond Botkiller Stickybomb Launcher Mk.I
	962, -- Silver Botkiller Stickybomb Launcher Mk.II
	971, -- Gold Botkiller Stickybomb Launcher Mk.II
	15009, -- Sudden Flurry
	15012, -- Carpet Bomber
	15024, -- Blasted Bombardier
	15038, -- Rooftop Wrangler
	15045, -- Liquid Asset
	15048, -- Pink Elephant
	15082, -- Autumn
	15083, -- Pumpkin Patch
	15084, -- Macabre Web
	15113, -- Sweet Dreams
	15137, -- Coffin Nail
	15138, -- Dressed to Kill
	15155 -- Blitzkrieg
)
aProjectileInfo[10] = DefineSimulProjectileDefinition({
	vecOffset = Vector3(16, 8, -6),
	vecAngularVelocity = Vector3(600, 0, 0),
	vecMaxs = Vector3(3.5, 3.5, 3.5),
	bCharges = true,
	flDamageRadius = 150,
	sModelName = "models/weapons/w_models/w_stickybomb.mdl",
	flGravity = 0.25,

	GetVelocity = function(self, flChargeBeginTime)
		return Vector3(900 + CLAMP(flChargeBeginTime / 4, 0, 1) * 1500, 0, 200)
	end,
})

AppendItemDefinitions(
	11,
	1150 -- The Quickiebomb Launcher
)
aProjectileInfo[11] = DefineDerivedProjectileDefinition(aProjectileInfo[10], {
	sModelName = "models/workshop/weapons/c_models/c_kingmaker_sticky/w_kingmaker_stickybomb.mdl",
	flGravity = 0.25,
	GetVelocity = function(self, flChargeBeginTime)
		return Vector3(900 + CLAMP(flChargeBeginTime / 1.2, 0, 1) * 1500, 0, 200)
	end,
})

AppendItemDefinitions(
	12,
	130 -- The Scottish Resistance
)
aProjectileInfo[12] = DefineDerivedProjectileDefinition(aProjectileInfo[10], {
	sModelName = "models/weapons/w_models/w_stickybomb_d.mdl",
	flGravity = 0.25,
})

AppendItemDefinitions(
	13,
	265 -- Sticky Jumper
)
aProjectileInfo[13] = DefineDerivedProjectileDefinition(aProjectileInfo[12], {
	iCollisionType = COLLISION_NONE,
	flGravity = 0.25,
})

AppendItemDefinitions(
	14,
	19, -- Grenade Launcher
	206, -- Grenade Launcher (Renamed/Strange)
	1007, -- Festive Grenade Launcher
	15077, -- Autumn
	15079, -- Macabre Web
	15091, -- Rainbow
	15092, -- Sweet Dreams
	15116, -- Coffin Nail
	15117, -- Top Shelf
	15142, -- Warhawk
	15158 -- Butcher Bird
)
aProjectileInfo[14] = DefineSimulProjectileDefinition({
	vecOffset = Vector3(16, 8, -6),
	vecVelocity = Vector3(1200, 0, 200),
	vecAngularVelocity = Vector3(600, 0, 0),
	flGravity = 0.25,
	vecMaxs = Vector3(2, 2, 2),
	flElasticity = 0.45,
	flLifetime = 2.175,
	flDamageRadius = 146,
	sModelName = "models/weapons/w_models/w_grenade_grenadelauncher.mdl",
})

AppendItemDefinitions(
	15,
	1151 -- The Iron Bomber
)
aProjectileInfo[15] = DefineDerivedProjectileDefinition(aProjectileInfo[14], {
	flElasticity = 0.09,
	flLifetime = 1.6,
	flDamageRadius = 124,
})

AppendItemDefinitions(
	16,
	308 -- The Loch-n-Load
)
aProjectileInfo[16] = DefineDerivedProjectileDefinition(aProjectileInfo[14], {
	iType = PROJECTILE_TYPE_PSEUDO,
	vecVelocity = Vector3(1500, 0, 200),
	flDrag = 0.225,
	flGravity = 1,
	flLifetime = 2.3,
	flDamageRadius = 0,
})

AppendItemDefinitions(
	17,
	996 -- The Loose Cannon
)
aProjectileInfo[17] = DefineDerivedProjectileDefinition(aProjectileInfo[14], {
	vecVelocity = Vector3(1440, 0, 200),
	vecMaxs = Vector3(6, 6, 6),
	bStopOnHittingEnemy = false,
	bCharges = true,
	sModelName = "models/weapons/w_models/w_cannonball.mdl",

	GetLifetime = function(self, flChargeBeginTime)
		return 1 * flChargeBeginTime
	end,
})

AppendItemDefinitions(
	18,
	56, -- The Huntsman
	1005, -- Festive Huntsman
	1092 -- The Fortified Compound
)
aProjectileInfo[18] = DefinePseudoProjectileDefinition({
	vecOffset = Vector3(23.5, -8, -3),
	vecMaxs = Vector3(0, 0, 0),
	iAlignDistance = 2000,
	bCharges = true,

	GetVelocity = function(self, flChargeBeginTime)
		return Vector3(1800 + CLAMP(flChargeBeginTime, 0, 1) * 800, 0, 0)
	end,

	GetGravity = function(self, flChargeBeginTime)
		return 0.5 - CLAMP(flChargeBeginTime, 0, 1) * 0.4
	end,
})

AppendItemDefinitions(
	19,
	39, -- The Flare Gun
	351, -- The Detonator
	595, -- The Manmelter
	1081 -- Festive Flare Gun
)
aProjectileInfo[19] = DefinePseudoProjectileDefinition({
	vecVelocity = Vector3(2000, 0, 0),
	vecMaxs = Vector3(0, 0, 0),
	flGravity = 0.3,
	flDrag = 0.5,
	iAlignDistance = 2000,
	flCollideWithTeammatesDelay = 0.25,

	GetOffset = function(self, bDucking, bIsFlipped)
		return Vector3(23.5, 12 * (bIsFlipped and -1 or 1), bDucking and 8 or -3)
	end,
})

AppendItemDefinitions(
	20,
	740 -- The Scorch Shot
)
aProjectileInfo[20] = DefineDerivedProjectileDefinition(aProjectileInfo[19], {
	flDamageRadius = 110,
})

AppendItemDefinitions(
	21,
	305, -- Crusader's Crossbow
	1079 -- Festive Crusader's Crossbow
)
aProjectileInfo[21] = DefinePseudoProjectileDefinition({
	vecOffset = Vector3(23.5, -8, -3),
	vecVelocity = Vector3(2400, 0, 0),
	vecMaxs = Vector3(3, 3, 3),
	flGravity = 0.2,
	iAlignDistance = 2000,
	iCollisionType = COLLISION_HEAL_TEAMMATES,
})

AppendItemDefinitions(
	22,
	997 -- The Rescue Ranger
)
aProjectileInfo[22] = DefineDerivedProjectileDefinition(aProjectileInfo[21], {
	vecMaxs = Vector3(1, 1, 1),
	iCollisionType = COLLISION_HEAL_BUILDINGS,
})

AppendItemDefinitions(
	23,
	17, -- Syringe Gun
	36, -- The Blutsauger
	204, -- Syringe Gun (Renamed/Strange)
	412 -- The Overdose
)
aProjectileInfo[23] = DefinePseudoProjectileDefinition({
	vecOffset = Vector3(16, 6, -8),
	vecVelocity = Vector3(1000, 0, 0),
	vecMaxs = Vector3(1, 1, 1),
	flGravity = 0.3,
	flCollideWithTeammatesDelay = 0,
})

AppendItemDefinitions(
	24,
	58, -- Jarate
	222, -- Mad Milk
	1083, -- Festive Jarate
	1105, -- The Self-Aware Beauty Mark
	1121 -- Mutated Milk
)
aProjectileInfo[24] = DefinePseudoProjectileDefinition({
	vecOffset = Vector3(16, 8, -6),
	vecVelocity = Vector3(1000, 0, 200),
	vecMaxs = Vector3(8, 8, 8),
	flGravity = 1.125,
	flDamageRadius = 200,
})

AppendItemDefinitions(
	25,
	812, -- The Flying Guillotine
	833 -- The Flying Guillotine (Genuine)
)
aProjectileInfo[25] = DefinePseudoProjectileDefinition({
	vecOffset = Vector3(23.5, 8, -3),
	vecVelocity = Vector3(3000, 0, 300),
	vecMaxs = Vector3(2, 2, 2),
	flGravity = 2.25,
	flDrag = 1.3,
})

AppendItemDefinitions(
	26,
	44 -- The Sandman
)
aProjectileInfo[26] = DefineSimulProjectileDefinition({
	vecVelocity = Vector3(2985.1118164063, 0, 298.51116943359),
	vecAngularVelocity = Vector3(0, 50, 0),
	vecMaxs = Vector3(4.25, 4.25, 4.25),
	flElasticity = 0.45,
	sModelName = "models/weapons/w_models/w_baseball.mdl",

	GetFirePosition = function(self, pLocalPlayer, vecLocalView, vecViewAngles, bIsFlipped)
		--https://github.com/ValveSoftware/source-sdk-2013/blob/0565403b153dfcde602f6f58d8f4d13483696a13/src/game/shared/tf/tf_weapon_bat.cpp#L232
		local vecFirePos = pLocalPlayer:GetAbsOrigin()
			+ ((Vector3(0, 0, 50) + (vecViewAngles:Forward() * 32)) * pLocalPlayer:GetPropFloat("m_flModelScale"))

		local resultTrace = TRACE_HULL(vecLocalView, vecFirePos, -Vector3(8, 8, 8), Vector3(8, 8, 8), MASK_SHOT_HULL) -- MASK_SOLID_BRUSHONLY

		return (resultTrace.fraction == 1) and resultTrace.endpos or nil
	end,
})

AppendItemDefinitions(
	27,
	648 -- The Wrap Assassin
)
aProjectileInfo[27] = DefineDerivedProjectileDefinition(aProjectileInfo[26], {
	vecMins = Vector3(-2.990180015564, -2.5989532470703, -2.483987569809),
	vecMaxs = Vector3(2.6593606472015, 2.5989530086517, 2.4839873313904),
	flElasticity = 0,
	flDamageRadius = 50,
	sModelName = "models/weapons/c_models/c_xms_festive_ornament.mdl",
})

AppendItemDefinitions(
	28,
	441 -- The Cow Mangler 5000
)
aProjectileInfo[28] = DefineDerivedProjectileDefinition(aProjectileInfo[1], {
	bGravity = false,
	GetOffset = function(self, bDucking, bIsFlipped)
		return Vector3(23.5, 8 * (bIsFlipped and 1 or -1), bDucking and 8 or -3)
	end,
})

--https://github.com/ValveSoftware/source-sdk-2013/blob/0565403b153dfcde602f6f58d8f4d13483696a13/src/game/shared/tf/tf_weapon_raygun.cpp#L249
AppendItemDefinitions(
	29,
	588 -- The Pomson 6000
)
aProjectileInfo[29] = DefineDerivedProjectileDefinition(aProjectileInfo[9], {
	vecAbsoluteOffset = Vector3(0, 0, -13),
	flCollideWithTeammatesDelay = 0,
	bGravity = false,
})

AppendItemDefinitions(
	30,
	1180 -- Gas Passer
)
aProjectileInfo[30] = DefinePseudoProjectileDefinition({
	vecOffset = Vector3(16, 8, -6),
	vecVelocity = Vector3(2000, 0, 200),
	vecMaxs = Vector3(8, 8, 8),
	flGravity = 1,
	flDrag = 1.32,
	flDamageRadius = 200,
})

AppendItemDefinitions(
	31,
	528 -- The Short Circuit
)
aProjectileInfo[31] = DefineBasicProjectileDefinition({
	vecOffset = Vector3(40, 15, -10),
	vecVelocity = Vector3(700, 0, 0),
	vecMaxs = Vector3(1, 1, 1),
	flCollideWithTeammatesDelay = 99999,
	flLifetime = 1.25,
	bGravity = false,
})

AppendItemDefinitions(
	32,
	42, -- Sandvich
	159, -- The Dalokohs Bar
	311, -- The Buffalo Steak Sandvich
	433, -- Fishcake
	863, -- Robo-Sandvich
	1002, -- Festive Sandvich
	1190 -- Second Banana
)
aProjectileInfo[32] = DefinePseudoProjectileDefinition({
	vecOffset = Vector3(0, 0, -8),
	vecAngleOffset = Vector3(-10, 0, 0),
	vecVelocity = Vector3(500, 0, 0),
	vecMaxs = Vector3(17, 17, 10),
	flGravity = 1.02,
	iTraceMask = MASK_SHOT_HULL, -- MASK_SHOT_HULL
	iCollisionType = COLLISION_HEAL_HURT,
})

return GetProjectileInformation

end)
__bundle_register("src.simulation.proj", function(require, _LOADED, __bundle_register, __bundle_modules)
--- Not used (yet)

local sim = {}

local env = physics.CreateEnvironment()

env:SetAirDensity(2.0)
env:SetGravity(Vector3(0, 0, -800))
env:SetSimulationTimestep(globals.TickInterval())

local MASK_SHOT_HULL = MASK_SHOT_HULL

---@type table<integer, PhysicsObject>
local projectiles = {}

local function CreateProjectile(model, i)
    local solid, collisionModel = physics.ParseModelByName(model)
    if not solid or not collisionModel then
        printc(255, 100, 100, 255, string.format("[PROJ AIMBOT] Failed to parse model: %s", model))
        return nil
    end

    local surfaceProp = solid:GetSurfacePropName()
    local objectParams = solid:GetObjectParameters()
    if not surfaceProp or not objectParams then
        printc(255, 100, 100, 255, "[PROJ AIMBOT] Invalid surface properties or parameters")
        return nil
    end

    local projectile = env:CreatePolyObject(collisionModel, surfaceProp, objectParams)
    if not projectile then
        printc(255, 100, 100, 255, "[PROJ AIMBOT] Failed to create poly object")
        return nil
    end

    projectiles[i] = projectile

    printc(150, 255, 150, 255, string.format("[PROJ AIMBOT] Projectile with model %s created", model))
    return projectile
end

--- source: https://developer.mozilla.org/en-US/docs/Games/Techniques/3D_collision_detection
---@param currentPos Vector3
---@param vecTargetPredictedPos Vector3
---@param weaponInfo WeaponInfo
---@param vecTargetMaxs Vector3
---@param vecTargetMins Vector3
local function IsIntersectingBB(currentPos, vecTargetPredictedPos, weaponInfo, vecTargetMaxs, vecTargetMins)
    local vecProjMins = weaponInfo.m_vecMins + currentPos
    local vecProjMaxs = weaponInfo.m_vecMaxs + currentPos

    local targetMins = vecTargetMins + vecTargetPredictedPos
    local targetMaxs = vecTargetMaxs + vecTargetPredictedPos

    -- check overlap on X, Y, and Z
    if vecProjMaxs.x < targetMins.x or vecProjMins.x > targetMaxs.x then return false end
    if vecProjMaxs.y < targetMins.y or vecProjMins.y > targetMaxs.y then return false end
    if vecProjMaxs.z < targetMins.z or vecProjMins.z > targetMaxs.z then return false end

    return true -- all axis overlap
end

---@param pTarget Entity The target
---@param pLocal Entity The localplayer
---@param pWeapon Entity The localplayer's weapon
---@param shootPos Vector3
---@param vecForward Vector3 The target direction the projectile should aim for
---@param nTime number Number of seconds we want to simulate
---@param weapon_info WeaponInfo
---@param charge_time number The charge time (0.0 to 1.0 for bows, 0.0 to 4.0 for stickies)
---@param vecPredictedPos Vector3
---@return ProjSimRet, boolean
function sim.Run(pTarget, pLocal, pWeapon, shootPos, vecForward, vecPredictedPos, nTime, weapon_info, charge_time)
    local projectile = projectiles[pWeapon:GetPropInt("m_iItemDefinitionIndex")]
    if not projectile then
        if weapon_info.m_sModelName and weapon_info.m_sModelName ~= "" then
            ---@diagnostic disable-next-line: cast-local-type
            projectile = CreateProjectile(weapon_info.m_sModelName, pWeapon:GetPropInt("m_iItemDefinitionIndex"))
        else
            if not projectiles[-1] then
                CreateProjectile("models/weapons/w_models/w_rocket.mdl", -1)
            end
            projectile = projectiles[-1]
        end
    end

    if not projectile then
        printc(255, 0, 0, 255, "[PROJ AIMBOT] Failed to acquire projectile instance!")
        return {}, false
    end

    projectile:Wake()

    local mins, maxs = weapon_info.m_vecMins, weapon_info.m_vecMaxs
    local targetmins, targetmaxs = pTarget:GetMaxs(), pTarget:GetMins()

    -- Decide trace mode: use line trace only for rocket-type projectiles
    local proj_type = pWeapon:GetWeaponProjectileType() or 0
    local use_line_trace = (
        proj_type == E_ProjectileType.TF_PROJECTILE_ROCKET or
        proj_type == E_ProjectileType.TF_PROJECTILE_FLAME_ROCKET or
        proj_type == E_ProjectileType.TF_PROJECTILE_SENTRY_ROCKET
    )
    local trace_mask = weapon_info.m_iTraceMask or MASK_SHOT_HULL
    local filter = function(ent)
        if ent:GetTeamNumber() ~= pLocal:GetTeamNumber() then
            return false
        end

        if ent:GetIndex() == pLocal:GetIndex() then
            return false
        end

        return true
    end

    -- Get the velocity vector from weapon info (includes upward velocity)
    local velocity_vector = weapon_info:GetVelocity(charge_time)
    local forward_speed = velocity_vector.x
    local upward_speed = velocity_vector.z or 0

    -- Calculate the final velocity vector with proper upward component
    local velocity = (vecForward * forward_speed) + (Vector3(0, 0, 1) * upward_speed)

    local has_gravity = weapon_info:HasGravity()
    if has_gravity then
        env:SetGravity(Vector3(0, 0, -800))
    else
        env:SetGravity(Vector3(0, 0, 0))
    end

    projectile:SetPosition(shootPos, vecForward, true)
    projectile:SetVelocity(velocity, weapon_info:GetAngularVelocity(charge_time))

    local tickInterval = globals.TickInterval()
    local positions = {}
    local hittarget = false

    while env:GetSimulationTime() < nTime do
        local currentPos = projectile:GetPosition()

        -- Perform a single collision trace per tick using the pre-decided mode
        local trace
        if use_line_trace then
            trace = engine.TraceLine(shootPos, currentPos, trace_mask, filter)
        else
            trace = engine.TraceHull(shootPos, currentPos, mins, maxs, trace_mask, filter)
        end

        if trace and trace.fraction >= 1 then
            local record = {
                pos = currentPos,
                time_secs = env:GetSimulationTime(),
            }

            positions[#positions + 1] = record
            shootPos = currentPos

            if IsIntersectingBB(currentPos, vecPredictedPos, weapon_info, targetmins, targetmaxs) then
                hittarget = true
                break
            end
        else
            break
        end

        env:Simulate(tickInterval)
    end

    env:ResetSimulationClock()
    projectile:Sleep()
    return positions, hittarget
end

return sim

end)
__bundle_register("src.simulation.player", function(require, _LOADED, __bundle_register, __bundle_modules)
---@diagnostic disable: duplicate-doc-field, missing-fields

local sim = {}

local MASK_PLAYERSOLID = MASK_PLAYERSOLID
local DoTraceHull = engine.TraceHull
local Vector3 = Vector3
local math_deg = math.deg
local math_rad = math.rad
local math_atan = math.atan
local math_cos = math.cos
local math_sin = math.sin
local math_abs = math.abs
local math_min = math.min
local math_max = math.max
local math_floor = math.floor
local math_pi = math.pi

local MIN_SPEED = 25
local MAX_ANGULAR_VEL = 360
local AIR_ACCELERATE = 10.0
local GROUND_ACCELERATE = 10.0
local SURFACE_FRICTION = 1.0
local MAX_CLIP_PLANES = 5
local DIST_EPSILON = 0.03125
local MAX_SAMPLES = 64
local IMPACT_NORMAL_FLOOR = 0.7

local temp_vec1 = Vector3()
local temp_vec2 = Vector3()
local temp_vec3 = Vector3()

local clip_planes = {}
for i = 1, MAX_CLIP_PLANES do
	clip_planes[i] = Vector3()
end

local position_samples = {}
local settings = require("src.settings")

local RuneTypes_t = {
	RUNE_NONE = -1,
	RUNE_STRENGTH = 0,
	RUNE_HASTE = 1,
	RUNE_REGEN = 2,
	RUNE_RESIST = 3,
	RUNE_VAMPIRE = 4,
	RUNE_REFLECT = 5,
	RUNE_PRECISION = 6,
	RUNE_AGILITY = 7,
	RUNE_KNOCKOUT = 8,
	RUNE_KING = 9,
	RUNE_PLAGUE = 10,
	RUNE_SUPERNOVA = 11,
	RUNE_TYPES_MAX = 12,
}

local function GetEntityOrigin(pEntity)
	return pEntity:GetPropVector("tflocaldata", "m_vecOrigin") or pEntity:GetAbsOrigin()
end

---@param pTarget Entity
---@return number
local function GetAirSpeedCap(pTarget)
	local m_hGrapplingHookTarget = pTarget:GetPropEntity("m_hGrapplingHookTarget")
	if m_hGrapplingHookTarget then
		if pTarget:GetCarryingRuneType() == RuneTypes_t.RUNE_AGILITY then
			local m_iClass = pTarget:GetPropInt("m_iClass")
			return (m_iClass == E_Character.TF2_Soldier or E_Character.TF2_Heavy) and 850 or 950
		end
		local _, tf_grapplinghook_move_speed = client.GetConVar("tf_grapplinghook_move_speed")
		return tf_grapplinghook_move_speed
	elseif pTarget:InCond(E_TFCOND.TFCond_Charging) then
		local _, tf_max_charge_speed = client.GetConVar("tf_max_charge_speed")
		return tf_max_charge_speed
	else
		local flCap = 30.0
		if pTarget:InCond(E_TFCOND.TFCond_ParachuteDeployed) then
			local _, tf_parachute_aircontrol = client.GetConVar("tf_parachute_aircontrol")
			flCap = flCap * tf_parachute_aircontrol
		end
		if pTarget:InCond(E_TFCOND.TFCond_HalloweenKart) then
			if pTarget:InCond(E_TFCOND.TFCond_HalloweenKartDash) then
				local _, tf_halloween_kart_dash_speed = client.GetConVar("tf_halloween_kart_dash_speed")
				return tf_halloween_kart_dash_speed
			end
			local _, tf_hallowen_kart_aircontrol = client.GetConVar("tf_hallowen_kart_aircontrol")
			flCap = flCap * tf_hallowen_kart_aircontrol
		end
		return flCap * pTarget:AttributeHookFloat("mod_air_control")
	end
end

-- Inline ClipVelocity to avoid function call overhead
local function ClipVelocity(velocity, normal, overbounce)
	local backoff = velocity.x * normal.x + velocity.y * normal.y + velocity.z * normal.z
	backoff = (backoff < 0) and (backoff * overbounce) or (backoff / overbounce)
	velocity.x = velocity.x - normal.x * backoff
	velocity.y = velocity.y - normal.y * backoff
	velocity.z = velocity.z - normal.z * backoff
end

local function AccelerateInPlace(velocity, wishdir, wishspeed, accel, dt, surf)
	local currentspeed = velocity:Length()
	local addspeed = wishspeed - currentspeed
	if addspeed <= 0 then
		return
	end

	local accelspeed = math_min(accel * dt * wishspeed * surf, addspeed)
	velocity.x = velocity.x + accelspeed * wishdir.x
	velocity.y = velocity.y + accelspeed * wishdir.y
	velocity.z = velocity.z + accelspeed * wishdir.z
end

local function AirAccelerateInPlace(v, wishdir, wishspeed, accel, dt, surf, pTarget)
	wishspeed = math_min(wishspeed, GetAirSpeedCap(pTarget))
	local currentspeed = v:Length()
	local addspeed = wishspeed - currentspeed
	if addspeed <= 0 then
		return
	end

	local accelspeed = math_min(accel * wishspeed * dt * surf, addspeed)
	v.x = v.x + accelspeed * wishdir.x
	v.y = v.y + accelspeed * wishdir.y
	v.z = v.z + accelspeed * wishdir.z
end

---@param pEntity Entity
local function AddPositionSample(pEntity)
	local index = pEntity:GetIndex()
	if not position_samples[index] then
		position_samples[index] = {}
	end

	local samples = position_samples[index]
	local sample = { pos = GetEntityOrigin(pEntity), time = globals.CurTime() }
	samples[#samples + 1] = sample

	if #samples > MAX_SAMPLES then
		table.remove(samples, 1)
	end
end

---@param pEntity Entity
---@return number
local function GetSmoothedAngularVelocity(pEntity)
	local samples = position_samples[pEntity:GetIndex()]
	if not samples or #samples < 3 then
		return 0
	end

	local ang_vels = {}
	local two_pi = 2 * math_pi

	for i = 1, #samples - 2 do
		local s1, s2, s3 = samples[i], samples[i + 1], samples[i + 2]
		local dt1, dt2 = s2.time - s1.time, s3.time - s2.time

		if dt1 > 0 and dt2 > 0 then
			local vel1 = (s2.pos - s1.pos) / dt1
			local vel2 = (s3.pos - s2.pos) / dt2

			if vel1:Length() >= MIN_SPEED and vel2:Length() >= MIN_SPEED then
				local yaw1 = math_atan(vel1.y, vel1.x)
				local yaw2 = math_atan(vel2.y, vel2.x)
				local diff = math_deg((yaw2 - yaw1 + math_pi) % two_pi - math_pi)
				local angular_velocity = diff / ((dt1 + dt2) * 0.5)

				if math_abs(angular_velocity) < MAX_ANGULAR_VEL then
					ang_vels[#ang_vels + 1] = angular_velocity
				end
			end
		end
	end

	if #ang_vels == 0 then
		return 0
	end

	if #ang_vels >= 3 then
		table.sort(ang_vels)
		return ang_vels[math_floor(#ang_vels * 0.5) + 1]
	end

	local smoothed = ang_vels[1]
	for i = 2, #ang_vels do
		smoothed = smoothed * 0.7 + ang_vels[i] * 0.3
	end
	return smoothed
end

function sim.RunBackground(entitylist)
	for _, player in pairs(entitylist) do
		AddPositionSample(player)
	end
end

local function TryPlayerMove(origin, velocity, frametime, mins, maxs, shouldHitEntity, surface_friction)
	local time_left = frametime
	local numplanes = 0

	for bumpcount = 0, 3 do
		if velocity:LengthSqr() < 0.0001 then
			break
		end

		temp_vec1.x = origin.x + velocity.x * time_left
		temp_vec1.y = origin.y + velocity.y * time_left
		temp_vec1.z = origin.z + velocity.z * time_left

		local trace = DoTraceHull(origin, temp_vec1, mins, maxs, MASK_PLAYERSOLID, shouldHitEntity)

		if trace.allsolid then
			velocity.x, velocity.y, velocity.z = 0, 0, 0
			return
		end

		if trace.fraction > 0 then
			origin.x, origin.y, origin.z = trace.endpos.x, trace.endpos.y, trace.endpos.z
		end

		if trace.fraction >= 0.99 then
			break
		end

		time_left = time_left * (1 - trace.fraction)

		if numplanes >= MAX_CLIP_PLANES then
			velocity.x, velocity.y, velocity.z = 0, 0, 0
			return
		end

		-- Store plane normal
		local plane = clip_planes[numplanes + 1]
		plane.x, plane.y, plane.z = trace.plane.x, trace.plane.y, trace.plane.z
		numplanes = numplanes + 1

		-- Just clip against the new plane
		local overbounce = (trace.plane.z > 0.7) and 1.0 or (1.0 + (1.0 - surface_friction) * 0.5)
		ClipVelocity(velocity, plane, overbounce)

		-- Check velocity against all planes
		local valid = true
		for i = 1, numplanes do
			local dot = velocity.x * clip_planes[i].x + velocity.y * clip_planes[i].y + velocity.z * clip_planes[i].z
			if dot < 0 then
				valid = false
				break
			end
		end

		if not valid and numplanes >= 2 then
			temp_vec2.x = clip_planes[1].y * clip_planes[2].z - clip_planes[1].z * clip_planes[2].y
			temp_vec2.y = clip_planes[1].z * clip_planes[2].x - clip_planes[1].x * clip_planes[2].z
			temp_vec2.z = clip_planes[1].x * clip_planes[2].y - clip_planes[1].y * clip_planes[2].x

			local len = temp_vec2:LengthSqr()
			if len > 0.001 then
				temp_vec2.x, temp_vec2.y, temp_vec2.z = temp_vec2.x / len, temp_vec2.y / len, temp_vec2.z / len
				local scalar = velocity.x * temp_vec2.x + velocity.y * temp_vec2.y + velocity.z * temp_vec2.z
				velocity.x, velocity.y, velocity.z = temp_vec2.x * scalar, temp_vec2.y * scalar, temp_vec2.z * scalar
			else
				velocity.x, velocity.y, velocity.z = 0, 0, 0
			end
		end
	end
end

local function StepMove(
	origin,
	velocity,
	frametime,
	mins,
	maxs,
	shouldHitEntity,
	surface_friction,
	step_size,
	is_on_ground
)
	local orig_x, orig_y, orig_z = origin.x, origin.y, origin.z
	local orig_vx, orig_vy, orig_vz = velocity.x, velocity.y, velocity.z

	-- Try regular move first
	TryPlayerMove(origin, velocity, frametime, mins, maxs, shouldHitEntity, surface_friction)

	local down_dist = (origin.x - orig_x) + (origin.y - orig_y)

	if not is_on_ground or down_dist > 5.0 or (orig_vx * orig_vx + orig_vy * orig_vy) < 1.0 then
		return
	end

	local down_x, down_y, down_z = origin.x, origin.y, origin.z
	local down_vx, down_vy, down_vz = velocity.x, velocity.y, velocity.z

	-- reset and try step up
	origin.x, origin.y, origin.z = orig_x, orig_y, orig_z
	velocity.x, velocity.y, velocity.z = orig_vx, orig_vy, orig_vz

	-- step up
	temp_vec1.x, temp_vec1.y, temp_vec1.z = origin.x, origin.y, origin.z + step_size + DIST_EPSILON
	local up_trace = DoTraceHull(origin, temp_vec1, mins, maxs, MASK_PLAYERSOLID, shouldHitEntity)

	if not up_trace.startsolid and not up_trace.allsolid then
		origin.x, origin.y, origin.z = up_trace.endpos.x, up_trace.endpos.y, up_trace.endpos.z
	end

	-- move forward
	local up_orig_x, up_orig_y = origin.x, origin.y
	TryPlayerMove(origin, velocity, frametime, mins, maxs, shouldHitEntity, surface_friction)

	local up_dist = (origin.x - up_orig_x) + (origin.y - up_orig_y)

	-- if stepping up didn't help, revert to original result
	if up_dist <= down_dist then
		origin.x, origin.y, origin.z = down_x, down_y, down_z
		velocity.x, velocity.y, velocity.z = down_vx, down_vy, down_vz
		return
	end

	-- step down to ground
	temp_vec1.x, temp_vec1.y = origin.x, origin.y
	temp_vec1.z = origin.z - step_size - DIST_EPSILON
	local down_trace = DoTraceHull(origin, temp_vec1, mins, maxs, MASK_PLAYERSOLID, shouldHitEntity)

	if down_trace.plane.z >= 0.7 and not down_trace.startsolid and not down_trace.allsolid then
		origin.x, origin.y, origin.z = down_trace.endpos.x, down_trace.endpos.y, down_trace.endpos.z
	end
end

local function ApplyFriction(velocity, pTarget, is_on_ground)
	local speed = velocity:Length()
	if speed < 0.1 then
		return
	end

	if is_on_ground then
		local _, sv_friction = client.GetConVar("sv_friction")
		local surfaceFriction = pTarget:GetPropFloat("m_flFriction") or SURFACE_FRICTION
		local _, sv_stopspeed = client.GetConVar("sv_stopspeed")

		local control = (speed < sv_stopspeed) and sv_stopspeed or speed
		local drop = control * sv_friction * surfaceFriction * globals.TickInterval()
		local newspeed = math_max(0, speed - drop)

		if newspeed ~= speed and speed > 0 then
			local scale = newspeed / speed
			velocity.x = velocity.x * scale
			velocity.y = velocity.y * scale
			velocity.z = velocity.z * scale
		end
	end
end

local function CategorizePosition(pos, vel, mins, maxs, shouldHitEntity)
	temp_vec1.x, temp_vec1.y, temp_vec1.z = pos.x, pos.y, pos.z
	temp_vec2.x, temp_vec2.y = pos.x, pos.y
	temp_vec2.z = pos.z - 66

	local is_on_ground = false
	local ground_normal = nil
	local surface_friction = 1.0

	-- check velocity in z - if shooting up fast, not on ground
	if vel.z <= 180.0 then
		local trace = DoTraceHull(temp_vec1, temp_vec2, mins, maxs, MASK_PLAYERSOLID, shouldHitEntity)

		-- check if we hit a walkable surface
		-- ground must have normal.z >= 0.7
		if trace.fraction < 0.06 and (trace.plane.z >= IMPACT_NORMAL_FLOOR) then
			is_on_ground = true
			ground_normal = Vector3(trace.plane.x, trace.plane.y, trace.plane.z)

			-- snap to ground if not in water and trace succeeded
			if not trace.startsolid and not trace.allsolid then
				-- move player down that small amount to stay on ground
				pos.x = temp_vec1.x + trace.fraction * (temp_vec2.x - temp_vec1.x)
				pos.y = temp_vec1.y + trace.fraction * (temp_vec2.y - temp_vec1.y)
				pos.z = temp_vec1.z + trace.fraction * (temp_vec2.z - temp_vec1.z)
			end
		end
	end

	return is_on_ground, ground_normal, surface_friction
end

local function TryPlayerMoveFast(origin, velocity, frametime, mins, maxs, shouldHitEntity)
	--- predict next position
	temp_vec1.x = origin.x + velocity.x * frametime
	temp_vec1.y = origin.y + velocity.y * frametime
	temp_vec1.z = origin.z + velocity.z * frametime

	--- simple ground check
	local trace = engine.TraceHull(origin, temp_vec1, mins, maxs, MASK_PLAYERSOLID, shouldHitEntity)
	if trace.allsolid then
		velocity.x, velocity.y, velocity.z = 0, 0, 0
		return
	end

	-- Move to end position or collision point
	origin.x, origin.y, origin.z = trace.endpos.x, trace.endpos.y, trace.endpos.z

	if trace.fraction < 1.0 then
		-- reflect velocity along surface normal
		local n = trace.plane
		local dot = velocity.x * n.x + velocity.y * n.y + velocity.z * n.z
		velocity.x = velocity.x - dot * n.x
		velocity.y = velocity.y - dot * n.y
		velocity.z = velocity.z - dot * n.z
	end
end

local function StepMoveFast(origin, velocity, frametime, mins, maxs, shouldHitEntity, step_size)
	-- save original state
	local ox, oy, oz = origin.x, origin.y, origin.z

	-- try flat move first
	TryPlayerMoveFast(origin, velocity, frametime, mins, maxs, shouldHitEntity)
	local flat_dx = (origin.x - ox) ^ 2 + (origin.y - oy) ^ 2

	-- step up attempt (skip if not moving enough horizontally)
	if flat_dx < 1.0 then
		return
	end

	-- move upward
	temp_vec1.x, temp_vec1.y, temp_vec1.z = ox, oy, oz + step_size
	local up_trace = engine.TraceHull(Vector3(ox, oy, oz), temp_vec1, mins, maxs, MASK_PLAYERSOLID, shouldHitEntity)
	if up_trace.allsolid then
		return
	end

	-- apply step-up position
	origin.x, origin.y, origin.z = up_trace.endpos.x, up_trace.endpos.y, up_trace.endpos.z

	-- move again from higher position
	TryPlayerMoveFast(origin, velocity, frametime, mins, maxs, shouldHitEntity)

	-- step down gently
	temp_vec1.x, temp_vec1.y, temp_vec1.z = origin.x, origin.y, origin.z - step_size
	local down_trace = engine.TraceHull(origin, temp_vec1, mins, maxs, MASK_PLAYERSOLID, shouldHitEntity)
	if not down_trace.allsolid then
		origin.x, origin.y, origin.z = down_trace.endpos.x, down_trace.endpos.y, down_trace.endpos.z
	end
end

---@param pInfo EntityInfo
---@param pTarget Entity
---@param initial_pos Vector3
---@param time number
---@return Vector3[]
function sim.Run(pInfo, pTarget, initial_pos, time)
	local tick_interval = globals.TickInterval()
	local local_player_index = client.GetLocalPlayerIndex()

	local surface_friction = pInfo.friction or 1.0
	local angular_velocity = pInfo.angvelocity * tick_interval
	local maxspeed = pInfo.maxspeed or 450
	local step_size = pInfo.stepsize or 18
	local mins = pInfo.mins
	local maxs = pInfo.maxs
	local _, sv_gravity = client.GetConVar("sv_gravity")
	local gravity_step = sv_gravity * tick_interval

	local pos = Vector3(initial_pos.x, initial_pos.y, initial_pos.z)
	local vel = Vector3(pInfo.velocity.x, pInfo.velocity.y, pInfo.velocity.z)

	local positions = {}

	local cos_yaw, sin_yaw
	if angular_velocity ~= 0 then
		local yaw = math_rad(angular_velocity)
		cos_yaw, sin_yaw = math_cos(yaw), math_sin(yaw)
	end

	local function shouldHitEntity(ent)
		local idx = ent:GetIndex()
		return idx ~= local_player_index and ent:GetTeamNumber() ~= pInfo.team
	end

	temp_vec3.x, temp_vec3.y, temp_vec3.z = 0, 0, -step_size
	local fast_mode = settings.sim.fast_mode

	for i = 1, time do
		local is_on_ground = CategorizePosition(pos, vel, mins, maxs, shouldHitEntity)

		-- Friction
		-- i wanted to use this, but my implementation does not like acceleration
		--ApplyFriction(vel, pTarget, is_on_ground)

		-- Apply rotation
		if angular_velocity ~= 0 then
			local vx, vy = vel.x, vel.y
			vel.x = vx * cos_yaw - vy * sin_yaw
			vel.y = vx * sin_yaw + vy * cos_yaw
		end

		temp_vec1.x, temp_vec1.y = pos.x + vel.x * tick_interval, pos.y + vel.y * tick_interval
		temp_vec1.z = pos.z + vel.z * tick_interval
		temp_vec2.x, temp_vec2.y, temp_vec2.z = temp_vec1.x, temp_vec1.y, temp_vec1.z - step_size

		if not is_on_ground then
			vel.z = vel.z - (gravity_step * 0.5)
		end

		-- Acceleration
		local horizontal_speed = vel:Length2DSqr()
		if horizontal_speed > 0.0001 then
			local inv_len = 1.0 / horizontal_speed
			-- This is the direction they're currently moving
			local wish_x = vel.x * inv_len
			local wish_y = vel.y * inv_len

			-- Apply angular rotation to predict their turning
			if angular_velocity ~= 0 then
				local vx, vy = wish_x, wish_y
				wish_x = vx * cos_yaw - vy * sin_yaw
				wish_y = vx * sin_yaw + vy * cos_yaw
			end

			temp_vec1.x, temp_vec1.y, temp_vec1.z = wish_x, wish_y, 0
			local wishspeed = math_min(horizontal_speed, maxspeed)

			if is_on_ground then
				AccelerateInPlace(vel, temp_vec1, wishspeed, GROUND_ACCELERATE, tick_interval, surface_friction)
			else
				AirAccelerateInPlace(
					vel,
					temp_vec1,
					wishspeed,
					AIR_ACCELERATE,
					tick_interval,
					surface_friction,
					pTarget
				)
			end

			vel.x = math_max(-3500, math_min(3500, vel.x)) -- clamp to reasonable bounds
			vel.y = math_max(-3500, math_min(3500, vel.y)) -- clamp to reasonable bounds
			vel.z = math_max(-3500, math_min(3500, vel.z)) -- clamp to reasonable bounds
		end

		-- Speed cap on ground
		if is_on_ground then
			local vel_length = vel:Length()
			if vel_length > maxspeed then
				local scale = maxspeed / vel_length
				vel.x, vel.y = vel.x * scale, vel.y * scale
			end
		end

		-- Move
		-- Branching here is probably not a good idea
		-- Make it select a function instead outside the loop !!!
		-- Im lazy, i'll do it later when i remember this comment lol
		if fast_mode then
			StepMoveFast(pos, vel, tick_interval, mins, maxs, shouldHitEntity, step_size)
		else
			StepMove(pos, vel, tick_interval, mins, maxs, shouldHitEntity, surface_friction, step_size, is_on_ground)
		end
		--StepMove(pos, vel, tick_interval, mins, maxs, shouldHitEntity, surface_friction, step_size, is_on_ground)
		--StepMoveFast(pos, vel, tick_interval, mins, maxs, shouldHitEntity, step_size)

		-- Gravity
		if not is_on_ground then
			vel.z = vel.z - gravity_step * 0.5
		elseif vel.z < 0 then
			vel.z = 0
		end

		positions[#positions + 1] = Vector3(pos.x, pos.y, pos.z)
	end

	return positions
end

sim.GetSmoothedAngularVelocity = GetSmoothedAngularVelocity
return sim

end)
__bundle_register("src.utils.entity", function(require, _LOADED, __bundle_register, __bundle_modules)
local ent_utils = {}

---@param plocal Entity
function ent_utils.GetShootPosition(plocal)
    return plocal:GetAbsOrigin() + plocal:GetPropVector("localdata", "m_vecViewOffset[0]")
end

---@param entity Entity
---@return table<integer, Vector3>
function ent_utils.GetBones(entity)
    local model = entity:GetModel()
    local studioHdr = models.GetStudioModel(model)

    local myHitBoxSet = entity:GetPropInt("m_nHitboxSet")
    local hitboxSet = studioHdr:GetHitboxSet(myHitBoxSet)
    local hitboxes = hitboxSet:GetHitboxes()

    --boneMatrices is an array of 3x4 float matrices
    local boneMatrices = entity:SetupBones()

    local bones = {}

    for i = 1, #hitboxes do
        local hitbox = hitboxes[i]
        local bone = hitbox:GetBone()

        local boneMatrix = boneMatrices[bone]
        if boneMatrix ~= nil then
            local bonePos = Vector3(boneMatrix[1][4], boneMatrix[2][4], boneMatrix[3][4])
            bones[i] = bonePos
        end
    end

    return bones
end

---@param player Entity
---@param shootpos Vector3
---@param viewangle EulerAngles
---@param PREFERRED_BONES table
function ent_utils.FindVisibleBodyPart(player, shootpos, utils, viewangle, PREFERRED_BONES)
    local bones = ent_utils.GetBones(player)
    local info = {}
    info.fov = math.huge
    info.angle = nil
    info.index = nil
    info.pos = nil

    for _, preferred_bone in ipairs(PREFERRED_BONES) do
        local bonePos = bones[preferred_bone]
        local trace = engine.TraceLine(shootpos, bonePos, MASK_SHOT_HULL)

        if trace and trace.fraction >= 0.6 then
            local angle = utils.PositionAngles(shootpos, bonePos)
            local fov = utils.AngleFov(angle, viewangle)

            if fov < info.fov then
                info.fov, info.angle, info.index = fov, angle, player:GetIndex()
                info.pos = bonePos
                break --- found a suitable bone, no need to check the other ones
            end
        end
    end

    return info
end

return ent_utils

end)
__bundle_register("src.utils.math", function(require, _LOADED, __bundle_register, __bundle_modules)
local Math = {}

--- Pasted from Lnx00's LnxLib
local function isNaN(x)
    return x ~= x
end

local M_RADPI = 180 / math.pi --- rad to deg

-- Calculates the angle between two vectors
---@param source Vector3
---@param dest Vector3
---@return EulerAngles angles
function Math.PositionAngles(source, dest)
    local delta = source - dest

    local pitch = math.atan(delta.z / delta:Length2D()) * M_RADPI
    local yaw = math.atan(delta.y / delta.x) * M_RADPI

    if delta.x >= 0 then
        yaw = yaw + 180
    end

    if isNaN(pitch) then
        pitch = 0
    end
    if isNaN(yaw) then
        yaw = 0
    end

    return EulerAngles(pitch, yaw, 0)
end

-- Calculates the FOV between two angles
---@param vFrom EulerAngles
---@param vTo EulerAngles
---@return number fov
function Math.AngleFov(vFrom, vTo)
    local vSrc = vFrom:Forward()
    local vDst = vTo:Forward()

    local fov = M_RADPI * math.acos(vDst:Dot(vSrc) / vDst:LengthSqr())
    if isNaN(fov) then
        fov = 0
    end

    return fov
end

local function NormalizeVector(vec)
    return vec / vec:Length()
end

---@param p0 Vector3 -- start position
---@param p1 Vector3 -- target position
---@param speed number -- projectile speed
---@param gravity number -- gravity constant
---@return EulerAngles?, number? -- Euler angles (pitch, yaw, 0)
function Math.SolveBallisticArc(p0, p1, speed, gravity)
    local diff = p1 - p0
    local dx = diff:Length2D()
    local dy = diff.z
    local speed2 = speed * speed
    local g = gravity

    local root = speed2 * speed2 - g * (g * dx * dx + 2 * dy * speed2)
    if root < 0 then
        return nil -- no solution
    end

    local sqrt_root = math.sqrt(root)
    local angle = math.atan((speed2 - sqrt_root) / (g * dx)) -- low arc

    -- Get horizontal direction (yaw)
    local yaw = (math.atan(diff.y, diff.x)) * M_RADPI

    -- Convert pitch from angle
    local pitch = -angle * M_RADPI -- negative because upward is negative pitch in most engines

    --- seconds
    local time = dx / (math.cos(pitch) * speed)

    return EulerAngles(pitch, yaw, 0), time
end

-- Returns both low and high arc EulerAngles when gravity > 0
---@param p0 Vector3
---@param p1 Vector3
---@param speed number
---@param gravity number
---@return EulerAngles|nil lowArc, EulerAngles|nil highArc
function Math.SolveBallisticArcBoth(p0, p1, speed, gravity)
    local diff = p1 - p0
    local dx = math.sqrt(diff.x * diff.x + diff.y * diff.y)
    if dx == 0 then
        return nil, nil
    end

    local dy = diff.z
    local g = gravity
    local speed2 = speed * speed

    local root = speed2 * speed2 - g * (g * dx * dx + 2 * dy * speed2)
    if root < 0 then
        return nil, nil
    end

    local sqrt_root = math.sqrt(root)
    local theta_low = math.atan((speed2 - sqrt_root) / (g * dx))
    local theta_high = math.atan((speed2 + sqrt_root) / (g * dx))

    local yaw = math.atan(diff.y, diff.x) * M_RADPI

    local pitch_low = -theta_low * M_RADPI
    local pitch_high = -theta_high * M_RADPI

    local low = EulerAngles(pitch_low, yaw, 0)
    local high = EulerAngles(pitch_high, yaw, 0)
    return low, high
end

---@param shootPos Vector3
---@param targetPos Vector3
---@param speed number
---@return number
function Math.EstimateTravelTime(shootPos, targetPos, speed)
    local distance = (targetPos - shootPos):Length2D()
    return distance / speed
end

---@param val number
---@param min number
---@param max number
function Math.clamp(val, min, max)
    return math.max(min, math.min(val, max))
end

function Math.GetBallisticFlightTime(p0, p1, speed, gravity)
    local diff = p1 - p0
    local dx = math.sqrt(diff.x ^ 2 + diff.y ^ 2)
    local dy = diff.z
    local speed2 = speed * speed
    local g = gravity

    local discriminant = speed2 * speed2 - g * (g * dx * dx + 2 * dy * speed2)
    if discriminant < 0 then
        return nil
    end

    local sqrt_discriminant = math.sqrt(discriminant)
    local angle = math.atan((speed2 - sqrt_discriminant) / (g * dx))

    -- Flight time calculation
    local vz = speed * math.sin(angle)
    local flight_time = (vz + math.sqrt(vz * vz + 2 * g * dy)) / g

    return flight_time
end

function Math.DirectionToAngles(direction)
    local pitch = math.asin(-direction.z) * M_RADPI
    local yaw = math.atan(direction.y, direction.x) * M_RADPI
    return Vector3(pitch, yaw, 0)
end

---@param offset Vector3
---@param direction Vector3
function Math.RotateOffsetAlongDirection(offset, direction)
    local forward = NormalizeVector(direction)
    local up = Vector3(0, 0, 1)
    local right = NormalizeVector(forward:Cross(up))
    up = NormalizeVector(right:Cross(forward))

    return forward * offset.x + right * offset.y + up * offset.z
end

Math.NormalizeVector = NormalizeVector
return Math

end)
__bundle_register("src.utils.weapon_utils", function(require, _LOADED, __bundle_register, __bundle_modules)
local wep_utils = {}

---@type table<integer, integer>
local ItemDefinitions = {}

local old_weapon, lastFire, nextAttack = nil, 0, 0

local function GetLastFireTime(weapon)
    return weapon:GetPropFloat("LocalActiveTFWeaponData", "m_flLastFireTime")
end

local function GetNextPrimaryAttack(weapon)
    return weapon:GetPropFloat("LocalActiveWeaponData", "m_flNextPrimaryAttack")
end

--- https://www.unknowncheats.me/forum/team-fortress-2-a/273821-canshoot-function.html
function wep_utils.CanShoot()
    local player = entities:GetLocalPlayer()
    if not player then
        return false
    end

    local weapon = player:GetPropEntity("m_hActiveWeapon")
    if not weapon or not weapon:IsValid() then
        return false
    end

    if weapon:GetPropInt("LocalWeaponData", "m_iClip1") == 0 then
        return false
    end

    local lastfiretime = GetLastFireTime(weapon)
    if lastFire ~= lastfiretime or weapon ~= old_weapon then
        lastFire = lastfiretime
        nextAttack = GetNextPrimaryAttack(weapon)
    end

    old_weapon = weapon
    return nextAttack <= globals.CurTime()
end

do
    local defs = {
        [222] = 11,
        [812] = 12,
        [833] = 12,
        [1121] = 11,
        [18] = -1,
        [205] = -1,
        [127] = -1,
        [228] = -1,
        [237] = -1,
        [414] = -1,
        [441] = -1,
        [513] = -1,
        [658] = -1,
        [730] = -1,
        [800] = -1,
        [809] = -1,
        [889] = -1,
        [898] = -1,
        [907] = -1,
        [916] = -1,
        [965] = -1,
        [974] = -1,
        [1085] = -1,
        [1104] = -1,
        [15006] = -1,
        [15014] = -1,
        [15028] = -1,
        [15043] = -1,
        [15052] = -1,
        [15057] = -1,
        [15081] = -1,
        [15104] = -1,
        [15105] = -1,
        [15129] = -1,
        [15130] = -1,
        [15150] = -1,
        [442] = -1,
        [1178] = -1,
        [39] = 8,
        [351] = 8,
        [595] = 8,
        [740] = 8,
        [1180] = 0,
        [19] = 5,
        [206] = 5,
        [308] = 5,
        [996] = 6,
        [1007] = 5,
        [1151] = 4,
        [15077] = 5,
        [15079] = 5,
        [15091] = 5,
        [15092] = 5,
        [15116] = 5,
        [15117] = 5,
        [15142] = 5,
        [15158] = 5,
        [20] = 1,
        [207] = 1,
        [130] = 3,
        [265] = 3,
        [661] = 1,
        [797] = 1,
        [806] = 1,
        [886] = 1,
        [895] = 1,
        [904] = 1,
        [913] = 1,
        [962] = 1,
        [971] = 1,
        [1150] = 2,
        [15009] = 1,
        [15012] = 1,
        [15024] = 1,
        [15038] = 1,
        [15045] = 1,
        [15048] = 1,
        [15082] = 1,
        [15083] = 1,
        [15084] = 1,
        [15113] = 1,
        [15137] = 1,
        [15138] = 1,
        [15155] = 1,
        [588] = -1,
        [997] = 9,
        [17] = 10,
        [204] = 10,
        [36] = 10,
        [305] = 9,
        [412] = 10,
        [1079] = 9,
        [56] = 7,
        [1005] = 7,
        [1092] = 7,
        [58] = 11,
        [1083] = 11,
        [1105] = 11,
        [42] = 13,
    }
    local maxIndex = 0
    for k, _ in pairs(defs) do
        if k > maxIndex then
            maxIndex = k
        end
    end
    for i = 1, maxIndex do
        ItemDefinitions[i] = defs[i] or false
    end
end

---@param val number
---@param min number
---@param max number
local function clamp(val, min, max)
    return math.max(min, math.min(val, max))
end

function wep_utils.GetWeaponDefinition(pWeapon)
    local definition_index = pWeapon:GetPropInt("m_iItemDefinitionIndex")
    return ItemDefinitions[definition_index], definition_index
end

-- Returns (offset, forward velocity, upward velocity, collision hull, gravity, drag)
function wep_utils.GetProjectileInformation(pWeapon, bDucking, iCase, iDefIndex, iWepID)
    local chargeTime = pWeapon:GetPropFloat("m_flChargeBeginTime") or 0
    if chargeTime ~= 0 then
        chargeTime = globals.CurTime() - chargeTime
    end

    -- Predefined offsets and collision sizes:
    local offsets = {
        Vector3(16, 8, -6), -- Index 1: Sticky Bomb, Iron Bomber, etc.
        Vector3(23.5, -8, -3), -- Index 2: Huntsman, Crossbow, etc.
        Vector3(23.5, 12, -3), -- Index 3: Flare Gun, Guillotine, etc.
        Vector3(16, 6, -8), -- Index 4: Syringe Gun, etc.
    }
    local collisionMaxs = {
        Vector3(0, 0, 0), -- For projectiles that use TRACE_LINE (e.g. rockets)
        Vector3(1, 1, 1),
        Vector3(2, 2, 2),
        Vector3(3, 3, 3),
    }

    if iCase == -1 then
        -- Rocket Launcher types: force a zero collision hull so that TRACE_LINE is used.
        local vOffset = Vector3(23.5, -8, bDucking and 8 or -3)
        local vCollisionMax = collisionMaxs[1] -- Zero hitbox
        local fForwardVelocity = 1200
        if iWepID == 22 or iWepID == 65 then
            vOffset.y = (iDefIndex == 513) and 0 or 12
            fForwardVelocity = (iWepID == 65) and 2000 or ((iDefIndex == 414) and 1550 or 1100)
        elseif iWepID == 109 then
            vOffset.y, vOffset.z = 6, -3
        else
            fForwardVelocity = 1200
        end
        return vOffset, fForwardVelocity, 0, vCollisionMax, 0, nil
    elseif iCase == 1 then
        return offsets[1], 900 + clamp(chargeTime / 4, 0, 1) * 1500, 200, collisionMaxs[3], 0, nil
    elseif iCase == 2 then
        return offsets[1], 900 + clamp(chargeTime / 1.2, 0, 1) * 1500, 200, collisionMaxs[3], 0, nil
    elseif iCase == 3 then
        return offsets[1], 900 + clamp(chargeTime / 4, 0, 1) * 1500, 200, collisionMaxs[3], 0, nil
    elseif iCase == 4 then
        return offsets[1], 1200, 200, collisionMaxs[4], 400, 0.45
    elseif iCase == 5 then
        local vel = (iDefIndex == 308) and 1500 or 1200
        local drag = (iDefIndex == 308) and 0.225 or 0.45
        return offsets[1], vel, 200, collisionMaxs[4], 400, drag
    elseif iCase == 6 then
        return offsets[1], 1440, 200, collisionMaxs[3], 560, 0.5
    elseif iCase == 7 then
        return offsets[2],
            1800 + clamp(chargeTime, 0, 1) * 800,
            0,
            collisionMaxs[2],
            200 - clamp(chargeTime, 0, 1) * 160,
            nil
    elseif iCase == 8 then
        -- Flare Gun: Use a small nonzero collision hull and a higher drag value to make drag noticeable.
        return Vector3(23.5, 12, bDucking and 8 or -3), 2000, 0, Vector3(0.1, 0.1, 0.1), 120, 0.5
    elseif iCase == 9 then
        local idx = (iDefIndex == 997) and 2 or 4
        return offsets[2], 2400, 0, collisionMaxs[idx], 80, nil
    elseif iCase == 10 then
        return offsets[4], 1000, 0, collisionMaxs[2], 120, nil
    elseif iCase == 11 then
        return Vector3(23.5, 8, -3), 1000, 200, collisionMaxs[4], 450, nil
    elseif iCase == 12 then
        return Vector3(23.5, 8, -3), 3000, 300, collisionMaxs[3], 900, 1.3
    elseif iCase == 13 then
        return Vector3(), 350, 0, collisionMaxs[4], 0.25, 0.1
    end
end

---@return WeaponInfo
function wep_utils.GetWeaponInfo(pWeapon, bDucking, iCase, iDefIndex, iWepID)
    local vOffset, fForwardVelocity, fUpwardVelocity, vCollisionMax, fGravity, fDrag =
        wep_utils.GetProjectileInformation(pWeapon, bDucking, iCase, iDefIndex, iWepID)

    return {
        vecOffset = vOffset,
        flForwardVelocity = fForwardVelocity,
        flUpwardVelocity = fUpwardVelocity,
        vecCollisionMax = vCollisionMax,
        flGravity = fGravity,
        flDrag = fDrag,
    }
end

---@param pLocal Entity
---@param weapon_info WeaponInfo
---@param eAngle EulerAngles
---@return Vector3
function wep_utils.GetShootPos(pLocal, weapon_info, eAngle)
    -- i stole this from terminator
    local vStartPosition = pLocal:GetAbsOrigin() + pLocal:GetPropVector("localdata", "m_vecViewOffset[0]")
    return weapon_info:GetFirePosition(pLocal, vStartPosition, eAngle, client.GetConVar("cl_flipviewmodels") == 1) --vStartPosition + vOffset, vOffset
end

return wep_utils

end)
return __bundle_require("__root")