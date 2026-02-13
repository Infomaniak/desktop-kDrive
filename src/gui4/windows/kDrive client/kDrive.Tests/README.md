# kDrive WinUI Test Project

This directory contains unit tests for the kDrive WinUI client application.

## Overview

The test project uses MSTest framework with the following NuGet packages:
- `Microsoft.NET.Test.Sdk` (v17.12.0)
- `MSTest.TestAdapter` (v3.7.0)
- `MSTest.TestFramework` (v3.7.0)
- `coverlet.collector` (v6.0.2) - For code coverage

## Test Coverage

### SocketServerCommProtocolTests

Comprehensive test suite for `SocketServerCommProtocol` class covering:

#### UpdateJsonBalance Tests
- Empty string handling
- Single opening brace
- Matching braces
- Nested braces
- Multiple JSON objects
- Complex nested structures
- Incomplete JSON
- Non-zero starting balance
- Deeply nested structures
- Only closing braces

#### NextId Tests
- Proper incrementation of request IDs
- Overflow handling (resets to 1 after reaching long.MaxValue)

#### CommData Tests
- Default values initialization
- SignalNum getter and setter
- RequestNum getter and setter
- Params property manipulation

#### SignalEventArgs Tests
- Constructor validation

#### Event Tests
- SignalReceived event subscription/unsubscription
- ConnectionLost event subscription/unsubscription

#### SendRequestAsync Tests
- Cancellation token handling
- Behavior without connection

## Building the Tests

### On Windows

```powershell
# Restore packages
dotnet restore "kDrive.Tests/kDrive.Tests.csproj"

# Build the test project
dotnet build "kDrive.Tests/kDrive.Tests.csproj" --configuration Debug

# Run all tests
dotnet test "kDrive.Tests/kDrive.Tests.csproj"
```

### From Visual Studio

1. Open `kDrive client.sln`
2. Build the solution (this will build both the main project and test project)
3. Open Test Explorer (Test → Test Explorer)
4. Run all tests

## Test Structure

```
kDrive.Tests/
├── kDrive.Tests.csproj                    # Test project file
├── README.md                              # This file
└── ServerCommunication/
    └── Services/
        └── SocketServerCommProtocolTests.cs  # SocketServerCommProtocol test suite
```

## Writing New Tests

When adding new tests:

1. Follow the existing naming convention: `MethodName_Scenario_ExpectedBehavior`
2. Use the Arrange-Act-Assert pattern
3. Include descriptive assertion messages
4. Group related tests using `#region` directives
5. Add appropriate test attributes:
   - `[TestClass]` for test classes
   - `[TestMethod]` for test methods
   - `[TestInitialize]` for setup (if needed)
   - `[TestCleanup]` for teardown (if needed)

## Code Coverage

To generate code coverage reports:

```powershell
dotnet test "kDrive.Tests/kDrive.Tests.csproj" --collect:"XPlat Code Coverage"
```

Coverage reports will be generated in the `TestResults` directory.

## Notes

- The test project targets `net9.0-windows10.0.19041` to match the main project
- Tests use reflection to access private methods and fields when testing internal logic
- Some tests verify behavior without an active server connection (expected scenario during startup)
