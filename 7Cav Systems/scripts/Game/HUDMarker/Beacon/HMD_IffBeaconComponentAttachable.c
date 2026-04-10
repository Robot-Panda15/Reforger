//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Placeable IFF attachment beacon: configurable battery life, off until placed. IFF_AttachableBeacon prefab embeds Grenade_IFF_Green_HudProxy as a child; HMD_PlacedDesignationComponent gates from ShouldShowIffOnHud(). Optional HUDMarkerComponent if no child designation. Do not add HMD_PlacedDesignationComponent on this root; it breaks placement.")]
class HMD_IffBeaconComponentAttachableClass : HMD_IffBeaconComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Extends HMD_IffBeaconComponent for RHS placeable IFF items. Server starts with beacon OFF and not placed;
//! HMD_IffBeaconExplosiveInventoryItemComponent calls NotifyPlacedInWorld after placement. Child HUD proxy designation syncs each frame from the parent beacon.
//! All deployment gates (`m_bPlacedInWorld`, inventory unplace) live here only; base beacon has none.
class HMD_IffBeaconComponentAttachable : HMD_IffBeaconComponent
{
	[Attribute("1800", UIWidgets.Auto, "Battery life in seconds while the beacon is ON after placement (transmitting).", category: "HMD")]
	protected float m_fBatteryLifeSeconds;

	//------------------------------------------------------------------------------------------------
	override protected bool DeploymentGateAllowsIff()
	{
		return m_bPlacedInWorld;
	}

	//------------------------------------------------------------------------------------------------
	override protected void AuthorityTickDeploymentGates(IEntity owner)
	{
		if (!owner)
			return;
		InventoryItemComponent inv = InventoryItemComponent.Cast(owner.FindComponent(InventoryItemComponent));
		if (inv && inv.GetParentSlot() && m_bPlacedInWorld)
		{
			m_bPlacedInWorld = false;
			if (Replication.IsRunning())
				Replication.BumpMe();
			RefreshHudRegistration();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		m_bUnplaceWhenInInventorySlot = true;
		super.OnPostInit(owner);
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			float life = m_fBatteryLifeSeconds;
			if (life <= 0)
				life = BEACON_TOTAL_SECONDS;
			m_fBatterySecondsRemaining = life;
			m_bPlacedInWorld = false;
			m_bBeaconActive = false;
			if (Replication.IsRunning())
				Replication.BumpMe();
			RefreshHudRegistration();
		}
	}
}
