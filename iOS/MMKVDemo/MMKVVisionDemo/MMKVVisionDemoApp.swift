//
//  MMKVVisionDemoApp.swift
//  MMKVVisionDemo
//
//  Created by Cyril Bonaccini on 15/02/2024.
//  Copyright © 2024 Lingol. All rights reserved.
//

import SwiftUI
import MMKV

@main
struct MMKVVisionDemoApp: App {
    init() {
        MMKV.initialize(rootDir: nil)
    }
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
