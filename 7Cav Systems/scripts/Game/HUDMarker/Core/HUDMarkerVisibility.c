//------------------------------------------------------------------------------------------------
//! Eligibility: HMD_HudMarkerEligibility (player, vehicle + HMD_HudMarkerEligibilityVehicleComponent, helmet camera, seat toggles).
//! View distance / policy: HMD_HudMarkerPolicyResolver (vehicle hull) or HMD_LaserDesignatorGadgetComponent while dismounted designator zoom.
//! Dismounted laser designator zoom: IFF + laser world dots follow the designator gadget prefab (HUD category), not Numpad 9 / character binocular component.
//! Dismounted IFF: only designator viewport or zoomed designator + GetHudIffWhileZoomed. Vehicle IFF: hull PolicyAllowsIffMarkers + vehicle laser / marking / lock + Numpad 9 (s_bShowIffMarkers).
//! ShouldRenderWorldMarkers(): true while designator zoomed; vehicle handheld binoculars disable overlay.
//! Rangefinder/code readout: HUDLaserMarkingComponent path (see ShouldShowLaserDesignatorReadout).
class HUDMarkerVisibility
{
	protected static bool s_bVehicleLaserVisibility;
	protected static bool s_bVehicleLaserMarkingMode;
	//! Numpad 9: drives pooled IFF in vehicle when hull policy and other gates pass; on foot, IFF is designator-only (this toggle does not bypass that).
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
		if (HMD_DesignatorViewportState.IsActive())
			return true;
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
		{
			HMD_LaserDesignatorGadgetComponent des = HMD_HandheldOpticZoom.FindActiveLocalDesignatorComp();
			return des && des.GetHudOwnLaserDesignationWhileZoomed();
		}
		if (!HMD_HudMarkerPolicyResolver.PolicyAllowsOwnLaserDesignationMarkers())
			return false;
		if (s_bVehicleLaserVisibility)
			return true;
		if (s_bVehicleLaserMarkingMode)
			return true;
		if (HMD_RangefinderHUDState.IsLockTargetReadout())
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Other players' laser designations (WCS). Numpad * when in vehicle HUD context; designator gadget when zoomed dismounted.
	static bool ShouldIncludeForeignLaserDesignationDotsInHUD()
	{
		if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
			return false;
		if (!HMD_HudMarkerEligibility.PassesHudMarkerWorldOverlayEligibility())
			return false;
		if (HMD_DesignatorViewportState.IsActive())
			return true;
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
		{
			HMD_LaserDesignatorGadgetComponent des = HMD_HandheldOpticZoom.FindActiveLocalDesignatorComp();
			return des && des.GetHudForeignLaserDesignationsWhileZoomed();
		}
		if (!HMD_HudMarkerPolicyResolver.PolicyAllowsForeignLaserDesignationMarkers())
			return false;
		return s_bVehicleLaserVisibility;
	}

	//------------------------------------------------------------------------------------------------
	//! Dismounted: designator viewport or zoom + gadget IFF pref only. Vehicle: hull policy + Numpad 9 (s_bShowIffMarkers) only, not Numpad * or marking.
	static bool ShouldIncludeIffMarkersInHUD()
	{
		if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
			return false;
		if (!HMD_HudMarkerEligibility.PassesHudMarkerWorldOverlayEligibility())
			return false;
		if (HMD_DesignatorViewportState.IsActive())
		{
			HMD_LaserDesignatorGadgetComponent des = HMD_HandheldOpticZoom.FindActiveLocalDesignatorComp();
			return des && des.GetHudIffWhileZoomed();
		}
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
		{
			HMD_LaserDesignatorGadgetComponent des = HMD_HandheldOpticZoom.FindActiveLocalDesignatorComp();
			return des && des.GetHudIffWhileZoomed();
		}

		PlayerController pc = GetGame().GetPlayerController();
		IEntity controlled = pc.GetControlledEntity();
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);
		const bool inVehicle = ch && ch.IsInVehicle();
		if (!inVehicle)
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
	static bool IsIffMarkerUserToggleOn()
	{
		return s_bShowIffMarkers;
	}

	//------------------------------------------------------------------------------------------------
	//! World marker overlay roots: designator zoom / handheld ADS, then any of local laser, foreign laser, or IFF inclusion (same drivers as per-kind fetches).
	static bool ShouldRenderWorldMarkers()
	{
		if (!HMD_HudMarkerEligibility.IsLocalPlayerConsciousForHud())
			return false;
		if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
			return false;
		if (HMD_DesignatorViewportState.IsActive())
			return true;
		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
			return true;
		if (!HMD_HudMarkerEligibility.PassesHudMarkerWorldOverlayEligibility())
			return false;
		if (s_bVehicleLaserVisibility)
			return true;
		if (s_bVehicleLaserMarkingMode)
			return true;
		if (HMD_RangefinderHUDState.IsLockTargetReadout())
			return true;
		return ShouldIncludeIffMarkersInHUD();
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

