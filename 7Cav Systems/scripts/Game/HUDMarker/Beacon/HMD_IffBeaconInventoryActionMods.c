//------------------------------------------------------------------------------------------------
modded class SCR_PickUpItemAction : SCR_PickUpItemAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;
		return !HMD_IffBeaconInvActionHelper.ShouldHideInventoryActionsForItem(GetOwner());
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_EquipGadgetAction : SCR_EquipGadgetAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;
		return !HMD_IffBeaconInvActionHelper.ShouldHideInventoryActionsForItem(GetOwner());
	}
}

//------------------------------------------------------------------------------------------------
//! Explosive / placeable items use weapon-equip actions in vicinity, not only gadget-equip.
modded class SCR_EquipWeaponAction : SCR_EquipWeaponAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;
		return !HMD_IffBeaconInvActionHelper.ShouldHideInventoryActionsForItem(GetOwner());
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_EquipWeaponHolsterAction : SCR_EquipWeaponHolsterAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;
		return !HMD_IffBeaconInvActionHelper.ShouldHideInventoryActionsForItem(GetOwner());
	}
}

//------------------------------------------------------------------------------------------------
class HMD_IffBeaconInvActionHelper
{
	//------------------------------------------------------------------------------------------------
	static bool ShouldHideInventoryActionsForItem(IEntity itemEntity)
	{
		if (!itemEntity)
			return false;
		HMD_IffBeaconComponent beacon = HMD_IffBeaconComponent.FindOnEntity(itemEntity);
		if (!beacon)
			return false;
		return beacon.ShouldHideInventoryActions();
	}
}
