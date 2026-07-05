# ZenRTS
# Copyright (C) 2026  Ian Torres
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import hashlib
import glob
import sys
import os

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "."

    libs = glob.glob(os.path.join(path, "lib/*.a")) + glob.glob(os.path.join(path, "lib/*.lib"))
    if not libs:
        print("No library found", file=sys.stderr)
        sys.exit(1)

    lib = libs[0]
    with open(lib, "rb") as f:
        h = hashlib.sha256(f.read()).hexdigest()

    checksum_path = os.path.join(path, "checksum.txt")
    with open(checksum_path, "w") as f:
        f.write(f"{h}  {lib}\n")

    print(f"SHA256({lib}) = {h}")

if __name__ == "__main__":
    main()
