//------------------------------------------------------------------------------------------------
class HMD_LocalHudMarkerSession
{
	//------------------------------------------------------------------------------------------------
	static void ClearClientSessionState()
	{
		HUDMarkerVisibility.Reset();
		HMD_LaserLockState.ClearAll();
		HMD_HUDLaserInput.ResetLockCycleIndex();
		HMD_RangefinderHUDState.Clear();
	}
}
