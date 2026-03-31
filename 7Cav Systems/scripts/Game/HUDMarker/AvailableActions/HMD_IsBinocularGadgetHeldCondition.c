//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomStringTitleField("Binoculars zoomed (in use)")]
//! Control-hint condition: only for gadgets with HMD_LaserDesignatorGadgetComponent, while zoomed or ADS (see HMD_HandheldOpticZoom).
class HMD_IsBinocularGadgetHeldCondition : SCR_AvailableActionCondition
{
	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		if (!data)
			return false;
		return GetReturnResult(HMD_HandheldOpticZoom.IsZoomedForHMD());
	}
}
