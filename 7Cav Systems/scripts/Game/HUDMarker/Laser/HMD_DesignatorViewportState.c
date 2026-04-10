//------------------------------------------------------------------------------------------------
//! Set by HMD_LaserDesignatorGadgetComponent while local player holds the designator and uses its zoom/viewport with valid overlay eligibility.
//! HUDMarkerVisibility + HMD_HudMarkerPolicyResolver read this to show all IFF and designation dots within the configured range.
class HMD_DesignatorViewportState
{
	protected static bool s_bActive;
	protected static float s_fMarkerMaxDistanceM;

	//------------------------------------------------------------------------------------------------
	static void Activate(float markerMaxDistanceM)
	{
		s_bActive = true;
		s_fMarkerMaxDistanceM = markerMaxDistanceM;
		if (s_fMarkerMaxDistanceM < 0)
			s_fMarkerMaxDistanceM = 0;
	}

	//------------------------------------------------------------------------------------------------
	static void Deactivate()
	{
		s_bActive = false;
		s_fMarkerMaxDistanceM = 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsActive()
	{
		return s_bActive;
	}

	//------------------------------------------------------------------------------------------------
	static float GetMarkerMaxDistanceM()
	{
		return s_fMarkerMaxDistanceM;
	}
}
