//------------------------------------------------------------------------------------------------
//! **Client-only** laser lock UI state (LSHIFT cycle, yellow highlight, rangefinder readout). This does **not**
//! by itself drive AGM114 guidance. Missiles use **server** fields on `HUDLaserMarkingComponent` (`m_bWeaponLaserLockActive`,
//! `m_vWeaponLaserLockWorld`) filled by `ClientSyncLockedWorldFromHud` -> `RpcAsk_SetWeaponLaserLockState`.
//! Independent of WCS `ALL_DESIGNATORS` / handheld designator components for the cycle list.
//! See `HUD_WEAPON_LOCK_AND_MISSILE.md` for the full pipeline and failure modes.
class HMD_LaserLockState
{
	protected static bool s_bLocked;
	protected static WCS_Armament_HandheldLaserDesignatorComponent s_LockedDesignator;
	//! When set (>= 1), lock uses HUDMarkerSystem pooled designation id (no WCS designator).
	protected static int s_iLockedHudDesignationId = -1;
	//! After own marking dot is unregistered, lock is kept by world position (designation id no longer valid).
	protected static bool s_bLockedHudWorldSnapshot;
	protected static vector s_vLockedHudWorldPos;
	protected static int s_iLockedHudSnapshotCode;
	//------------------------------------------------------------------------------------------------
	static bool IsLocked()
	{
		return s_bLocked;
	}

	//------------------------------------------------------------------------------------------------
	static WCS_Armament_HandheldLaserDesignatorComponent GetLockedDesignator()
	{
		return s_LockedDesignator;
	}

