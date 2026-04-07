//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Turret entity: deploy local laser marking (Numpad /) and usage-tooltip gates. World HUD own-laser visibility is hull HMD_HudMarkerEligibilityVehicleComponent. Attach on turret; set aim bone to proc anim (e.g. v_gun_01).")]
class HUDLaserTurretMarkingComponentClass : HUDLaserMarkingComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Turret-based marking: trace from gun bone when set, else falls back to camera.
class HUDLaserTurretMarkingComponent : HUDLaserMarkingComponent
{
	[Attribute("0", UIWidgets.CheckBox, "Outside gunner weapon optic: require HMD helmet for deploy + laser-usage tooltips (not hull own-laser dot). Ignored while TurretController.IsWeaponADS() (resolved gunner seat).", category: "Laser")]
	protected bool m_bRequireHmdHelmetForOutsideTurretLaser;

	//------------------------------------------------------------------------------------------------
	//! When outside gunner weapon optic (see HMD_HmdVehicleHudRestriction.IsLocalPlayerInGunnerWeaponOpticCamera): if enabled, same helmet gate as hull policy (global helmet policy off = pass).
	bool PassesOutsideTurretCameraHmdGate()
	{
		ChimeraWorld world = GetGame().GetWorld();
		bool pass = false;
		if (!m_bRequireHmdHelmetForOutsideTurretLaser)
			pass = true;
		else if (HMD_HmdVehicleHudRestriction.IsLocalPlayerInTurretScriptedCamera())
			pass = true;
		else if (!world)
			pass = false;
		else if (!HMD_HmdVehicleHudRestriction.VehicleHudShouldRestrictToCameraOnly(world))
			pass = true;
		else
			pass = HMD_HmdVehicleHudRestriction.LocalPlayerHasHmdHelmetCapability(world);
		return pass;
	}

	//------------------------------------------------------------------------------------------------
	//! Own designation dot fade distance: hull marker policy; fallback = laser max range.
	override protected float HMD_GetMarkerDotRegistrationVisibilityDistanceM()
	{
		float d = HMD_HudMarkerPolicyResolver.GetEffectiveMaxViewDistanceM();
		if (d > 0.001)
			return d;
		return m_fLaserMaxRange;
	}

	[Attribute("v_gun_01", UIWidgets.EditBox, "Bone name for barrel / LOS (e.g. v_gun_01). Empty = camera fallback.", category: "Laser")]
	protected string m_sAimBoneName;

	[Attribute("", UIWidgets.EditBox, "Optional second LOS bone; may also be set via HMD_LaserDesignationVehicleTurretComponent.", category: "Laser")]
	protected string m_sAimBoneNameAlternate;

	//------------------------------------------------------------------------------------------------
	override protected bool HMD_DefaultUseAirVehicleCodes()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected void HMD_TryApplyDesignationConfig()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;
		HMD_LaserDesignationVehicleTurretComponent cfg = HMD_LaserDesignationVehicleTurretComponent.Cast(owner.FindComponent(HMD_LaserDesignationVehicleTurretComponent));
		if (!cfg)
			return;
		m_fLaserMaxRange = cfg.GetOwnDesignationDistanceM();
		m_fLaserUpdateRateHz = cfg.GetDesignationUpdateRateHz();
		m_bGroundVehicleCodes1200 = cfg.GetGroundVehicleCodes1200();
		m_bAirVehicleCodes1300 = cfg.GetAirVehicleCodes1300();
		string b = cfg.GetAimBoneName();
		if (b && b != "")
			m_sAimBoneName = b;
		string b2 = cfg.GetAimBoneNameAlternate();
		if (b2 && b2 != "")
			m_sAimBoneNameAlternate = b2;
		HMD_ClampInitialDisplayLaserCode();
	}

	//------------------------------------------------------------------------------------------------
	override protected bool HMD_PassesTurretLaserOutsideCameraHmdGate()
	{
		return PassesOutsideTurretCameraHmdGate();
	}

	//------------------------------------------------------------------------------------------------
	protected bool HMD_TryBoneAim(IEntity owner, BaseWorld world, string boneName, out vector outStart, out vector outDirNorm)
	{
		if (!owner || !world || !boneName || boneName == "")
			return false;
		Animation anim = owner.GetAnimation();
		if (!anim)
			return false;
		TNodeId boneId = anim.GetBoneIndex(boneName);
		if (boneId == -1)
			return false;
		vector boneTM[4];
		anim.GetBoneMatrix(boneId, boneTM);
		vector worldTM[4];
		owner.GetWorldTransform(worldTM);
		vector localDir = boneTM[2].Normalized();
		outDirNorm = worldTM[0] * localDir[0] + worldTM[1] * localDir[1] + worldTM[2] * localDir[2];
		float len = outDirNorm.Length();
		if (len < 0.001)
			return false;
		outDirNorm = outDirNorm * (1.0 / len);
		vector localPos = boneTM[3];
		outStart = owner.GetOrigin() + worldTM[0] * localPos[0] + worldTM[1] * localPos[1] + worldTM[2] * localPos[2];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool HMD_GetLaserAim(IEntity owner, BaseWorld world, out vector outStart, out vector outDirNorm)
	{
		if (!owner || !world)
			return false;
		if (HMD_TryBoneAim(owner, world, m_sAimBoneName, outStart, outDirNorm))
			return true;
		if (m_sAimBoneNameAlternate && m_sAimBoneNameAlternate != "")
		{
			if (HMD_TryBoneAim(owner, world, m_sAimBoneNameAlternate, outStart, outDirNorm))
				return true;
		}
		vector camTM[4];
		world.GetCurrentCamera(camTM);
		outStart = camTM[3];
		outDirNorm = camTM[2].Normalized();
		return true;
	}
}
