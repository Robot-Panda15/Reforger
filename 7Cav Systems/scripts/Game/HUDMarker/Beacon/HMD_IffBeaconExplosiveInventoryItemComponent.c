//------------------------------------------------------------------------------------------------
//! Inherits placeable inventory behavior (same as explosive charges) so we can override PlacementDone.
class HMD_IffBeaconExplosiveInventoryItemComponentClass : SCR_PlaceableInventoryItemComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Notifies HMD_IffBeaconComponent when place-mode placement completes so pickup/equip can be hidden.
//! PlacementDone is declared on SCR_PlaceableInventoryItemComponent (not on SCR_ExplosiveChargeInventoryItemComponent), so this component subclasses SCR_PlaceableInventoryItemComponent directly.
class HMD_IffBeaconExplosiveInventoryItemComponent : SCR_PlaceableInventoryItemComponent
{
	//------------------------------------------------------------------------------------------------
	override void PlacementDone(notnull ChimeraCharacter user)
	{
		super.PlacementDone(user);
		IEntity owner = GetOwner();
		if (!owner)
			return;
		HMD_IffBeaconComponent beacon = HMD_IffBeaconComponent.FindOnEntity(owner);
		if (beacon)
			beacon.NotifyPlacedInWorld();
	}
}
