//------------------------------------------------------------------------------------------------
//! Turret slew toward a world point and collect laser designations in the current camera view.
class HMD_VehicleLaserAimHelpers
{
	//! Half-angle (degrees) from camera forward: designations within this cone count as "in view".
	static const float DEFAULT_CAMERA_HALF_ANGLE_DEG = 60.0;
	//! Lock target cycle (LSHIFT): max angle from camera forward to designation (same cone test as CollectDesignationIdsInCameraCone).
	static const float LOCK_CYCLE_CAMERA_HALF_ANGLE_DEG = 5.0;

	protected static const float SLEW_INTERVAL_MS = 20.0;
	protected static const float SLEW_LERP_FRAC = 0.24;
	protected static const float SLEW_STOP_ANGLE_DEG = 0.01;
	protected static const int SLEW_MAX_TICKS = 300;

	protected static bool s_SlewActive;
	protected static TurretControllerComponent s_SlewTc;
	protected static WCS_Armament_VehicleWeaponStationComponent s_SlewWs;
	protected static WCS_Armament_HandheldLaserDesignatorComponent s_SlewDes;
	protected static int s_SlewTicks;

	//------------------------------------------------------------------------------------------------
	static bool TryResolveTurretControllerFromLocalGunner(IEntity localChar, out TurretControllerComponent outTC)
	{
		outTC = null;
		if (!localChar)
			return false;
		SCR_CompartmentAccessComponent cac = SCR_CompartmentAccessComponent.Cast(localChar.FindComponent(SCR_CompartmentAccessComponent));
		if (!cac)
			return false;
		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot)
			return false;
		ExtBaseCompartmentSlot ext = slot;
		if (ext)
		{
			outTC = ext.GetAttachedTurret();
			if (!outTC)
			{
				BaseControllerComponent bc = ext.GetController();
				outTC = TurretControllerComponent.Cast(bc);
			}
		}
		else
		{
			outTC = slot.GetAttachedTurret();
		}
		return outTC != null;
	}

	//------------------------------------------------------------------------------------------------
	protected static float WrapAnglePi(float a)
	{
		if (a > 3.14159265)
			a -= 6.2831853;
		if (a < -3.14159265)
			a += 6.2831853;
		if (a > 3.14159265)
			a -= 6.2831853;
		if (a < -3.14159265)
			a += 6.2831853;
		return a;
	}

	//------------------------------------------------------------------------------------------------
	//! Delta from current aim to target in turret-owner local axes (better than world XZ + asin Y alone).
	protected static void ComputeAimDeltaInTurretFrame(TurretComponent tur, WCS_Armament_VehicleWeaponStationComponent ws, vector targetWorld, out float outDYawRad, out float outDPitchRad, out vector outWantDir, out vector outCurDir)
	{
		outDYawRad = 0;
		outDPitchRad = 0;
		outWantDir = vector.Zero;
		outCurDir = vector.Zero;
		IEntity turOwner = tur.GetOwner();
		if (!turOwner)
			return;
		vector origin = turOwner.GetOrigin();
		if (ws)
			origin = ws.GetAimOrigin();
		vector wantDir = targetWorld - origin;
		float dist = wantDir.Length();
		if (dist < 0.5)
			return;
		wantDir = wantDir * (1.0 / dist);
		vector curDir = tur.GetAimingDirectionWorld();
		float clen = curDir.Length();
		if (clen < 0.001)
			return;
		curDir = curDir * (1.0 / clen);
		vector m[4];
		turOwner.GetWorldTransform(m);
		float lxW = vector.Dot(m[0], wantDir);
		float lyW = vector.Dot(m[1], wantDir);
		float lzW = vector.Dot(m[2], wantDir);
		float cx = vector.Dot(m[0], curDir);
		float cy = vector.Dot(m[1], curDir);
		float cz = vector.Dot(m[2], curDir);
		outDYawRad = WrapAnglePi(Math.Atan2(lxW, lzW) - Math.Atan2(cx, cz));
		outDPitchRad = Math.Asin(Math.Clamp(lyW, -1.0, 1.0)) - Math.Asin(Math.Clamp(cy, -1.0, 1.0));
		outWantDir = wantDir;
		outCurDir = curDir;
	}

	//------------------------------------------------------------------------------------------------
	//! During smooth slew: wanted + angles only (no SetAimingRotation snap).
	protected static void ApplySlewSmooth(TurretControllerComponent tc, float yawRad, float pitchRad, float yawDeg, float pitchDeg)
	{
		if (!tc)
			return;
		if (tc.GetCanAimOnlyInADS() && !tc.IsWeaponADS())
			tc.SetWeaponADS(true);
		TurretComponent tur = tc.GetTurretComponent();
		if (tur)
		{
			vector wantedDeg = Vector(yawDeg, pitchDeg, 0);
			tur.SetAimingRotationWanted(wantedDeg);
		}
		tc.SetAimingAngles(yawRad, pitchRad);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SlewTickOne()
	{
		if (!s_SlewActive || !s_SlewTc)
		{
			s_SlewActive = false;
			return;
		}
		TurretComponent tur = s_SlewTc.GetTurretComponent();
		if (!tur)
		{
			s_SlewActive = false;
			return;
		}
		vector targetWorld = vector.Zero;
		WCS_Armament_HandheldLaserDesignatorComponent des = s_SlewDes;
		if (des && des.IsDesignating() && des.HasValidDesignation())
			targetWorld = des.GetDesignatedLocation();
		else if (!HMD_LaserLockState.TryGetLockedTargetWorldPosition(targetWorld))
		{
			s_SlewActive = false;
			s_SlewDes = null;
			s_SlewTc = null;
			s_SlewWs = null;
			return;
		}
		float dYaw;
		float dPitch;
		vector wantDir;
		vector curDir;
		ComputeAimDeltaInTurretFrame(tur, s_SlewWs, targetWorld, dYaw, dPitch, wantDir, curDir);
		vector curDeg = tur.GetAimingRotation();
		float targetNy = curDeg[0] + dYaw * Math.RAD2DEG;
		float targetNp = curDeg[1] + dPitch * Math.RAD2DEG;
		float newNy = curDeg[0] + (targetNy - curDeg[0]) * SLEW_LERP_FRAC;
		float newNp = curDeg[1] + (targetNp - curDeg[1]) * SLEW_LERP_FRAC;
		float yawRad = newNy / Math.RAD2DEG;
		float pitchRad = newNp / Math.RAD2DEG;
		ApplySlewSmooth(s_SlewTc, yawRad, pitchRad, newNy, newNp);
		vector adPost = tur.GetAimingDirectionWorld();
		float alenPost = adPost.Length();
		if (alenPost > 0.001)
			adPost = adPost * (1.0 / alenPost);
		float dotPost = Math.Clamp(vector.Dot(adPost, wantDir), -1.0, 1.0);
		float angDeg = Math.Acos(dotPost) * Math.RAD2DEG;
		s_SlewTicks++;
		bool done = false;
		if (angDeg <= SLEW_STOP_ANGLE_DEG)
			done = true;
		if (s_SlewTicks >= SLEW_MAX_TICKS)
			done = true;
		if (done)
		{
			bool maxOut = false;
			if (s_SlewTicks >= SLEW_MAX_TICKS)
				maxOut = true;
			if (maxOut)
			{
				ComputeAimDeltaInTurretFrame(tur, s_SlewWs, targetWorld, dYaw, dPitch, wantDir, curDir);
				curDeg = tur.GetAimingRotation();
				targetNy = curDeg[0] + dYaw * Math.RAD2DEG;
				targetNp = curDeg[1] + dPitch * Math.RAD2DEG;
				yawRad = targetNy / Math.RAD2DEG;
				pitchRad = targetNp / Math.RAD2DEG;
				ApplySlewSmooth(s_SlewTc, yawRad, pitchRad, targetNy, targetNp);
			}
			s_SlewActive = false;
			s_SlewDes = null;
			s_SlewTc = null;
			s_SlewWs = null;
			return;
		}
		GetGame().GetCallqueue().CallLater(SlewTickOne, SLEW_INTERVAL_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Smooth interpolated slew toward the locked designation (re-reads spot each tick).
	static bool TryBeginSlewToLockedDesignation(IEntity localChar, WCS_Armament_HandheldLaserDesignatorComponent des)
	{
		if (!localChar || !des || !des.HasValidDesignation())
			return false;
		TurretControllerComponent tc;
		if (!TryResolveTurretControllerFromLocalGunner(localChar, tc))
			return false;
		TurretComponent tur = tc.GetTurretComponent();
		if (!tur)
			return false;
		s_SlewActive = true;
		s_SlewTc = tc;
		s_SlewWs = HMD_VehicleHUDLaserHelpers.FindVehicleWeaponStationForLocalGunner();
		s_SlewDes = des;
		s_SlewTicks = 0;
		GetGame().GetCallqueue().CallLater(SlewTickOne, 0, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Slew when lock is HUD id / snapshot only (no WCS designator ref).
	static bool TryBeginSlewToLockedTargetFromLockState(IEntity localChar)
	{
		vector w = vector.Zero;
		if (!localChar || !HMD_LaserLockState.TryGetLockedTargetWorldPosition(w))
			return false;
		TurretControllerComponent tc;
		if (!TryResolveTurretControllerFromLocalGunner(localChar, tc))
			return false;
		TurretComponent tur = tc.GetTurretComponent();
		if (!tur)
			return false;
		s_SlewActive = true;
		s_SlewTc = tc;
		s_SlewWs = HMD_VehicleHUDLaserHelpers.FindVehicleWeaponStationForLocalGunner();
		s_SlewDes = null;
		s_SlewTicks = 0;
		GetGame().GetCallqueue().CallLater(SlewTickOne, 0, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills outList with active designations whose spot lies within maxHalfAngleDeg of current camera forward (sorted by angle).
	static void BuildActiveDesignationsInCameraView(float maxHalfAngleDeg, array<WCS_Armament_HandheldLaserDesignatorComponent> outList)
	{
		if (!outList)
			return;
		outList.Clear();
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		vector camTM[4];
		world.GetCurrentCamera(camTM);
		vector camPos = camTM[3];
		vector forward = camTM[2];
		float flen = forward.Length();
		if (flen < 0.001)
			return;
		forward = forward * (1.0 / flen);
		ref array<WCS_Armament_HandheldLaserDesignatorComponent> tmp = {};
		ref array<float> tmpAngles = {};
		int n = WCS_Armament_HandheldLaserDesignatorComponent.ALL_DESIGNATORS.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			WCS_Armament_HandheldLaserDesignatorComponent d = WCS_Armament_HandheldLaserDesignatorComponent.ALL_DESIGNATORS[i];
			if (!d || !d.IsDesignating() || !d.HasValidDesignation())
				continue;
			vector pos = d.GetDesignatedLocation();
			vector toT = pos - camPos;
			float dlen = toT.Length();
			if (dlen < 0.01)
				continue;
			toT = toT * (1.0 / dlen);
			float dot = vector.Dot(forward, toT);
			dot = Math.Clamp(dot, -1.0, 1.0);
			float angDeg = Math.Acos(dot) * Math.RAD2DEG;
			if (angDeg > maxHalfAngleDeg)
				continue;
			tmp.Insert(d);
			tmpAngles.Insert(angDeg);
		}
		int cnt = tmp.Count();
		int a;
		int b;
		for (a = 0; a < cnt; a++)
		{
			for (b = a + 1; b < cnt; b++)
			{
				if (tmpAngles[a] > tmpAngles[b])
				{
					float tf = tmpAngles[a];
					tmpAngles[a] = tmpAngles[b];
					tmpAngles[b] = tf;
					WCS_Armament_HandheldLaserDesignatorComponent td = tmp[a];
					tmp[a] = tmp[b];
					tmp[b] = td;
				}
			}
		}
		for (i = 0; i < cnt; i++)
			outList.Insert(tmp[i]);
	}
}
