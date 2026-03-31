//------------------------------------------------------------------------------------------------

//! Shared laser trace + rangefinder helpers for vehicle marking components.

class HMD_LaserMarkingTraceUtils

{

	//------------------------------------------------------------------------------------------------

	static vector ComputeLaserHitPos(BaseWorld world, vector start, vector dirNorm, float maxRange, IEntity excludeExtra, IEntity localChar, out float traceFrac)

	{

		vector end = start + (dirNorm * maxRange);

		TraceParam trace = new TraceParam();

		trace.Start = start;

		trace.End = end;

		trace.Flags = TraceFlags.DEFAULT | TraceFlags.ANY_CONTACT;

		trace.LayerMask = EPhysicsLayerDefs.Projectile;

		if (localChar)

			trace.Exclude = localChar.GetRootParent();

		ref array<IEntity> excl = {};

		if (excludeExtra)

			excl.Insert(excludeExtra);

		trace.ExcludeArray = excl;

		float frac = world.TraceMove(trace, null);

		if (frac < 0)

			frac = 0;

		if (frac > 1)

			frac = 1;

		traceFrac = frac;

		return start + (end - start) * frac;

	}



	//------------------------------------------------------------------------------------------------

	static void ClearVehicleMarkingReadout()

	{

		HMD_RangefinderHUDState.ClearLasingReadoutOnly();

		HMD_RangefinderHUDState.SetDesignatorCode(0);

	}

}


