//------------------------------------------------------------------------------------------------

//! Laser code ranges: handheld gadget 1111–1200; vehicle ground band 1211–1299; vehicle air band 1311–1399.

class HMD_LaserCodeRules
{

	//------------------------------------------------------------------------------------------------

	//! Same wrap as handheld designator gadget (1111–1200; 1200 non-target).

	static int WrapHandheldRange(int code)
	{
		if (code < 1111)
			return 1200;
		if (code > 1200)
			return 1111;
		return code;
	}

	//------------------------------------------------------------------------------------------------

	//! Ground vehicle marking (inclusive).

	static int WrapGroundVehicleMarking(int code)
	{
		if (code < 1211)
			return 1299;
		if (code > 1299)
			return 1211;
		return code;
	}

	//------------------------------------------------------------------------------------------------

	//! Air vehicle marking (inclusive).

	static int WrapAirVehicleMarking(int code)
	{
		if (code < 1311)
			return 1399;
		if (code > 1399)
			return 1311;
		return code;
	}

	//------------------------------------------------------------------------------------------------

	//! @deprecated Use WrapGroundVehicleMarking.

	static int WrapCameraVehicleMarking(int code)
	{
		return WrapGroundVehicleMarking(code);
	}

	//------------------------------------------------------------------------------------------------

	//! @deprecated Use WrapAirVehicleMarking.

	static int WrapTurretVehicleMarking(int code)
	{
		return WrapAirVehicleMarking(code);
	}

	//------------------------------------------------------------------------------------------------
	//! Handheld 1111–1200: air vehicle 1311–1399 matches N−200; ground vehicle 1211–1299 matches N−100.
	static bool CodesMatchForWeaponLock(int vehicleGunnerDisplayCode, int handheldLaserCode)
	{
		if (vehicleGunnerDisplayCode == handheldLaserCode)
			return true;
		if (vehicleGunnerDisplayCode >= 1311 && vehicleGunnerDisplayCode <= 1399)
			return (vehicleGunnerDisplayCode - 200) == handheldLaserCode;
		if (vehicleGunnerDisplayCode >= 1211 && vehicleGunnerDisplayCode <= 1299)
			return (vehicleGunnerDisplayCode - 100) == handheldLaserCode;
		return false;
	}

}


