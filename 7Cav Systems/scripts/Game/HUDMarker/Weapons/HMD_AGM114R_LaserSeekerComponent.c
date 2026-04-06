//------------------------------------------------------------------------------------------------
//! AGM114R: when `TryGetHmdLaserTargetWorld` succeeds, guides on that point; otherwise defers to base WCS seeker (no HMD override of `m_vTargetPosition`).
class HMD_AGM114R_LaserSeekerComponentClass : WCS_Armament_LaserSeekerComponentClass
{
}

class HMD_AGM114R_LaserSeekerComponent : WCS_Armament_LaserSeekerComponent
{
	//------------------------------------------------------------------------------------------------
	override void CalculateTargetPosition()
	{
		bool isServer = !m_RplComponent || !m_RplComponent.IsProxy();
		if (!isServer)
			return;

		bool canSync = m_RplComponent && Replication.FindId(m_RplComponent) != RplId.Invalid();

		if (m_TargetDesignator)
		{
			super.CalculateTargetPosition();
			return;
		}

		if (m_WeaponStationComponent)
		{
			IEntity stationOwner = m_WeaponStationComponent.GetOwner();
			vector hmdPos;
			if (HMD_WcsLaserVehicleDesignatorBridge.TryGetHmdLaserTargetWorld(stationOwner, hmdPos))
			{
				//! Bridge already applied lock, cone, or self-marking fallback; do not gate on seeker FOV (self-lase / below-aircraft spots are often off missile boresight).
				vector designatedPos = hmdPos;
				m_vLastValidTargetPosition = designatedPos;
				m_vTargetPosition = designatedPos;
				if (canSync)
					Rpc(RpcDo_SyncTargetPosition, m_vTargetPosition);
				return;
			}
			//! No bridge target: do not set guidance here; base `WCS_Armament_LaserSeekerComponent` applies.
		}

		super.CalculateTargetPosition();
	}
}
