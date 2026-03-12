//------------------------------------------------------------------------------------------------
// Debug logging for DecalMaterialSwitcher - writes NDJSON to $profile:debug-927988.log
// #region agent log
class DecalSwitcherDebug
{
	static void Log(string location, string message, string dataKey, string dataVal, string hypothesisId)
	{
		FileHandle f = FileIO.OpenFile("$profile:debug-927988.log", FileMode.APPEND);
		if (!f || !f.IsOpen())
			return;
		string json = "{\"sessionId\":\"927988\",\"location\":\"" + location + "\",\"message\":\"" + message + "\"";
		if (dataKey != "")
			json += ",\"data\":{\"" + dataKey + "\":\"" + dataVal + "\"}";
		if (hypothesisId != "")
			json += ",\"hypothesisId\":\"" + hypothesisId + "\"";
		json += ",\"timestamp\":" + string.ToString(System.GetTickCount()) + "}\n";
		f.Write(json);
		f.Close();
	}

	static void LogCanBeShown(string action, string ownerNull, string switcherNull, string count, string current, string result, string hypothesisId)
	{
		FileHandle f = FileIO.OpenFile("$profile:debug-927988.log", FileMode.APPEND);
		if (!f || !f.IsOpen())
			return;
		string json = "{\"sessionId\":\"927988\",\"location\":\"CanBeShownScript\",\"message\":\"" + action + "\",\"data\":{\"ownerNull\":\"" + ownerNull + "\",\"switcherNull\":\"" + switcherNull + "\",\"count\":\"" + count + "\",\"current\":\"" + current + "\",\"result\":\"" + result + "\"},\"hypothesisId\":\"" + hypothesisId + "\",\"timestamp\":" + string.ToString(System.GetTickCount()) + "}\n";
		f.Write(json);
		f.Close();
	}
}
// #endregion
