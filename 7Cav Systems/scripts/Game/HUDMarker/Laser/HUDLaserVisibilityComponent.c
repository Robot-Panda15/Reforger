//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Vehicle: Numpad * toggles HUD laser designations (others' lasers). IFF entity markers use Numpad 9. Set Visibility slot names (comma-separated); must match compartments on this vehicle (discovered at runtime).")]
class HUDLaserVisibilityComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Per-client: show other players' laser markers as HUD dots. Does not enable marking or rangefinder readout.
class HUDLaserVisibilityComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Comma-separated compartment Unique names that may use visibility. Empty = no seats.", category: "HUD")]
	protected string m_sVisibilitySlotNames;

	protected ref array<string> m_aVisibilitySlotNamesCache;
	protected bool m_bVisibilitySlotNamesParsed;

	protected ref array<string> m_aDiscoveredSlotNames;
	protected bool m_bDiscoveredSlotsParsed;

	protected bool m_bLocalVisibilityEnabled;

	//------------------------------------------------------------------------------------------------
	protected void ParseVisibilitySlotNames()
	{
		if (m_bVisibilitySlotNamesParsed)
			return;
		m_bVisibilitySlotNamesParsed = true;
		if (!m_aVisibilitySlotNamesCache)
			m_aVisibilitySlotNamesCache = new array<string>();
		HMD_VehicleHudSlotNamePolicy.ParseCommaSlotList(m_sVisibilitySlotNames, m_aVisibilitySlotNamesCache);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs once per component: collects all compartment identity names on this vehicle hierarchy.
	protected void EnsureDiscoveredCompartmentNames()
	{
		if (m_bDiscoveredSlotsParsed)
			return;
		m_bDiscoveredSlotsParsed = true;
		if (!m_aDiscoveredSlotNames)
			m_aDiscoveredSlotNames = new array<string>();
		HMD_VehicleHudSlotNamePolicy.EnsureDiscoveredCompartmentNames(GetOwner(), m_aDiscoveredSlotNames);
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicle has visibility HUD and the occupant's slot name is listed. Used for HUD hints and input.
	static bool IsVisibilityEnabledForVehicleSlot(BaseCompartmentSlot slot)
	{
		if (!slot)
			return false;
		IEntity root = HMD_VehicleHUDLaserHelpers.ResolveVehicleHUDVisibilityRoot(slot.GetOwner());
		if (!root)
			return false;
		HUDLaserVisibilityComponent vis = HUDLaserVisibilityComponent.Cast(root.FindComponent(HUDLaserVisibilityComponent));
		if (!vis)
			return false;
		return vis.IsLocalSlotAllowed(slot);
	}

	//------------------------------------------------------------------------------------------------
	bool GetLocalVisibilityEnabled()
	{
		return m_bLocalVisibilityEnabled;
	}

	//------------------------------------------------------------------------------------------------
	bool IsLocalSlotAllowed(BaseCompartmentSlot slot)
	{
		ParseVisibilitySlotNames();
		if (!m_aVisibilitySlotNamesCache || m_aVisibilitySlotNamesCache.Count() == 0)
			return false;
		EnsureDiscoveredCompartmentNames();
		return HMD_VehicleHudSlotNamePolicy.EvaluateSlotAgainstLists(slot, m_aVisibilitySlotNamesCache, m_aDiscoveredSlotNames);
	}

	//------------------------------------------------------------------------------------------------
	void RequestToggleLocalVisibility(IEntity localCharacter, BaseCompartmentSlot slot)
	{
		IEntity vehicle = GetOwner();
		if (!vehicle || !localCharacter || !slot)
			return;
		if (HMD_VehicleHUDLaserHelpers.ResolveVehicleHUDVisibilityRoot(slot.GetOwner()) != vehicle)
			return;
		if (!IsLocalSlotAllowed(slot))
			return;
		m_bLocalVisibilityEnabled = !m_bLocalVisibilityEnabled;
	}
}
