"""
Test Runner Script

Runs all tests with detailed output and summary statistics.
"""

import sys
import unittest
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))


def run_all_tests():
    """Run all tests and display summary"""
    print("="*70)
    print("ACCL Packet Trace Generator - Test Suite")
    print("="*70)
    print()
    
    # Discover and run all tests
    loader = unittest.TestLoader()
    start_dir = Path(__file__).parent
    suite = loader.discover(str(start_dir), pattern='test_*.py')
    
    # Run with verbose output
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Print summary
    print()
    print("="*70)
    print("Test Summary")
    print("="*70)
    print(f"Tests run: {result.testsRun}")
    print(f"Successes: {result.testsRun - len(result.failures) - len(result.errors)}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print(f"Skipped: {len(result.skipped)}")
    print("="*70)
    
    # Return exit code
    return 0 if result.wasSuccessful() else 1


def run_specific_tests(test_names):
    """Run specific test modules"""
    print("="*70)
    print(f"Running specific tests: {', '.join(test_names)}")
    print("="*70)
    print()
    
    suite = unittest.TestSuite()
    loader = unittest.TestLoader()
    
    for test_name in test_names:
        try:
            # Load the test module
            module_name = f"tests.{test_name}" if not test_name.startswith('tests.') else test_name
            suite.addTests(loader.loadTestsFromName(module_name))
        except Exception as e:
            print(f"Error loading {test_name}: {e}")
    
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    return 0 if result.wasSuccessful() else 1


def main():
    """Main entry point"""
    if len(sys.argv) > 1:
        # Run specific tests
        test_names = sys.argv[1:]
        exit_code = run_specific_tests(test_names)
    else:
        # Run all tests
        exit_code = run_all_tests()
    
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
