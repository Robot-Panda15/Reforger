//------------------------------------------------------------------------------------------------
//! Single source of truth: when vehicle HUD laser hints apply and when Numpad * / may toggle components.
//! If the seat is not enabled for the component (pilot/gunner/crew flags), there is no tooltip and no toggle.
class HMD_VehicleHUDLaserInputPolicy
{
	//------------------------------------------------------------------------------------------------
	static bool MayUseVehicleHUDLaserVisibility(IEntity localChar)
	{
		if (!localChar)
			return false;
		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
		if (!cac)
			return false;
		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot)
			return false;
		if (!slot.GetOwner())
			return false;
		if (HUDMarkerVisibility.IsVehicleBinocularViewActive())
			return false;
		return HUDLaserVisibilityComponent.IsVisibilityEnabledForVehicleSlot(slot);
	}

	//------------------------------------------------------------------------------------------------
	static bool MayUseVehicleHUDLaserMarking(IEntity localChar)
	{
		if (!localChar)
			return false;
		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
		if (!cac)
			return false;
		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot)
			return false;
		if (!slot.GetOwner())
			return false;
		if (HUDMarkerVisibility.IsVehicleBinocularViewActive())
			return false;
		return HUDLaserMarkingComponent.IsMarkingEnabledForVehicleSlot(slot);
	}
}
