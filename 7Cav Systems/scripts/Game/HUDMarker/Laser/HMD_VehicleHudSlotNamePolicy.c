//------------------------------------------------------------------------------------------------
//! Shared comma-slot list parsing + compartment discovery + slot identity checks for vehicle HUD laser components.
class HMD_VehicleHudSlotNamePolicy
{
	//------------------------------------------------------------------------------------------------
	//! Caller must pass a non-null cache array (create empty before first call).
	static void ParseCommaSlotList(string csvSlotNames, notnull array<string> cache)
	{
		HMD_VehicleHUDLaserHelpers.SplitCommaSeparatedNames(csvSlotNames, cache);
	}

	//------------------------------------------------------------------------------------------------
	//! Caller must pass a non-null discovered array (create empty before first call).
	static void EnsureDiscoveredCompartmentNames(IEntity ownerEntity, notnull array<string> discovered)
	{
		IEntity vehicle = ownerEntity;
		if (!vehicle)
			return;
		IEntity discoverRoot = vehicle.GetRootParent();
		if (!discoverRoot)
			discoverRoot = vehicle;
		HMD_VehicleHUDLaserHelpers.CollectCompartmentUniqueNames(discoverRoot, discovered);
	}

	//------------------------------------------------------------------------------------------------
	static bool EvaluateSlotAgainstLists(BaseCompartmentSlot slot, notnull array<string> allowedSlotNames, array<string> discoveredOptional)
	{
		string id = HMD_VehicleHUDLaserHelpers.GetSlotIdentityName(slot);
		if (id == "")
			return false;
		if (!HMD_VehicleHUDLaserHelpers.StringInList(id, allowedSlotNames))
			return false;
		if (discoveredOptional && discoveredOptional.Count() > 0)
			return HMD_VehicleHUDLaserHelpers.StringInList(id, discoveredOptional);
		return true;
	}
}
