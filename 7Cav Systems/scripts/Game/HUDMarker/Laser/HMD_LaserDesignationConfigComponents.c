//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Defaults for handheld / vehicle-binocular laser designator (1111-1200 range). Attach on the player character.")]
class HMD_LaserDesignationBinocularComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class HMD_LaserDesignationBinocularComponent : ScriptComponent
{
	[Attribute("4000", UIWidgets.Slider, "Own designation trace length and HUD dot fade distance (m)", "100 20000 100", category: "Laser")]
	protected float m_fOwnDesignationDistanceM;

	[Attribute("1111", UIWidgets.EditBox, "Default laser code (1111-1200)", category: "Laser")]
	protected int m_iDefaultLaserCode;

	[Attribute("0", UIWidgets.Slider, "Trace/HUD update cap (Hz). 0 = every frame.", "0 60 1", category: "Laser")]
	protected float m_fDesignationUpdateRateHz;

	float GetOwnDesignationDistanceM()
	{
		return m_fOwnDesignationDistanceM;
	}

	int GetDefaultLaserCode()
	{
		return m_iDefaultLaserCode;
	}

	float GetDesignationUpdateRateHz()
	{
		return m_fDesignationUpdateRateHz;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Defaults for HUDLaserCameraMarkingComponent (commander / camera LOS). Same entity as marking.")]
class HMD_LaserDesignationVehicleCameraComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class HMD_LaserDesignationVehicleCameraComponent : ScriptComponent
{
	[Attribute("4000", UIWidgets.Slider, "Own designation trace length and HUD dot fade (m)", "100 20000 100", category: "Laser")]
	protected float m_fOwnDesignationDistanceM;

	[Attribute("1", UIWidgets.CheckBox, "Ground Vehicle Codes (1200): iterate 1211-1299", category: "Laser")]
	protected bool m_bGroundVehicleCodes1200;

	[Attribute("0", UIWidgets.CheckBox, "Air Vehicle Codes (1300): iterate 1311-1399", category: "Laser")]
	protected bool m_bAirVehicleCodes1300;

	[Attribute("0", UIWidgets.Slider, "Trace/HUD update cap (Hz). 0 = every frame.", "0 60 1", category: "Laser")]
	protected float m_fDesignationUpdateRateHz;

	float GetOwnDesignationDistanceM()
	{
		return m_fOwnDesignationDistanceM;
	}

	bool GetGroundVehicleCodes1200()
	{
		return m_bGroundVehicleCodes1200;
	}

	bool GetAirVehicleCodes1300()
	{
		return m_bAirVehicleCodes1300;
	}

	float GetDesignationUpdateRateHz()
	{
		return m_fDesignationUpdateRateHz;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Defaults for HUDLaserTurretMarkingComponent. Aim bones override turret marking defaults when set.")]
class HMD_LaserDesignationVehicleTurretComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class HMD_LaserDesignationVehicleTurretComponent : ScriptComponent
{
	[Attribute("4000", UIWidgets.Slider, "Own designation trace length and HUD dot fade (m)", "100 20000 100", category: "Laser")]
	protected float m_fOwnDesignationDistanceM;

	[Attribute("0", UIWidgets.CheckBox, "Ground Vehicle Codes (1200): iterate 1211-1299", category: "Laser")]
	protected bool m_bGroundVehicleCodes1200;

	[Attribute("1", UIWidgets.CheckBox, "Air Vehicle Codes (1300): iterate 1311-1399", category: "Laser")]
	protected bool m_bAirVehicleCodes1300;

	[Attribute("0", UIWidgets.Slider, "Trace/HUD update cap (Hz). 0 = every frame.", "0 60 1", category: "Laser")]
	protected float m_fDesignationUpdateRateHz;

	[Attribute("v_gun_01", UIWidgets.EditBox, "Primary aim bone (empty = use HUDLaserTurretMarkingComponent only)", category: "Laser")]
	protected string m_sAimBoneName;

	[Attribute("", UIWidgets.EditBox, "Optional second bone for LOS (empty = unused)", category: "Laser")]
	protected string m_sAimBoneNameAlternate;

	float GetOwnDesignationDistanceM()
	{
		return m_fOwnDesignationDistanceM;
	}

	bool GetGroundVehicleCodes1200()
	{
		return m_bGroundVehicleCodes1200;
	}

	bool GetAirVehicleCodes1300()
	{
		return m_bAirVehicleCodes1300;
	}

	float GetDesignationUpdateRateHz()
	{
		return m_fDesignationUpdateRateHz;
	}

	string GetAimBoneName()
	{
		return m_sAimBoneName;
	}

	string GetAimBoneNameAlternate()
	{
		return m_sAimBoneNameAlternate;
	}
}
