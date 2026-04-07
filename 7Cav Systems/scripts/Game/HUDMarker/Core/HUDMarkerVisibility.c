//------------------------------------------------------------------------------------------------
//! Eligibility: HMD_HudMarkerEligibility (player, vehicle + HMD_HudMarkerEligibilityVehicleComponent, helmet camera, seat toggles).
//! View distance / IFF & laser checkboxes / HUD refresh: HMD_HudMarkerPolicyResolver + HMD_HudMarkerEligibility* components.
//! Content toggles (see ShouldIncludeLocal/ForeignLaserDesignationDotsInHUD vs ShouldIncludeIffMarkersInHUD):
//! - Foreign laser dots: Numpad * (s_bVehicleLaserVisibility), or dismounted handheld optics zoom (not vehicle binos); hull policy PolicyAllowsForeignLaserDesignationMarkers.
//! - Own laser dots (HUD pool / local marking, lock readout, etc.): hull HMD_HudMarkerEligibilityVehicleComponent own-laser checkbox; independent of Numpad *.
//! - IFF markers: Numpad 9 (s_bShowIffMarkers) only.
//! ShouldRenderWorldMarkers(): vehicle handheld binoculars / designator zoom disable all world-marker dots (IFF + laser); dismounted optics zoom may still show them.
//! Rangefinder/code readout: HUDLaserMarkingComponent path (see ShouldShowLaserDesignatorReadout).
class HUDMarkerVisibility
{
	protected static bool s_bVehicleLaserVisibility;
	protected static bool s_bVehicleLaserMarkingMode;
	//! Numpad 9: IFF pool on/off (independent of vehicle laser visibility/marking).
	protected static bool s_bShowIffMarkers = true;

	//------------------------------------------------------------------------------------------------
	//! Local player is in a vehicle and looking through handheld optics (binoculars / Target Designator): vehicle laser HUD/toggles are ignored; optics drive marking/readout.
	static bool IsVehicleBinocularViewActive()
	{
		return HMD_HudMarkerEligibility.IsVehicleBinocularViewActive();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsVisible()
	{
		return s_bVehicleLaserVisibility;
	}

	//------------------------------------------------------------------------------------------------
	//! Pooled designations (own turret, placed spots, rangefinder lock dot, etc.). Not gated by Numpad * for others' lasers.
	static bool ShouldIncludeLocalLaserDesignationDotsInHUD()
	{
		if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
			return false;
		if (!HMD_HudMarkerEligibility.PassesHudMarkerWorldOverlayEligibility())
			return false;
		if (!HMD_HudMarkerPolicyResolver.PolicyAllowsOwnLaserDesignationMarkers())
			return false;
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
			return true;
		if (s_bVehicleLaserVisibility)
			return true;
		if (s_bVehicleLaserMarkingMode)
			return true;
		if (HMD_RangefinderHUDState.IsLockTargetReadout())
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Other players' laser designations (WCS). Numpad * (s_bVehicleLaserVisibility) when in vehicle HUD context.
	static bool ShouldIncludeForeignLaserDesignationDotsInHUD()
	{
		if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
			return false;
		if (!HMD_HudMarkerEligibility.PassesHudMarkerWorldOverlayEligibility())
			return false;
		if (!HMD_HudMarkerPolicyResolver.PolicyAllowsForeignLaserDesignationMarkers())
			return false;
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
			return true;
		return s_bVehicleLaserVisibility;
	}

	//------------------------------------------------------------------------------------------------
	static bool ShouldIncludeIffMarkersInHUD()
	{
		if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
			return false;
		if (!HMD_HudMarkerEligibility.PassesHudMarkerWorldOverlayEligibility())
			return false;
		if (!HMD_HudMarkerPolicyResolver.PolicyAllowsIffMarkers())
			return false;
		return s_bShowIffMarkers;
	}

	//------------------------------------------------------------------------------------------------
	static void ToggleIffMarkers()
	{
		s_bShowIffMarkers = !s_bShowIffMarkers;
	}

	//------------------------------------------------------------------------------------------------
	static void SetShowIffMarkers(bool show)
	{
		s_bShowIffMarkers = show;
	}

	//------------------------------------------------------------------------------------------------
	//! World marker dots: vehicle toggles, or dismounted optics zoom. Vehicle handheld binoculars: no overlay.
	static bool ShouldRenderWorldMarkers()
	{
		if (!HMD_HudMarkerEligibility.IsLocalPlayerConsciousForHud())
			return false;
		PlayerController pc = GetGame().GetPlayerController();
		IEntity controlled = null;
		if (pc)
			controlled = pc.GetControlledEntity();
		if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
			return false;
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
			return true;
		if (!HMD_HudMarkerEligibility.PassesHudMarkerWorldOverlayEligibility())
			return false;
		if (s_bVehicleLaserVisibility)
			return true;
		SCR_ChimeraCharacter chV = SCR_ChimeraCharacter.Cast(controlled);
		if (chV && chV.IsInVehicle() && s_bShowIffMarkers)
			return true;
		if (s_bVehicleLaserMarkingMode)
			return true;
		if (HMD_RangefinderHUDState.IsLockTargetReadout())
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Rangefinder / laser code readout: in vehicle, marking mode, lock readout, or vehicle binocular zoom.
	//! Dismounted: only while zoomed through handheld optics (not from laser code / lasing state alone).
	static bool ShouldShowLaserDesignatorReadout()
	{
		if (!HMD_HudMarkerEligibility.IsLocalPlayerConsciousForHud())
			return false;
		PlayerController pc = GetGame().GetPlayerController();
		IEntity controlled = pc.GetControlledEntity();
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);
		const bool inVehicle = ch && ch.IsInVehicle();
		if (inVehicle)
		{
			if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
				return true;
			if (!HMD_HudMarkerEligibility.PassesVehicleHudMarkerReadoutEligibility())
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
		return !HMD_HudMarkerEligibility.IsVehicleBinocularViewActive();
	}

	//------------------------------------------------------------------------------------------------
	//! @deprecated Use ShouldRenderWorldMarkers
	static bool ShouldRenderMarkers()
	{
		return ShouldRenderWorldMarkers();
	}

	//------------------------------------------------------------------------------------------------
	//! When true: vehicle HUD marker tooltips and * / / / 9 inputs are disabled (matches states where vehicle marker overlay is not usable).
	static bool ShouldVehicleHudMarkerControlsBeDisabled()
	{
		return HMD_HudMarkerEligibility.ShouldVehicleHudMarkerControlsBeDisabled();
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