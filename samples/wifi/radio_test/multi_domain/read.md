# PHY RF parameter overrides (nRF71, GDRAM)

This applies only when **WIFI_NRF71** and **PHY_RF_PARAM_GDRAM** (`CONFIG_NRF71_PHY_RF_PARAM_GDRAM`) are enabled.

## What it does

Firmware RF data is loaded from **22** hex strings defined in `nrfxlib/nrf71_wifi/include/nrf71_wifi_rf.h` as `NRF_WIFI_PARAMS1` … `NRF_WIFI_PARAMS22`. At radio-test init, `nrf_wifi_fmac_config_rf_params()` parses each string and writes the binary blob to the corresponding RPU GDRAM slot.

The shell command **`config_phy_rf_param`** (under `wifi radio_test`) stores a **host-side override** for any of the 22 strings. The override is applied the next time `nrf_wifi_fmac_config_rf_params()` runs (typically from `nrf_wifi_radio_test_conf_init()` when you call **`init`**).

Overrides persist until you change the same index again or reset the device. There is no “clear to default” in the shell; reboot clears RAM overrides.

## Command

```text
wifi radio_test config_phy_rf_param <n> <hex>
```

- **`<n>`**: parameter index **1** … **22** (maps to `NRF_WIFI_PARAMSn` in the header).
- **`<hex>`**: contiguous hex string (even length, `0-9` / `a-f` / `A-F`), **no spaces**. Length must be below the driver limit (512 characters including safety margin for the longest built-in string).

### Example: PARAMS10

Default in the header is `0000000000` (10 hex digits → 5 bytes). To set the same explicitly before init:

```text
wifi radio_test config_phy_rf_param 10 0000000000
wifi radio_test init ...
```

### Edge ceilings (PARAM9 only)

**`config_edge_ceilings`** sets **only** `NRF_WIFI_PARAMS9` (GDRAM index 9). It takes a **single** hex string of **70** characters (**35** bytes). Call **before** **`init`**, same as `config_phy_rf_param`.

```text
wifi radio_test config_edge_ceilings <70 hex chars>
wifi radio_test init ...
```

### Antenna gain (PARAM10 only)

**`config_antenna_gain`** sets **only** `NRF_WIFI_PARAMS10`. Hex string must be **10** characters (**5** bytes). Call **before** **`init`**.

```text
wifi radio_test config_antenna_gain <10 hex chars>
wifi radio_test init ...
```

## Requirements

1. Call **`config_phy_rf_param`**, **`config_edge_ceilings`**, or **`config_antenna_gain`** **before** **`init`** so that init programs GDRAM with your hex.
2. Wrong hex length or invalid characters are rejected; the command returns an error and leaves the previous override for that index unchanged (or no override if none was set).

## Related

- **`config_vtf_params`**: same pattern—set host-side values before **`init`**, then init pushes VTF fields to the RPU.
