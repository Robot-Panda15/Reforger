//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Placeable IFF inventory: notifies beacon on placement. Parent Grenade_IFF_Green_HudProxy (or similar) under the beacon prefab; HMD_IffBeaconComponent enables its HMD_PlacedDesignationComponent when transmitting.")]
class HMD_IffBeaconExplosiveInventoryItemComponentClass : SCR_PlaceableInventoryItemComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Notifies HMD_IffBeaconComponent when place-mode placement completes.
class HMD_IffBeaconExplosiveInventoryItemComponent : SCR_PlaceableInventoryItemComponent
{
	//------------------------------------------------------------------------------------------------
	protected void HMD_TryNotifyBeaconPlaced(IEntity owner)
	{
		if (!owner)
			return;
		HMD_IffBeaconComponent b = HMD_IffBeaconComponent.FindOnEntity(owner);
		if (b)
			b.NotifyPlacedInWorld();
	}

	//------------------------------------------------------------------------------------------------
	override void PlacementDone(notnull ChimeraCharacter user)
	{
		super.PlacementDone(user);
		HMD_TryNotifyBeaconPlaced(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlacedOnGround()
	{
		super.OnPlacedOnGround();
		HMD_TryNotifyBeaconPlaced(GetOwner());
	}
}
