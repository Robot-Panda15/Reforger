//------------------------------------------------------------------------------------------------
//! Extension of AUS_UH60_FlightHUD to display altitude in meters instead of feet
//------------------------------------------------------------------------------------------------

modded class AUS_UH60_FlightHUD
{
    override void UpdateHUD(IEntity controlledEntity)
    {
        super.UpdateHUD(controlledEntity);

        // Override altitude display: use meters directly, no feet conversion
        float altitude = controlledEntity.GetOrigin()[1];
        if (m_AltitudeWidget)
            m_AltitudeWidget.SetTextFormat("ALT %1 M", Math.Round(altitude));
    }
}