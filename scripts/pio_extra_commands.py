Import("env")  # type: ignore

# ota_packager.py attaches packaging to the filesystem build, which depends on
# the firmware. Both targets use that same build graph and publish to bin/.
for name, title in (
    ("package_ota", "Baue Firmware, SPIFFS und OTA-Paket nach bin/"),
    ("build_bins", "Erzeuge beide BINs und OTA-Manifest nach bin/"),
):
    env.AddCustomTarget(  # type: ignore
        name=name,
        dependencies=[env.Alias("buildprog")],  # type: ignore
        actions=[],
        title=title,
    )
