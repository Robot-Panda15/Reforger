//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Vehicle / seat: camera-aimed laser marking (Numpad /). Use on hull or seats where aim follows the player view / optics, not a articulated turret bone.")]
class HUDLaserCameraMarkingComponentClass : HUDLaserMarkingComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Camera-based vehicle marking: rangefinder + virtual dot along the active camera view (binocular-style).
class HUDLaserCameraMarkingComponent : HUDLaserMarkingComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void HMD_ClampInitialDisplayLaserCode()
	{
		if (m_iDisplayLaserCode < 1111 || m_iDisplayLaserCode > 1200)
			m_iDisplayLaserCode = 1111;
	}

	//------------------------------------------------------------------------------------------------
	override protected int HMD_WrapVehicleMarkingCode(int candidate)
	{
		return HMD_LaserCodeRules.WrapCameraVehicleMarking(candidate);
	}
}
