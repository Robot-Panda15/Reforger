//------------------------------------------------------------------------------------------------
//! True when local player passes the same HK commander seat gate as CAV_M2A3_HunterKillerComponent.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class CAV_HunterKillerHintCondition : SCR_AvailableActionCondition
{
	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		if (!data)
			return false;

		return GetReturnResult(CAV_HunterKillerCommanderUtil.CommanderGateFailReason() == 0);
	}
}
