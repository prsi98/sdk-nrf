.. _wifi_radio_test_sd:

.. ncs-sample::
   :title: Wi-Fi: Bluetooth LE Wi-Fi Radio test (Single domain)

   The Bluetooth LE Wi-Fi Radio test (Single domain) sample demonstrates how to use the radio test for both Wi-Fi® and Bluetooth® LE protocols using a single domain image, that is, both running on the same core (application core).
   The sample shows how to use the radio test subcommands to configure the parameters and run the radio test.

   The sample supports both nRF70 Series companion ICs and nRF71 Series SoCs; see the subcommands page for chip-specific shell commands.
   For nRF70 Series builds, the sample also supports programming Factory Information Configuration Registers (FICR) fields in the nRF7002 one-time programmable (OTP) memory.
   FICR programming is not available on nRF71 Series devices (for example, nRF7120 DK).

   See the subpages for detailed documentation on the sample and its features.

.. toctree::
   :maxdepth: 1
   :caption: Subpages:

   sample_description.rst
   testing.rst
   radio_test_subcommands.rst
   ficr.rst