	//------------------------------------------------------------------------------------------------
	static void SetLockedDesignator(WCS_Armament_HandheldLaserDesignatorComponent designator)
	{
		s_LockedDesignator = designator;
		if (designator)
		{
			s_iLockedHudDesignationId = -1;
			s_bLockedHudWorldSnapshot = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	static void SetLocked(bool locked)
	{
		s_bLocked = locked;
		if (!locked)
		{
			s_LockedDesignator = null;
			s_iLockedHudDesignationId = -1;
			s_bLockedHudWorldSnapshot = false;
			HMD_RangefinderHUDState.ClearLockTargetReadout();
		}
	}

	//------------------------------------------------------------------------------------------------
	static int GetLockedHudDesignationId()
	{
		return s_iLockedHudDesignationId;
	}

	//------------------------------------------------------------------------------------------------
	//! Lock to a designation (e.g. cycle-in-view). Does not validate designation.
	static void ApplyManualLockToDesignator(WCS_Armament_HandheldLaserDesignatorComponent des)
	{
		if (!des)
			return;
		s_bLocked = true;
		s_LockedDesignator = des;
		s_iLockedHudDesignationId = -1;
		s_bLockedHudWorldSnapshot = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Lock to HUDMarkerSystem pooled designation (placed markers / gadget dots with no WCS ALL_DESIGNATORS entry).
	static void ApplyManualLockToHudDesignation(int hudDesignationId)
	{
		if (hudDesignationId < 1)
			return;
		s_bLocked = true;
		s_LockedDesignator = null;
		s_iLockedHudDesignationId = hudDesignationId;
		s_bLockedHudWorldSnapshot = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Before unregistering a pooled local designation: preserve lock if it referenced this id OR a WCS designator
	//! aimed at the same world point (vehicle laser often appears in WCS cycle before HUD pool id).
	//! \param fallbackLaserCode Used when label parse yields 0 (e.g. name missing in pool).
	static void MigrateHudLockBeforeUnregisterLocalDesignation(int designationId, vector designationWorldPos, string designationName, int fallbackLaserCode)
	{
		if (!s_bLocked || designationId < 1)
			return;

		//! Case A: lock stored as HUD pooled id (same as marking dot).
		if (s_iLockedHudDesignationId == designationId)
		{
			s_iLockedHudDesignationId = -1;
			s_bLockedHudWorldSnapshot = true;
			s_vLockedHudWorldPos = designationWorldPos;
			s_iLockedHudSnapshotCode = ParseHudLabelCode(designationName);
			if (s_iLockedHudSnapshotCode == 0 && fallbackLaserCode > 0)
				s_iLockedHudSnapshotCode = fallbackLaserCode;
			return;
		}

		//! Case B: lock is WCS designator (s_iLockedHudDesignationId == -1) but same aim point as this pool entry.
		if (s_LockedDesignator && s_LockedDesignator.HasValidDesignation())
		{
			vector lockW = s_LockedDesignator.GetDesignatedLocation();
			if (vector.Distance(lockW, designationWorldPos) <= HMD_MarkerVisuals.LOCK_MATCH_M)
			{
				int code = HMD_MarkerVisuals.GetDesignatorHudDisplayCode(s_LockedDesignator);
				if (code == 0)
					code = ParseHudLabelCode(designationName);
				if (code == 0 && fallbackLaserCode > 0)
					code = fallbackLaserCode;
				s_LockedDesignator = null;
				s_iLockedHudDesignationId = -1;
				s_bLockedHudWorldSnapshot = true;
				s_vLockedHudWorldPos = designationWorldPos;
				s_iLockedHudSnapshotCode = code;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ClearAll()
	{
		s_bLocked = false;
		s_LockedDesignator = null;
		s_iLockedHudDesignationId = -1;
		s_bLockedHudWorldSnapshot = false;
		HMD_RangefinderHUDState.ClearLockTargetReadout();
	}

	//------------------------------------------------------------------------------------------------
	//! When a designator component entity is destroyed, clear lock UI state that still references it.
	static void ClearIfLockedDesignator(WCS_Armament_HandheldLaserDesignatorComponent des)
	{
		if (!des)
			return;
		if (s_LockedDesignator == des)
			SetLocked(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Parse designation label (e.g. "1688") to laser code; non-numeric labels yield 0.
	protected static int ParseHudLabelCode(string nm)
	{
		if (!nm || nm.Length() == 0)
			return 0;
		int result = 0;
		int i;
		int len = nm.Length();
		for (i = 0; i < len; i++)
		{
			string ch = nm.Substring(i, 1);
			int d = -1;
			if (ch == "0") d = 0;
			else if (ch == "1") d = 1;
			else if (ch == "2") d = 2;
			else if (ch == "3") d = 3;
			else if (ch == "4") d = 4;
			else if (ch == "5") d = 5;
			else if (ch == "6") d = 6;
			else if (ch == "7") d = 7;
			else if (ch == "8") d = 8;
			else if (ch == "9") d = 9;
			else
				return 0;
			result = result * 10 + d;
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolve locked target world position: WCS designator, HUD snapshot (after own dot removed), or pooled id.
	static bool TryGetLockedTargetWorldPosition(out vector outPos)
	{
		outPos = vector.Zero;
		if (!s_bLocked)
			return false;
		if (s_LockedDesignator && s_LockedDesignator.HasValidDesignation())
		{
			outPos = s_LockedDesignator.GetDesignatedLocation();
			return true;
		}
		if (s_bLockedHudWorldSnapshot)
		{
			outPos = s_vLockedHudWorldPos;
			return true;
		}
		if (s_iLockedHudDesignationId >= 1)
		{
			ChimeraWorld world = GetGame().GetWorld();
			if (world)
			{
				HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
				if (sys && sys.TryGetDesignationWorldPositionById(s_iLockedHudDesignationId, outPos))
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: refresh bottom rangefinder readout for current lock (moving targets / camera).
	static void RefreshLockedTargetReadout()
	{
		if (!s_bLocked)
		{
			HMD_RangefinderHUDState.ClearLockTargetReadout();
			return;
		}
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		vector camTM[4];
		world.GetCurrentCamera(camTM);
		vector camPos = camTM[3];
		vector targetPos = vector.Zero;
		bool haveTarget = false;
		int code = 0;
		if (s_LockedDesignator && s_LockedDesignator.HasValidDesignation())
		{
			targetPos = s_LockedDesignator.GetDesignatedLocation();
			haveTarget = true;
			code = HMD_MarkerVisuals.GetDesignatorHudDisplayCode(s_LockedDesignator);
		}
		else if (s_bLockedHudWorldSnapshot)
		{
			targetPos = s_vLockedHudWorldPos;
			haveTarget = true;
			code = s_iLockedHudSnapshotCode;
		}
		else if (s_iLockedHudDesignationId >= 1)
		{
			HUDMarkerSystem sys = HUDMarkerSystem.GetInstance(world);
			if (sys && sys.TryGetDesignationWorldPositionById(s_iLockedHudDesignationId, targetPos))
			{
				haveTarget = true;
				string nm;
				if (sys.TryGetDesignationNameById(s_iLockedHudDesignationId, nm))
					code = ParseHudLabelCode(nm);
			}
		}
		if (!haveTarget)
			return;
		float rangeM = vector.Distance(camPos, targetPos);
		string gridStr = HMD_RangefinderGeo.FormatEightDigitGrid(targetPos);
		float bearingDeg = HMD_RangefinderGeo.BearingDegCameraToTarget(camPos, targetPos);
		HMD_RangefinderHUDState.SetLockTargetReadout(rangeM, gridStr, bearingDeg, code);
	}

	//------------------------------------------------------------------------------------------------
	//! After HUDMarkerSystem.GetMarkerData: tint locked designation label (dot uses Lock_Element.edds in HUDMarkerDisplayHelper).
	static void ApplyLockedHighlightToMarkerArrays(array<vector> positions, array<int> labelColors)
	{
		if (!s_bLocked || !positions || !labelColors)
			return;
		vector lockPos = vector.Zero;
		if (!TryGetLockedTargetWorldPosition(lockPos))
			return;
		int yellowLbl = Color.FromRGBA(255, 255, 200, 255).PackToInt();
		float matchM = HMD_MarkerVisuals.LOCK_MATCH_M;
		int n = positions.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			if (vector.Distance(positions[i], lockPos) > matchM)
				continue;
			if (labelColors && i < labelColors.Count())
				labelColors[i] = yellowLbl;
		}
	}
}
