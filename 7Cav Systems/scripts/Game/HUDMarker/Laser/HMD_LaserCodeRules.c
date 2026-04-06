//------------------------------------------------------------------------------------------------

//! Laser code ranges: handheld gadget uses 1111–1200; vehicle turret marking uses 1311–1399.

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

	//! Vehicle camera marking: match handheld range.

	static int WrapCameraVehicleMarking(int code)

	{

		return WrapHandheldRange(code);

	}



	//------------------------------------------------------------------------------------------------

	//! Turret vehicle marking codes (inclusive).

	static int WrapTurretVehicleMarking(int code)

	{

		if (code < 1311)

			return 1399;

		if (code > 1399)

			return 1311;

		return code;

	}

	//------------------------------------------------------------------------------------------------
	//! Gunner vehicle marking may use 1311-1399; handheld gadget uses 1111-1200. Same channel: turret N maps to handheld N-200.
	static bool CodesMatchForWeaponLock(int vehicleGunnerDisplayCode, int handheldLaserCode)
	{
		if (vehicleGunnerDisplayCode == handheldLaserCode)
			return true;
		if (vehicleGunnerDisplayCode >= 1311 && vehicleGunnerDisplayCode <= 1399)
			return (vehicleGunnerDisplayCode - 200) == handheldLaserCode;
		return false;
	}

}


