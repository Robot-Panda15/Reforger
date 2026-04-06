//------------------------------------------------------------------------------------------------
//! Shared hold-interact + scroll (SCR_AdjustSignalAction) while beacon is OFF; subclasses supply number vs text cycling.
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
	override protected void HandleAction(float value)
	{
		if (value == 0)
			return;
		int dir = 1;
		if (value < 0)
			dir = -1;
		if (m_pBeacon)
		{
			OnScrollDirection(dir);
			m_fTargetValue = GetScrollNormalized01();
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
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected HMD_IffBeaconComponent ResolveBeaconForName()
	{
		if (m_pBeacon)
			return m_pBeacon;
		return HMD_IffBeaconComponent.FindOnEntity(GetOwner());
	}
}
