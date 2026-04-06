//------------------------------------------------------------------------------------------------
class HMD_IffBeaconTurnOnAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		HMD_IffBeaconComponent b = HMD_IffBeaconComponent.FindOnEntity(GetOwner());
		return b && !b.IsBeaconActive() && b.GetBatteryFraction01() > 0;
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
			b.TrySetBeaconActive(true);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		HMD_IffBeaconComponent b = HMD_IffBeaconComponent.FindOnEntity(GetOwner());
		if (!b)
		{
			outName = "Turn IFF beacon ON";
			return true;
		}
		outName = string.Format("Turn IFF beacon ON (%1)", b.GetPreviewLabel());
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
