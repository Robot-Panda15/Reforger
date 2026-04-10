//------------------------------------------------------------------------------------------------
//! Bridges WCS missile / AGM114R seekers to vehicle laser data.
//!
//! **Not** the same as `HMD_LaserLockState` (client-only LSHIFT / yellow HUD highlight). The bridge runs on
//! **server** and reads `HUDLaserMarkingComponent.m_bWeaponLaserLockActive` + `m_vWeaponLaserLockWorld`, which are
//! set only when the **owning client** runs `ClientSyncLockedWorldFromHud` and `RpcAsk_SetWeaponLaserLockState`
//! fires (see `HUD_WEAPON_LOCK_AND_MISSILE.md`). If the HUD shows lock but these fields are zero on server,
//! the pipeline from LaserLockState -> ClientSync -> RPC never applied (wrong replication gate, no comp, etc.).

class HMD_WcsLaserVehicleDesignatorBridge

{

	//------------------------------------------------------------------------------------------------

	//! No aim geometry. Skips only self-lase when firerCharacterHint is set (same vehicle + same character).

	protected static WCS_Armament_HandheldLaserDesignatorComponent ResolveFallbackOffVehicleDesignator(IEntity weaponStationOwner, IEntity firerCharacterHint)

	{

		if (!weaponStationOwner)

			return null;

		IEntity firerRoot = weaponStationOwner.GetRootParent();

		if (!firerRoot)

			firerRoot = weaponStationOwner;

		int n = WCS_Armament_HandheldLaserDesignatorComponent.ALL_DESIGNATORS.Count();

		for (int i = 0; i < n; i++)

		{

			WCS_Armament_HandheldLaserDesignatorComponent d = WCS_Armament_HandheldLaserDesignatorComponent.ALL_DESIGNATORS[i];

			if (!d || !d.IsDesignating() || !d.HasValidDesignation())

				continue;

			IEntity desOwner = d.GetOwner();

			if (desOwner)

			{

				IEntity desRoot = desOwner.GetRootParent();

				if (!desRoot)

					desRoot = desOwner;

				if (firerCharacterHint && desRoot == firerRoot)

				{

					IEntity desChar = HMD_VehicleHUDLaserHelpers.ResolveChimeraCharacterEntity(desOwner);

					if (desChar && desChar == firerCharacterHint)

						continue;

				}

			}

			return d;

		}

		return null;

	}



	//------------------------------------------------------------------------------------------------

	protected static WCS_Armament_HandheldLaserDesignatorComponent ResolveForMissileLaunchInternal(IEntity weaponStationOwner)

	{

		if (!weaponStationOwner)

			return null;

		IEntity root = weaponStationOwner.GetRootParent();

		if (!root)

			root = weaponStationOwner;

		GenericComponent g = HMD_VehicleHUDLaserHelpers.FindComponentInHierarchy(root, WCS_Armament_WeaponStationComponent);

		WCS_Armament_WeaponStationComponent ws = WCS_Armament_WeaponStationComponent.Cast(g);

		if (!ws)

		{

			WCS_Armament_HandheldLaserDesignatorComponent fb = ResolveFallbackOffVehicleDesignator(weaponStationOwner, null);

			return fb;

		}

		WCS_Armament_HandheldLaserDesignatorComponent des = null;

		string fail = "";

		if (!TryLockBestDesignatorInAim(ws, des, fail, null))

			return null;

		return des;

	}



	//------------------------------------------------------------------------------------------------

	//! Server: best designator for this weapon station (aim cone + range; no gunner code).

	static WCS_Armament_HandheldLaserDesignatorComponent ResolveForMissileLaunch(IEntity weaponStationOwner)

	{

		return ResolveForMissileLaunchInternal(weaponStationOwner);

	}



	//------------------------------------------------------------------------------------------------

	//! World point for AGM114R seeker: HUD weapon-lock world first; then replicated self-lase on this vehicle; else aim-cone resolve via TryLockBestDesignatorInAim (self-lase first, then FOV only if HUD lock active).

	static bool TryGetHmdLaserTargetWorld(IEntity weaponStationOwner, out vector outWorld)

	{

		outWorld = vector.Zero;

		if (!weaponStationOwner)

			return false;


		IEntity root = weaponStationOwner.GetRootParent();

		if (!root)

			root = weaponStationOwner;

		ref array<HUDLaserMarkingComponent> marks = {};

		HMD_VehicleHUDLaserHelpers.CollectHudLaserMarkingComponentsInHierarchy(root, marks);

		int mi;

		for (mi = 0; mi < marks.Count(); mi++)

		{

			HUDLaserMarkingComponent lm = marks[mi];

			if (!lm || !lm.IsWeaponLaserLockActive())

				continue;

			vector lw = lm.GetWeaponLaserLockWorld();

			if (vector.DistanceSq(lw, vector.Zero) > 0.0001)

			{

				outWorld = lw;

				return true;

			}

		}

		//! Self-lase first: valid replicated marking hit on this vehicle before FOV scan of other lasers.
		for (mi = 0; mi < marks.Count(); mi++)

		{

			HUDLaserMarkingComponent lmSelf = marks[mi];

			if (!lmSelf || !lmSelf.GetReplicatedLaserHitValid())

				continue;

			vector ml = lmSelf.GetDesignatedLocation();

			if (vector.DistanceSq(ml, vector.Zero) <= 0.0001)

				continue;

			outWorld = ml;

			return true;

		}

		//! No HUD weapon lock and no self-lase: only other designators in weapon aim cone (TryLockBestDesignatorInAim).
		WCS_Armament_HandheldLaserDesignatorComponent d = ResolveForMissileLaunchInternal(weaponStationOwner);

		if (!d)

			return false;

		outWorld = d.GetDesignatedLocation();

		return true;

	}



