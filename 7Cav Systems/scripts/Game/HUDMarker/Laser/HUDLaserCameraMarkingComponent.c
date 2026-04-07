//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Vehicle / seat: camera-aimed laser marking (Numpad /). Use on hull or seats where aim follows the player view / optics, not a articulated turret bone.")]
class HUDLaserCameraMarkingComponentClass : HUDLaserMarkingComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Camera-based vehicle marking: rangefinder + virtual dot along the active camera view (binocular-style).
class HUDLaserCameraMarkingComponent : HUDLaserMarkingComponent
{
	[Attribute("4000", UIWidgets.Slider, "HUD fade distance (m) for virtual laser dot", "100 20000 100", category: "Laser")]
	protected float m_fMarkerVisibilityDistance;

	//------------------------------------------------------------------------------------------------
	override protected float HMD_GetMarkerDotRegistrationVisibilityDistanceM()
	{
		return m_fMarkerVisibilityDistance;
	}

	//------------------------------------------------------------------------------------------------
	override protected void HMD_TryApplyDesignationConfig()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;
		HMD_LaserDesignationVehicleCameraComponent cfg = HMD_LaserDesignationVehicleCameraComponent.Cast(owner.FindComponent(HMD_LaserDesignationVehicleCameraComponent));
		if (!cfg)
			return;
		m_fLaserMaxRange = cfg.GetOwnDesignationDistanceM();
		m_fMarkerVisibilityDistance = cfg.GetOwnDesignationDistanceM();
		m_fLaserUpdateRateHz = cfg.GetDesignationUpdateRateHz();
		m_bGroundVehicleCodes1200 = cfg.GetGroundVehicleCodes1200();
		m_bAirVehicleCodes1300 = cfg.GetAirVehicleCodes1300();
		HMD_ClampInitialDisplayLaserCode();
	}
}
