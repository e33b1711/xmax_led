# Liefert die Firmware-Version aus der Datei VERSION als Compiler-Flag (für platformio.ini).
import os
here = os.path.dirname(os.path.abspath(__file__))
v = open(os.path.join(here, "..", "VERSION")).read().strip()
print(f'-DFW_VERSION=\\"{v}\\"')
