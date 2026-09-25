# Debugging Nintendont

Since Nintendont doesn't run within Dolphin, so one has to debug on real hardware.  
This *can* be done via a [USB Gecko](https://wiibrew.org/wiki/USB_Gecko) device, and while it's very inaccessible, it's very much preferred as it will give you real time logging. It is possible to write logs to the storage device that contains the games, but that's very unreliable. For that method, make sure that the `Enable Logs` setting within Nintendont is enabled before you boot the game.  
Afterwards, any `dbgprintf` calls will be appended to the `ndebug.log` file (or created first if it did not exist yet).  
The `global.h` header contains lots of useful definitions for this, like `HW_TIMER`.
