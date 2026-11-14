# Desk weather

Open **Apps → Weather** on Desk Home. Choose **Change location**, enter a city,
connect to Wi-Fi, then select the matching city and region. Choose **Refresh** to
fetch its forecast. No city is preselected. Tapping the Desk weather widget opens
this screen. When no forecast is shown, the widget says whether to choose a city,
refresh, or check the SD card; Home never fetches weather itself.

The Desk widget shows temperature with its unit, condition, humidity and daily
precipitation probabilities. Precipitation amounts appear only on the Weather
screen. The screen shows current temperature, condition and relative humidity; today's
precipitation probability and total amount; and three days of high/low temperatures,
probability and amount. Precipitation includes rain and the water equivalent of
snow. Probability is the daily maximum probability, not a percentage of the day.
Missing optional values appear as a dash. Celsius/mm is the default; **Units**
switches to Fahrenheit/inches and saves that choice. Dates and times use the
selected city's time zone, independently of the reader's time zone.

Search and refresh are manual. They use the existing Wi-Fi picker and switch
Wi-Fi off afterward. Home only reads the saved forecast; it never connects or
updates automatically. Back, the Home gesture or Cancel interrupts a request.
Power-button sleep cancels the request before sleep. Changing city clears the old
forecast after the new selection saves successfully; refresh to show the new city.

The update line gives the last successful fetch time. Without a trusted device
clock it says **As of**, using the provider's observation time. **Saved** indicates
an unknown clock/fetch time, a future fetch time, or data over one hour old.
Saved forecast dates remain unchanged; old data is never relabeled as today's
forecast. Failed refreshes retain the previous successful forecast and timestamp.

## Storage and recovery

Location, units and the last successful forecast are stored together on SD in
`/.crosspoint/weather-a.bin` and `weather-b.bin`. Back up both files. Each is a
512-byte versioned record with generation number and CRC32. Writes use
`weather-t.bin`, close and read it back, then promote it to the inactive slot.
The previous active slot remains available if a write or rename fails. CRC32
checks accidental damage; it does not authenticate data.

Home's load is read-only. A corrupt slot can fall back to its valid peer. If both
are damaged, a slot cannot be read, or a newer unsupported version is present,
the files are preserved and editing is disabled. Back up those files before
removing only the weather files to start again. Never delete all of `.crosspoint`:
it also contains books' reading positions, settings, statistics and app data.
SD removal or power loss during FAT directory updates still needs device testing;
these snapshots are recovery protection, not a guarantee against card failure.

## Provider and connection

Weather data: [Open-Meteo](https://open-meteo.com/), under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
Location search: Open-Meteo's [geocoding API](https://open-meteo.com/en/docs/geocoding-api),
based on [GeoNames](https://www.geonames.org/).
See the [forecast API documentation](https://open-meteo.com/en/docs) for variables,
model coverage and limitations. The free endpoint is for non-commercial use;
check [Open-Meteo's terms](https://open-meteo.com/en/terms) before redistribution
or commercial use. No API key is embedded.

City searches send the typed city name to Open-Meteo. Forecast requests send the
coordinates of the explicitly selected result. There is no automatic location
detection, background polling or weather alert service. Forecasts are model data,
not safety advice.

Connections reuse the firmware's existing HTTPS client policy: transport is
encrypted, but server certificate verification is disabled at the pinned SDK
revision because no CA bundle is configured. Weather therefore does not verify
the server's identity. Adding verified TLS is a separate shared-network change.
Responses are capped at 8 KiB and parsed in a bounded temporary arena after the
HTTP client is destroyed. Request cancellation and timeouts reuse the pinned SDK.

Weather icons are converted Lucide assets; their licence and regeneration steps
are in [the icon guide](../src/components/icons/README.md).
