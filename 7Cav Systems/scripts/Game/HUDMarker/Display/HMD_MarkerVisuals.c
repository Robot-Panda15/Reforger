//------------------------------------------------------------------------------------------------
//! HUD icons: IFF marker = green circle; own designation = red circle, no under-dot label; foreign designation = Lase square + code; lock = Lock_Element.
class HMD_MarkerVisuals
{
	static const string TEX_CIRCLE = "{73B3D8BBB785B5B9}UI/Textures/Common/circleFull.edds";
	static const string TEX_LOCK = "{9C736E10B379FBB3}Assets/Radar/Lock_Element.edds";
	static const string TEX_LASE = "{59E06B62680B6CB3}Assets/Radar/Lase_Element1.edds";

	//! IFF markers (HUDMarkerComponent): visibility-only world dots; green circle texture + component tint.
	static const int KIND_IFF_MARKER = 0;
	//! Local player designation (gadget / vehicle gunner aim): red circle texture + red tint.
	static const int KIND_OWN_DESIGNATION = 1;
	//! Other players' designations from ALL_DESIGNATORS: Lase_Element square on HUD (not the IFF circle dot).
	static const int KIND_FOREIGN_DESIGNATION = 2;

	static const float FOREIGN_DESIGNATION_VIS_M = 4000.0;
	static const float LOCK_MATCH_M = 2.5;
	//! HUD scale for foreign designation Lase square icon only (own designation uses pool base size; not IFF markers).
	static const float LASER_DESIGNATOR_DOT_SIZE_MULT = 3.0;

	//------------------------------------------------------------------------------------------------
	//! Replicated laser code for HUD label (handheld gadget or vehicle turret bridge); 0 if unknown.
	static int GetDesignatorHudDisplayCode(WCS_Armament_HandheldLaserDesignatorComponent d)
	{
		if (!d)
			return 0;
		HMD_LaserDesignatorGadgetComponent g = HMD_LaserDesignatorGadgetComponent.Cast(d);
		if (g)
			return g.GetLaserCodeForWeaponSystems();
		HUDLaserMarkingComponent hudMark = HUDLaserMarkingComponent.Cast(d);
		if (hudMark)
			return hudMark.GetLaserCodeForWeaponSystems();
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLocalDesignatorForHud(WCS_Armament_HandheldLaserDesignatorComponent designator, IEntity localChar)
	{
		if (!designator || !localChar)
			return false;
		IEntity owner = designator.GetOwner();
		if (!owner)
			return false;
		if (owner == localChar)
			return true;
		IEntity ownerRoot = owner.GetRootParent();
		IEntity localRoot = localChar.GetRootParent();
		if (ownerRoot && localRoot && ownerRoot == localRoot)
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! After pooled local designations: append other players' designations (same vehicle / character excluded).
	static void AppendForeignDesignations(
		array<vector> positions,
		array<string> names,
		array<int> markerColors,
		array<int> labelColors,
		array<float> visibilityDistances,
		array<int> markerVisualKinds)
	{
		if (!positions || !names || !markerVisualKinds)
			return;
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return;
		int n = WCS_Armament_HandheldLaserDesignatorComponent.ALL_DESIGNATORS.Count();
		int greenDot = Color.FromRGBA(0, 255, 0, 255).PackToInt();
		int whiteLbl = Color.FromRGBA(255, 255, 255, 255).PackToInt();
		int i;
		for (i = 0; i < n; i++)
		{
			WCS_Armament_HandheldLaserDesignatorComponent d = WCS_Armament_HandheldLaserDesignatorComponent.ALL_DESIGNATORS[i];
			if (!d || !d.IsDesignating() || !d.HasValidDesignation())
				continue;
			//! Vehicle marking: HUD dots and lock use HUDMarkerSystem pool; WCS row is for missiles only.
			if (HUDLaserMarkingComponent.Cast(d))
				continue;
			//! Static placed test / mission spots: HUD dot comes from HUDMarkerSystem only; skip duplicate WCS foreign LASE tag.
			if (HMD_PlacedDesignationComponent.Cast(d))
				continue;
			if (IsLocalDesignatorForHud(d, localChar))
				continue;
			vector pos = d.GetDesignatedLocation();
			positions.Insert(pos);
			int code = GetDesignatorHudDisplayCode(d);
			string hudLabel = "LASE";
			if (code > 0)
				hudLabel = string.Format("%1", code);
			names.Insert(hudLabel);
			if (markerColors)
				markerColors.Insert(greenDot);
			if (labelColors)
				labelColors.Insert(whiteLbl);
			if (visibilityDistances)
				visibilityDistances.Insert(FOREIGN_DESIGNATION_VIS_M);
			markerVisualKinds.Insert(KIND_FOREIGN_DESIGNATION);
		}
	}

	//------------------------------------------------------------------------------------------------
	static string ResolveMarkerDotTexture(int kind, bool lockedMatch)
	{
		if (lockedMatch)
			return TEX_LOCK;
		if (kind == KIND_FOREIGN_DESIGNATION)
			return TEX_LASE;
		return TEX_CIRCLE;
	}

	//------------------------------------------------------------------------------------------------
	static bool LockedPositionMatches(vector worldPos, vector lockPos, float matchM)
	{
		return vector.Distance(worldPos, lockPos) <= matchM;
	}
}
