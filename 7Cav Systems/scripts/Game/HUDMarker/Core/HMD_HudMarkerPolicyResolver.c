//------------------------------------------------------------------------------------------------
//! Resolves HMD_HudMarkerEligibility* components for the current view (binocular / vehicle hull).
//! Own vs foreign laser designation visibility: hull HMD_HudMarkerEligibilityVehicleComponent. Turret marking does not drive world HUD policy.
class HMD_HudMarkerPolicyResolver
{
	protected static const float DEFAULT_MAX_VIEW_M = 0;
	protected static const float DEFAULT_UPDATE_HZ = 0;

	//------------------------------------------------------------------------------------------------
	protected static IEntity GetLocalVehicleRoot()
	{
		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return null;
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(localChar);
		if (!ch || !ch.IsInVehicle())
			return null;
		IEntity root = localChar.GetRootParent();
		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! When requireHelmet is false: always apply. When true: apply only if HMD helmet rules allow (or global helmet policy is off).
	protected static bool CanApplyHmdHelmetGatedVehiclePolicy(ChimeraWorld world, bool requireHelmetForPolicy)
	{
		if (!requireHelmetForPolicy)
			return true;
		if (!world)
			return false;
		if (!HMD_HmdVehicleHudRestriction.VehicleHudShouldRestrictToCameraOnly(world))
			return true;
		return HMD_HmdVehicleHudRestriction.LocalPlayerHasHmdHelmetCapability(world);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DenyMarkerPolicy(out float outMaxViewDistanceM, out bool outAllowIffMarkers, out bool outAllowForeignLaserDesignations, out bool outAllowOwnLaserDesignations, out float outHudMarkerUpdateRateHz)
	{
		outMaxViewDistanceM = DEFAULT_MAX_VIEW_M;
		outAllowIffMarkers = false;
		outAllowForeignLaserDesignations = false;
		outAllowOwnLaserDesignations = false;
		outHudMarkerUpdateRateHz = DEFAULT_UPDATE_HZ;
	}

	//------------------------------------------------------------------------------------------------
	//! Hull + binocular policy. Vehicle root must carry HMD_HudMarkerEligibilityVehicleComponent. Own-laser vs foreign-laser flags come from hull (or binocular single laser checkbox for both).
	static void GetResolvedPolicy(out float outMaxViewDistanceM, out bool outAllowIffMarkers, out bool outAllowForeignLaserDesignations, out bool outAllowOwnLaserDesignations, out float outHudMarkerUpdateRateHz)
	{
		outMaxViewDistanceM = DEFAULT_MAX_VIEW_M;
		outAllowIffMarkers = true;
		outAllowForeignLaserDesignations = true;
		outAllowOwnLaserDesignations = true;
		outHudMarkerUpdateRateHz = DEFAULT_UPDATE_HZ;

		ChimeraWorld world = GetGame().GetWorld();

		IEntity localChar = SCR_PlayerController.GetLocalControlledEntity();
		if (!localChar)
			return;
		if (!HMD_HudMarkerEligibility.IsLocalPlayerConsciousForHud())
		{
			DenyMarkerPolicy(outMaxViewDistanceM, outAllowIffMarkers, outAllowForeignLaserDesignations, outAllowOwnLaserDesignations, outHudMarkerUpdateRateHz);
			return;
		}
		//! Vehicle handheld binoculars: no marker policy (all dots off in HUDMarkerVisibility).
		if (HMD_HudMarkerEligibility.IsVehicleBinocularViewActive())
			return;

		IEntity vehicleRoot = GetLocalVehicleRoot();

		if (vehicleRoot)
		{
			if (!HMD_VehicleHUDLaserHelpers.FindComponentInHierarchy(vehicleRoot, HMD_HudMarkerEligibilityVehicleComponent))
			{
				DenyMarkerPolicy(outMaxViewDistanceM, outAllowIffMarkers, outAllowForeignLaserDesignations, outAllowOwnLaserDesignations, outHudMarkerUpdateRateHz);
				return;
			}
		}

		if (vehicleRoot && HMD_HudMarkerEligibility.VehicleCameraOnlySuppressesMarkerHud())
		{
			DenyMarkerPolicy(outMaxViewDistanceM, outAllowIffMarkers, outAllowForeignLaserDesignations, outAllowOwnLaserDesignations, outHudMarkerUpdateRateHz);
			return;
		}

		if (HMD_HandheldOpticZoom.IsZoomedForHMD())
		{
			HMD_HudMarkerEligibilityBinocularComponent bin = HMD_HudMarkerEligibilityBinocularComponent.Cast(localChar.FindComponent(HMD_HudMarkerEligibilityBinocularComponent));
			if (bin)
			{
				outMaxViewDistanceM = bin.GetMaxMarkerViewDistanceM();
				outAllowIffMarkers = bin.GetPolicyAllowIffMarkers();
				bool binLaser = bin.GetPolicyAllowLaserDesignations();
				outAllowForeignLaserDesignations = binLaser;
				outAllowOwnLaserDesignations = binLaser;
				outHudMarkerUpdateRateHz = bin.GetHudMarkerUpdateRateHz();
			}
			return;
		}

		if (vehicleRoot)
		{
			HMD_HudMarkerEligibilityVehicleComponent veh = HMD_HudMarkerEligibilityVehicleComponent.Cast(vehicleRoot.FindComponent(HMD_HudMarkerEligibilityVehicleComponent));
			if (veh)
			{
				//! Any vehicle weapon/optic camera (turret, ADS, etc.): apply hull policy when require-helmet is on but no HMD. Gunner-vs-interior for laser marking is IsLocalPlayerInGunnerWeaponOpticCamera / MayUse, not this.
				bool canApplyHull = CanApplyHmdHelmetGatedVehiclePolicy(world, veh.GetRequireHmdHelmetForPolicy());
				if (!canApplyHull)
				{
					if (HMD_HmdVehicleHudRestriction.IsLocalPlayerInVehicleCameraView())
						canApplyHull = true;
				}
				if (canApplyHull)
				{
					outMaxViewDistanceM = veh.GetMaxMarkerViewDistanceM();
					outAllowIffMarkers = veh.GetPolicyAllowIffMarkers();
					outAllowForeignLaserDesignations = veh.GetPolicyAllowLaserDesignations();
					outAllowOwnLaserDesignations = veh.GetPolicyAllowOwnLaserDesignations();
					outHudMarkerUpdateRateHz = veh.GetHudMarkerUpdateRateHz();
				}
				else
				{
					DenyMarkerPolicy(outMaxViewDistanceM, outAllowIffMarkers, outAllowForeignLaserDesignations, outAllowOwnLaserDesignations, outHudMarkerUpdateRateHz);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static float GetEffectiveMaxViewDistanceM()
	{
		float d;
		bool allowIff;
		bool allowForeign;
		bool allowOwn;
		float hz;
		GetResolvedPolicy(d, allowIff, allowForeign, allowOwn, hz);
		return d;
	}

	//------------------------------------------------------------------------------------------------
	static bool PolicyAllowsIffMarkers()
	{
		float d;
		bool allowIff;
		bool allowForeign;
		bool allowOwn;
		float hz;
		GetResolvedPolicy(d, allowIff, allowForeign, allowOwn, hz);
		return allowIff;
	}

	//------------------------------------------------------------------------------------------------
	//! Other players' laser designation dots (WCS); Numpad * visibility pool.
	static bool PolicyAllowsForeignLaserDesignationMarkers()
	{
		float d;
		bool allowIff;
		bool allowForeign;
		bool allowOwn;
		float hz;
		GetResolvedPolicy(d, allowIff, allowForeign, allowOwn, hz);
		return allowForeign;
	}

	//------------------------------------------------------------------------------------------------
	//! Local own laser designation dot (HUD pool / vehicle marking); hull checkbox.
	static bool PolicyAllowsOwnLaserDesignationMarkers()
	{
		float d;
		bool allowIff;
		bool allowForeign;
		bool allowOwn;
		float hz;
		GetResolvedPolicy(d, allowIff, allowForeign, allowOwn, hz);
		return allowOwn;
	}

	//------------------------------------------------------------------------------------------------
	static float GetEffectiveHudMarkerUpdateRateHz()
	{
		float d;
		bool allowIff;
		bool allowForeign;
		bool allowOwn;
		float hz;
		GetResolvedPolicy(d, allowIff, allowForeign, allowOwn, hz);
		return hz;
	}
}
