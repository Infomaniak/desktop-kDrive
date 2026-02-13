/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using Infomaniak.kDrive.ServerCommunication.Services;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Reflection;
using System.Text.Json.Nodes;

namespace Infomaniak.kDrive.Tests.ServerCommunication.Services
{
    [TestClass]
    public class SocketServerCommProtocolTests
    {
        #region UpdateJsonBalance Tests

        [TestMethod]
        public void UpdateJsonBalance_EmptyString_ReturnsMinusOneAndZeroBalance()
        {
            // Arrange
            string input = "";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 for empty string");
            Assert.AreEqual(-1, endIndex, "EndIndex should be -1 for empty string");
        }

        [TestMethod]
        public void UpdateJsonBalance_SingleOpenBrace_IncreasesBalanceToOne()
        {
            // Arrange
            string input = "{";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(1, balance, "Balance should be 1 for single opening brace");
            Assert.AreEqual(-1, endIndex, "EndIndex should be -1 since JSON is not complete");
        }

        [TestMethod]
        public void UpdateJsonBalance_MatchingBraces_ReturnsCorrectEndIndex()
        {
            // Arrange
            string input = "{}";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 for matching braces");
            Assert.AreEqual(1, endIndex, "EndIndex should be 1 (position of closing brace)");
        }

        [TestMethod]
        public void UpdateJsonBalance_NestedBraces_TracksBalanceCorrectly()
        {
            // Arrange
            string input = "{{}}";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 for nested matching braces");
            Assert.AreEqual(3, endIndex, "EndIndex should be 3 (position of final closing brace)");
        }

        [TestMethod]
        public void UpdateJsonBalance_MultipleJsonObjects_FindsFirstComplete()
        {
            // Arrange
            string input = "{}{}";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 after first complete object");
            Assert.AreEqual(1, endIndex, "EndIndex should be 1 (end of first object)");
        }

        [TestMethod]
        public void UpdateJsonBalance_ComplexNestedStructure_FindsCorrectEnd()
        {
            // Arrange
            string input = "{\"type\":1,\"params\":{\"nested\":{}}}";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 for complete JSON");
            Assert.AreEqual(input.Length - 1, endIndex, "EndIndex should be at last closing brace");
        }

        [TestMethod]
        public void UpdateJsonBalance_IncompleteJson_NegativeOneEndIndex()
        {
            // Arrange
            string input = "{\"type\":1,\"params\":";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(1, balance, "Balance should be 1 for incomplete JSON");
            Assert.AreEqual(-1, endIndex, "EndIndex should be -1 for incomplete JSON");
        }

        [TestMethod]
        public void UpdateJsonBalance_StartingWithNonZeroBalance_ContinuesCorrectly()
        {
            // Arrange
            string input = "}}";
            int balance = 2;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 after two closing braces");
            Assert.AreEqual(1, endIndex, "EndIndex should be 1");
        }

        [TestMethod]
        public void UpdateJsonBalance_DeeplyNestedStructure_HandlesCorrectly()
        {
            // Arrange
            string input = "{{{{}}}}";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 for deeply nested matching braces");
            Assert.AreEqual(7, endIndex, "EndIndex should be 7");
        }

        [TestMethod]
        public void UpdateJsonBalance_OnlyClosingBraces_DecreasesBalance()
        {
            // Arrange
            string input = "}}}";
            int balance = 3;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 after three closing braces");
            Assert.AreEqual(2, endIndex, "EndIndex should be 2");
        }

        #endregion

        #region NextId Tests

        [TestMethod]
        public void NextId_IncrementsProperly()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var nextIdProperty = GetNextIdProperty();

            // Act
            long firstId = (long)nextIdProperty.GetValue(protocol)!;
            long secondId = (long)nextIdProperty.GetValue(protocol)!;
            long thirdId = (long)nextIdProperty.GetValue(protocol)!;

            // Assert
            Assert.AreEqual(1, firstId, "First ID should be 1");
            Assert.AreEqual(2, secondId, "Second ID should be 2");
            Assert.AreEqual(3, thirdId, "Third ID should be 3");
        }

        [TestMethod]
        public void NextId_HandlesOverflow()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var counterField = GetRequestIdCounterField();
            
            // Set counter to near maximum
            counterField.SetValue(protocol, long.MaxValue - 1);
            var nextIdProperty = GetNextIdProperty();

            // Act
            long beforeOverflow = (long)nextIdProperty.GetValue(protocol)!;
            long afterOverflow = (long)nextIdProperty.GetValue(protocol)!;

