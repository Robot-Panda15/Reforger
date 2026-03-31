//------------------------------------------------------------------------------------------------
//! Two independent content toggles (see ShouldIncludeLaserDesignationDotsInHUD vs ShouldIncludeIffMarkersInHUD):
//! - Laser designations: vehicle HUDLaserVisibility / marking, or handheld zoom (see ShouldIncludeLaserDesignationDotsInHUD).
//! - IFF markers: Numpad 9 (s_bShowIffMarkers) only; never turns laser dots on/off.
//! ShouldRenderWorldMarkers() is only "may we draw on the world-marker HUD layer" — it must allow IFF in-vehicle without requiring laser HUD visibility.
//! Rangefinder/code readout: HUDLaserMarkingComponent path (see ShouldShowLaserDesignatorReadout).
class HUDMarkerVisibility
{
	protected static bool s_bVehicleLaserVisibility;
	protected static bool s_bVehicleLaserMarkingMode;
	//! Numpad 9: IFF pool on/off (independent of vehicle laser visibility/marking).
	protected static bool s_bShowIffMarkers = true;

	//------------------------------------------------------------------------------------------------
	//! False when no local controlled entity, or character is dead/incapacitated (shared gate for world dots + rangefinder readout).
	protected static bool IsLocalPlayerAliveForHud()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		IEntity controlled = pc.GetControlledEntity();
		if (!controlled)
			return false;
		SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(controlled.FindComponent(SCR_CharacterControllerComponent));
		if (ccc)
		{
			ECharacterLifeState lifeState = ccc.GetLifeState();
			if (lifeState == ECharacterLifeState.DEAD || lifeState == ECharacterLifeState.INCAPACITATED)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Local player is in a vehicle and looking through handheld optics (binoculars / Target Designator): vehicle laser HUD/toggles are ignored; optics drive marking/readout.
	static bool IsVehicleBinocularViewActive()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		IEntity controlled = pc.GetControlledEntity();
		if (!controlled)
			return false;
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);
		if (!ch || !ch.IsInVehicle())
			return false;
		return HMD_HandheldOpticZoom.IsZoomedForHMD();
	}

	//------------------------------------------------------------------------------------------------
	//! When helmet prefab list is set: crew without HMD helmet only see marker HUD while in vehicle weapon/optic camera, not while looking around. Handheld binocular/designator in vehicle is unchanged.
	protected static bool VehicleCameraOnlySuppressesMarkerHud()
	{
		if (IsVehicleBinocularViewActive())
			return false;
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!ch || !ch.IsInVehicle())
			return false;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return false;
		if (!HMD_HmdVehicleHudRestriction.VehicleHudShouldRestrictToCameraOnly(world))
			return false;
		return !HMD_HmdVehicleHudRestriction.IsLocalPlayerInVehicleCameraView();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsVisible()
	{
		return s_bVehicleLaserVisibility || s_bVehicleLaserMarkingMode;
	}

	//------------------------------------------------------------------------------------------------
	//! Pooled + foreign laser designation dots (not IFF entities). Independent of Numpad 9 / IFF toggle.
	static bool ShouldIncludeLaserDesignationDotsInHUD()
	{
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
			return true;
		if (VehicleCameraOnlySuppressesMarkerHud())
			return false;
		if (!IsVehicleBinocularViewActive())
		{
			if (s_bVehicleLaserVisibility)
				return true;
			if (s_bVehicleLaserMarkingMode)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool ShouldIncludeIffMarkersInHUD()
	{
		return s_bShowIffMarkers;
	}

	//------------------------------------------------------------------------------------------------
	static void ToggleIffMarkers()
	{
		s_bShowIffMarkers = !s_bShowIffMarkers;
	}

	//------------------------------------------------------------------------------------------------
	//! World marker dots: vehicle toggles, or binocular zoom (when zoomed in a vehicle, vehicle laser flags are ignored).
	static bool ShouldRenderWorldMarkers()
	{
		if (!IsLocalPlayerAliveForHud())
			return false;
		PlayerController pc = GetGame().GetPlayerController();
		IEntity controlled = pc.GetControlledEntity();
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
			return true;
		if (VehicleCameraOnlySuppressesMarkerHud())
			return false;
		//! World overlay layer: optics zoom, vehicle laser HUD, or (in vehicle) IFF toggle alone — laser vs IFF content still gated separately in GetMarkerData.
		if (!IsVehicleBinocularViewActive())
		{
			if (s_bVehicleLaserVisibility)
				return true;
			if (s_bVehicleLaserMarkingMode)
				return true;
			SCR_ChimeraCharacter chV = SCR_ChimeraCharacter.Cast(controlled);
			if (chV && chV.IsInVehicle() && s_bShowIffMarkers)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Rangefinder / laser code readout: in vehicle, marking mode, lock readout, or vehicle binocular zoom.
	//! Dismounted: only while zoomed through handheld optics (not from laser code / lasing state alone).
	static bool ShouldShowLaserDesignatorReadout()
	{
		if (!IsLocalPlayerAliveForHud())
			return false;
		PlayerController pc = GetGame().GetPlayerController();
		IEntity controlled = pc.GetControlledEntity();
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);
		const bool inVehicle = ch && ch.IsInVehicle();
		if (inVehicle)
		{
			if (IsVehicleBinocularViewActive())
				return true;
			if (VehicleCameraOnlySuppressesMarkerHud())
				return false;
			if (HMD_RangefinderHUDState.IsLockTargetReadout())
				return true;
			return s_bVehicleLaserMarkingMode;
		}
		//! Dismounted: only while looking through handheld optics (zoom/ADS). No readout from stored code/lasing alone.
		return HMD_HandheldOpticZoom.IsZoomedForHMD();
	}

	//------------------------------------------------------------------------------------------------
	//! White vehicle-turret readout layout: in vehicle with readout, but not handheld binocular zoom (turret/gunner camera HUD).
	static bool ShouldUseVehicleTurretLaserReadoutLayout()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		IEntity controlled = pc.GetControlledEntity();
		if (!controlled)
			return false;
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);
		if (!ch || !ch.IsInVehicle())
			return false;
		if (!ShouldShowLaserDesignatorReadout())
			return false;
		return !IsVehicleBinocularViewActive();
	}

	//------------------------------------------------------------------------------------------------
	//! @deprecated Use ShouldRenderWorldMarkers
	static bool ShouldRenderMarkers()
	{
		return ShouldRenderWorldMarkers();
	}

	//------------------------------------------------------------------------------------------------
	static void SetVehicleLaserVisibilityEnabled(bool enabled)
	{
		s_bVehicleLaserVisibility = enabled;
	}

	//------------------------------------------------------------------------------------------------
	static void SetVehicleLaserMarkingMode(bool enabled)
	{
		s_bVehicleLaserMarkingMode = enabled;
	}

	//------------------------------------------------------------------------------------------------
	static void ClearVehicleLaserModes()
	{
		s_bVehicleLaserVisibility = false;
		s_bVehicleLaserMarkingMode = false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsVehicleLaserMarkingMode()
	{
		return s_bVehicleLaserMarkingMode;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_bVehicleLaserVisibility = false;
		s_bVehicleLaserMarkingMode = false;
		s_bShowIffMarkers = true;
	}
}
