//------------------------------------------------------------------------------------------------
//! Read-only style: shows remaining marker battery vs 30 min total.
class HMD_IffBeaconBatteryReadoutAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return HMD_IffBeaconComponent.FindOnEntity(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		HMD_IffBeaconComponent b = HMD_IffBeaconComponent.FindOnEntity(GetOwner());
		if (!b)
		{
			outName = "IFF beacon battery";
			return true;
		}
		int mins = (int)(b.GetBatteryMinutesRemaining() + 0.5);
		if (mins < 0)
			mins = 0;
		outName = string.Format("IFF marker time remaining: %1 min", mins);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}
}
