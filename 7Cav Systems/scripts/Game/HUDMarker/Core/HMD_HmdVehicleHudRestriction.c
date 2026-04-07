//------------------------------------------------------------------------------------------------
//! When the HMD helmet list is non-empty: crew without a matching helmet / capability tag only see markers & designations in weapon/optic scripted cameras, not while looking around the vehicle interior (including visual zoom on the same interior cam).
class HMD_HmdVehicleHudRestriction
{
	//------------------------------------------------------------------------------------------------
	protected static void CollectCharacterAttachmentEntities(IEntity root, int depth, int maxDepth, notnull array<IEntity> outEnts)
	{
		if (!root || depth > maxDepth)
			return;
		outEnts.Insert(root);
		IEntity child = root.GetChildren();
		while (child)
		{
			CollectCharacterAttachmentEntities(child, depth + 1, maxDepth, outEnts);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True if any entity under the character has HMD_HelmetCapabilityComponent (typically helmet attachment).
	static bool CharacterHasHelmetCapabilityTag(SCR_ChimeraCharacter ch)
	{
		if (!ch)
			return false;
		array<IEntity> ents = {};
		CollectCharacterAttachmentEntities(ch, 0, 12, ents);
		int i;
		for (i = 0; i < ents.Count(); i++)
		{
			IEntity e = ents[i];
			if (!e)
				continue;
			if (HMD_HelmetCapabilityComponent.Cast(e.FindComponent(HMD_HelmetCapabilityComponent)))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True if any attachment prefab ResourceName matches the configured list (exact compare).
	static bool CharacterHelmetMatchesPrefabList(SCR_ChimeraCharacter ch, notnull array<ResourceName> prefabPaths)
	{
		if (!ch || prefabPaths.IsEmpty())
			return false;
		array<IEntity> ents = {};
		CollectCharacterAttachmentEntities(ch, 0, 12, ents);
		int i;
		for (i = 0; i < ents.Count(); i++)
		{
			IEntity e = ents[i];
			if (!e)
				continue;
			EntityPrefabData pd = e.GetPrefabData();
			if (!pd)
				continue;
			ResourceName rn = pd.GetPrefabName();
			int p;
			for (p = 0; p < prefabPaths.Count(); p++)
			{
				if (rn == prefabPaths[p])
					return true;
			}
		}
		return false;
	}

	//! Wearing HMD helmet: HMD_HelmetCapabilityComponent on an attachment, or prefab path matches HUDMarkerSystem list when list is non-empty. No caching - safe to call every frame (helmet removal updates immediately).
	static bool LocalPlayerHasHmdHelmetCapability(ChimeraWorld world)
	{
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return false;
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!ch)
			return false;
		if (CharacterHasHelmetCapabilityTag(ch))
			return true;
		if (sys.HasHmdHelmetPrefabConfig() && sys.LocalPlayerHelmetMatchesConfiguredPrefabList(ch))
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True when HUDMarkerSystem applies helmet policy and local player does not qualify as HMD (live attachment + prefab check each call).
	static bool VehicleHudShouldRestrictToCameraOnly(ChimeraWorld world)
	{
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys)
			return false;
		if (!sys.HasHmdHelmetPrefabConfig() && !sys.EnforceHmdHelmetInVehicles())
			return false;
		return !LocalPlayerHasHmdHelmetCapability(world);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the active scripted camera is weapon / optic (turret or vehicle ADS). Do not use PlayerCamera focus mode - interior visual zoom pushes focus without switching camera.
	static bool IsLocalPlayerInVehicleCameraView()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!ch || !ch.IsInVehicle())
			return false;
		CameraHandlerComponent handler = CameraHandlerComponent.Cast(ch.FindComponent(SCR_CharacterCameraHandlerComponent));
		if (!handler)
			return false;
		ScriptedCameraItem cur = handler.GetCurrentCamera();
		if (!cur)
			return false;
		if (CharacterCamera1stPersonTurret.Cast(cur))
			return true;
		if (CharacterCamera1stPersonTurretTransition.Cast(cur))
			return true;
		if (CharacterCameraADSVehicle.Cast(cur))
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Gunner "in weapon optic" for turret policy: TurretController.IsWeaponADS() only (no scripted camera casts, no IsUsingWeaponSights). Seats without a resolvable turret controller are false here; use hull / other eligibility elsewhere.
	static bool IsLocalPlayerInGunnerWeaponOpticCamera()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!ch || !ch.IsInVehicle())
			return false;
		TurretControllerComponent tc;
		if (!HMD_VehicleLaserAimHelpers.TryResolveTurretControllerFromLocalGunner(ch, tc))
			return false;
		return tc.IsWeaponADS();
	}

	//------------------------------------------------------------------------------------------------
	//! Alias: gunner turret optic = TurretController IsWeaponADS when TC resolves.
	static bool IsLocalPlayerInTurretScriptedCamera()
	{
		return IsLocalPlayerInGunnerWeaponOpticCamera();
	}
}