            // Assert
            Assert.AreEqual(long.MaxValue, beforeOverflow, "Should return MaxValue before overflow");
            Assert.AreEqual(1, afterOverflow, "Should reset to 1 after overflow");
        }

        #endregion

        #region CommData Tests

        [TestMethod]
        public void CommData_DefaultValues_AreSetCorrectly()
        {
            // Arrange & Act
            var commData = new IServerCommProtocol.CommData();

            // Assert
            Assert.AreEqual(CommMessageType.Unknown, commData.Type, "Default Type should be Unknown");
            Assert.AreEqual(-1, commData.Id, "Default Id should be -1");
            Assert.AreEqual(-1, commData.Num, "Default Num should be -1");
            Assert.AreEqual(ExitCode.Unknown, commData.Code, "Default Code should be Unknown");
            Assert.AreEqual(ExitCause.Unknown, commData.Cause, "Default Cause should be Unknown");
            Assert.IsNotNull(commData.Params, "Params should be initialized");
        }

        [TestMethod]
        public void CommData_SignalNum_GetterAndSetter_WorkCorrectly()
        {
            // Arrange
            var commData = new IServerCommProtocol.CommData();

            // Act
            commData.SignalNum = SignalNum.USER_ADDED;

            // Assert
            Assert.AreEqual(SignalNum.USER_ADDED, commData.SignalNum, "SignalNum getter should return the set value");
            Assert.AreEqual((int)SignalNum.USER_ADDED, commData.Num, "Num should match SignalNum integer value");
        }

        [TestMethod]
        public void CommData_RequestNum_GetterAndSetter_WorkCorrectly()
        {
            // Arrange
            var commData = new IServerCommProtocol.CommData();

            // Act
            commData.RequestNum = RequestNum.USER_INFOLIST;

            // Assert
            Assert.AreEqual(RequestNum.USER_INFOLIST, commData.RequestNum, "RequestNum getter should return the set value");
            Assert.AreEqual((int)RequestNum.USER_INFOLIST, commData.Num, "Num should match RequestNum integer value");
        }

        [TestMethod]
        public void CommData_Params_CanBeSetAndRetrieved()
        {
            // Arrange
            var commData = new IServerCommProtocol.CommData();
            var parameters = new JsonObject
            {
                ["key1"] = "value1",
                ["key2"] = 42
            };

            // Act
            commData.Params = parameters;

            // Assert
            Assert.AreEqual(parameters, commData.Params, "Params should be the same object");
            Assert.AreEqual("value1", commData.Params["key1"]?.ToString(), "Parameter key1 should be accessible");
            Assert.AreEqual(42, (int?)commData.Params["key2"], "Parameter key2 should be accessible");
        }

        #endregion

        #region SignalEventArgs Tests

        [TestMethod]
        public void SignalEventArgs_Constructor_SetsPropertiesCorrectly()
        {
            // Arrange
            var signalNum = SignalNum.DRIVE_ADDED;
            var signalData = new JsonObject { ["driveId"] = 123 };

            // Act
            var eventArgs = new IServerCommProtocol.SignalEventArgs(signalNum, signalData);

            // Assert
            Assert.AreEqual(signalNum, eventArgs.SignalNum, "SignalNum should be set correctly");
            Assert.AreEqual(signalData, eventArgs.SignalData, "SignalData should be set correctly");
        }

        #endregion

        #region Event Tests

        [TestMethod]
        public void SignalReceived_Event_CanBeSubscribedAndUnsubscribed()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            bool eventRaised = false;
            EventHandler<IServerCommProtocol.SignalEventArgs> handler = (sender, args) => { eventRaised = true; };

            // Act
            protocol.SignalReceived += handler;
            protocol.SignalReceived -= handler;

            // Assert - no exception should be thrown
            Assert.IsFalse(eventRaised, "Event should not have been raised");
        }

        [TestMethod]
        public void ConnectionLost_Event_CanBeSubscribedAndUnsubscribed()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            bool eventRaised = false;
            EventHandler handler = (sender, args) => { eventRaised = true; };

            // Act
            protocol.ConnectionLost += handler;
            protocol.ConnectionLost -= handler;

            // Assert - no exception should be thrown
            Assert.IsFalse(eventRaised, "Event should not have been raised");
        }

        #endregion

        #region SendRequestAsync Tests

        [TestMethod]
        public async Task SendRequestAsync_WithCancellationToken_CanBeCanceled()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var cts = new CancellationTokenSource();
            var parameters = new JsonObject();

            // Act
            cts.Cancel(); // Cancel immediately
            var result = await protocol.SendRequestAsync(RequestNum.USER_INFOLIST, parameters, cts.Token);

            // Assert
            Assert.IsNotNull(result, "Result should not be null even when canceled");
            // When canceled before connection, returns empty CommData
            Assert.AreEqual(CommMessageType.Unknown, result.Type, "Type should be Unknown for canceled request");
        }

        [TestMethod]
        public async Task SendRequestAsync_WithoutConnection_ReturnsEmptyCommData()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var parameters = new JsonObject { ["test"] = "value" };
            var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(100)); // Short timeout

            // Act
            var result = await protocol.SendRequestAsync(RequestNum.USER_INFOLIST, parameters, cts.Token);

            // Assert
            Assert.IsNotNull(result, "Result should not be null");
            Assert.AreEqual(CommMessageType.Unknown, result.Type, "Type should be Unknown when no connection");
        }

        #endregion

        #region Helper Methods

        /// <summary>
        /// Invokes the private static UpdateJsonBalance method using reflection
        /// </summary>
        private static void InvokeUpdateJsonBalance(string str, ref int balance, ref int endIndex)
        {
            var method = typeof(SocketServerCommProtocol).GetMethod(
                "UpdateJsonBalance",
                BindingFlags.NonPublic | BindingFlags.Static);

            Assert.IsNotNull(method, "UpdateJsonBalance method should exist");

            object[] parameters = new object[] { str, balance, endIndex };
            method.Invoke(null, parameters);
            
            balance = (int)parameters[1];
            endIndex = (int)parameters[2];
        }

        /// <summary>
        /// Gets the NextId property using reflection
        /// </summary>
        private static PropertyInfo GetNextIdProperty()
        {
            var property = typeof(SocketServerCommProtocol).GetProperty(
                "NextId",
                BindingFlags.NonPublic | BindingFlags.Instance);

            Assert.IsNotNull(property, "NextId property should exist");
            return property;
        }

        /// <summary>
        /// Gets the _requestIdCounter field using reflection
        /// </summary>
        private static FieldInfo GetRequestIdCounterField()
        {
            var field = typeof(SocketServerCommProtocol).GetField(
                "_requestIdCounter",
                BindingFlags.NonPublic | BindingFlags.Instance);

            Assert.IsNotNull(field, "_requestIdCounter field should exist");
            return field;
        }

        #endregion
    }
}
