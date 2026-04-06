//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "HUD", description: "Turret entity: laser marking aligned to turret / gun bone (Numpad /). Attach on the turret, not the parent hull. Set aim bone to match your proc anim (e.g. v_gun_01).")]
class HUDLaserTurretMarkingComponentClass : HUDLaserMarkingComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Turret-based marking: trace from gun bone when set, else falls back to camera.
class HUDLaserTurretMarkingComponent : HUDLaserMarkingComponent
{
	[Attribute("v_gun_01", UIWidgets.EditBox, "Bone name for barrel / LOS (e.g. v_gun_01). Empty = camera fallback.", category: "Laser")]
	protected string m_sAimBoneName;

	//------------------------------------------------------------------------------------------------
	override protected void HMD_ClampInitialDisplayLaserCode()
	{
		if (m_iDisplayLaserCode < 1311 || m_iDisplayLaserCode > 1399)
			m_iDisplayLaserCode = 1311;
	}

	//------------------------------------------------------------------------------------------------
	override protected int HMD_WrapVehicleMarkingCode(int candidate)
	{
		return HMD_LaserCodeRules.WrapTurretVehicleMarking(candidate);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool HMD_GetLaserAim(IEntity owner, BaseWorld world, out vector outStart, out vector outDirNorm)
	{
		if (!owner || !world)
			return false;
		string boneName = m_sAimBoneName;
		if (boneName && boneName != "")
		{
			Animation anim = owner.GetAnimation();
			if (anim)
			{
				TNodeId boneId = anim.GetBoneIndex(boneName);
				if (boneId != -1)
				{
					vector boneTM[4];
					anim.GetBoneMatrix(boneId, boneTM);
					vector worldTM[4];
					owner.GetWorldTransform(worldTM);
					vector localDir = boneTM[2].Normalized();
					outDirNorm = worldTM[0] * localDir[0] + worldTM[1] * localDir[1] + worldTM[2] * localDir[2];
					float len = outDirNorm.Length();
					if (len > 0.001)
						outDirNorm = outDirNorm * (1.0 / len);
					vector localPos = boneTM[3];
					outStart = owner.GetOrigin() + worldTM[0] * localPos[0] + worldTM[1] * localPos[1] + worldTM[2] * localPos[2];
					return true;
				}
			}
		}
		vector camTM[4];
		world.GetCurrentCamera(camTM);
		outStart = camTM[3];
		outDirNorm = camTM[2].Normalized();
		return true;
	}
}
