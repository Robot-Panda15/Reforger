//------------------------------------------------------------------------------------------------
//! When the HMD helmet list is non-empty: crew without a matching helmet / capability tag only see markers & designations while in vehicle camera (focus), not while looking around the vehicle.
class HMD_HmdVehicleHudRestriction
{
	protected static const float FOCUS_MODE_IN_CAMERA_MIN = 0.25;

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

	//------------------------------------------------------------------------------------------------
	//! Wearing HMD helmet: HMD_HelmetCapabilityComponent on an attachment, or prefab path matches HUDMarkerSystem list when list is non-empty.
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
	//! True when helmet prefab list is non-empty and local player does not qualify as HMD (tag or list match).
	static bool VehicleHudShouldRestrictToCameraOnly(ChimeraWorld world)
	{
		HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
		if (!sys || !sys.HasHmdHelmetPrefabConfig())
			return false;
		return !LocalPlayerHasHmdHelmetCapability(world);
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicle weapon / optic camera (focus) vs looking around the vehicle interior / exterior orbit.
	static bool IsLocalPlayerInVehicleCameraView()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		PlayerCamera cam = pc.GetPlayerCamera();
		if (!cam)
			return false;
		return cam.GetFocusMode() > FOCUS_MODE_IN_CAMERA_MIN;
	}
}
