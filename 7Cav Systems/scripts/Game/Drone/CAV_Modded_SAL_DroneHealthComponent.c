//------------------------------------------------------------------------------------------------
//! Extension only:
//! - Damage death: SCR_DamageManagerComponent.IsDestroyed() forces DisconnectDrone (server), then deletes drone root.
//! - Other deaths: same as base intent — RPL health <= 0, water (no auto-delete for those here).
//------------------------------------------------------------------------------------------------
modded class SAL_DroneHealthComponent
{
	protected bool m_CAV_DisconnectForDeathSent;
	protected bool m_CAV_PendingDamageDestroyDelete;

	//------------------------------------------------------------------------------------------------
	protected void CAV_DeleteDroneRootDeferred(IEntity root)
	{
		if (!root)
			return;
		SCR_EntityHelper.DeleteEntityAndChildren(root);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CAV_ShouldDisconnectThisFrame(IEntity owner, out string deathReason)
	{
		deathReason = "";

		if (!owner || !m_DroneController)
			return false;

		// Damage manager destroyed: always treat as death for disconnect (not tied to console app or m_bIsActive).
		if (m_DamageManager && m_DamageManager.IsDestroyed())
		{
			deathReason = "DamageManager.IsDestroyed";
			return true;
		}

		if (m_bIsDestroyed || m_DroneController.m_bIsTriggered)
			return false;

		if (!m_DroneController.m_bIsActive)
			return false;

		if (m_fRplDroneHealth <= 0)
		{
			deathReason = "RplHealth<=0";
			return true;
		}

		World w = m_World;
		if (!w)
			w = GetGame().GetWorld();
		if (w)
		{
			EWaterSurfaceType waterType;
			vector waterSurfacePos;
			vector transformWS[4];
			vector obbExtents;
			ChimeraWorldUtils.TryGetWaterSurface(w, owner.GetOrigin(), waterSurfacePos, waterType, transformWS, obbExtents);
			if (waterType != EWaterSurfaceType.WST_NONE)
			{
				deathReason = "water";
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFixedFrame(IEntity owner, float timeSlice)
	{
		if (Replication.IsServer() && !m_CAV_DisconnectForDeathSent && m_DroneManager)
		{
			string reason;
			if (CAV_ShouldDisconnectThisFrame(owner, reason))
			{
				m_CAV_DisconnectForDeathSent = true;

				int ctrlOwner = -1;
				if (m_DroneController)
					ctrlOwner = m_DroneController.m_iOwner;

				int resolvedId = ctrlOwner;
				if (resolvedId < 0)
					resolvedId = m_DroneManager.GetDronesOwner(m_Id);

				if (resolvedId >= 0)
					m_DroneManager.DisconnectDrone(m_Id, resolvedId);

				// After damage-system destroy: remove drone entity once disconnect has been issued (or no owner).
				if (reason == "DamageManager.IsDestroyed" && !m_CAV_PendingDamageDestroyDelete && GetGame().GetCallqueue())
				{
					m_CAV_PendingDamageDestroyDelete = true;
					GetGame().GetCallqueue().CallLater(CAV_DeleteDroneRootDeferred, 300, false, owner);
				}
			}
		}

		super.EOnFixedFrame(owner, timeSlice);
	}
}
