//
//  AppDelegate.swift
//  kdrive-desktop-poc
//
//  Created by chrilarc on 10.04.2025.
//

import Cocoa

@main
class AppDelegate: NSObject, NSApplicationDelegate {
    var _serverConnection: NSXPCConnection?
    var _serverGuiService: XPCGuiProtocol?
    var _queryId: Int = 1000000
        
    override init() {
        super.init()
    }
    
    func queryId() -> Int {
        self._queryId += 1
        return self._queryId
    }
    
    func applicationDidFinishLaunching(_ aNotification: Notification) {
        print("[KD] Initialize connection with login item agent")
        let connection = NSXPCConnection(machServiceName: "864VDCS2QY.com.infomaniak.drive.desktopclient.LoginItemAgent")
        print("[KD] Set exported interface for connection with login agent")
        connection.exportedInterface = NSXPCInterface(with: XPCLoginItemRemoteProtocol.self)
        connection.exportedObject = self
        print("[KD] Set remote object interface for connection with login agent")
        connection.remoteObjectInterface = NSXPCInterface(with: XPCLoginItemProtocol.self)
        print("[KD] Resume connection with login item agent")
        connection.activate()
        print("[KD] Set remote object proxy for connection with login agent")
        let loginItemService: XPCLoginItemProtocol? = connection.remoteObjectProxyWithErrorHandler({ error in
            print("[KD] Connection with login item agent invalidated/interrupted:", error)
            return
        }) as? XPCLoginItemProtocol
        print("[KD] Get server gui endpoint from login item agent")
        loginItemService?.serverGuiEndpoint({ endPoint in
            if endPoint != nil {
                print("[KD] Server gui endpoint received \(endPoint!)")
                self.connectToServer(endPoint: endPoint!)
            }
        })
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }
    
    func connectToServer(endPoint: NSXPCListenerEndpoint) -> Void {
        if _serverConnection != nil {
            print("[KD] Already connected to server")
            return
        }
        
        print("[KD] Setup connection with server")
        _serverConnection = NSXPCConnection(listenerEndpoint: endPoint)
        print("[KD] Set exported interface for connection with server")
        _serverConnection!.exportedInterface = NSXPCInterface(with: XPCGuiRemoteProtocol.self)
        _serverConnection!.exportedObject = self
        print("[KD] Set remote object interface for connection with server")
        _serverConnection!.remoteObjectInterface = NSXPCInterface(with: XPCGuiProtocol.self)
        print("[KD] Resume connection with server")
        _serverConnection!.activate()
        print("[KD] Set remote object proxy for connection with server")
        _serverGuiService = _serverConnection!.remoteObjectProxyWithErrorHandler({ error in
            print("[KD] Connection with server invalidated/interrupted:", error)
            self._serverConnection = nil
            self._serverGuiService = nil
            return
        }) as? XPCGuiProtocol
    }
    
    func sendQuery(queryType: Int, msg: String) {
        let queryId = queryId()
        let query = "\(queryId);\(queryType);\(msg)"
        print("[KD] Send query \(query)")
        _serverGuiService?.sendQuery(query.data(using: .utf8))
    }

    // XPCLoginItemRemoteProtocol protocol implementation
    @objc func processType(_ callback: (ProcessType) -> Void) {
        print("[KD] Process type asked")
        callback(client)
    }
    
    @objc func serverIsRunning(endPoint: NSXPCListenerEndpoint) {
        print("[KD] Server is running")
        self.connectToServer(endPoint: endPoint)
    }
    
    // XPCGuiRemoteProtocol protocol implementation
    @objc func sendSignal(_ msg: Data) {
        let msgStr = String(data: msg, encoding: .utf8)!
        print("[KD] Signal received \(msgStr)")
    }
}

