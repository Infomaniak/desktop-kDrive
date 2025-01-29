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

#import <Cocoa/Cocoa.h>
#import <SystemExtensions/SystemExtensions.h>

@interface SystemExtensionRequestDelegate : NSObject <OSSystemExtensionRequestDelegate>
{
    dispatch_semaphore_t endSemaphore;
}

- (intptr_t) wait;

@end	

// Test app (Extension installer)
int main(int argc, const char *argv[]) {
    if (@available(macOS 10.15, *)) {
        NSBundle *appBundle = [NSBundle mainBundle];
        NSString *liteSyncExtMachName = [appBundle objectForInfoDictionaryKey:@"LiteSyncExtMachName"];
        if (liteSyncExtMachName == nil) {
            NSLog(@"[KD] No LiteSyncExtMachName");
            exit(EXIT_FAILURE);
        }
        
        // Activate extension
        SystemExtensionRequestDelegate *delegate = [[SystemExtensionRequestDelegate alloc] init];
     
        OSSystemExtensionRequest *osreqInst = [OSSystemExtensionRequest
            activationRequestForExtension:liteSyncExtMachName
            queue:dispatch_get_main_queue()];
        osreqInst.delegate = delegate;
        
        [OSSystemExtensionManager.sharedManager submitRequest:osreqInst];
        
        NSLog(@"[KD] Activation request sent");
        
        while ([delegate wait]) {
                [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0]];
        }
        
        NSLog(@"[KD] Activation request returned");
    }
    
    [[NSApplication sharedApplication] terminate:NULL];
}

@implementation SystemExtensionRequestDelegate

- (id) init {
    if (self = [super init]) {
        endSemaphore = dispatch_semaphore_create(0);
    }
    return self;
}

- (OSSystemExtensionReplacementAction) request:
    (nonnull OSSystemExtensionRequest *) request
    actionForReplacingExtension:(nonnull OSSystemExtensionProperties *) existing
    withExtension:(nonnull OSSystemExtensionProperties *) ext {
    NSLog(@"[KD] Replacing extension %@ version %@ with version %@", request.identifier, existing.bundleShortVersion, ext.bundleShortVersion);
    return OSSystemExtensionReplacementActionReplace;
}

- (void) request:
    (nonnull OSSystemExtensionRequest *) request
    didFailWithError:(nonnull NSError *) error {
    NSLog(@"[KD] ERROR: System extension request failed: %@", error.localizedDescription);
    dispatch_semaphore_signal(endSemaphore);
}
 
- (void) request:
    (nonnull OSSystemExtensionRequest *) request
    didFinishWithResult:(OSSystemExtensionRequestResult) result {
    if(result == OSSystemExtensionRequestWillCompleteAfterReboot) {
        NSLog(@"[KD] System extension will apply after reboot. %ld", (long)result);
    }
    else {
        NSLog(@"[KD] Extension %@ registered", request.identifier);
    }
    dispatch_semaphore_signal(endSemaphore);
}
 
- (void) requestNeedsUserApproval:(nonnull OSSystemExtensionRequest *)request {
    NSLog(@"[KD] Extension %@ requires user approval", request.identifier);
}

- (intptr_t) wait {
    return dispatch_semaphore_wait(endSemaphore, DISPATCH_TIME_NOW);
}

@end
