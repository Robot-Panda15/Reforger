//------------------------------------------------------------------------------------------------
class HMD_IffBeaconTurnOffAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		HMD_IffBeaconComponent b = HMD_IffBeaconComponent.FindOnEntity(GetOwner());
		return b && b.IsBeaconActive();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		HMD_IffBeaconComponent b = HMD_IffBeaconComponent.FindOnEntity(pOwnerEntity);
		if (b)
			b.TrySetBeaconActive(false);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Turn IFF beacon OFF";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return true;
	}
}
