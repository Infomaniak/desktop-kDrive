import os
import sys

# Expose the .github/scripts modules to the test suite
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
