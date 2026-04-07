//------------------------------------------------------------------------------------------------
//! Shared hold-interact + scroll (SCR_AdjustSignalAction) while beacon is OFF; subclasses supply number vs text cycling.
//! Server applies IFF IDs from replicated action data (OnLoadActionData); optimistic m_fTargetValue must match the next discrete step so the signal lerp is not stale.
class HMD_IffBeaconScrollActionBase : SCR_AdjustSignalAction
{
	protected HMD_IffBeaconComponent m_pBeacon;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);
		m_pBeacon = HMD_IffBeaconComponent.FindOnEntity(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;
		return m_pBeacon && m_pBeacon.CanConfigure();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override protected float SCR_GetMinimumValue()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	override protected float SCR_GetMaximumValue()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override protected float SCR_GetCurrentValue()
	{
		if (!m_pBeacon)
			return 0;
		return GetScrollNormalized01();
	}

	//------------------------------------------------------------------------------------------------
	protected float GetScrollNormalized01()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetScrollNormalized01AfterStep(int dir)
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	override protected void HandleAction(float value)
	{
		if (value == 0)
			return;
		int dir = 1;
		if (value < 0)
			dir = -1;
		if (m_pBeacon)
		{
			float nextT = GetScrollNormalized01AfterStep(dir);
			OnScrollDirection(dir);
			m_fTargetValue = nextT;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnScrollDirection(int dir)
	{
	}

	//------------------------------------------------------------------------------------------------
	override protected bool OnSaveActionData(ScriptBitWriter writer)
	{
		float lerp = Math.Lerp(GetMinimumValue(), GetMaximumValue(), m_fTargetValue);
		writer.WriteFloat01(lerp);
		PlayMovementAndStopSound(lerp);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool OnLoadActionData(ScriptBitReader reader)
	{
		if (m_bIsAdjustedByPlayer)
			return true;

		float lerp;
		reader.ReadFloat01(lerp);
		m_fTargetValue = Math.InverseLerp(GetMinimumValue(), GetMaximumValue(), lerp);
		PlayMovementAndStopSound(lerp);
		if (Replication.IsRunning() && Replication.IsServer())
			ApplyServerScrollFromNormalized(m_fTargetValue);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyServerScrollFromNormalized(float normalized01)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected HMD_IffBeaconComponent ResolveBeaconForName()
	{
		if (m_pBeacon)
			return m_pBeacon;
		return HMD_IffBeaconComponent.FindOnEntity(GetOwner());
	}
}
