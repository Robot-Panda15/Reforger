//------------------------------------------------------------------------------------------------
enum E_HMD_VehicleHudLaserSeatPolicy
{
	MARKING,
	VISIBILITY,
	MARKING_OR_VISIBILITY
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomStringTitleField("Vehicle HUD laser seat policy")]
//! Control-hint condition: same rules as HMD_HudMarkerEligibility (seat + global eligibility) for marking / visibility seats.
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
				return GetReturnResult(HMD_HudMarkerEligibility.MayUseVehicleHUDLaserMarking(localChar));
			case E_HMD_VehicleHudLaserSeatPolicy.VISIBILITY:
				return GetReturnResult(HMD_HudMarkerEligibility.MayUseVehicleHUDLaserVisibility(localChar));
			case E_HMD_VehicleHudLaserSeatPolicy.MARKING_OR_VISIBILITY:
			{
				bool vis = HMD_HudMarkerEligibility.MayUseVehicleHUDLaserVisibility(localChar);
				bool mark = HMD_HudMarkerEligibility.MayUseVehicleHUDLaserMarking(localChar);
				return GetReturnResult(vis || mark);
			}
		}
		return GetReturnResult(false);
	}
}
