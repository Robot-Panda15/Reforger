//------------------------------------------------------------------------------------------------
enum E_HMD_VehicleHudLaserSeatPolicy
{
	MARKING,
	VISIBILITY,
	MARKING_OR_VISIBILITY
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomStringTitleField("Vehicle HUD laser seat policy")]
//! Control-hint condition: same rules as HMD_VehicleHUDLaserInputPolicy for marking / visibility seats.
class HMD_VehicleHudLaserSeatCondition : SCR_AvailableActionCondition
{
	[Attribute("0", UIWidgets.ComboBox, "Which vehicle HUD seat policy", "", ParamEnumArray.FromEnum(E_HMD_VehicleHudLaserSeatPolicy), category: "HMD")]
	protected E_HMD_VehicleHudLaserSeatPolicy m_ePolicy;

	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		if (!data)
			return false;
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return GetReturnResult(false);
		switch (m_ePolicy)
		{
			case E_HMD_VehicleHudLaserSeatPolicy.MARKING:
				return GetReturnResult(HMD_VehicleHUDLaserInputPolicy.MayUseVehicleHUDLaserMarking(localChar));
			case E_HMD_VehicleHudLaserSeatPolicy.VISIBILITY:
				return GetReturnResult(HMD_VehicleHUDLaserInputPolicy.MayUseVehicleHUDLaserVisibility(localChar));
			case E_HMD_VehicleHudLaserSeatPolicy.MARKING_OR_VISIBILITY:
			{
				bool vis = HMD_VehicleHUDLaserInputPolicy.MayUseVehicleHUDLaserVisibility(localChar);
				bool mark = HMD_VehicleHUDLaserInputPolicy.MayUseVehicleHUDLaserMarking(localChar);
				return GetReturnResult(vis || mark);
			}
		}
		return GetReturnResult(false);
	}
}