	//------------------------------------------------------------------------------------------------

	//! Replicated self-lase on firer vehicle first; else closest designation in aim cone only if any HUD weapon lock is active on this vehicle (no off-board FOV without lock). Skips same-vehicle self when firerCharacterHint matches. Half-angle 90 deg; range 5000 m.

	static bool TryLockBestDesignatorInAim(WCS_Armament_WeaponStationComponent ws, out WCS_Armament_HandheldLaserDesignatorComponent outDes, out string outFailReason, IEntity firerCharacterHint)

	{

		outDes = null;

		outFailReason = "";

		if (!ws)

		{

			outFailReason = "no_ws";

			return false;

		}

		IEntity stationOwner = ws.GetOwner();

		if (!stationOwner)

		{

			outFailReason = "no_station_owner";

			return false;

		}

		IEntity firerRoot = stationOwner.GetRootParent();

		if (!firerRoot)

			firerRoot = stationOwner;

		ref array<HUDLaserMarkingComponent> firerMarks = {};

		HMD_VehicleHUDLaserHelpers.CollectHudLaserMarkingComponentsInHierarchy(firerRoot, firerMarks);

		int smi;

		for (smi = 0; smi < firerMarks.Count(); smi++)

		{

			HUDLaserMarkingComponent lmSelf = firerMarks[smi];

			if (!lmSelf || !lmSelf.GetReplicatedLaserHitValid())

				continue;

			vector sPos = lmSelf.GetDesignatedLocation();

			if (vector.DistanceSq(sPos, vector.Zero) <= 0.0001)

				continue;

			outDes = lmSelf;

			outFailReason = "";

			return true;

		}

		bool bHudLockForCone = false;

		for (smi = 0; smi < firerMarks.Count(); smi++)

		{

			HUDLaserMarkingComponent lmL = firerMarks[smi];

			if (lmL && lmL.IsWeaponLaserLockActive())

			{

				bHudLockForCone = true;

				break;

			}

		}

		if (!bHudLockForCone)

		{

			outFailReason = "no_hud_lock_for_cone";

			return false;

		}

		float m_fStage1MaxRange = 5000.0;

		float m_fLockFOVLimit = 90.0;

		vector aimOrigin = ws.GetAimOrigin();

		vector aimDirection = ws.GetAimDirection();

		float closestAngle = m_fLockFOVLimit;

		WCS_Armament_HandheldLaserDesignatorComponent closestDesignator = null;

		int n = WCS_Armament_HandheldLaserDesignatorComponent.ALL_DESIGNATORS.Count();

		int cntInactive;

		int cntSameSelf;

		int cntRange;

		int cntAngleGE;

		cntInactive = 0;

		cntSameSelf = 0;

		cntRange = 0;

		cntAngleGE = 0;

		for (int i = 0; i < n; i++)

		{

			WCS_Armament_HandheldLaserDesignatorComponent designator = WCS_Armament_HandheldLaserDesignatorComponent.ALL_DESIGNATORS[i];

			if (!designator || !designator.IsDesignating() || !designator.HasValidDesignation())

			{

				cntInactive++;

				continue;

			}

			IEntity desOwner = designator.GetOwner();

			if (desOwner)

			{

				IEntity desRoot = desOwner.GetRootParent();

				if (!desRoot)

					desRoot = desOwner;

				if (desRoot == firerRoot)

				{

					if (firerCharacterHint)

					{

						IEntity desChar = HMD_VehicleHUDLaserHelpers.ResolveChimeraCharacterEntity(desOwner);

						if (desChar && desChar == firerCharacterHint)

						{

							cntSameSelf++;

							continue;

						}

					}

				}

			}

			vector laserPos = designator.GetDesignatedLocation();

			float dist = vector.Distance(aimOrigin, laserPos);

			if (dist > m_fStage1MaxRange)

			{

				cntRange++;

				continue;

			}

			vector dirToLaser = (laserPos - aimOrigin).Normalized();

			float dotProduct = vector.Dot(aimDirection, dirToLaser);

			if (dotProduct < -1.0)

				dotProduct = -1.0;

			if (dotProduct > 1.0)

				dotProduct = 1.0;

			float angleToLaser = Math.Acos(dotProduct) * Math.RAD2DEG;

			if (angleToLaser >= m_fLockFOVLimit)

			{

				cntAngleGE++;

				continue;

			}

			if (angleToLaser < closestAngle)

			{

				closestAngle = angleToLaser;

				closestDesignator = designator;

			}

		}

		if (!closestDesignator)

		{

			outFailReason = string.Format("no_match:total=%1|inactive=%2|sameSelf=%3|range=%4|angleGE=%5|", n, cntInactive, cntSameSelf, cntRange, cntAngleGE);
			outFailReason += string.Format("maxM=%1|maxDeg=%2", m_fStage1MaxRange, m_fLockFOVLimit);

			return false;

		}

		outDes = closestDesignator;

		return true;

	}

}


