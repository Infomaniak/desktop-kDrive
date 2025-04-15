//
//  ViewController.swift
//  kdrive-desktop-poc
//
//  Created by chrilarc on 10.04.2025.
//

import Cocoa

class ViewController: NSViewController {
    @IBOutlet weak var syncIdLabel: NSTextField!
    @IBOutlet weak var syncIdField: NSTextField!
    
    override func viewDidLoad() {
        super.viewDidLoad()

        // Do any additional setup after loading the view.
    }

    override var representedObject: Any? {
        didSet {
        // Update the view, if already loaded.
        }
    }
    
    @IBAction func pauseClicked(_ sender: NSButton) {
        let msg = "\(syncIdField.intValue)"
        if let appDelegate = NSApplication.shared.delegate as? AppDelegate {
            appDelegate.sendQuery(queryType: 1, msg: msg)
        }
    }
    
    @IBAction func resumeClicked(_ sender: NSButton) {
        let msg = "\(syncIdField.intValue)"
        if let appDelegate = NSApplication.shared.delegate as? AppDelegate {
            appDelegate.sendQuery(queryType: 2, msg: msg)
        }
    }
}

