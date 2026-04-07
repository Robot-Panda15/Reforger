//------------------------------------------------------------------------------------------------
//! HUD marker / vehicle laser eligibility: layered checks (player -> vehicle -> helmet -> seat / policy).
//! 1) Player: conscious (ALIVE). 2) Vehicle: HMD_HudMarkerEligibilityVehicleComponent on hull hierarchy.
//! 3) Global HMD helmet: interior view suppressed when policy requires helmet and not in weapon/optic camera.
//! 4) Tooltips & Numpad * / / / 9: per-seat lists; HUDLaserTurretMarkingComponent outside-optic HMD gates marking deployment + usage tooltips (MayUseVehicleHUDLaserMarking). Own-laser dot visibility: hull HMD_HudMarkerEligibilityVehicleComponent.
//! 5) IFF / laser dots policy: HMD_HudMarkerPolicyResolver (hull) + HMD_HudMarkerEligibility* components.

class HMD_HudMarkerEligibility

{

	//------------------------------------------------------------------------------------------------

	//! Requires ALIVE (excludes dead and incapacitated).

	static bool IsLocalPlayerConsciousForHud()

	{

		PlayerController pc = GetGame().GetPlayerController();

		if (!pc)

			return false;

		IEntity controlled = pc.GetControlledEntity();

		if (!controlled)

			return false;

		SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(controlled.FindComponent(SCR_CharacterControllerComponent));

		if (ccc)

			return ccc.GetLifeState() == ECharacterLifeState.ALIVE;

		return true;

	}



	//------------------------------------------------------------------------------------------------

	//! Local player is in a vehicle whose hierarchy includes HMD_HudMarkerEligibilityVehicleComponent (hull policy gate).

	static bool LocalPlayerVehicleHasHudMarkerEligibilityVehicleComponent()

