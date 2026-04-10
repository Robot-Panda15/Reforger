//------------------------------------------------------------------------------------------------
//! Hold interact + scroll (SCR_AdjustSignalAction) to cycle IFF number while beacon is OFF. No entity signal — state syncs via HMD_IffBeaconComponent.
class HMD_IffBeaconScrollNumberAction : HMD_IffBeaconScrollActionBase
{
	//------------------------------------------------------------------------------------------------
	override protected float GetScrollNormalized01()
	{
		if (!m_pBeacon)
			return 0;
		return m_pBeacon.GetNumberNormalized01();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnScrollDirection(int dir)
	{
		m_pBeacon.TryCycleNumberDirection(dir);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		HMD_IffBeaconComponent b = ResolveBeaconForName();
		if (!b)
		{
			outName = "IFF number (hold + scroll)";
			return true;
		}
		outName = string.Format("IFF number: %1 (hold + scroll)", b.GetPreviewNumberString());
		return true;
	}
}
