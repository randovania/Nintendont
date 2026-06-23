# Debugging Nintendont

Since Nintendont doesn't run within Dolphin, so one has to debug on real hardware.  
This *can* be done via a [USB Gecko](https://wiibrew.org/wiki/USB_Gecko) device, but its very inaccessibe. Instead the common method is to write logs to the storage device that contains the games. For that make sure that the `Enable Logs` setting within Nintendont is enabled before you boot the game.  
Afterwards, any `dbgprintf` calls will be appended to the `ndebug.log` file (or created first if it did not exist yet).  
The `global.h` header contains lots of useful definitions for this, like `HW_TIMER`.
