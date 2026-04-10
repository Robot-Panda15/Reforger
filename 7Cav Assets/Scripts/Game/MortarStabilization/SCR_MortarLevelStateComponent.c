//------------------------------------------------------------------------------------------------
class SCR_MortarLevelStateComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Replicated flag + leveled world pose (yaw + origin). Entity transform is not auto-synced from
//! SetWorldTransform; proxies/JIP apply the same matrix from these props in OnMortarLevelReplicated.
//! Server-only reference to the spawned foundation mesh (removed when the mortar is deleted).
class SCR_MortarLevelStateComponent : ScriptComponent
{
	[RplProp(onRplName: "OnMortarLevelReplicated")]
	protected bool m_bLeveled;

	[RplProp(onRplName: "OnMortarLevelReplicated")]
	protected vector m_vLevelWorldOrigin;

	[RplProp(onRplName: "OnMortarLevelReplicated")]
	protected float m_fLevelYawDeg;

	protected IEntity m_pLevelFoundation;

	//------------------------------------------------------------------------------------------------
	bool IsMortarLeveled()
	{
		return m_bLeveled;
	}

	//------------------------------------------------------------------------------------------------
	//! Authority only; call after the foundation entity is spawned.
	void ServerRegisterLevelFoundation(IEntity foundation)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return;

		m_pLevelFoundation = foundation;
	}

	//------------------------------------------------------------------------------------------------
	//! Authority only: applies transform on server, stores replicated pose, registers foundation.
	void ServerCompleteLeveling(vector worldMat[4], IEntity foundation)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return;

		if (m_bLeveled)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		owner.SetWorldTransform(worldMat);
		owner.Update();

		m_vLevelWorldOrigin = worldMat[3];
		m_fLevelYawDeg = worldMat[2].ToYaw();

		if (foundation)
			ServerRegisterLevelFoundation(foundation);

		m_bLeveled = true;
		if (Replication.IsRunning())
			Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyLeveledWorldTransformFromReplication(IEntity owner)
	{
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(m_fLevelYawDeg, 0, 0), mat);
		mat[3] = m_vLevelWorldOrigin;
		owner.SetWorldTransform(mat);
		owner.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Proxies, listen clients, and JIP: apply pose when replicated props arrive (authority usually skips local writes).
	void OnMortarLevelReplicated()
	{
		if (!m_bLeveled)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (!Replication.IsRunning() || Replication.IsClient())
			ApplyLeveledWorldTransformFromReplication(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_pLevelFoundation && (!Replication.IsRunning() || Replication.IsServer()))
		{
			IEntity foundation = m_pLevelFoundation;
			m_pLevelFoundation = null;
			if (foundation && foundation.GetWorld())
				SCR_EntityHelper.DeleteEntityAndChildren(foundation);
		}

		super.OnDelete(owner);
	}
}
