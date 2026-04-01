//------------------------------------------------------------------------------------------------
class SCR_MortarLevelStateComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Replicated flag: this mortar has already been leveled (hides Level mortar action for all clients).
//! Server-only reference to the spawned foundation mesh (removed when the mortar is deleted).
class SCR_MortarLevelStateComponent : ScriptComponent
{
	[RplProp()]
	protected bool m_bLeveled;

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
		if (!Replication.IsServer())
			return;

		m_pLevelFoundation = foundation;
	}

	//------------------------------------------------------------------------------------------------
	//! Authority only; call after leveling completes successfully.
	void ServerSetMortarLeveled()
	{
		if (!Replication.IsServer())
			return;

		if (m_bLeveled)
			return;

		m_bLeveled = true;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer() && m_pLevelFoundation)
		{
			IEntity foundation = m_pLevelFoundation;
			m_pLevelFoundation = null;
			if (foundation && foundation.GetWorld())
				SCR_EntityHelper.DeleteEntityAndChildren(foundation);
		}

		super.OnDelete(owner);
	}
}
