import sys
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

from testgpio.app import create_app


app = create_app()


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5820, debug=False)
