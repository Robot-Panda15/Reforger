//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Policy when local player uses handheld optics in a vehicle (binocular / designator zoom). Attach on the player character.")]
class HMD_HudMarkerEligibilityBinocularComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Max range to see IFF/laser HUD dots, which marker classes are shown, and how often the world-marker HUD refreshes.
class HMD_HudMarkerEligibilityBinocularComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.Slider, "Clamp HUD marker view distance (m). 0 = use each marker's own limit only.", "0 20000 100", category: "HUD")]
	protected float m_fMaxMarkerViewDistanceM;

	[Attribute("1", UIWidgets.CheckBox, "Allow IFF entity markers (HUD dots)", category: "HUD")]
	protected bool m_bPolicyAllowIffMarkers = true;

	[Attribute("1", UIWidgets.CheckBox, "Allow laser designation dots (own + others)", category: "HUD")]
	protected bool m_bPolicyAllowLaserDesignations = true;

	[Attribute("0", UIWidgets.Slider, "World marker HUD refresh cap (Hz). 0 = every frame.", "0 60 1", category: "HUD")]
	protected float m_fHudMarkerUpdateRateHz;

	float GetMaxMarkerViewDistanceM()
	{
		return m_fMaxMarkerViewDistanceM;
	}

	bool GetPolicyAllowIffMarkers()
	{
		return m_bPolicyAllowIffMarkers;
	}

	bool GetPolicyAllowLaserDesignations()
	{
		return m_bPolicyAllowLaserDesignations;
	}

	float GetHudMarkerUpdateRateHz()
	{
		return m_fHudMarkerUpdateRateHz;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Hull HUD marker policy (IFF/laser, view distance, refresh), own-laser designation visibility, and Numpad * (others' laser dots) seat list. Attach on vehicle root.")]
class HMD_HudMarkerEligibilityVehicleComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class HMD_HudMarkerEligibilityVehicleComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.Slider, "Clamp HUD marker view distance (m). 0 = use each marker's own limit only.", "0 20000 100", category: "HUD")]
	protected float m_fMaxMarkerViewDistanceM;

	[Attribute("1", UIWidgets.CheckBox, "Allow IFF entity markers (HUD dots)", category: "HUD")]
	protected bool m_bPolicyAllowIffMarkers = true;

	[Attribute("1", UIWidgets.CheckBox, "Allow own laser designation dot (HUD pool / local vehicle marking)", category: "HUD")]
	protected bool m_bPolicyAllowOwnLaserDesignations = true;

	[Attribute("1", UIWidgets.CheckBox, "Allow other players' laser designation dots (WCS / Numpad * pool)", category: "HUD")]
	protected bool m_bPolicyAllowLaserDesignations = true;

	[Attribute("0", UIWidgets.Slider, "World marker HUD refresh cap (Hz). 0 = every frame.", "0 60 1", category: "HUD")]
	protected float m_fHudMarkerUpdateRateHz;

	[Attribute("0", UIWidgets.CheckBox, "When set: apply this policy only if HUDMarkerSystem HMD helmet rules are satisfied (capability tag or configured helmet prefab). Ignored when global helmet policy is off.", category: "HUD")]
	protected bool m_bRequireHmdHelmetForPolicy;

	[Attribute("", UIWidgets.EditBox, "Comma-separated compartment Unique names that may use Numpad * (others' laser HUD dots). Empty = no seats.", category: "HUD")]
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
	//! Vehicle has eligibility + the occupant's slot name is listed. Used for HUD hints and Numpad * input.
	static bool IsVisibilityEnabledForVehicleSlot(BaseCompartmentSlot slot)
	{
		if (!slot)
			return false;
		IEntity root = HMD_VehicleHUDLaserHelpers.ResolveVehicleHudMarkerEligibilityVehicleRoot(slot.GetOwner());
		if (!root)
			return false;
		HMD_HudMarkerEligibilityVehicleComponent veh = HMD_HudMarkerEligibilityVehicleComponent.Cast(root.FindComponent(HMD_HudMarkerEligibilityVehicleComponent));
		if (!veh)
			return false;
		return veh.IsLocalSlotAllowed(slot);
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
		if (HMD_VehicleHUDLaserHelpers.ResolveVehicleHudMarkerEligibilityVehicleRoot(slot.GetOwner()) != vehicle)
			return;
		if (!IsLocalSlotAllowed(slot))
			return;
		m_bLocalVisibilityEnabled = !m_bLocalVisibilityEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Called when local player boards this vehicle (new hull): Numpad * starts off until they toggle.
	void ResetLocalVisibilityForNewOccupant()
	{
		m_bLocalVisibilityEnabled = false;
	}

	float GetMaxMarkerViewDistanceM()
	{
		return m_fMaxMarkerViewDistanceM;
	}

	bool GetPolicyAllowIffMarkers()
	{
		return m_bPolicyAllowIffMarkers;
	}

	bool GetPolicyAllowLaserDesignations()
	{
		return m_bPolicyAllowLaserDesignations;
	}

	bool GetPolicyAllowOwnLaserDesignations()
	{
		return m_bPolicyAllowOwnLaserDesignations;
	}

	float GetHudMarkerUpdateRateHz()
	{
		return m_fHudMarkerUpdateRateHz;
	}

	bool GetRequireHmdHelmetForPolicy()
	{
		return m_bRequireHmdHelmetForPolicy;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Legacy optional turret entity; world HUD policy is resolved from hull HMD_HudMarkerEligibilityVehicleComponent. HUDLaserTurretMarkingComponent controls deployment + usage tooltips only.")]
class HMD_HudMarkerEligibilityVehicleTurretComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Legacy prefab fields: HMD_HudMarkerPolicyResolver no longer reads this component (hull policy owns world HUD). Safe to remove from turrets over time.
class HMD_HudMarkerEligibilityVehicleTurretComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.Slider, "Legacy: unused by resolver.", "0 20000 100", category: "HUD")]
	protected float m_fMaxMarkerViewDistanceM;

	[Attribute("1", UIWidgets.CheckBox, "Legacy: unused by resolver.", category: "HUD")]
	protected bool m_bPolicyAllowIffMarkers = true;

	[Attribute("1", UIWidgets.CheckBox, "Legacy: unused by resolver.", category: "HUD")]
	protected bool m_bPolicyAllowLaserDesignations = true;

	[Attribute("0", UIWidgets.Slider, "World marker HUD refresh cap (Hz). 0 = every frame.", "0 60 1", category: "HUD")]
	protected float m_fHudMarkerUpdateRateHz;

	float GetMaxMarkerViewDistanceM()
	{
		return m_fMaxMarkerViewDistanceM;
	}

	bool GetPolicyAllowIffMarkers()
	{
		return m_bPolicyAllowIffMarkers;
	}

	bool GetPolicyAllowLaserDesignations()
	{
		return m_bPolicyAllowLaserDesignations;
	}

	float GetHudMarkerUpdateRateHz()
	{
		return m_fHudMarkerUpdateRateHz;
	}
}