	{

		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();

		if (!localChar)

			return false;

		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(localChar);

		if (!ch || !ch.IsInVehicle())

			return false;

		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));

		if (!cac)

			return false;

		BaseCompartmentSlot slot = cac.GetCompartment();

		if (!slot || !slot.GetOwner())

			return false;

		IEntity vehicleRoot = slot.GetOwner().GetRootParent();

		if (!vehicleRoot)

			vehicleRoot = slot.GetOwner();

		return HMD_VehicleHUDLaserHelpers.FindComponentInHierarchy(vehicleRoot, HMD_HudMarkerEligibilityVehicleComponent) != null;

	}



	//------------------------------------------------------------------------------------------------

	//! Local player is in a vehicle and looking through handheld optics (binoculars / Target Designator).

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

	//! When HMD helmet policy applies: crew without a qualifying helmet only see marker HUD in weapon/optic scripted camera.

	//! Only meaningful once the vehicle carries HMD_HudMarkerEligibilityVehicleComponent (see LocalPlayerVehicleHasHudMarkerEligibilityVehicleComponent).

	static bool VehicleCameraOnlySuppressesMarkerHud()

	{

		if (IsVehicleBinocularViewActive())

			return false;

		PlayerController pc = GetGame().GetPlayerController();

		if (!pc)

			return false;

		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());

		if (!ch || !ch.IsInVehicle())

			return false;

		if (!LocalPlayerVehicleHasHudMarkerEligibilityVehicleComponent())

			return false;

		ChimeraWorld world = GetGame().GetWorld();

		if (!world)

			return false;

		if (!HMD_HmdVehicleHudRestriction.VehicleHudShouldRestrictToCameraOnly(world))

			return false;

		//! Qualifying HMD helmet: not restricted to weapon/optic camera for hull marker HUD (see comment above).
		if (HMD_HmdVehicleHudRestriction.LocalPlayerHasHmdHelmetCapability(world))

			return false;

		return !HMD_HmdVehicleHudRestriction.IsLocalPlayerInVehicleCameraView();

	}



	//------------------------------------------------------------------------------------------------

	//! World-marker overlay + designation dots: player OK, not vehicle handheld optics zoom; in vehicle requires hull eligibility component and passes helmet camera rule.

	static bool PassesHudMarkerWorldOverlayEligibility()

	{

		if (!IsLocalPlayerConsciousForHud())

			return false;

		if (IsVehicleBinocularViewActive())

			return false;

		PlayerController pc = GetGame().GetPlayerController();

		if (!pc)

			return false;

		IEntity controlled = pc.GetControlledEntity();

		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);

		if (ch && ch.IsInVehicle())

		{

			if (!LocalPlayerVehicleHasHudMarkerEligibilityVehicleComponent())

				return false;

			if (VehicleCameraOnlySuppressesMarkerHud())

				return false;

		}

		return true;

	}



	//------------------------------------------------------------------------------------------------

	//! In-vehicle laser readout / turret HUD: requires hull eligibility component (same vehicle gate as world overlay), then helmet camera rule.

	static bool PassesVehicleHudMarkerReadoutEligibility()

	{

		if (!IsLocalPlayerConsciousForHud())

			return false;

		if (!LocalPlayerVehicleHasHudMarkerEligibilityVehicleComponent())

			return false;

		if (VehicleCameraOnlySuppressesMarkerHud())

			return false;

		return true;

	}



	//------------------------------------------------------------------------------------------------

	//! When true: vehicle HUD marker tooltips and * / / / 9 inputs are disabled (hull eligibility + global helmet camera rule). Turret marking-seat HMD only affects MayUseVehicleHUDLaserMarking.

	static bool ShouldVehicleHudMarkerControlsBeDisabled()

	{

		if (!IsLocalPlayerConsciousForHud())

			return true;

		if (SCR_EditorManagerEntity.GetInstance().IsOpened())

			return true;

		if (IsVehicleBinocularViewActive())

			return true;

		PlayerController pc = GetGame().GetPlayerController();

		IEntity controlled = null;

		if (pc)

			controlled = pc.GetControlledEntity();

		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);

		if (ch && ch.IsInVehicle())

		{

			if (!LocalPlayerVehicleHasHudMarkerEligibilityVehicleComponent())

				return true;

		}

		if (VehicleCameraOnlySuppressesMarkerHud())

			return true;

		return false;

	}



	//------------------------------------------------------------------------------------------------

	//! Numpad * (laser designations visibility): seat must allow visibility component; blocked when global eligibility fails.

	static bool MayUseVehicleHUDLaserVisibility(IEntity localChar)

	{

		if (!localChar)

			return false;

		if (ShouldVehicleHudMarkerControlsBeDisabled())

			return false;

		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));

		if (!cac)

			return false;

		BaseCompartmentSlot slot = cac.GetCompartment();

		if (!slot)

			return false;

		if (!slot.GetOwner())

			return false;

		return HMD_HudMarkerEligibilityVehicleComponent.IsVisibilityEnabledForVehicleSlot(slot);

	}



	//------------------------------------------------------------------------------------------------

	//! Numpad / (marking): marking slot only. In gunner weapon/optic camera (turret 1p, transition, or vehicle ADS): always allowed for that slot. Outside that optic: HUDLaserTurretMarkingComponent may require HMD helmet only while in that component's marking seat; other seats use hull rules only.

	static bool MayUseVehicleHUDLaserMarking(IEntity localChar)

	{

		if (!localChar)

			return false;

		if (!IsLocalPlayerConsciousForHud())

			return false;

		if (SCR_EditorManagerEntity.GetInstance().IsOpened())

			return false;

		if (IsVehicleBinocularViewActive())

			return false;

		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));

		if (!cac)

			return false;

		BaseCompartmentSlot slot = cac.GetCompartment();

		if (!slot)

			return false;

		if (!slot.GetOwner())

			return false;

		if (!HUDLaserMarkingComponent.IsMarkingEnabledForVehicleSlot(slot))

			return false;

		if (HMD_HmdVehicleHudRestriction.IsLocalPlayerInTurretScriptedCamera())

			return true;

		HUDLaserMarkingComponent mark = HUDLaserMarkingComponent.FindMarkingComponentForVehicleSlot(slot);

		HUDLaserTurretMarkingComponent turretMark = HUDLaserTurretMarkingComponent.Cast(mark);

		if (turretMark && !turretMark.PassesOutsideTurretCameraHmdGate())

			return false;

		if (ShouldVehicleHudMarkerControlsBeDisabled())

			return false;

		return true;

	}



	//------------------------------------------------------------------------------------------------

	//! Numpad 9 (IFF): blocked when global eligibility fails; no seat filter.

	static bool MayUseVehicleHudIffMarkersToggle(IEntity localChar)

	{

		if (!localChar)

			return false;

		if (ShouldVehicleHudMarkerControlsBeDisabled())

			return false;

		return true;

	}

}

