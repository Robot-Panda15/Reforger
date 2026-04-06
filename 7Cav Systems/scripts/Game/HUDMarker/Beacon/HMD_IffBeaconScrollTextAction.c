//------------------------------------------------------------------------------------------------
//! Hold interact + scroll (SCR_AdjustSignalAction) to cycle IFF text while beacon is OFF. No entity signal — state syncs via HMD_IffBeaconComponent.
class HMD_IffBeaconScrollTextAction : HMD_IffBeaconScrollActionBase
{
	//------------------------------------------------------------------------------------------------
	override protected float GetScrollNormalized01()
	{
		if (!m_pBeacon)
			return 0;
		return m_pBeacon.GetTextIndexNormalized01();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnScrollDirection(int dir)
	{
		m_pBeacon.TryCycleTextDirection(dir);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		HMD_IffBeaconComponent b = ResolveBeaconForName();
		if (!b)
		{
			outName = "IFF text (hold + scroll)";
			return true;
		}
		outName = string.Format("IFF text: %1 (hold + scroll)", b.GetPreviewTextCode());
		return true;
	}
}
