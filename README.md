# testwm
Hacked up window manager for Windows 7 for personal comfort, based off of
HashTWM

Main motive was to fix the window position resetting problem in Windows 7
when turning off monitors connected via DP/mDP. Due to the power managment
pin, the OS would detect DP/mDP monitors as disconnected when shut off,
and would move any open windows to the main monitor (or virtual monitor
that has a resolution of 1280x1024 if all monitors are off).

Common problem with lack of any working solution for desktop gpus:
* https://answers.microsoft.com/en-us/windows/forum/windows_7-hardware/windows-7-movesresizes-windows-on-monitor-power/1653aafb-848b-464a-8c69-1a68fbd106aa
* https://answers.microsoft.com/en-us/windows/forum/windows_10-hardware/windows-10-multiple-display-windows-are-moved-and/2b9d5a18-45cc-4c50-b16e-fd95dbf27ff3

The fix this tool contains:
* Ctrl + Alt + S = save window positions
* [turn off monitors]
* [turn on monitors]
* Ctrl + Alt + R = restore window positions

Also added some personal comfort things to it.
