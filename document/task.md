Task:
Make a windows app.
- It has very small size.
- It connects USB Mass Storage Device (Finger print device) with device io control windows api function.
- This project is to manage settings of finger print device.
- Users could register fingerprints
- Users could register passwords for each finger.

Finger Print Device:
- If a user touch his finger to the device, device acts as keyboard and generate keys which are registered with this device.
This means that this device has two interface - Mass Stroage(Setting), HID-Keyboard.

Require:
- This app must have very small size so that we could embed this app to the device which has small flash size.

Constraints (agreed):
- Flash / binary size limit: 100 KB (102400 bytes). Enforced by cmake/check_size.cmake on every build.
