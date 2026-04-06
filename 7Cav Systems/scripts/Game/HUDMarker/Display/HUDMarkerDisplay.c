//------------------------------------------------------------------------------------------------
//! Base HUD display for world markers - widget pool only; marker logic in extension
class HUDMarkerDisplay : SCR_InfoDisplayExtended
{
	protected ref array<ImageWidget> m_MarkerDots = {};
	protected ref array<TextWidget> m_MarkerLabels = {};
	protected const int MARKER_POOL_SIZE = 32;

	//! Reused each frame for GetMarkerData to reduce allocations
	protected ref array<vector> m_HMDMarkerPositions = {};
	protected ref array<string> m_HMDMarkerNames = {};
	protected ref array<int> m_HMDMarkerDotColors = {};
	protected ref array<int> m_HMDMarkerLabelColors = {};
	protected ref array<float> m_HMDMarkerVisDist = {};
	protected ref array<int> m_HMDMarkerVisualKinds = {};

	//------------------------------------------------------------------------------------------------
	override void DisplayStartDraw(IEntity owner)
	{
		super.DisplayStartDraw(owner);
		PopulateMarkerPool();
	}

	//------------------------------------------------------------------------------------------------
	override protected void DisplayUpdate(IEntity owner, float timeSlice)
	{
		super.DisplayUpdate(owner, timeSlice);
		if (m_MarkerDots.IsEmpty() && m_wRoot)
			PopulateMarkerPool();
	}

	//------------------------------------------------------------------------------------------------
	protected void PopulateMarkerPool()
	{
		if (!m_wRoot || !m_MarkerDots.IsEmpty())
			return;
		HUDMarkerDisplayHelper.FillMarkerPoolFromRoot(m_wRoot, MARKER_POOL_SIZE, m_MarkerDots, m_MarkerLabels);
	}
}
