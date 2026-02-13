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
    /// <summary>
    /// Extended tests for message handling and signal processing in SocketServerCommProtocol
    /// </summary>
    [TestClass]
    public class SocketServerCommProtocolExtendedTests
    {
        #region HandleServerMessageAsync Tests

        [TestMethod]
        public void HandleServerMessage_WithRequestType_InvokesPendingRequest()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var tcs = new TaskCompletionSource<IServerCommProtocol.CommData>(
                TaskCreationOptions.RunContinuationsAsynchronously);
            
            var pendingRequests = GetPendingRequestsField();
            var requests = (System.Collections.Concurrent.ConcurrentDictionary<long, TaskCompletionSource<IServerCommProtocol.CommData>>)
                pendingRequests.GetValue(protocol)!;
            requests[123] = tcs;

            var commData = new IServerCommProtocol.CommData
            {
                Type = CommMessageType.Request,
                Id = 123,
                RequestNum = RequestNum.USER_INFOLIST,
                Code = ExitCode.Ok
            };

            // Act
            InvokeHandleServerMessage(protocol, commData);
            
            // Give async operations time to complete
            var completed = tcs.Task.Wait(TimeSpan.FromMilliseconds(500));

            // Assert
            Assert.IsTrue(completed, "TaskCompletionSource should have been set");
            Assert.AreEqual(commData, tcs.Task.Result, "Result should match the sent data");
        }

        [TestMethod]
        public void HandleServerMessage_WithUnknownRequestId_DoesNotThrow()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var commData = new IServerCommProtocol.CommData
            {
                Type = CommMessageType.Request,
                Id = 999, // Non-existent ID
                RequestNum = RequestNum.USER_INFOLIST
            };

            // Act & Assert - should not throw
            InvokeHandleServerMessage(protocol, commData);
        }

        [TestMethod]
        public void HandleServerMessage_WithSignalType_RaisesSignalEvent()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            bool eventRaised = false;
            SignalNum receivedSignal = SignalNum.Unknown;
            JsonObject? receivedParams = null;

            protocol.SignalReceived += (sender, args) =>
            {
                eventRaised = true;
                receivedSignal = args.SignalNum;
                receivedParams = args.SignalData;
            };

            var parameters = new JsonObject { ["driveId"] = 456 };
            var commData = new IServerCommProtocol.CommData
            {
                Type = CommMessageType.Signal,
                SignalNum = SignalNum.DRIVE_ADDED,
                Params = parameters
            };

            // Act
            InvokeHandleServerMessage(protocol, commData);

            // Assert
            Assert.IsTrue(eventRaised, "Signal event should have been raised");
            Assert.AreEqual(SignalNum.DRIVE_ADDED, receivedSignal, "Signal number should match");
            Assert.AreEqual(parameters, receivedParams, "Parameters should match");
        }

        [TestMethod]
        public void HandleServerMessage_WithSignalTypeAndNullParams_UsesEmptyJsonObject()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            JsonObject? receivedParams = null;

            protocol.SignalReceived += (sender, args) =>
            {
                receivedParams = args.SignalData;
            };

            var commData = new IServerCommProtocol.CommData
            {
                Type = CommMessageType.Signal,
                SignalNum = SignalNum.USER_ADDED,
                Params = null! // Null params
            };

            // Act
            InvokeHandleServerMessage(protocol, commData);

            // Assert
            Assert.IsNotNull(receivedParams, "Params should not be null");
            Assert.AreEqual(0, receivedParams.Count, "Params should be empty");
        }

        [TestMethod]
        public void HandleServerMessage_WithUnknownMessageType_DoesNotThrow()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var commData = new IServerCommProtocol.CommData
            {
                Type = (CommMessageType)999, // Invalid type
                Id = 1
            };

            // Act & Assert - should not throw
            InvokeHandleServerMessage(protocol, commData);
        }

        #endregion

        #region RaiseSignal Tests

        [TestMethod]
        public void RaiseSignal_WithSubscriber_InvokesEvent()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            bool eventRaised = false;
            SignalNum receivedSignal = SignalNum.Unknown;

            protocol.SignalReceived += (sender, args) =>
            {
                eventRaised = true;
                receivedSignal = args.SignalNum;
            };

            var parameters = new JsonObject();

            // Act
            InvokeRaiseSignal(protocol, SignalNum.SYNC_ADDED, parameters);

            // Assert
            Assert.IsTrue(eventRaised, "Event should have been raised");
            Assert.AreEqual(SignalNum.SYNC_ADDED, receivedSignal, "Signal should match");
        }

        [TestMethod]
        public void RaiseSignal_WithoutSubscriber_DoesNotThrow()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var parameters = new JsonObject();

            // Act & Assert - should not throw
            InvokeRaiseSignal(protocol, SignalNum.SYNC_REMOVED, parameters);
        }

        [TestMethod]
        public void RaiseSignal_WithMultipleSubscribers_InvokesAll()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            int eventRaisedCount = 0;

            EventHandler<IServerCommProtocol.SignalEventArgs> handler1 = (s, e) => { eventRaisedCount++; };
            EventHandler<IServerCommProtocol.SignalEventArgs> handler2 = (s, e) => { eventRaisedCount++; };
            EventHandler<IServerCommProtocol.SignalEventArgs> handler3 = (s, e) => { eventRaisedCount++; };

            protocol.SignalReceived += handler1;
            protocol.SignalReceived += handler2;
            protocol.SignalReceived += handler3;

            var parameters = new JsonObject();

            // Act
            InvokeRaiseSignal(protocol, SignalNum.USER_UPDATED, parameters);

            // Assert
            Assert.AreEqual(3, eventRaisedCount, "All three subscribers should have been invoked");
        }

        #endregion

        #region UpdateJsonBalance Edge Cases

        [TestMethod]
        public void UpdateJsonBalance_WithMixedContent_IgnoresNonBraceCharacters()
        {
            // Arrange
            string input = "abc{def}ghi";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0");
            Assert.AreEqual(7, endIndex, "EndIndex should point to closing brace at position 7");
        }

        [TestMethod]
        public void UpdateJsonBalance_WithJsonString_FindsCorrectEnd()
        {
            // Arrange - This is similar to actual JSON with quotes and commas
            string input = "{\"key\":\"value\",\"num\":42}";
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0");
            Assert.AreEqual(input.Length - 1, endIndex, "EndIndex should be at last character");
        }

        [TestMethod]
        public void UpdateJsonBalance_LargeNestingDepth_HandlesCorrectly()
        {
            // Arrange - 20 levels deep
            string input = new string('{', 20) + new string('}', 20);
            int balance = 0;
            int endIndex = -1;

            // Act
            InvokeUpdateJsonBalance(input, ref balance, ref endIndex);

            // Assert
            Assert.AreEqual(0, balance, "Balance should be 0 for balanced deep nesting");
            Assert.AreEqual(39, endIndex, "EndIndex should be at last closing brace");
        }

        #endregion

        #region CommData Integration Tests

        [TestMethod]
        public void CommData_AllProperties_CanBeSetAndRetrieved()
        {
            // Arrange & Act
            var commData = new IServerCommProtocol.CommData
            {
                Type = CommMessageType.Request,
                Id = 42,
                RequestNum = RequestNum.SYNC_INFOLIST,
                Code = ExitCode.Ok,
                Cause = ExitCause.HttpErr,
                Params = new JsonObject { ["test"] = "data" }
            };

            // Assert
            Assert.AreEqual(CommMessageType.Request, commData.Type);
            Assert.AreEqual(42, commData.Id);
            Assert.AreEqual(RequestNum.SYNC_INFOLIST, commData.RequestNum);
            Assert.AreEqual((int)RequestNum.SYNC_INFOLIST, commData.Num);
            Assert.AreEqual(ExitCode.Ok, commData.Code);
            Assert.AreEqual(ExitCause.HttpErr, commData.Cause);
            Assert.IsNotNull(commData.Params);
            Assert.AreEqual("data", commData.Params["test"]?.ToString());
        }

        [TestMethod]
        public void CommData_SignalAndRequestNum_CanBeSwitched()
        {
            // Arrange
            var commData = new IServerCommProtocol.CommData();

            // Act - Set as RequestNum first
            commData.RequestNum = RequestNum.USER_INFOLIST;
            int numAfterRequest = commData.Num;

            // Then set as SignalNum
            commData.SignalNum = SignalNum.DRIVE_UPDATED;
            int numAfterSignal = commData.Num;

            // Assert
            Assert.AreEqual((int)RequestNum.USER_INFOLIST, numAfterRequest);
            Assert.AreEqual((int)SignalNum.DRIVE_UPDATED, numAfterSignal);
            Assert.AreEqual(SignalNum.DRIVE_UPDATED, commData.SignalNum);
        }

        #endregion

        #region Concurrent Request Handling Tests

        [TestMethod]
        public async Task PendingRequests_ConcurrentOperations_AreThreadSafe()
        {
            // Arrange
            var protocol = new SocketServerCommProtocol();
            var pendingRequests = GetPendingRequestsField();
            var requests = (System.Collections.Concurrent.ConcurrentDictionary<long, TaskCompletionSource<IServerCommProtocol.CommData>>)
                pendingRequests.GetValue(protocol)!;

            var tasks = new List<Task>();
            const int taskCount = 100;

            // Act - Add many requests concurrently
            for (int i = 0; i < taskCount; i++)
            {
                int id = i;
                var task = Task.Run(() =>
                {
                    var tcs = new TaskCompletionSource<IServerCommProtocol.CommData>(
                        TaskCreationOptions.RunContinuationsAsynchronously);
                    requests[id] = tcs;
                });
                tasks.Add(task);
            }

            await Task.WhenAll(tasks);

            // Assert
            Assert.AreEqual(taskCount, requests.Count, "All requests should have been added");
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
        /// Invokes the private HandleServerMessageAsync method using reflection
        /// </summary>
        private static void InvokeHandleServerMessage(SocketServerCommProtocol protocol, IServerCommProtocol.CommData data)
        {
            var method = typeof(SocketServerCommProtocol).GetMethod(
                "HandleServerMessageAsync",
                BindingFlags.NonPublic | BindingFlags.Instance);

            Assert.IsNotNull(method, "HandleServerMessageAsync method should exist");
            method.Invoke(protocol, new object[] { data });
        }

        /// <summary>
        /// Invokes the private RaiseSignal method using reflection
        /// </summary>
        private static void InvokeRaiseSignal(SocketServerCommProtocol protocol, SignalNum signalNum, JsonObject parameters)
        {
            var method = typeof(SocketServerCommProtocol).GetMethod(
                "RaiseSignal",
                BindingFlags.NonPublic | BindingFlags.Instance);

            Assert.IsNotNull(method, "RaiseSignal method should exist");
            method.Invoke(protocol, new object[] { signalNum, parameters });
        }

        /// <summary>
        /// Gets the _pendingRequests field using reflection
        /// </summary>
        private static FieldInfo GetPendingRequestsField()
        {
            var field = typeof(SocketServerCommProtocol).GetField(
                "_pendingRequests",
                BindingFlags.NonPublic | BindingFlags.Instance);

            Assert.IsNotNull(field, "_pendingRequests field should exist");
            return field;
        }

        #endregion
    }
}
